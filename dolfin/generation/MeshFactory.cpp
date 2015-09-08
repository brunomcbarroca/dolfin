// Copyright (C) 2005-2015 Anders Logg, Chris Richardson
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
// Modified by Garth N. Wells 2007
// Modified by Nuno Lopes 2008
// Modified by Kristian B. Oelgaard 2009
// Modified by Chris Richardson 2015

#include <dolfin/common/Timer.h>
#include <dolfin/mesh/MeshEditor.h>
#include <dolfin/mesh/MeshPartitioning.h>

#include "MeshFactory.h"

using namespace dolfin;

//-----------------------------------------------------------------------------
std::shared_ptr<Mesh> MeshFactory::UnitCubeMesh(MPI_Comm mpi_comm,
                                                std::size_t nx,
                                                std::size_t ny,
                                                std::size_t nz,
                                                MeshOptions options)
{
  std::shared_ptr<Mesh> mesh = std::shared_ptr<Mesh>(new Mesh(mpi_comm));
  build_box_mesh(mesh, Point(0.0, 0.0, 0.0), Point(1.0, 1.0, 1.0),
                 nx, ny, nz, options);
  return mesh;
}
//-----------------------------------------------------------------------------
std::shared_ptr<Mesh> MeshFactory::BoxMesh(MPI_Comm mpi_comm,
                                           const Point& p0, const Point& p1,
                                           std::size_t nx,
                                           std::size_t ny,
                                           std::size_t nz,
                                           MeshOptions options)
{
  std::shared_ptr<Mesh> mesh = std::shared_ptr<Mesh>(new Mesh(mpi_comm));
  build_box_mesh(mesh, p0, p1, nx, ny, nz, options);
  return mesh;
}
//-----------------------------------------------------------------------------
std::shared_ptr<Mesh> MeshFactory::UnitSquareMesh(MPI_Comm mpi_comm,
                                                  std::size_t nx, std::size_t ny,
                                                  MeshOptions options)
{
  return MeshFactory::RectangleMesh(mpi_comm, Point(0.0, 0.0), Point(1.0, 1.0),
                                    nx, ny, options);
}
//-----------------------------------------------------------------------------
std::shared_ptr<Mesh> MeshFactory::UnitSquareMesh(MPI_Comm mpi_comm,
                                                  std::size_t nx, std::size_t ny,
                                                  std::string diagonal)
{
  return MeshFactory::RectangleMesh(mpi_comm, Point(0.0, 0.0), Point(1.0, 1.0),
                                    nx, ny, diagonal);
}
//-----------------------------------------------------------------------------
std::shared_ptr<Mesh> MeshFactory::RectangleMesh(MPI_Comm mpi_comm,
                                                 const Point& p0, const Point& p1,
                                                 std::size_t nx, std::size_t ny,
                                                 MeshOptions options)
{
  std::shared_ptr<Mesh> mesh = std::shared_ptr<Mesh>(new Mesh(mpi_comm));
  // Check options
  std::string diagonal;
  if ((options&MeshOptions::alternating) == MeshOptions::alternating)
  {
    if ((options&MeshOptions::left) == MeshOptions::left)
      diagonal = "left/right";
    else
      diagonal = "right/left";
  }
  else if ((options&MeshOptions::crossed) == MeshOptions::crossed)
    diagonal = "crossed";
  else if ((options&MeshOptions::left) == MeshOptions::left)
    diagonal = "left";
  else if ((options&MeshOptions::right) == MeshOptions::right)
    diagonal = "right";
  else
    dolfin_error("MeshFactory.cpp",
                 "determine mesh options",
                 "Unknown mesh diagonal definition: allowed MeshOptions are \"left\", \"right\", \"alternating\" and \"crossed\"");
  build_rectangle_mesh(mesh, p0, p1, nx, ny, diagonal);
  return mesh;
}
//-----------------------------------------------------------------------------
std::shared_ptr<Mesh> MeshFactory::RectangleMesh(MPI_Comm mpi_comm,
                                                 const Point& p0, const Point& p1,
                                                 std::size_t nx, std::size_t ny,
                                                 std::string diagonal)
{
  std::shared_ptr<Mesh> mesh = std::shared_ptr<Mesh>(new Mesh(mpi_comm));
  build_rectangle_mesh(mesh, p0, p1, nx, ny, diagonal);
  return mesh;
}
//-----------------------------------------------------------------------------
std::shared_ptr<Mesh> MeshFactory::IntervalMesh(MPI_Comm mpi_comm,
                                                std::size_t nx, double a,
                                                double b, MeshOptions options)
{
  std::shared_ptr<Mesh> mesh = std::shared_ptr<Mesh>(new Mesh(mpi_comm));

  // Receive mesh according to parallel policy
  if (MPI::is_receiver(mesh->mpi_comm()))
  {
    MeshPartitioning::build_distributed_mesh(*mesh);
    return mesh;
  }

  if (std::abs(a - b) < DOLFIN_EPS)
  {
    dolfin_error("Interval.cpp",
                 "create interval",
                 "Length of interval is zero. Consider checking your dimensions");
  }

  if (b < a)
  {
    dolfin_error("Interval.cpp",
                 "create interval",
                 "Length of interval is negative. Consider checking the order of your arguments");
  }

  if (nx < 1)
  {
    dolfin_error("Interval.cpp",
                 "create interval",
                 "Number of points on interval is (%d), it must be at least 1", nx);
  }

  rename("mesh", "Mesh of the interval (a, b)");

  // Open mesh for editing
  MeshEditor editor;
  editor.open(*mesh, CellType::interval, 1, 1);

  // Create vertices and cells:
  editor.init_vertices_global((nx+1), (nx+1));
  editor.init_cells_global(nx, nx);

  // Create main vertices:
  for (std::size_t ix = 0; ix <= nx; ix++)
  {
    const std::vector<double>
      x(1, a + (static_cast<double>(ix)*(b - a)/static_cast<double>(nx)));
    editor.add_vertex(ix, x);
  }

  // Create intervals
  for (std::size_t ix = 0; ix < nx; ix++)
  {
    std::vector<std::size_t> cell(2);
    cell[0] = ix; cell[1] = ix + 1;
    editor.add_cell(ix, cell);
  }

  // Close mesh editor
  editor.close();

  // Broadcast mesh according to parallel policy
  if (MPI::is_broadcaster(mesh->mpi_comm()))
  {
    std::cout << "Building mesh (dist 0a)" << std::endl;
    MeshPartitioning::build_distributed_mesh(*mesh);
    std::cout << "Building mesh (dist 1a)" << std::endl;
  }

  return mesh;
}
//-----------------------------------------------------------------------------
std::shared_ptr<Mesh> MeshFactory::UnitTetrahedronMesh(MPI_Comm mpi_comm,
                                                       MeshOptions options)
{
  if (MPI::size(mpi_comm) != 1)
    dolfin_error("MeshFactory.cpp",
                 "generate UnitTetraHedronMesh",
                 "Cannot generate distributed mesh");

  std::shared_ptr<Mesh> mesh = std::shared_ptr<Mesh>(new Mesh(mpi_comm));

  // Open mesh for editing
  MeshEditor editor;
  editor.open(*mesh, CellType::tetrahedron, 3, 3);

  // Create vertices
  editor.init_vertices_global(4, 4);
  std::vector<double> x(3);
  x[0] = 0.0; x[1] = 0.0; x[2] = 0.0;
  editor.add_vertex(0, x);

  x[0] = 1.0; x[1] = 0.0; x[2] = 0.0;
  editor.add_vertex(1, x);

  x[0] = 0.0; x[1] = 1.0; x[2] = 0.0;
  editor.add_vertex(2, x);

  x[0] = 0.0; x[1] = 0.0; x[2] = 1.0;
  editor.add_vertex(3, x);

  // Create cells
  editor.init_cells_global(1, 1);
  std::vector<std::size_t> cell_data(4);
  cell_data[0] = 0; cell_data[1] = 1; cell_data[2] = 2; cell_data[3] = 3;
  editor.add_cell(0, cell_data);

  // Close mesh editor
  editor.close();

  return mesh;
}
//-----------------------------------------------------------------------------
std::shared_ptr<Mesh> MeshFactory::UnitTriangleMesh(MPI_Comm mpi_comm,
                                                    MeshOptions options)
{
  if (MPI::size(mpi_comm) != 1)
    dolfin_error("MeshFactory.cpp",
                 "generate UnitTriangleMesh",
                 "Cannot generate distributed mesh");

  std::shared_ptr<Mesh> mesh = std::shared_ptr<Mesh>(new Mesh(mpi_comm));

  // Open mesh for editing
  MeshEditor editor;
  editor.open(*mesh, CellType::triangle, 2, 2);

  // Create vertices
  editor.init_vertices_global(3, 3);
  std::vector<double> x(2);
  x[0] = 0.0; x[1] = 0.0;
  editor.add_vertex(0, x);
  x[0] = 1.0; x[1] = 0.0;
  editor.add_vertex(1, x);
  x[0] = 0.0; x[1] = 1.0;
  editor.add_vertex(2, x);

  // Create cells
  editor.init_cells_global(1, 1);
  std::vector<std::size_t> cell_data(3);
  cell_data[0] = 0; cell_data[1] = 1; cell_data[2] = 2;
  editor.add_cell(0, cell_data);

  // Close mesh editor
  editor.close();

  return mesh;
}
//-----------------------------------------------------------------------------
void MeshFactory::build_rectangle_mesh(std::shared_ptr<Mesh> mesh,
                                       const Point& p0, const Point& p1,
                                       std::size_t nx, std::size_t ny,
                                       std::string diagonal)
{
  // Receive mesh according to parallel policy
  if (MPI::is_receiver(mesh->mpi_comm()))
  {
    MeshPartitioning::build_distributed_mesh(*mesh);
    return;
  }

  // Extract minimum and maximum coordinates
  const double x0 = std::min(p0.x(), p1.x());
  const double x1 = std::max(p0.x(), p1.x());
  const double y0 = std::min(p0.y(), p1.y());
  const double y1 = std::max(p0.y(), p1.y());

  const double a = x0;
  const double b = x1;
  const double c = y0;
  const double d = y1;

  if (std::abs(x0 - x1) < DOLFIN_EPS || std::abs(y0 - y1) < DOLFIN_EPS)
  {
    dolfin_error("Rectangle.cpp",
                 "create rectangle",
                 "Rectangle seems to have zero width, height or depth. Consider checking your dimensions");
  }

  if (nx < 1 || ny < 1)
  {
    dolfin_error("RectangleMesh.cpp",
                 "create rectangle",
                 "Rectangle has non-positive number of vertices in some dimension: number of vertices must be at least 1 in each dimension");
  }

  rename("mesh", "Mesh of the unit square (a,b) x (c,d)");
  // Open mesh for editing
  MeshEditor editor;
  editor.open(*mesh, CellType::triangle, 2, 2);

  // Create vertices and cells:
  if (diagonal == "crossed")
  {
    editor.init_vertices_global((nx + 1)*(ny + 1) + nx*ny,
                                  (nx + 1)*(ny + 1) + nx*ny);
    editor.init_cells_global(4*nx*ny, 4*nx*ny);
  }
  else
  {
    editor.init_vertices_global((nx + 1)*(ny + 1), (nx + 1)*(ny + 1));
    editor.init_cells_global(2*nx*ny, 2*nx*ny);
  }

  // Storage for vertices
  std::vector<double> x(2);

  // Create main vertices:
  std::size_t vertex = 0;
  for (std::size_t iy = 0; iy <= ny; iy++)
  {
    x[1] = c + ((static_cast<double>(iy))*(d - c)/static_cast<double>(ny));
    for (std::size_t ix = 0; ix <= nx; ix++)
    {
      x[0] = a + ((static_cast<double>(ix))*(b - a)/static_cast<double>(nx));
      editor.add_vertex(vertex, x);
      vertex++;
    }
  }

  // Create midpoint vertices if the mesh type is crossed
  if (diagonal == "crossed")
  {
    for (std::size_t iy = 0; iy < ny; iy++)
    {
      x[1] = c +(static_cast<double>(iy) + 0.5)*(d - c)/static_cast<double>(ny);
      for (std::size_t ix = 0; ix < nx; ix++)
      {
        x[0] = a + (static_cast<double>(ix) + 0.5)*(b - a)/static_cast<double>(nx);
        editor.add_vertex(vertex, x);
        vertex++;
      }
    }
  }

  // Create triangles
  std::size_t cell = 0;
  if (diagonal == "crossed")
  {
    boost::multi_array<std::size_t, 2> cells(boost::extents[4][3]);
    for (std::size_t iy = 0; iy < ny; iy++)
    {
      for (std::size_t ix = 0; ix < nx; ix++)
      {
        const std::size_t v0 = iy*(nx + 1) + ix;
        const std::size_t v1 = v0 + 1;
        const std::size_t v2 = v0 + (nx + 1);
        const std::size_t v3 = v1 + (nx + 1);
        const std::size_t vmid = (nx + 1)*(ny + 1) + iy*nx + ix;

        // Note that v0 < v1 < v2 < v3 < vmid.
        cells[0][0] = v0; cells[0][1] = v1; cells[0][2] = vmid;
        cells[1][0] = v0; cells[1][1] = v2; cells[1][2] = vmid;
        cells[2][0] = v1; cells[2][1] = v3; cells[2][2] = vmid;
        cells[3][0] = v2; cells[3][1] = v3; cells[3][2] = vmid;

        // Add cells
        for (auto _cell = cells.begin(); _cell != cells.end(); ++_cell)
          editor.add_cell(cell++, *_cell);
      }
    }
  }
  else if (diagonal == "left" || diagonal == "right" || diagonal == "right/left" || diagonal == "left/right")
  {
    std::string local_diagonal = diagonal;
    boost::multi_array<std::size_t, 2> cells(boost::extents[2][3]);
    for (std::size_t iy = 0; iy < ny; iy++)
    {
      // Set up alternating diagonal
      if (diagonal == "right/left")
      {
        if (iy % 2)
          local_diagonal = "right";
        else
          local_diagonal = "left";
      }
      if (diagonal == "left/right")
      {
        if (iy % 2)
          local_diagonal = "left";
        else
          local_diagonal = "right";
      }

      for (std::size_t ix = 0; ix < nx; ix++)
      {
        const std::size_t v0 = iy*(nx + 1) + ix;
        const std::size_t v1 = v0 + 1;
        const std::size_t v2 = v0 + (nx + 1);
        const std::size_t v3 = v1 + (nx + 1);
        std::vector<std::size_t> cell_data;

        if(local_diagonal == "left")
        {
          cells[0][0] = v0; cells[0][1] = v1; cells[0][2] = v2;
          cells[1][0] = v1; cells[1][1] = v2; cells[1][2] = v3;
          if (diagonal == "right/left" || diagonal == "left/right")
            local_diagonal = "right";
        }
        else
        {
          cells[0][0] = v0; cells[0][1] = v1; cells[0][2] = v3;
          cells[1][0] = v0; cells[1][1] = v2; cells[1][2] = v3;
          if (diagonal == "right/left" || diagonal == "left/right")
            local_diagonal = "left";
        }
        editor.add_cell(cell++, cells[0]);
        editor.add_cell(cell++, cells[1]);
      }
    }
  }

  // Close mesh editor
  editor.close();

  // Broadcast mesh according to parallel policy
  if (MPI::is_broadcaster(mesh->mpi_comm()))
  {
    MeshPartitioning::build_distributed_mesh(*mesh);
    return;
  }
}
//-----------------------------------------------------------------------------
void MeshFactory::build_box_mesh(std::shared_ptr<Mesh> mesh,
                                       const Point& p0, const Point& p1,
                                       std::size_t nx, std::size_t ny,
                                       std::size_t nz,
                                       MeshOptions options)
{
  // Check options
  if (options != MeshOptions::none)
    dolfin_error("MeshFactory.cpp",
                 "determine mesh options",
                 "Unknown mesh options for BoxMesh");

  Timer timer("Build BoxMesh");

  // Receive mesh according to parallel policy
  if (MPI::is_receiver(mesh->mpi_comm()))
  {
    MeshPartitioning::build_distributed_mesh(*mesh);
    return;
  }

  // Extract minimum and maximum coordinates
  const double x0 = std::min(p0.x(), p1.x());
  const double x1 = std::max(p0.x(), p1.x());
  const double y0 = std::min(p0.y(), p1.y());
  const double y1 = std::max(p0.y(), p1.y());
  const double z0 = std::min(p0.z(), p1.z());
  const double z1 = std::max(p0.z(), p1.z());

  const double a = x0;
  const double b = x1;
  const double c = y0;
  const double d = y1;
  const double e = z0;
  const double f = z1;

  if (std::abs(x0 - x1) < DOLFIN_EPS || std::abs(y0 - y1) < DOLFIN_EPS
      || std::abs(z0 - z1) < DOLFIN_EPS )
  {
    dolfin_error("BoxMesh.cpp",
                 "create box",
                 "Box seems to have zero width, height or depth. Consider checking your dimensions");
  }

  if ( nx < 1 || ny < 1 || nz < 1 )
  {
    dolfin_error("BoxMesh.cpp",
                 "create box",
                 "BoxMesh has non-positive number of vertices in some dimension: number of vertices must be at least 1 in each dimension");
  }

  rename("mesh", "Mesh of the cuboid (a,b) x (c,d) x (e,f)");

  // Open mesh for editing
  MeshEditor editor;
  editor.open(*mesh, CellType::tetrahedron, 3, 3);

  // Storage for vertex coordinates
  std::vector<double> x(3);

  // Create vertices
  editor.init_vertices_global((nx + 1)*(ny + 1)*(nz + 1),
                              (nx + 1)*(ny + 1)*(nz + 1));
  std::size_t vertex = 0;
  for (std::size_t iz = 0; iz <= nz; iz++)
  {
    x[2] = e + (static_cast<double>(iz))*(f-e) / static_cast<double>(nz);
    for (std::size_t iy = 0; iy <= ny; iy++)
    {
      x[1] = c + (static_cast<double>(iy))*(d-c) / static_cast<double>(ny);
      for (std::size_t ix = 0; ix <= nx; ix++)
      {
        x[0] = a + (static_cast<double>(ix))*(b-a) / static_cast<double>(nx);
        editor.add_vertex(vertex, x);
        vertex++;
      }
    }
  }

  // Create tetrahedra
  editor.init_cells_global(6*nx*ny*nz, 6*nx*ny*nz);
  std::size_t cell = 0;
  boost::multi_array<std::size_t, 2> cells(boost::extents[6][4]);
  for (std::size_t iz = 0; iz < nz; iz++)
  {
    for (std::size_t iy = 0; iy < ny; iy++)
    {
      for (std::size_t ix = 0; ix < nx; ix++)
      {
        const std::size_t v0 = iz*(nx + 1)*(ny + 1) + iy*(nx + 1) + ix;
        const std::size_t v1 = v0 + 1;
        const std::size_t v2 = v0 + (nx + 1);
        const std::size_t v3 = v1 + (nx + 1);
        const std::size_t v4 = v0 + (nx + 1)*(ny + 1);
        const std::size_t v5 = v1 + (nx + 1)*(ny + 1);
        const std::size_t v6 = v2 + (nx + 1)*(ny + 1);
        const std::size_t v7 = v3 + (nx + 1)*(ny + 1);

        // Note that v0 < v1 < v2 < v3 < vmid.
        cells[0][0] = v0; cells[0][1] = v1; cells[0][2] = v3; cells[0][3] = v7;
        cells[1][0] = v0; cells[1][1] = v1; cells[1][2] = v7; cells[1][3] = v5;
        cells[2][0] = v0; cells[2][1] = v5; cells[2][2] = v7; cells[2][3] = v4;
        cells[3][0] = v0; cells[3][1] = v3; cells[3][2] = v2; cells[3][3] = v7;
        cells[4][0] = v0; cells[4][1] = v6; cells[4][2] = v4; cells[4][3] = v7;
        cells[5][0] = v0; cells[5][1] = v2; cells[5][2] = v6; cells[5][3] = v7;

        // Add cells
        for (auto _cell = cells.begin(); _cell != cells.end(); ++_cell)
          editor.add_cell(cell++, *_cell);
      }
    }
  }

  // Close mesh editor
  editor.close();

  // Broadcast mesh according to parallel policy
  if (MPI::is_broadcaster(mesh->mpi_comm()))
  {
    MeshPartitioning::build_distributed_mesh(*mesh);
    return;
  }
}
//-----------------------------------------------------------------------------
