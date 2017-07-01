// Copyright (C) 2017 Chris Richardson and Garth N. Wells
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

#include <iostream>
#include <memory>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <dolfin/la/Matrix.h>
#include <dolfin/la/GenericVector.h>
#include <dolfin/la/Vector.h>

namespace py = pybind11;

namespace dolfin_wrappers
{
  void la(py::module& m)
  {

    // dolfin::Matrix class
    py::class_<dolfin::Matrix, std::shared_ptr<dolfin::Matrix>>
      (m, "Matrix", "DOLFIN Matrix object")
      .def(py::init<MPI_Comm>());

    //-----------------------------------------------------------------------------
    // dolfin::Vector class
    py::class_<dolfin::Vector, std::shared_ptr<dolfin::Vector>>
      (m, "Vector", "DOLFIN Vector object")
      .def(py::init<MPI_Comm>());

    //-----------------------------------------------------------------------------
    // dolfin::Vector class
    py::class_<dolfin::GenericVector, std::shared_ptr<dolfin::GenericVector>>
      (m, "GenericVector", "DOLFIN GenericVector object");


  }

}
