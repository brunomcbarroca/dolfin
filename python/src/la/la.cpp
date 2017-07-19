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
#include <typeinfo>

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/eigen.h>
#include <pybind11/stl.h>

#include <dolfin/la/solve.h>
#include <dolfin/la/GenericLinearOperator.h>
#include <dolfin/la/GenericLinearSolver.h>
#include <dolfin/la/GenericTensor.h>
#include <dolfin/la/GenericMatrix.h>
#include <dolfin/la/GenericVector.h>
#include <dolfin/la/IndexMap.h>
#include <dolfin/la/LinearAlgebraObject.h>
#include <dolfin/la/Matrix.h>
#include <dolfin/la/Vector.h>
#include <dolfin/la/Scalar.h>
#include <dolfin/la/DefaultFactory.h>
#include <dolfin/la/EigenFactory.h>
#include <dolfin/la/EigenMatrix.h>
#include <dolfin/la/EigenVector.h>
#include <dolfin/la/PETScFactory.h>
#include <dolfin/la/PETScMatrix.h>
#include <dolfin/la/PETScVector.h>
#include <dolfin/la/LUSolver.h>
#include <dolfin/la/KrylovSolver.h>

#include "../mpi_interface.h"

namespace py = pybind11;

namespace dolfin_wrappers
{
  void la(py::module& m)
  {
    // dolfin::GenericLinearOperator class
    //py::class_<dolfin::LinearAlgerabObject, std::shared_ptr<dolfin::LinearAlgerabObject>>
    //  (m, "LinearAlgebraObject", "DOLFIN LinearAlgebraObject object");

    // dolfin::IndexMap
    py::class_<dolfin::IndexMap, std::shared_ptr<dolfin::IndexMap>> index_map(m, "IndexMap");
    index_map.def("size", &dolfin::IndexMap::size);

    py::enum_<dolfin::IndexMap::MapSize>(index_map, "MapSize")
      .value("ALL", dolfin::IndexMap::MapSize::ALL)
      .value("OWNED", dolfin::IndexMap::MapSize::OWNED)
      .value("UNOWNED", dolfin::IndexMap::MapSize::UNOWNED)
      .value("GLOBAL", dolfin::IndexMap::MapSize::GLOBAL);

    // dolfin::GenericLinearOperator class
    py::class_<dolfin::GenericLinearOperator, std::shared_ptr<dolfin::GenericLinearOperator>>
      (m, "GenericLinearOperator", "DOLFIN GenericLinearOperator object");

    // dolfin::GenericTensor class
    py::class_<dolfin::GenericTensor, std::shared_ptr<dolfin::GenericTensor>>
      (m, "GenericTensor", "DOLFIN GenericTensor object");

    // dolfin::GenericMatrix class
    py::class_<dolfin::GenericMatrix, std::shared_ptr<dolfin::GenericMatrix>,
               dolfin::GenericTensor, dolfin::GenericLinearOperator>
      (m, "GenericMatrix", "DOLFIN GenericMatrix object");

    // dolfin::GenericVector class
    py::class_<dolfin::GenericVector, std::shared_ptr<dolfin::GenericVector>,
               dolfin::GenericTensor>
      (m, "GenericVector", "DOLFIN GenericVector object")
      .def("__getitem__", [](dolfin::GenericVector& self, py::slice slice)
           { std::size_t start, stop, step, slicelength;
             if (!slice.compute(self.size(), &start, &stop, &step, &slicelength))
               throw py::error_already_set();
             if (start != 0 or stop != self.size() or step != 1)
               throw std::range_error("Only full slices are supported");
             std::vector<double> values;
             self.get_local(values);
             return values;})
      .def("__getitem__", &dolfin::GenericVector::getitem)
      .def("__setitem__", [](dolfin::GenericVector& self, py::slice slice, double value)
           { std::size_t start, stop, step, slicelength;
             if (!slice.compute(self.size(), &start, &stop, &step, &slicelength))
               throw py::error_already_set();
             if (start != 0 or stop != self.size() or step != 1)
               throw std::range_error("Only full slices are supported");
             self = value; })
      .def("__setitem__", &dolfin::GenericVector::setitem)
      .def("get_local", [](const dolfin::GenericVector& instance, const std::vector<long>& rows)
           {
             std::vector<dolfin::la_index> _rows(rows.begin(), rows.end());
             py::array_t<double> data(rows.size());
             instance.get_local(data.mutable_data(), _rows.size(), _rows.data());
             return data;
           })
      .def("set_local", [](dolfin::GenericVector& instance, std::vector<double> values)
           {
             std::vector<dolfin::la_index> indices(values.size());
             std::iota(indices.begin(), indices.end(), 0);
             instance.set_local(values.data(), values.size(), indices.data());
           });

    // dolfin::GenericLinearSolver class
    py::class_<dolfin::GenericLinearSolver, std::shared_ptr<dolfin::GenericLinearSolver>>
      (m, "GenericLinearSolver", "DOLFIN GenericLinearSolver object");

    //-----------------------------------------------------------------------------
    // dolfin::Matrix class
    py::class_<dolfin::Matrix, std::shared_ptr<dolfin::Matrix>, dolfin::GenericMatrix,
               dolfin::GenericTensor, dolfin::GenericLinearOperator>
      (m, "Matrix", "DOLFIN Matrix object")
      .def(py::init<>())
      .def(py::init<MPI_Comm>())
      .def("init_vector", &dolfin::Matrix::init_vector)
      .def("local_range", &dolfin::Matrix::local_range)
      .def("norm", &dolfin::Matrix::norm)
      .def("nnz", &dolfin::Matrix::nnz)
      .def("shared_instance", (std::shared_ptr<dolfin::LinearAlgebraObject>(dolfin::Matrix::*)())
           &dolfin::Matrix::shared_instance)
      .def("size", &dolfin::Matrix::size);

    // FIXME: Most (all?) of this belongs with GenericVector
    // dolfin::Vector class
    py::class_<dolfin::Vector, std::shared_ptr<dolfin::Vector>,
               dolfin::GenericVector, dolfin::GenericTensor>
      (m, "Vector", "DOLFIN Vector object")
      .def(py::init<>())
      .def(py::init<const dolfin::Vector&>())
      .def(py::init<MPI_Comm>())
      .def(py::init<MPI_Comm, std::size_t>())
      .def("init", (void (dolfin::Vector::*)(std::pair<std::size_t, std::size_t>))&dolfin::Vector::init)
      .def("mpi_comm", &dolfin::Vector::mpi_comm)
      .def("size", &dolfin::Vector::size)
      .def("__add__", [](dolfin::Vector& self, dolfin::Vector& v)
           {
             dolfin::Vector a(self);
             a += v;
             return a;
           })
      .def("__add__", [](dolfin::Vector& self, double b)
           {
             dolfin::Vector a(self);
             a += b;
             return a;
           })
      .def("__sub__", [](dolfin::Vector& self, dolfin::Vector& v)
           {
             dolfin::Vector a(self);
             a -= v;
             return a;
           })
      .def("__sub__", [](dolfin::Vector& self, double b)
           {
             dolfin::Vector a(self);
             a -= b;
             return a;
           })
      .def("__iadd__", (const dolfin::GenericVector& (dolfin::Vector::*)(double))
           &dolfin::Vector::operator+=)
      .def("__iadd__", (const dolfin::Vector& (dolfin::Vector::*)(const dolfin::GenericVector&))
           &dolfin::Vector::operator+=)
      .def("__isub__", (const dolfin::GenericVector& (dolfin::Vector::*)(double))
           &dolfin::Vector::operator-=)
      .def("__isub__", (const dolfin::Vector& (dolfin::Vector::*)(const dolfin::GenericVector&))
           &dolfin::Vector::operator-=)
      .def("__imul__", (const dolfin::Vector& (dolfin::Vector::*)(double))
           &dolfin::Vector::operator*=)
      .def("__imul__", (const dolfin::Vector& (dolfin::Vector::*)(const dolfin::GenericVector&))
           &dolfin::Vector::operator*=)
      .def("__itruediv__", (const dolfin::Vector& (dolfin::Vector::*)(double))
           &dolfin::Vector::operator/=)
      .def("__setitem__", [](dolfin::Vector& self, dolfin::la_index index, double value)
           { self.instance()->setitem(index, value); })
      .def("__setitem__", [](dolfin::Vector& self, py::slice slice, double value)
           { std::size_t start, stop, step, slicelength;
             if (!slice.compute(self.size(), &start, &stop, &step, &slicelength))
               throw py::error_already_set();
             if (start != 0 or stop != self.size() or step != 1)
               throw std::range_error("Only full slices are supported");
             *self.instance() = value; })
      .def("sum", (double (dolfin::Vector::*)() const) &dolfin::Vector::sum)
      .def("min", &dolfin::Vector::min)
      .def("max", &dolfin::Vector::max)
      .def("abs", &dolfin::Vector::abs)
      .def("norm", &dolfin::Vector::norm)
      .def("inner", &dolfin::Vector::inner)
      .def("axpy", &dolfin::Vector::axpy)
      .def("zero", &dolfin::Vector::zero)
      .def("local_size", &dolfin::Vector::local_size)
      .def("local_range", &dolfin::Vector::local_range)
      .def("owns_index", &dolfin::Vector::owns_index)
      .def("get_local", [](dolfin::Vector& self)
           {
             std::vector<double> values;
             self.get_local(values);
             return values;
           })
      .def("add_local", [](dolfin::Vector& self, const std::vector<double>& values)
           {
             std::vector<dolfin::la_index> indices(values.size());
             std::iota(indices.begin(), indices.end(), 0);
             self.add_local(values.data(), values.size(), indices.data());
           })
      .def("apply", &dolfin::Vector::apply)
      .def("str", &dolfin::Vector::str)
      .def("shared_instance", (std::shared_ptr<dolfin::LinearAlgebraObject>(dolfin::Vector::*)())
           &dolfin::Vector::shared_instance)
      .def("backend_type", [](dolfin::Vector& self)
           {
             // Experiment with determining backend type
             auto instance = self.instance();
             auto type_index = std::type_index(typeid(*instance));
             if (type_index == std::type_index(typeid(dolfin::EigenVector)))
               return "EigenVector";
#ifdef HAS_PETSC
             else if (type_index == std::type_index(typeid(dolfin::PETScVector)))
               return "PETScVector";
#endif
             else
               return "Unknown";
           });

    //----------------------------------------------------------------------------
    // dolfin::Scalar
    py::class_<dolfin::Scalar, std::shared_ptr<dolfin::Scalar>, dolfin::GenericTensor>
      (m, "Scalar")
      .def(py::init<>())
      .def(py::init<MPI_Comm>());

    //----------------------------------------------------------------------------
    // dolfin::GenericLinearAlgebraFactory class
    py::class_<dolfin::GenericLinearAlgebraFactory, std::shared_ptr<dolfin::GenericLinearAlgebraFactory>>
      (m, "GenericLinearAlgebraFactory", "DOLFIN GenericLinearAlgebraFactory object");

    //----------------------------------------------------------------------------
    // dolfin::DefaultFactory class
    py::class_<dolfin::DefaultFactory, std::shared_ptr<dolfin::DefaultFactory>>
      (m, "DefaultFactory", "DOLFIN DefaultFactory object")
      .def_static("factory", &dolfin::DefaultFactory::factory)
      .def("create_matrix", &dolfin::DefaultFactory::create_matrix)
      .def("create_vector", &dolfin::DefaultFactory::create_vector);

    //----------------------------------------------------------------------------
    // dolfin::EigenFactory class
    py::class_<dolfin::EigenFactory, std::shared_ptr<dolfin::EigenFactory>,
      dolfin::GenericLinearAlgebraFactory>
      (m, "EigenFactory", "DOLFIN EigenFactory object")
      .def("instance", &dolfin::EigenFactory::instance)
      .def("create_matrix", &dolfin::EigenFactory::create_matrix)
      .def("create_vector", &dolfin::EigenFactory::create_vector);

    //----------------------------------------------------------------------------
    // dolfin::EigenVector class
    py::class_<dolfin::EigenVector, std::shared_ptr<dolfin::EigenVector>,
               dolfin::GenericVector, dolfin::GenericTensor>
      (m, "EigenVector", "DOLFIN EigenVector object")
      .def(py::init<MPI_Comm, std::size_t>())
      .def("array", (Eigen::VectorXd& (dolfin::EigenVector::*)()) &dolfin::EigenVector::vec,
           py::return_value_policy::reference_internal);

    //----------------------------------------------------------------------------
    // dolfin::EigenMatrix class
    py::class_<dolfin::EigenMatrix, std::shared_ptr<dolfin::EigenMatrix>,
               dolfin::GenericMatrix, dolfin::GenericTensor, dolfin::GenericLinearOperator>
      (m, "EigenMatrix", "DOLFIN EigenMatrix object")
      .def(py::init<>())
      .def(py::init<std::size_t, std::size_t>())
      .def("array", (dolfin::EigenMatrix::eigen_matrix_type& (dolfin::EigenMatrix::*)()) &dolfin::EigenMatrix::mat,
           py::return_value_policy::reference_internal);

    #ifdef HAS_PETSC
    py::class_<dolfin::PETScObject, std::shared_ptr<dolfin::PETScObject>>(m, "PETScObject");

    // dolfin::PETScFactory class
    py::class_<dolfin::PETScFactory, std::shared_ptr<dolfin::PETScFactory>,
      dolfin::GenericLinearAlgebraFactory>
      (m, "PETScFactory", "DOLFIN PETScFactory object")
      .def("instance", &dolfin::PETScFactory::instance)
      .def("create_matrix", &dolfin::PETScFactory::create_matrix)
      .def("create_vector", &dolfin::PETScFactory::create_vector);

    //----------------------------------------------------------------------------
    // dolfin::PETScVector class
    py::class_<dolfin::PETScVector, std::shared_ptr<dolfin::PETScVector>,
               dolfin::GenericVector, dolfin::PETScObject>
      (m, "PETScVector", "DOLFIN PETScVector object")
      .def(py::init<MPI_Comm>())
      .def(py::init<MPI_Comm, std::size_t>());

    // dolfin::PETScBaseMatrix class
    py::class_<dolfin::PETScBaseMatrix, std::shared_ptr<dolfin::PETScBaseMatrix>,
               dolfin::PETScObject, dolfin::Variable>(m, "PETScBaseMatrix");

    // dolfin::PETScMatrix class
    py::class_<dolfin::PETScMatrix, std::shared_ptr<dolfin::PETScMatrix>,
               dolfin::GenericMatrix, dolfin::PETScBaseMatrix>
      (m, "PETScMatrix", "DOLFIN PETScMatrix object")
      .def(py::init<>())
      .def(py::init<MPI_Comm>());
    #endif

    //-----------------------------------------------------------------------------
    // dolfin::LUSolver class
    py::class_<dolfin::LUSolver, std::shared_ptr<dolfin::LUSolver>>
      (m, "LUSolver", "DOLFIN LUSolver object")
      .def(py::init<MPI_Comm, std::shared_ptr<const dolfin::GenericLinearOperator>,
           std::string>(),
           py::arg("comm"), py::arg("A"), py::arg("method") = "default")
      .def("solve", (std::size_t (dolfin::LUSolver::*)(dolfin::GenericVector&,
                                                       const dolfin::GenericVector&))
           &dolfin::LUSolver::solve);

    //-----------------------------------------------------------------------------
    // dolfin::KrylovSolver class
    py::class_<dolfin::KrylovSolver, std::shared_ptr<dolfin::KrylovSolver>,
               dolfin::GenericLinearSolver>
      (m, "KrylovSolver", "DOLFIN KrylovSolver object")
      .def(py::init<std::shared_ptr<const dolfin::GenericLinearOperator>,
           std::string, std::string>(), py::arg("A"),
           py::arg("method")="default", py::arg("preconditioner")="default")
      .def(py::init<MPI_Comm, std::shared_ptr<const dolfin::GenericLinearOperator>,
           std::string, std::string>(), py::arg("comm"), py::arg("A"),
           py::arg("method")="default", py::arg("preconditioner")="default")
      .def("solve", (std::size_t (dolfin::KrylovSolver::*)(dolfin::GenericVector&,
                                                           const dolfin::GenericVector&))
           &dolfin::KrylovSolver::solve);

    m.def("as_backend_type", [](dolfin::GenericTensor& v) { return v.shared_instance(); });

    m.def("has_linear_algebra_backend", &dolfin::has_linear_algebra_backend);
    m.def("linear_algebra_backends", &dolfin::linear_algebra_backends);
    m.def("has_krylov_solver_method", &dolfin::has_krylov_solver_method);
    m.def("has_krylov_solver_preconditioner", &dolfin::has_krylov_solver_preconditioner);
  }
}
