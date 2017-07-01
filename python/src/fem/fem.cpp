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

#include <dolfin/fem/DirichletBC.h>
#include <dolfin/fem/DofMap.h>
#include <dolfin/fem/FiniteElement.h>
#include <dolfin/function/FunctionSpace.h>
#include <dolfin/function/GenericFunction.h>
#include <dolfin/mesh/SubDomain.h>

#include <ufc.h>


namespace py = pybind11;

namespace dolfin_wrappers
{
  void fem(py::module& m)
  {
    // ufc::foo wrappers
    py::class_<ufc::finite_element, std::shared_ptr<ufc::finite_element>>
      (m, "ufc_finite_element", "UFC finite element object");
    py::class_<ufc::dofmap, std::shared_ptr<ufc::dofmap>>
      (m, "ufc_dofmap", "UFC dofmap object");

    m.def("make_ufc_finite_element",
          [](std::uintptr_t e)
          {
            ufc::finite_element * p = reinterpret_cast<ufc::finite_element *>(e);
            return std::shared_ptr<const ufc::finite_element>(p);
          });

    m.def("make_ufc_dofmap",
          [](std::uintptr_t e)
          {
            ufc::dofmap * p = reinterpret_cast<ufc::dofmap *>(e);
            return std::shared_ptr<const ufc::dofmap>(p);
          });

    // dolfin::FiniteElement class
    py::class_<dolfin::FiniteElement, std::shared_ptr<dolfin::FiniteElement>>
      (m, "FiniteElement", "DOLFIN FiniteElement object")
      .def(py::init<std::shared_ptr<const ufc::finite_element>>())
      .def("signature", &dolfin::FiniteElement::signature);

    py::class_<dolfin::GenericDofMap, std::shared_ptr<dolfin::GenericDofMap>>
      (m, "GenericDofMap", "DOLFIN DofMap object");

    // dolfin::DofMap class
    py::class_<dolfin::DofMap, std::shared_ptr<dolfin::DofMap>, dolfin::GenericDofMap>
      (m, "DofMap", "DOLFIN DofMap object")
      .def(py::init<std::shared_ptr<const ufc::dofmap>, const dolfin::Mesh&>());

    // dolfin::DirichletBC class
    py::class_<dolfin::DirichletBC, std::shared_ptr<dolfin::DirichletBC>>
      (m, "DirichletBC", "DOLFIN DirichletBC object")
      .def(py::init<std::shared_ptr<const dolfin::FunctionSpace>,
                    std::shared_ptr<const dolfin::GenericFunction>,
                    std::shared_ptr<const dolfin::SubDomain>>());

  }

}