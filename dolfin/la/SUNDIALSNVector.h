// Copyright (C) 2017
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
//
// Modified by Chris Hadjigeorgiou: 2017.
//
// First added:  2017-01-31
// Last changed: 2011-02-07

#ifndef __DOLFIN_N_VECTOR_H
#define __DOLFIN_N_VECTOR_H

#ifdef HAS_SUNDIALS

#include <string>
#include <utility>
#include <memory>
#include <dolfin/common/types.h>
#include <sundials/sundials_nvector.h>
#include "DefaultFactory.h"
#include "GenericVector.h"
#include "Vector.h"

namespace dolfin
{

  template<typename T> class Array;

  class SUNDIALSNVector
  {
  public:

    /// Create empty vector
    SUNDIALSNVector(MPI_Comm comm=MPI_COMM_WORLD)
    {
      DefaultFactory factory;
      vector = factory.create_vector(comm);
    }

    /// Create vector of size N
    SUNDIALSNVector(MPI_Comm comm, std::size_t N)
    {
      DefaultFactory factory;
      vector = factory.create_vector(comm);
      vector->init(N);
      N_V = std::make_shared<_generic_N_Vector>();
      N_V->ops = &ops;
      N_V->content = (void *)(vector.get());
    }

    /// Copy constructor
    SUNDIALSNVector(const SUNDIALSNVector& x) : vector(x.vector->copy()) {}

    /// Create an SUNDIALSNVector from a GenericVector
    SUNDIALSNVector(const GenericVector& x) : vector(x.copy())
    {
      N_V = std::make_shared<_generic_N_Vector>();
      N_V->ops = &ops;
      N_V->content = (void *)(vector.get());
    }

    //--- Implementation of N_Vector ops

    static N_Vector_ID N_VGetVectorID(N_Vector nv)
    {
      // ID for custom SUNDIALSNVector implementation
      return SUNDIALS_NVEC_CUSTOM;
    }

    static void N_VConst(double c, N_Vector z)
    {
      auto v = static_cast<GenericVector *>(z->content);
      *v = c;
    }

    static N_Vector N_VClone(N_Vector z)
    {
      auto vz = static_cast<GenericVector *>(z->content);
      auto vector = vz->copy();

      V = new N_Vector;
      V->ops = &ops;
      V->content = (void *)(vector.get());

      return &V;
    }

    static N_Vector N_VDestroy(N_Vector z)
    {
      delete &z;
    }

    static void N_VProd(N_Vector x, N_Vector y, N_Vector z)
    {
      auto vx = static_cast<GenericVector*>(x->content);
      auto vy = static_cast<GenericVector*>(y->content);

      // FIXME: should we check that z->content is actually pointing
      // to a GenericVector? e.g. dynamic_cast with try/catch?
      auto vz = static_cast<GenericVector*>(z->content);

      // Copy x to z
      *vz = *vx;
      // Multiply by y
      *vz *= *vy;
    }

    static void N_VDiv(N_Vector x, N_Vector y, N_Vector z)
    {
      // z = 1/y
      N_VInv(y, z);

      // z = z*x
      auto vx = static_cast<GenericVector *>(x->content);
      auto vz = static_cast<GenericVector *>(z->content);
      *vz *= *vx;
    }

    static void N_VScale(double c, N_Vector x, N_Vector z)
    {
      auto vx = static_cast<GenericVector *>(x->content);
      auto vz = static_cast<GenericVector *>(z->content);

      // z = c*x
      *vz = *vx;
      *vz *= c;
    }

    static void N_VAbs(N_Vector x, N_Vector z)
    {
      auto vx = static_cast<GenericVector *>(x->content);
      auto vz = static_cast<GenericVector *>(z->content);

      *vz = *vx;
      vz->abs();
    }

    static void N_VInv(N_Vector x, N_Vector z)
    {
      auto vx = static_cast<GenericVector *>(x->content);
      auto vz = static_cast<GenericVector *>(z->content);

      // z = 1/x
      std::vector<double> xvals;
      vx->get_local(xvals);
      for (auto &val : xvals)
        val = 1.0/val;
      vz->set_local(xvals);
    }

    static void N_VAddConst(N_Vector x, double c, N_Vector z)
    {
      auto vx = static_cast<GenericVector *>(x->content);
      auto vz = static_cast<GenericVector *>(z->content);

      *vz = *vx;
      *vz += c;

    }

    static double N_VDotProd(N_Vector x, N_Vector z)
    {
      auto vx = static_cast<GenericVector *>(x->content);
      auto vz = static_cast<GenericVector *>(z->content);

      //*vz *= *vx;
      return vx->inner(vz);

    }

    static double N_VMaxNorm(N_Vector x)
    {
	double c;
	return c;
    }

    static double N_VMin(N_Vector x)
    {
	double d;
	return d;
    }
    //-----------------------------------------------------------------------------

    /// Get underlying raw SUNDIALS N_Vector struct
    N_Vector nvector() const
    {
      N_V->content = (void *)(vector.get());
      return N_V.get();
    }

    /// Get underlying GenericVector
    std::shared_ptr<GenericVector> vec() const
    {
      return vector;
    }


    /// Assignment operator
    const SUNDIALSNVector& operator= (const SUNDIALSNVector& x)
    { *vector = *x.vector; return *this; }

  private:

    // Pointer to concrete implementation
    std::shared_ptr<GenericVector> vector;

    // Pointer to SUNDIALS struct
    std::shared_ptr<_generic_N_Vector> N_V;


/* Structure containing function pointers to vector operations  */
    struct _generic_N_Vector_Ops ops = {N_VGetVectorID,    //   N_Vector_ID (*N_VGetVectorID)(SUNDIALSNVector);
                                        N_VClone,    //   NVector    (*N_VClone)(NVector);
                                        NULL,    //   NVector    (*N_VCloneEmpty)(NVector);
                                        N_VDestroy,    //   void        (*N_VDestroy)(NVector);
                                        NULL,    //   void        (*N_VSpace)(NVector, long int *, long int *);
                                        NULL,    //   realtype*   (*N_VGetArrayPointer)(NVector);
                                        NULL,    //   void        (*N_VSetArrayPointer)(realtype *, NVector);
                                        N_VLinearSum,    //   void        (*N_VLinearSum)(realtype, NVector, realtype, NVector, NVector);
                                        N_VConst,          //   void        (*N_VConst)(realtype, NVector);
                                        N_VProd,    //   void        (*N_VProd)(NVector, NVector, NVector);
                                        N_VDiv,    //   void        (*N_VDiv)(NVector, NVector, NVector);
                                        N_VScale,    //   void        (*N_VScale)(realtype, NVector, NVector);
                                        N_VAbs,    //   void        (*N_VAbs)(NVector, NVector);
                                        N_VInv,    //   void        (*N_VInv)(NVector, NVector);
                                        N_VAddConst,    //   void        (*N_VAddConst)(NVector, realtype, NVector);
                                        N_VDotProd,    //   realtype    (*N_VDotProd)(NVector, NVector);
                                        N_VMaxNorm,    //   realtype    (*N_VMaxNorm)(NVector);
                                        N_VWrmsNorm,    //   realtype    (*N_VWrmsNorm)(NVector, NVector);
                                        NULL,    //   realtype    (*N_VWrmsNormMask)(NVector, NVector, NVector);
                                        N_VMin,    //   realtype    (*N_VMin)(NVector);
                                        NULL,    //   realtype    (*N_VWl2Norm)(NVector, NVector);
                                        NULL,    //   realtype    (*N_VL1Norm)(NVector);
                                        NULL,    //   void        (*N_VCompare)(realtype, NVector, NVector);
                                        NULL,    //   booleantype (*N_VInvtest)(NVector, NVector);
                                        NULL,    //   booleantype (*N_VConstrMask)(NVector, NVector, NVector);
                                        NULL};    //   realtype    (*N_VMinQuotient)(NVector, NVector);
  };


}

#endif

#endif
