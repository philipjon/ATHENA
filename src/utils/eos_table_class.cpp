//========================================================================================
// Athena++ astrophysical MHD code
// Copyright(C) 2014 James M. Stone <jmstone@princeton.edu> and other code contributors
// Licensed under the 3-clause BSD License, see LICENSE file for details
//======================================================================================
//! \file eos_table_class.cpp
//  \brief Implements class EosTable for an EOS lookup table
//======================================================================================

// C++ headers
#include <cmath>   // sqrt()
#include <cfloat>  // FLT_MIN
#include <iostream> // ifstream
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept> // std::invalid_argument

// Athena++ headers
#include "../athena.hpp"
#include "../athena_arrays.hpp"
#include "interp_table.hpp"
#include "../parameter_input.hpp"
#include "../field/field.hpp"
#include "../coordinates/coordinates.hpp"
#include "../inputs/hdf5_reader.hpp"
#include "../inputs/ascii_table_reader.hpp"

// Order of datafields for HDF5 EOS tables
const char *var_names[] = {"p/e(e/rho,rho)", "e/p(p/rho,rho)", "asq*rho/p(p/rho,rho)",
                           "asq*rho/h(h/rho,rho)"};

//----------------------------------------------------------------------------------------
//! \fn void ReadBinaryTable(std::string fn, EosTable *peos_table)
//  \brief Read data from binary EOS table and initialize interpolated table.
void ReadBinaryTable(std::string fn, EosTable *peos_table) {
  std::ifstream eos_file(fn.c_str(), std::ios::binary);
  Real egasOverPres;
  if (eos_file.is_open()) {
    eos_file.seekg(0, std::ios::beg);
    eos_file.read(reinterpret_cast<char*>(&peos_table->nVar), sizeof(peos_table->nVar));
    eos_file.read(reinterpret_cast<char*>(&peos_table->nEgas), sizeof(peos_table->nEgas));
    eos_file.read(reinterpret_cast<char*>(&peos_table->nRho), sizeof(peos_table->nRho));
    eos_file.read(reinterpret_cast<char*>(&peos_table->logEgasMin),
                   sizeof(peos_table->logEgasMin));
    eos_file.read(reinterpret_cast<char*>(&peos_table->logEgasMax),
                   sizeof(peos_table->logEgasMax));
    eos_file.read(reinterpret_cast<char*>(&peos_table->logRhoMin),
                   sizeof(peos_table->logRhoMin));
    eos_file.read(reinterpret_cast<char*>(&peos_table->logRhoMax),
                   sizeof(peos_table->logRhoMax));
    peos_table->EosRatios.NewAthenaArray(peos_table->nVar);
    eos_file.read(reinterpret_cast<char*>(peos_table->EosRatios.data()),
                   peos_table->nVar * sizeof(peos_table->logRhoMin));
    peos_table->table.SetSize(peos_table->nVar, peos_table->nEgas, peos_table->nRho);
    peos_table->table.SetX1lim(peos_table->logRhoMin, peos_table->logRhoMax);
    peos_table->table.SetX2lim(peos_table->logEgasMin, peos_table->logEgasMax);
    eos_file.read(reinterpret_cast<char*>(peos_table->table.data.data()),
         peos_table->nVar * peos_table->nRho * peos_table->nEgas
         * sizeof(peos_table->logRhoMin));
    eos_file.close();
  }
  else throw std::invalid_argument("Unable to open eos table: " + fn);
}

//----------------------------------------------------------------------------------------
//! \fn void ReadBinaryTable(std::string fn, EosTable *peos_table, ParameterInput *pin)
//  \brief Read data from HDF5 EOS table and initialize interpolated table.
void ReadHDF5Table(std::string fn, EosTable *peos_table, ParameterInput *pin) {
#ifndef HDF5OUTPUT
  std::stringstream msg;
  msg << "### FATAL ERROR in EquationOfState::PrepEOS" << std::endl
      << "HDF5 EOS table specified, but HDF5 flag is not enabled."  << std::endl;
  throw std::runtime_error(msg.str().c_str());
#endif
  bool read_ratios = pin->GetOrAddBoolean("hydro", "EOS_read_ratios", true);
  std::string dens_lim_field =
    pin->GetOrAddString("hydro", "EOS_dens_lim_field", "LogDensLim");
  std::string espec_lim_field =
    pin->GetOrAddString("hydro", "EOS_espec_lim_field", "LogEspecLim");
  HDF5TableLoader(fn.c_str(), &peos_table->table, 4, var_names,
                  espec_lim_field.c_str(), dens_lim_field.c_str());
  peos_table->table.GetSize(peos_table->nVar, peos_table->nEgas, peos_table->nRho);
  peos_table->table.GetX2lim(peos_table->logEgasMin, peos_table->logEgasMax);
  peos_table->table.GetX1lim(peos_table->logRhoMin, peos_table->logRhoMax);
  peos_table->EosRatios.NewAthenaArray(peos_table->nVar);
  if (read_ratios) {
    std::string ratio_field=pin->GetOrAddString("hydro", "EOS_ratio_field", "ratios");
    int zero[] = {0};
    int pnVar[] = {peos_table->nVar};
    HDF5ReadRealArray(fn.c_str(), ratio_field.c_str(), 1, zero, pnVar,
                      1, zero, pnVar, peos_table->EosRatios);
    if (peos_table->EosRatios(0) <= 0) {
      std::stringstream msg;
      msg << "### FATAL ERROR in EquationOfState::PrepEOS" << std::endl
          << "Invalid ratio. " << fn.c_str() << ", " << ratio_field << ", "
          << peos_table->EosRatios(0) << std::endl;
      throw std::runtime_error(msg.str().c_str());
    }
  } else {
    for (int i=0; i<peos_table->nVar; ++i) peos_table->EosRatios(i) = 1.0;
  }
}

//----------------------------------------------------------------------------------------
//! \fn void ReadAsciiTable(std::string fn, EosTable *peos_table, ParameterInput *pin)
//  \brief Read data from HDF5 EOS table and initialize interpolated table.
void ReadAsciiTable(std::string fn, EosTable *peos_table, ParameterInput *pin) {
  bool read_ratios = pin->GetOrAddBoolean("hydro", "EOS_read_ratios", true);
  AthenaArray<Real> *pratios = NULL;
  if (read_ratios) pratios = &peos_table->EosRatios;
  ASCIITableLoader(fn.c_str(), peos_table->table, pratios);
  peos_table->table.GetSize(peos_table->nVar, peos_table->nEgas, peos_table->nRho);
  peos_table->table.GetX2lim(peos_table->logEgasMin, peos_table->logEgasMax);
  peos_table->table.GetX1lim(peos_table->logRhoMin, peos_table->logRhoMax);
  if (!read_ratios) {
    for (int i=0; i<peos_table->nVar; ++i) peos_table->EosRatios(i) = 1.0;
  }
}

// EosTable constructor
EosTable::EosTable (ParameterInput *pin) {
  std::string EOS_fn, EOS_file_type;
  EOS_fn = pin->GetString("hydro", "EOS_file_name");
  EOS_file_type = pin->GetString("hydro", "EOS_file_type");
  bool read_ratios = pin->GetOrAddBoolean("hydro", "EOS_read_ratios", true);
  rhoUnit = pin->GetOrAddReal("hydro", "EosRhoUnit", 1.0);
  eUnit = pin->GetOrAddReal("hydro", "EosEgasUnit", 1.0);
  hUnit = eUnit/rhoUnit;
  table = InterpTable2D();

  if (EOS_file_type.compare("binary") == 0) { //Raw binary
    ReadBinaryTable(EOS_fn, this);
  } else if (EOS_file_type.compare("hdf5") == 0) { // HDF5 table
    ReadHDF5Table(EOS_fn, this, pin);
  } else if (EOS_file_type.compare("ascii") == 0) { // ASCII/text table
    ReadAsciiTable(EOS_fn, this, pin);
  } else {
    std::stringstream msg;
    msg << "### FATAL ERROR in EosTable::EosTable" << std::endl
        << "EOS table of type '" << EOS_file_type << "' not recognized."  << std::endl
        << "Options are 'ascii', 'binary', and 'hdf5'." << std::endl;
    throw std::runtime_error(msg.str().c_str());
  }
}

//----------------------------------------------------------------------------------------
//! \fn Real EosTable::GetEosData(int kOut, Real var, Real rho)
//  \brief Gets interpolated data from EOS table assuming 'var' has dimensions
//         of energy per volume.
Real EosTable::GetEosData(int kOut, Real var, Real rho) {
  Real x1 = std::log10(rho * rhoUnit);
  Real x2 = std::log10(var * EosRatios(kOut) * eUnit) - x1;
  return std::pow((Real)10, table.interpolate(kOut, x2, x1));
}

// EosTable destructor
EosTable::~EosTable() {
  EosRatios.DeleteAthenaArray();
}
