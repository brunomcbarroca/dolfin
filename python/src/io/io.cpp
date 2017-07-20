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
      .def("__lshift__", (void (dolfin::File::*)(const dolfin::Parameters&)) &dolfin::File::operator<<)
      .def("__rshift__", (void (dolfin::File::*)(dolfin::Parameters&)) &dolfin::File::operator>>)
      .def("__lshift__", (void (dolfin::File::*)(const dolfin::Mesh&)) &dolfin::File::operator<<)
      .def("__rshift__", (void (dolfin::File::*)(dolfin::Mesh&)) &dolfin::File::operator>>);

    // dolfin::GenericFile
    py::class_<dolfin::GenericFile, std::shared_ptr<dolfin::GenericFile>>(m, "GenericFile")
    .def("__lshift__", (void (dolfin::GenericFile::*)(const dolfin::Parameters&)) &dolfin::GenericFile::operator<<);

    // dolfin::VTKFile
    py::class_<dolfin::VTKFile, std::shared_ptr<dolfin::VTKFile>>(m, "VTKFile")
      .def(py::init<std::string, std::string>())
      .def("__lshift__",  [](py::object& obj, const dolfin::Mesh& mesh) { dolfin::VTKFile *cls = obj.cast<dolfin::VTKFile*>(); *cls << mesh; })
      .def("write", [](py::object& obj, const dolfin::Mesh& mesh) { dolfin::VTKFile *cls = obj.cast<dolfin::VTKFile*>(); *cls << mesh; });

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

    // dolfin::XDMFFile
    py::class_<dolfin::XDMFFile, std::shared_ptr<dolfin::XDMFFile>> xdmffile(m, "XDMFFile");

    py::enum_<dolfin::XDMFFile::Encoding>(xdmffile, "Encoding")
      .value("HDF5", dolfin::XDMFFile::Encoding::HDF5)
      .value("ASCII", dolfin::XDMFFile::Encoding::ASCII);

    xdmffile
      .def(py::init<MPI_Comm, std::string>())
      .def(py::init<std::string>())
      .def("__enter__", [](dolfin::XDMFFile& self){ return &self; })
      .def("__exit__", [](dolfin::XDMFFile& self, py::args args, py::kwargs kwargs){ self.close(); })
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::Function&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("u"), py::arg("encoding")=dolfin::XDMFFile::Encoding::HDF5)
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::Function&, double, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("u"), py::arg("t"), py::arg("encoding")=dolfin::XDMFFile::Encoding::HDF5)
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::Mesh&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mesh"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5);

    xdmffile
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::MeshValueCollection<bool>&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mvc"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5)
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::MeshValueCollection<std::size_t>&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mvc"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5)
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::MeshValueCollection<int>&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mvc"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5)
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::MeshValueCollection<double>&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mvc"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5);

    xdmffile
      .def("read", (void (dolfin::XDMFFile::*)(dolfin::MeshValueCollection<bool>&, std::string))
           &dolfin::XDMFFile::read, py::arg("mvc"), py::arg("name") = "")
      .def("read", (void (dolfin::XDMFFile::*)(dolfin::MeshValueCollection<std::size_t>&, std::string))
           &dolfin::XDMFFile::read, py::arg("mvc"), py::arg("name") = "")
      .def("read", (void (dolfin::XDMFFile::*)(dolfin::MeshValueCollection<int>&, std::string))
           &dolfin::XDMFFile::read, py::arg("mvc"), py::arg("name") = "")
      .def("read", (void (dolfin::XDMFFile::*)(dolfin::MeshValueCollection<double>&, std::string))
           &dolfin::XDMFFile::read, py::arg("mvc"), py::arg("name") = "");

    xdmffile
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::MeshFunction<bool>&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mvc"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5)
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::MeshFunction<std::size_t>&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mvc"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5)
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::MeshFunction<int>&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mvc"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5)
      .def("write", (void (dolfin::XDMFFile::*)(const dolfin::MeshFunction<double>&, dolfin::XDMFFile::Encoding))
           &dolfin::XDMFFile::write, py::arg("mvc"), py::arg("encoding") = dolfin::XDMFFile::Encoding::HDF5);

    xdmffile
      .def("read", (void (dolfin::XDMFFile::*)(dolfin::MeshFunction<bool>&, std::string))
           &dolfin::XDMFFile::read, py::arg("mvc"), py::arg("name") = "")
      .def("read", (void (dolfin::XDMFFile::*)(dolfin::MeshFunction<std::size_t>&, std::string))
           &dolfin::XDMFFile::read, py::arg("mvc"), py::arg("name") = "")
      .def("read", (void (dolfin::XDMFFile::*)(dolfin::MeshFunction<int>&, std::string))
           &dolfin::XDMFFile::read, py::arg("mvc"), py::arg("name") = "")
      .def("read", (void (dolfin::XDMFFile::*)(dolfin::MeshFunction<double>&, std::string))
           &dolfin::XDMFFile::read, py::arg("mvc"), py::arg("name") = "");



  }

}
