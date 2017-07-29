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

#include <memory>
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <dolfin/io/File.h>
#include <dolfin/io/GenericFile.h>
#include <dolfin/io/HDF5Attribute.h>
#include <dolfin/io/HDF5File.h>
#include <dolfin/io/VTKFile.h>
#include <dolfin/io/XDMFFile.h>
#include <dolfin/la/GenericVector.h>
#include <dolfin/mesh/Mesh.h>
#include <dolfin/mesh/MeshFunction.h>
#include <dolfin/mesh/MeshValueCollection.h>

#include "../mpi_interface.h"

namespace py = pybind11;

namespace dolfin_wrappers
{

  void io(py::module& m)
  {
    // dolfin::File
    py::class_<dolfin::File, std::shared_ptr<dolfin::File>>(m, "File")
      .def(py::init<std::string>())
      .def(py::init<std::string, std::string>())
      //
      .def("__lshift__", (void (dolfin::File::*)(const dolfin::Parameters&)) &dolfin::File::operator<<)
      //
      .def("__lshift__", (void (dolfin::File::*)(const dolfin::Mesh&)) &dolfin::File::operator<<)
      .def("__lshift__", (void (dolfin::File::*)(const std::pair<const dolfin::Mesh*, double>)) &dolfin::File::operator<<)
      //
      .def("__lshift__", (void (dolfin::File::*)(const dolfin::Function&)) &dolfin::File::operator<<)
      .def("__lshift__", (void (dolfin::File::*)(const std::pair<const dolfin::Function*, double>)) &dolfin::File::operator<<)
       //
      .def("__lshift__", (void (dolfin::File::*)(const dolfin::MeshFunction<int>&)) &dolfin::File::operator<<)
      .def("__lshift__", (void (dolfin::File::*)(const std::pair<const dolfin::MeshFunction<int>*, double>)) &dolfin::File::operator<<)
      .def("__lshift__", (void (dolfin::File::*)(const dolfin::MeshFunction<std::size_t>&)) &dolfin::File::operator<<)
      .def("__lshift__", (void (dolfin::File::*)(const std::pair<const dolfin::MeshFunction<std::size_t>*, double>)) &dolfin::File::operator<<)
      .def("__lshift__", (void (dolfin::File::*)(const dolfin::MeshFunction<double>&)) &dolfin::File::operator<<)
      .def("__lshift__", (void (dolfin::File::*)(const std::pair<const dolfin::MeshFunction<double>*, double>)) &dolfin::File::operator<<)
      .def("__lshift__", (void (dolfin::File::*)(const dolfin::MeshFunction<bool>&)) &dolfin::File::operator<<)
      .def("__lshift__", (void (dolfin::File::*)(const std::pair<const dolfin::MeshFunction<bool>*, double>)) &dolfin::File::operator<<)
      // Unpack
      .def("__lshift__", [](dolfin::File& instance, py::tuple u)
           {
             auto _u = u[0].attr("_cpp_object").cast<dolfin::Function*>();
             auto _t = u[1].cast<double>();
             instance << std::make_pair(_u, _t);
           })
      .def("__lshift__", [](dolfin::File& instance, py::object u)
           {
             auto _u = u.attr("_cpp_object").cast<dolfin::Function&>();
             instance << _u;
           })
      // Read
      .def("__rshift__", (void (dolfin::File::*)(dolfin::Parameters&)) &dolfin::File::operator>>)
      .def("__rshift__", (void (dolfin::File::*)(dolfin::Mesh&)) &dolfin::File::operator>>);

    // dolfin::VTKFile
    py::class_<dolfin::VTKFile, std::shared_ptr<dolfin::VTKFile>>(m, "VTKFile")
      .def(py::init<std::string, std::string>())
      .def("__lshift__",  [](dolfin::VTKFile& instance, const dolfin::Mesh& mesh)
           { instance << mesh; })
      .def("write", [](dolfin::VTKFile& instance, const dolfin::Mesh& mesh)
           { instance << mesh; });

#ifdef HAS_HDF5
    // HDF5
    py::class_<dolfin::HDF5Attribute, std::shared_ptr<dolfin::HDF5Attribute>>(m, "HDF5Attribute")
      //.def("__getitem__", [](const dolfin::HDF5Attribute& instance, std::string name){ return instance[name]; })
      .def("__setitem__", [](dolfin::HDF5Attribute& instance, std::string name, std::string value){ instance.set(name, value); })
      .def("__setitem__", [](dolfin::HDF5Attribute& instance, std::string name, double value){ instance.set(name, value); })
      .def("__setitem__", [](dolfin::HDF5Attribute& instance, std::string name, std::size_t value){ instance.set(name, value); })
      .def("__setitem__", [](dolfin::HDF5Attribute& instance, std::string name, py::array_t<double> values)
           {
             std::vector<double> _values(values.shape()[0]);
             std::copy_n(values.data(), _values.size(), _values.begin());
             instance.set(name, _values);
           })
      .def("__setitem__", [](dolfin::HDF5Attribute& instance, std::string name, py::array_t<std::size_t> values)
           {
             std::vector<std::size_t> _values(values.shape()[0]);
             std::copy_n(values.data(), _values.size(), _values.begin());
             instance.set(name, _values);
           })

      .def("__getitem__", [](const dolfin::HDF5Attribute& instance, std::string name)
           {
             const std::string type = instance.type_str(name);
             if (type == "string")
             {
               std::string attr;
               instance.get(name, attr);
               return py::cast(attr);
             }
             else if (type == "float")
             {
               double attr;
               instance.get(name, attr);
               return py::cast(attr);
             }
             else if (type == "int")
             {
               std::size_t attr;
               instance.get(name, attr);
               return py::cast(attr);
             }
             else if (type == "vectorfloat")
             {
               std::vector<double> attr;
               instance.get(name, attr);
               return py::cast(attr);
             }
             else if (type == "vectorint")
             {
               std::vector<std::size_t> attr;
               instance.get(name, attr);
               return py::cast(attr);
             }
             else
             {
               throw std::runtime_error("HDF5 attribute type unknown.");
               return py::object();
             }
           })
      .def("__contains__", [](const dolfin::HDF5Attribute& instance, std::string key) { return instance.exists(key); })
      .def("list_attributes", &dolfin::HDF5Attribute::list_attributes)
      .def("type_str", &dolfin::HDF5Attribute::type_str);

    // dolfin::HDF5File
    py::class_<dolfin::HDF5File, std::shared_ptr<dolfin::HDF5File>> (m, "HDF5File")
      .def(py::init<MPI_Comm, std::string, std::string>())
      .def("__enter__", [](dolfin::HDF5File& self){ return &self; })
      .def("__exit__", [](dolfin::HDF5File& self, py::args args, py::kwargs kwargs){ self.close(); })
      .def("write", (void (dolfin::HDF5File::*)(const dolfin::Mesh&, std::string)) &dolfin::HDF5File::write)
      .def("read", (void (dolfin::HDF5File::*)(dolfin::Mesh&, std::string, bool) const) &dolfin::HDF5File::read)
      .def("close", &dolfin::HDF5File::close)
      .def("write", (void (dolfin::HDF5File::*)(const dolfin::MeshValueCollection<bool>&, std::string))
           &dolfin::HDF5File::write, py::arg("mvc"), py::arg("name"))
      .def("write", (void (dolfin::HDF5File::*)(const dolfin::MeshValueCollection<std::size_t>&, std::string))
           &dolfin::HDF5File::write, py::arg("mvc"), py::arg("name"))
      .def("write", (void (dolfin::HDF5File::*)(const dolfin::MeshValueCollection<double>&, std::string))
           &dolfin::HDF5File::write, py::arg("mvc"), py::arg("name"))
      .def("read", (void (dolfin::HDF5File::*)(dolfin::MeshValueCollection<bool>&, std::string) const)
           &dolfin::HDF5File::read, py::arg("mvc"), py::arg("name"))
      .def("read", (void (dolfin::HDF5File::*)(dolfin::MeshValueCollection<std::size_t>&, std::string) const)
           &dolfin::HDF5File::read, py::arg("mvc"), py::arg("name"))
      .def("read", (void (dolfin::HDF5File::*)(dolfin::MeshValueCollection<double>&, std::string) const)
           &dolfin::HDF5File::read, py::arg("mvc"), py::arg("name"))
      .def("write", (void (dolfin::HDF5File::*)(const dolfin::MeshFunction<bool>&, std::string))
           &dolfin::HDF5File::write, py::arg("meshfunction"), py::arg("name"))
      .def("write", (void (dolfin::HDF5File::*)(const dolfin::MeshFunction<std::size_t>&, std::string))
           &dolfin::HDF5File::write, py::arg("meshfunction"), py::arg("name"))
      .def("write", (void (dolfin::HDF5File::*)(const dolfin::MeshFunction<int>&, std::string))
           &dolfin::HDF5File::write, py::arg("meshfunction"), py::arg("name"))
      .def("write", (void (dolfin::HDF5File::*)(const dolfin::MeshFunction<double>&, std::string))
           &dolfin::HDF5File::write, py::arg("meshfunction"), py::arg("name"))
      .def("read", (void (dolfin::HDF5File::*)(dolfin::MeshFunction<bool>&, std::string) const)
           &dolfin::HDF5File::read, py::arg("meshfunction"), py::arg("name"))
      .def("read", (void (dolfin::HDF5File::*)(dolfin::MeshFunction<std::size_t>&, std::string) const)
           &dolfin::HDF5File::read, py::arg("meshfunction"), py::arg("name"))
      .def("read", (void (dolfin::HDF5File::*)(dolfin::MeshFunction<int>&, std::string) const)
           &dolfin::HDF5File::read, py::arg("meshfunction"), py::arg("name"))
      .def("read", (void (dolfin::HDF5File::*)(dolfin::MeshFunction<double>&, std::string) const)
           &dolfin::HDF5File::read, py::arg("meshfunction"), py::arg("name"))
      .def("write", (void (dolfin::HDF5File::*)(const dolfin::GenericVector&, std::string))
           &dolfin::HDF5File::write, py::arg("vector"), py::arg("name"))
      .def("read", (void (dolfin::HDF5File::*)(dolfin::GenericVector&, std::string, bool) const)
           &dolfin::HDF5File::read, py::arg("vector"), py::arg("name"), py::arg("use_partitioning"))
      .def("attributes", &dolfin::HDF5File::attributes);

#endif

    // dolfin::XDMFFile
    py::class_<dolfin::XDMFFile, std::shared_ptr<dolfin::XDMFFile>> xdmf_file(m, "XDMFFile");

    // dolfin::XDMFFile::Encoding (enum)
    py::enum_<dolfin::XDMFFile::Encoding>(xdmf_file, "Encoding")
      .value("HDF5", dolfin::XDMFFile::Encoding::HDF5)
      .value("ASCII", dolfin::XDMFFile::Encoding::ASCII);

    // dolfin::XDMFFile::write
    xdmf_file
      .def(py::init<MPI_Comm, std::string>())
      .def(py::init<std::string>())
      .def("__enter__", [](dolfin::XDMFFile& self){ return &self; })
      .def("__exit__", [](dolfin::XDMFFile& self, py::args args, py::kwargs kwargs){ self.close(); })
      // Function
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::Function&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("u"), py::arg("encoding")=dolfin::XDMFFile::Encoding::HDF5)
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::Function&, double, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("u"), py::arg("t"), py::arg("encoding")=dolfin::XDMFFile::Encoding::HDF5)
      // Mesh
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::Mesh&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mesh"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5)
      // MeshFunction
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::MeshFunction<bool>&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mvc"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5)
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::MeshFunction<std::size_t>&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mvc"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5)
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::MeshFunction<int>&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mvc"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5)
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::MeshFunction<double>&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mvc"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5)
      // MeshValueCollection
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::MeshValueCollection<bool>&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mvc"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5)
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::MeshValueCollection<std::size_t>&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mvc"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5)
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::MeshValueCollection<int>&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mvc"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5)
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::MeshValueCollection<double>&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mvc"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5)
      // py:object / dolfin.function.Function (these function are
      // registered last so the the specialised version are prefered.
      .def("write", [](dolfin::XDMFFile& instance, const py::object u, dolfin::XDMFFile::Encoding encoding)
           {
             auto _u = u.attr("_cpp_object").cast<dolfin::Function*>();
             instance.write(*_u, encoding);
           }, py::arg("u"), py::arg("encoding")=dolfin::XDMFFile::Encoding::HDF5)
      .def("write", [](dolfin::XDMFFile& instance, const py::object u, double t, dolfin::XDMFFile::Encoding encoding)
           {
             auto _u = u.attr("_cpp_object").cast<dolfin::Function*>();
             instance.write(*_u, t, encoding);
           }, py::arg("u"), py::arg("t"), py::arg("encoding")=dolfin::XDMFFile::Encoding::HDF5)
      //
      .def("write_checkpoint", [](dolfin::XDMFFile& instance, const dolfin::Function& u,
                                  std::string function_name,
                                  double time_step, dolfin::XDMFFile::Encoding encoding)
           { instance.write_checkpoint(u, function_name, time_step, encoding); },
           py::arg("u"), py::arg("function_name"), py::arg("time_step")=0.0,
           py::arg("encoding")=dolfin::XDMFFile::Encoding::HDF5)
      .def("write_checkpoint", [](dolfin::XDMFFile& instance, const py::object u, std::string function_name,
                                  double time_step, dolfin::XDMFFile::Encoding encoding)
           {
             auto _u = u.attr("_cpp_object").cast<dolfin::Function*>();
             instance.write_checkpoint(*_u, function_name, time_step, encoding);
           },
           py::arg("u"), py::arg("function_name"), py::arg("time_step")=0.0,
           py::arg("encoding")=dolfin::XDMFFile::Encoding::HDF5);


    // XDFMFile::read
    xdmf_file
      // Mesh
      .def("read", (void (dolfin::XDMFFile::*)(dolfin::Mesh&) const) &dolfin::XDMFFile::read)
      // MeshFunction
      .def("read", (void (dolfin::XDMFFile::*)(dolfin::MeshFunction<bool>&, std::string))
           &dolfin::XDMFFile::read, py::arg("mf"), py::arg("name") = "")
      .def("read", (void (dolfin::XDMFFile::*)(dolfin::MeshFunction<std::size_t>&, std::string))
           &dolfin::XDMFFile::read, py::arg("mf"), py::arg("name") = "")
      .def("read", (void (dolfin::XDMFFile::*)(dolfin::MeshFunction<int>&, std::string))
           &dolfin::XDMFFile::read, py::arg("mf"), py::arg("name") = "")
      .def("read", (void (dolfin::XDMFFile::*)(dolfin::MeshFunction<double>&, std::string))
           &dolfin::XDMFFile::read, py::arg("mf"), py::arg("name") = "")
    // MeshValueCollection
    .def("read", (void (dolfin::XDMFFile::*)(dolfin::MeshValueCollection<bool>&, std::string))
           &dolfin::XDMFFile::read, py::arg("mvc"), py::arg("name") = "")
      .def("read", (void (dolfin::XDMFFile::*)(dolfin::MeshValueCollection<std::size_t>&, std::string))
           &dolfin::XDMFFile::read, py::arg("mvc"), py::arg("name") = "")
      .def("read", (void (dolfin::XDMFFile::*)(dolfin::MeshValueCollection<int>&, std::string))
           &dolfin::XDMFFile::read, py::arg("mvc"), py::arg("name") = "")
      .def("read", (void (dolfin::XDMFFile::*)(dolfin::MeshValueCollection<double>&, std::string))
           &dolfin::XDMFFile::read, py::arg("mvc"), py::arg("name") = "")
      //
      .def("read_checkpoint", &dolfin::XDMFFile::read_checkpoint, py::arg("u"), py::arg("name"),
           py::arg("counter")=-1)
      .def("read_checkpoint", [](dolfin::XDMFFile& instance, py::object u, std::string name, std::int64_t counter)
           {
             auto _u = u.attr("_cpp_object").cast<dolfin::Function*>();
             instance.read_checkpoint(*_u, name, counter);
           },
           py::arg("u"), py::arg("name"), py::arg("counter")=-1);

  }

}
