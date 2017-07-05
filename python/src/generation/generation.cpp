// Copyright (C) 2017 Garth N. Wells
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

#include <dolfin/geometry/Point.h>
#include <dolfin/generation/BoxMesh.h>
#include <dolfin/generation/UnitCubeMesh.h>
#include <dolfin/generation/UnitSquareMesh.h>
#include <dolfin/generation/UnitIntervalMesh.h>
#include <dolfin/generation/UnitQuadMesh.h>
#include <dolfin/generation/IntervalMesh.h>

namespace py = pybind11;

namespace dolfin_wrappers
{

  void generation(py::module& m)
  {
    // dolfin::IntervalMesh
    py::class_<dolfin::IntervalMesh, std::shared_ptr<dolfin::IntervalMesh>, dolfin::Mesh>(m, "IntervalMesh")
      .def(py::init<std::size_t, double, double>())
      .def(py::init<MPI_Comm, std::size_t, double, double>());

    // dolfin::UnitIntervalMesh
    py::class_<dolfin::UnitIntervalMesh, std::shared_ptr<dolfin::UnitIntervalMesh>,
               dolfin::IntervalMesh, dolfin::Mesh>(m, "UnitIntervalMesh")
      .def(py::init<std::size_t>())
      .def(py::init<MPI_Comm, std::size_t>());

    // dolfin::UnitSquareMesh
    py::class_<dolfin::UnitSquareMesh, std::shared_ptr<dolfin::UnitSquareMesh>, dolfin::Mesh>(m, "UnitSquareMesh")
      .def(py::init<std::size_t, std::size_t>())
      .def(py::init<MPI_Comm, std::size_t, std::size_t>())
      .def(py::init<std::size_t, std::size_t, std::string>())
      .def(py::init<MPI_Comm, std::size_t, std::size_t, std::string>());

    // dolfin::UnitCubeMesh
    py::class_<dolfin::UnitCubeMesh, std::shared_ptr<dolfin::UnitCubeMesh>, dolfin::Mesh>(m, "UnitCubeMesh")
      .def(py::init<std::size_t, std::size_t, std::size_t>())
      .def(py::init<MPI_Comm, std::size_t, std::size_t, std::size_t>());

    // dolfin::UnitQuadMesh
    py::class_<dolfin::UnitQuadMesh, std::shared_ptr<dolfin::UnitQuadMesh>, dolfin::Mesh>(m, "UnitQuadMesh")
      .def(py::init<std::size_t, std::size_t>())
      .def(py::init<MPI_Comm, std::size_t, std::size_t>());

    // dolfin::BoxMesh
    py::class_<dolfin::BoxMesh, std::shared_ptr<dolfin::BoxMesh>, dolfin::Mesh>(m, "BoxMesh")
      .def(py::init<const dolfin::Point&, const dolfin::Point&, std::size_t, std::size_t, std::size_t>())
      .def(py::init<MPI_Comm, const dolfin::Point&, const dolfin::Point&, std::size_t, std::size_t, std::size_t>());

  }

}
