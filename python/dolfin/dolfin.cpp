// Copyright (C) 2017 Garth N. Wells
//
// This file is part of FFC.
//
// FFC is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// FFC is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with FFC. If not, see <http://www.gnu.org/licenses/>.


#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace dolfin_wrappers
{
  void mesh(py::module& m);
}


PYBIND11_PLUGIN(dolfin_test)
{
  // Create module
  py::module m("dolfin_test", "DOLFIN Python interface");

  //m.def("add", &add, "A function which adds two numbers");

  // Create Mesh submodule
  py::module mesh = m.def_submodule("mesh", "DOLFIN mesh library");
  dolfin_wrappers::mesh(mesh);

  // Create dofmap submodule
  //py::module dofmap = m.def_submodule("dofmap", "UFC dofmap");
  //ufc_wrappers::dofmap(dofmap);

  return m.ptr();
}
