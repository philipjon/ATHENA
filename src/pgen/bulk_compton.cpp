//======================================================================================
/* Athena++ astrophysical MHD code
 * Copyright (C) 2014 James M. Stone  <jmstone@princeton.edu>
 *
 * This program is free software: you can redistribute and/or modify it under the terms
 * of the GNU General Public License (GPL) as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of GNU GPL in the file LICENSE included in the code
 * distribution.  If not see <http://www.gnu.org/licenses/>.
 *====================================================================================*/

// C++ headers
#include <iostream>   // endl
#include <fstream>
#include <sstream>    // stringstream
#include <stdexcept>  // runtime_error
#include <string>     // c_str()
#include <cmath>      // sqrt
#include <algorithm>  // min

// Athena++ headers
#include "../athena.hpp"
#include "../athena_arrays.hpp"
#include "../mesh/mesh.hpp"
#include "../parameter_input.hpp"
#include "../hydro/hydro.hpp"
#include "../eos/eos.hpp"
#include "../bvals/bvals.hpp"
#include "../hydro/srcterms/hydro_srcterms.hpp"
#include "../field/field.hpp"
#include "../coordinates/coordinates.hpp"
#include "../radiation/radiation.hpp"
#include "../radiation/integrators/rad_integrators.hpp"


//======================================================================================
/*! \file beam.cpp
 *  \brief Beam test for the radiative transfer module
 *
 *====================================================================================*/




//======================================================================================
//! \fn void MeshBlock::ProblemGenerator(ParameterInput *pin)
//  \brief beam test
//======================================================================================
void MeshBlock::ProblemGenerator(ParameterInput *pin)
{

  Real tgas, er1,er2,er3, sigma1,sigma2,sigma3;
  Real vmax = 3.e2;

  er1 = pin->GetOrAddReal("problem","er_1",10.0);
  er2 = pin->GetOrAddReal("problem","er_2",20.0);
  er3 = pin->GetOrAddReal("problem","er_3",30.0);    
  tgas = pin->GetOrAddReal("problem","tgas",1.0);
  sigma1 = pin->GetOrAddReal("problem","sigma_1",100.0);
  sigma2 = pin->GetOrAddReal("problem","sigma_2",200.0);
  sigma3 = pin->GetOrAddReal("problem","sigma_3",300.0);
  
  Real gamma = peos->GetGamma();
  
  // Initialize hydro variable
  for(int k=ks; k<=ke; ++k) {
    for (int j=js; j<=je; ++j) {
      for (int i=is; i<=ie; ++i) {
        Real xpos = pcoord->x1v(i);
        Real ypos = pcoord->x2v(j);
        phydro->u(IDN,k,j,i) = 1.0;
        phydro->u(IM1,k,j,i) = vmax*sin(xpos*PI*2.0); 
        phydro->u(IM2,k,j,i) = 0.0;
        phydro->u(IM3,k,j,i) = 0.0;
        if (NON_BAROTROPIC_EOS){

          phydro->u(IEN,k,j,i) = tgas/(gamma-1.0);
          phydro->u(IEN,k,j,i) += 0.5*SQR(phydro->u(IM1,k,j,i))/phydro->u(IDN,k,j,i);
          phydro->u(IEN,k,j,i) += 0.5*SQR(phydro->u(IM2,k,j,i))/phydro->u(IDN,k,j,i);
          phydro->u(IEN,k,j,i) += 0.5*SQR(phydro->u(IM3,k,j,i))/phydro->u(IDN,k,j,i);
        }
      }
    }
  }
  
  //Now initialize opacity and specific intensity
  if(RADIATION_ENABLED || IM_RADIATION_ENABLED){
    int nfreq = prad->nfreq;
    int nang = prad->nang;
    AthenaArray<Real> ir_cm;
    ir_cm.NewAthenaArray(prad->n_fre_ang);

    Real *ir_lab;
    
    for(int k=ks; k<=ke; ++k) {
      for (int j=js; j<=je; ++j) {
        for (int i=is; i<=ie; ++i) {

          for(int ifr=0; ifr<prad->nfreq; ++ifr){
            ir_lab = &(prad->ir(k,j,i,ifr*nang));
            Real emission = 1.0;
            // Initialize with blackbody spectrum
            if(ifr == nfreq-1){
              emission *= (1.0-prad->FitBlackBody(prad->nu_grid(ifr)));
            }else{
              emission *= prad->BlackBodySpec(prad->nu_grid(ifr),
                                      prad->nu_grid(ifr+1));
            }

/*            if(ifr == 0)
              er_ini = er1;
            else if(ifr == 1)
              er_ini = er2;
            else
              er_ini = er3;          
*/        
//          prad->pradintegrator->ComToLab(vx,vy,vz,mux,muy,muz,ir_cm,ir_lab);

            for(int n=0; n<prad->nang; n++){
               ir_lab[n] = emission;
            }

          }// end ifr
          
        }
      }
    }

    prad->kappa_es = 10.0;

    for(int k=0; k<ncells3; ++k)
     for(int j=0; j<ncells2; ++j)
       for(int i=0; i<ncells1; ++i){
          for (int ifr=0; ifr < nfreq; ++ifr){
            if(ifr == 0){
              prad->sigma_s(k,j,i,ifr) = 10.0;
              prad->sigma_a(k,j,i,ifr) = 0.0;
              prad->sigma_pe(k,j,i,ifr) = 0.0;
              prad->sigma_p(k,j,i,ifr) = 0.0;
            }else if(ifr == 1){
              prad->sigma_s(k,j,i,ifr) = 10.0;
              prad->sigma_a(k,j,i,ifr) = 0.0;
              prad->sigma_pe(k,j,i,ifr) = 0.0;
              prad->sigma_p(k,j,i,ifr) = 0.0;
            }else{
              prad->sigma_s(k,j,i,ifr) = 10.0;
              prad->sigma_a(k,j,i,ifr) = 0.0;
              prad->sigma_pe(k,j,i,ifr) = 0.0;   
              prad->sigma_p(k,j,i,ifr) = 0.0;          
            }

          }

       }
    
    ir_cm.DeleteAthenaArray();
    
  }// End Rad
  
  return;
}


void MeshBlock::UserWorkInLoop(void)
{


  Real gamma = peos->GetGamma();

  Real vmax = 3.e2;
  Real tgas = 1.0;    
  for(int k=ks; k<=ke; k++) {
   for(int j=js; j<=je; j++) {
     for (int i=is; i<=ie; ++i) {
        Real xpos = pcoord->x1v(i);
        Real ypos = pcoord->x2v(j);
        phydro->u(IDN,k,j,i) = 1.0;
        phydro->u(IM1,k,j,i) = vmax*sin(xpos*PI*2.0); 
        phydro->u(IM2,k,j,i) = 0.0;
        phydro->u(IM3,k,j,i) = 0.0;
        if (NON_BAROTROPIC_EOS){

          phydro->u(IEN,k,j,i) = tgas/(gamma-1.0);
          phydro->u(IEN,k,j,i) += 0.5*SQR(phydro->u(IM1,k,j,i))/phydro->u(IDN,k,j,i);
          phydro->u(IEN,k,j,i) += 0.5*SQR(phydro->u(IM2,k,j,i))/phydro->u(IDN,k,j,i);
          phydro->u(IEN,k,j,i) += 0.5*SQR(phydro->u(IM3,k,j,i))/phydro->u(IDN,k,j,i);
        }
      }

     
   }
  }

  

}


