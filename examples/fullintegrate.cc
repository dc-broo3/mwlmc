/*
example code:
integrate an orbit in the MW-LMC potential

standalone compile string (MacOSX):

clang++ --std=c++17 -I/usr/local/include/eigen3/ -I../include/ fullintegrate.cc -o obj/fullintegrate

(assumes that eigen3 headers are in /usr/local/include/eigen3/)

MSP 19 Apr 2022 cleaned version v0.2.1

*/

#include <iostream>
#include <iomanip>
#include <fstream>
#include <math.h>
#include <stdio.h>

// eigen includes
#include <Eigen/Dense>
using Eigen::MatrixXd;

// set model parameters
#include "modelfiles.h"

// the spherical expansion headers
#include "sphexpansion.h"

// the cylindrical expansion headers
#include "cylexpansion.h"


class MWLMC
{
private:

  void initialise();

public:


  SphExpansion* MW;
  SphExpansion* LMC;
  CylExpansion* MWD;

  // the constructor (no type, no arguments, all defined in modelfiles.h)
  MWLMC();

  // print an orbit array
  void print_orbit(MatrixXd orbit, string orbitfile);

  // return total forces
  void all_forces(MatrixXd mwcoefs, MatrixXd lmccoefs, MatrixXd mwdcoscoefs, MatrixXd mwdsincoefs,
                               double t, double x, double y, double z,
                               double& fx, double& fy, double& fz, bool verbose);

  void orbit(vector<double> xinit,
        vector<double> vinit,
        int nint,
        double dt,
        MatrixXd& orbit,
    bool fixedtime=false);

};



MWLMC::MWLMC()
{
  initialise();
};

void MWLMC::initialise()
{
  // MW
  cout << "Initialising MW ... " << endl;
  MW = new SphExpansion(sph_cache_name_mw, model_file_mw, coef_file_mw, orient_file_mw);

  // LMC
  cout << "Initialising LMC ... " << endl;
  LMC = new SphExpansion(sph_cache_name_lmc, model_file_lmc, coef_file_lmc, orient_file_lmc);

  // MW
  cout << "Initialising MW disc ... " << endl;
  MWD = new CylExpansion(cyl_cache_name_mw, cyl_coef_name_mw, cyl_orient_name_mw);
};


void MWLMC::print_orbit(MatrixXd orbit, string orbitfile)
{
  ofstream outorbit;
  outorbit.open(orbitfile);

  outorbit << "# t [Gyr]; x [kpc]; y [kpc]; z [kpc]; vx [km/s] ; vy [km/s] ; vz [km/s] ; f_x [km/s/s] ; f_y [km/s/s] ; f_z [km/s/s];" << endl;

  for (int i=0; i<orbit.cols(); i++) {

    outorbit << setw(14) << orbit(9,i);

    for (int j=0; j<9; j++) {
      outorbit << setw(14) << orbit(j,i);
    }
    outorbit << endl;
  }

  outorbit.close();

}


void MWLMC::all_forces(MatrixXd mwcoefs, MatrixXd lmccoefs, MatrixXd mwdcoscoefs, MatrixXd mwdsincoefs,
                  double t, double x, double y, double z,
                  double& fx, double& fy, double& fz, bool verbose)
{
  /*
    specs: take a time, x,y,z; return x,y,z forces, in physical units
    input/output units must be physical
    where x0,y0,z0 = present-day galactic centre
       and      t0 = present day (so previous times are negative)

   if we don't want to pass the entire SphExpansion objects, the necessary pieces can be broken out in a fairly straightforward way.

   */

  // zero out forces
  fx = fy = fz = 0.;

  // translate all times and positions into exp virial units
  double tvir,xvir,yvir,zvir;
  physical_to_virial_time(t,tvir);
  physical_to_virial_length(x,y,z, xvir,yvir,zvir);

  // reset time to have the correct system zero (e.g. pericentre is T=0)
  tvir += reference_time;

  //cout << "TIME=" << tvir << endl;

  // initialise the centre vectors
  vector<double> zerocoords(3),mw_centre(3),lmc_centre(3),mwd_centre(3);

  // get the present-day MWD coordinates: the zero of the system
  return_centre(reference_time, MWD->orient, zerocoords);

  //cout << "Coordinate zero:" << setw(14) << zerocoords[0] << setw(14) << zerocoords[1] << setw(14) << zerocoords[2] << endl;

  // get the centres of the expansions at the specified times in exp reference space
  return_centre(tvir,  MW->orient,  mw_centre);
  return_centre(tvir, LMC->orient, lmc_centre);
  return_centre(tvir, MWD->orient, mwd_centre);

  // shift the expansion centres to the pericentre coordinate system
  for (int j=0;j<=2;j++) {
    mw_centre[j]  -= zerocoords[j];
    lmc_centre[j] -= zerocoords[j];
    mwd_centre[j] -= zerocoords[j];
  }

  if (verbose) {
    cout << "MW virial centre (x,y,z)=(" << mw_centre[0] << ","<< mw_centre[1] << ","<< mw_centre[2] << ")" <<endl;
    cout << "MWD virial centre (x,y,z)=(" << mwd_centre[0] << ","<< mwd_centre[1] << ","<< mwd_centre[2] << ")" <<endl;
    cout << "LMC virial centre (x,y,z)=(" << lmc_centre[0] << ","<< lmc_centre[1] << ","<< lmc_centre[2] << ")" <<endl;
  }

  double rtmp,phitmp,thetatmp,r2tmp;
  double tpotl0,tpotl,fr,ft,fp;
  double fxtmp,fytmp,fztmp;

  double xphys,yphys,zphys,fxphys,fyphys,fzphys;

  // compute spherical coordinates in the frame of the MW expansion
  cartesian_to_spherical(xvir-mwd_centre[0], yvir-mwd_centre[1], zvir-mwd_centre[2], rtmp, phitmp, thetatmp);

  //cout << setw(14) << rtmp << setw(14) << phitmp << setw(14) << thetatmp << endl;

  MW->determine_fields_at_point_sph( mwcoefs,
  rtmp,thetatmp,phitmp,
  tpotl0,tpotl,
       fr,ft,fp);

  spherical_forces_to_cartesian(rtmp, phitmp, thetatmp,
  fr, fp, ft,
  fxtmp, fytmp, fztmp);

  virial_to_physical_force (fxtmp,fytmp,fztmp,fxphys,fyphys,fzphys);

  // add MW force to total
  fx += fxphys;
  fy += fyphys;
  fz += fzphys;

  r2tmp = sqrt((xvir-mwd_centre[0])*(xvir-mwd_centre[0]) + (yvir-mwd_centre[1])*(yvir-mwd_centre[1]));

  // same procedure for the disc
  MWD->determine_fields_at_point_cyl(mwdcoscoefs,mwdsincoefs,
       r2tmp,phitmp,zvir-mwd_centre[2],
       tpotl0,tpotl,
       fr,fp,fztmp);

  cylindrical_forces_to_cartesian(rtmp, phitmp,
    fr, fp,
    fxtmp, fytmp);

  virial_to_physical_force (fxtmp,fytmp,fztmp,fxphys,fyphys,fzphys);

  // add MW force to total
  fx += fxphys;
  fy += fyphys;
  fz += fzphys;

  // same procedure for LMC
  cartesian_to_spherical(xvir-lmc_centre[0], yvir-lmc_centre[1], zvir-lmc_centre[2], rtmp, phitmp, thetatmp);

  //cout << setw(14) << rtmp << setw(14) << phitmp << setw(14) << thetatmp << endl;

  LMC->determine_fields_at_point_sph(lmccoefs,
  rtmp,thetatmp,phitmp,
  tpotl0,tpotl,
       fr,ft,fp);

  spherical_forces_to_cartesian(rtmp, phitmp, thetatmp,
  fr, fp, ft,
  fxtmp, fytmp, fztmp);

  // reset to physical units
  virial_to_physical_force (fxtmp,fytmp,fztmp,fxphys,fyphys,fzphys);

  // add LMC force to total
  fx += fxphys;
  fy += fyphys;
  fz += fzphys;

}



void MWLMC::orbit(
        vector<double> xinit,
        vector<double> vinit,
        int nint,
        double dt,
        MatrixXd& orbit,
    bool fixedtime)
{
  /*

   */
  double fx,fy,fz;


  double tvir;

  // include the forces for now
  orbit.resize(10,nint);

  // cout << "Orbit dims " << orbit.rows() << " x " << orbit.cols() << endl;


  // initialise beginning values
  orbit(0,0) = xinit[0];
  orbit(1,0) = xinit[1];
  orbit(2,0) = xinit[2];
  orbit(3,0) = vinit[0];
  orbit(4,0) = vinit[1];
  orbit(5,0) = vinit[2];

  //now step forward one, using leapfrog (drift-kick-drift) integrator?
  //    https://en.wikipedia.org/wiki/Leapfrog_integration
  //
  int step = 1;

  // get the initial coefficient values: the time here is in tvir units, so always start with 0
  MatrixXd tcoefsmw,tcoefslmc;
  MW->select_coefficient_time(0., tcoefsmw,15);
  LMC->select_coefficient_time(0., tcoefslmc);

  MatrixXd mwcoscoefs,mwsincoefs;
  MWD->select_coefficient_time(0.0, mwcoscoefs, mwsincoefs);

  // not applying time offsets here; think about whether this is a problem
  double tphys;
  virial_to_physical_time(0.,tphys);
  // return forces for the initial step
  all_forces(tcoefsmw, tcoefslmc, mwcoscoefs, mwsincoefs,
       tphys, orbit(0,0),orbit(1,0),orbit(2,0),
       fx, fy, fz, false);

  orbit(6,0) = fx;
  orbit(7,0) = fy;
  orbit(8,0) = fz;

  int j;

  for (step=1; step<nint; step++) {

    // advance timestep: this is in physical units by definition.
    orbit(9,step) = dt*step;

    // find the current virial time
    physical_to_virial_time(dt*(step),tvir);

  if (fixedtime) {
  tvir = 0.0;
  } else {

      // get coefficients at the current virial time
      MW->select_coefficient_time(tvir, tcoefsmw);
      LMC->select_coefficient_time(tvir, tcoefslmc);
      MWD->select_coefficient_time(tvir, mwcoscoefs, mwsincoefs);

  }

    // advance positions
    for (j=0; j<3; j++) {
      orbit(j,step) = orbit(j,step-1)   + (orbit(j+3,step-1)*dt  )  + (0.5*orbit(j+6,step-1)  * (dt*dt));
    }

    // calculate new forces: time goes in as physical time (e.g. kpc/km/s)
    all_forces(tcoefsmw, tcoefslmc, mwcoscoefs, mwsincoefs,
         dt*(step-1), orbit(0,step),orbit(1,step),orbit(2,step),
         fx, fy, fz, false);

  orbit(6,step) = fx;
  orbit(7,step) = fy;
  orbit(8,step) = fz;

  // advance velocities
  for (j=3; j<6; j++) {
  orbit(j,step) = orbit(j,step-1) + (0.5*(orbit(j+3,step-1)+orbit(j+3,step))  * dt );
  }

  }

}






int main () {

  MWLMC* Model = new MWLMC();

  vector<double> zerocoords(3);
  return_centre(reference_time, Model->MWD->orient, zerocoords);

  cout << "Reference time:" << setw(14) << reference_time << endl;
  cout << "Coordinate zero:" << setw(14) << zerocoords[0] << setw(14) << zerocoords[1] << setw(14) << zerocoords[2] << endl;


  // get the time in exp system units by 1) converting the virial units, 2) applying time offset for present-day

  // initial position offset from centre
  vector<double> xphys(3),xinit(3);
  vector<double> vxphys(3),vxinit(3);

  // MW ORBIT z: Disc-like orbit
  xphys[0] = 0.03;
  xphys[1] = 0.0;
  xphys[2] = 0.0;
  vxphys[0] = 0.0;
  vxphys[1] = 1.9;
  vxphys[2] = 0.;

  virial_to_physical_length(xphys[0],xphys[1],xphys[2],xinit[0],xinit[1],xinit[2]);
  virial_to_physical_velocity(vxphys[0],vxphys[1],vxphys[2],vxinit[0],vxinit[1],vxinit[2]);

  cout << "Input pos/vel: " << xinit[0] << " " << xinit[1] << " " << xinit[2] << " " <<
    vxinit[0] << " " << vxinit[1] << " " << vxinit[2] << " " << endl;

  double nint=1000;

  // call this time in kpc/km/s units
  double dt;

  // force sampling at the native rate of the exp simulation as an interpolation test
  virial_to_physical_time(0.0005,dt);
  MatrixXd orbit;

  // try a solar-like orbit
  bool fixedtime=true;
  Model->orbit(xinit, vxinit, nint, dt, orbit, fixedtime);

  // print the output
  string orbitfile="tests/solarorbit.txt";
  Model->print_orbit(orbit,orbitfile);

}
