// Copyright (C) 2017 Chris Richardson and Chris Hadjigeorgiou
//
// This file is part of DOLFIN.
//
// DOLFIN is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// DOLFIN is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with DOLFIN. If not, see <http://www.gnu.org/licenses/>.

#ifndef __DOLFIN_C_VODE_H
#define __DOLFIN_C_VODE_H

#ifdef HAS_SUNDIALS

#include <dolfin/la/SUNDIALSNVector.h>

#include <cvode/cvode.h>
#include <cvode/cvode_impl.h>
#include <sunlinsol/sunlinsol_spgmr.h>
#include <sundials/sundials_dense.h>
#include <sundials/sundials_types.h>
#include <sundials/sundials_iterative.h>

namespace dolfin
{
  /// Wrapper class to SUNDIALS CVODE
  class CVode
  {
  public:

    // These enums are used by PYBIND11 to map the definitions from C
    enum LMM { cv_bdf = CV_BDF, cv_adams = CV_ADAMS };

    enum ITER { cv_functional = CV_FUNCTIONAL, cv_newton = CV_NEWTON };

    /// Constructor
    CVode(int cv_lmm, int cv_iter);
    
    /// Destructor
    virtual ~CVode();

    /// Initialise CVode
    void init(std::shared_ptr<GenericVector> u0, double atol, double rtol, long int mxsteps = 0);

    /// Advance time by timestep dt
    double step(double dt);

    /// Get current time
    double get_time() const;

    /// Set the current time
    void set_time(double t0);

    /// Overloaded function for time derivatives of u at time t
    /// Given the vector u, at time t, provide the time derivative udot.
    virtual void derivs(double t, std::shared_ptr<GenericVector> u,
                        std::shared_ptr<GenericVector> udot);

    /// Overloaded Jabocian function
    virtual int Jacobian(std::shared_ptr<GenericVector> u,
                          std::shared_ptr<GenericVector> Ju,
   		          double t, std::shared_ptr<GenericVector> y,
                          std::shared_ptr<GenericVector> fy);

    virtual int JacobianSetup(double t,
                          std::shared_ptr<GenericVector> Jv,
                          std::shared_ptr<GenericVector> y);

    std::map<std::string,double> statistics();

  private:
    // Internal callback from CVode to get time derivatives - passed on to derivs (above)
    static int f(realtype t, N_Vector u, N_Vector udot, void *user_data);

    static int fJacSetup(double t, N_Vector y, N_Vector fy, void *user_data);

    static int fJac(N_Vector u, N_Vector fu, double t, N_Vector y, N_Vector fy, void* , N_Vector tmp);

    // Vector of values - wrapper around dolfin::GenericVector
    std::shared_ptr<SUNDIALSNVector> _u;

    // SUNDIALS Linear Solver
    std::unique_ptr<_generic_SUNLinearSolver> ls;

    // Current time
    double t;
    int cv_lmm, cv_iter;

    // Pointer to CVode memory struct
    void *cvode_mem;
    SUNLinearSolver sunls;

  };

}

#endif

#endif
