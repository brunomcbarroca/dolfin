// Copyright (C) 2017 Nate Sime and Chris Richardson
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

#include <dolfin/common/ArrayView.h>
#include <dolfin/function/Function.h>
#include <dolfin/mesh/Mesh.h>
#include <dolfin/mesh/MeshEditor.h>
#include <dolfin/mesh/Facet.h>
#include <dolfin/mesh/Vertex.h>
#include "Point.h"
#include "CollisionDetection.h"

#include "GeometricContact.h"

using namespace dolfin;

//-----------------------------------------------------------------------------
std::vector<Point> GeometricContact::create_deformed_segment_volume_3d(Mesh& mesh, std::size_t facet_index, const Function& u)
{
  Facet facet(mesh, facet_index);
  Point X1 = Vertex(mesh, facet.entities(0)[0]).point();
  Point X2 = Vertex(mesh, facet.entities(0)[1]).point();
  Point X3 = Vertex(mesh, facet.entities(0)[2]).point();

  Array<double> uval(3);

  const Array<double> _x1(3, X1.coordinates());
  u.eval(uval, _x1);
  Point x1 = X1 + Point(uval);

  const Array<double> _x2(3, X2.coordinates());
  u.eval(uval, _x2);
  Point x2 = X2 + Point(uval);

  const Array<double> _x3(3, X3.coordinates());
  u.eval(uval, _x3);
  Point x3 = X3 + Point(uval);

  return {X1, X2, X3, x1, x2, x3};
}
//-----------------------------------------------------------------------------
bool GeometricContact::check_tet_set_collision(const Mesh& master_mesh, std::size_t mindex,
                                               const Mesh& slave_mesh, std::size_t sindex)
{

  for (unsigned int i = mindex; i < mindex + 3; ++i)
    for (unsigned int j = sindex; j < sindex + 3; ++j)
    {
      if (CollisionDetection::collides_tetrahedron_tetrahedron(Cell(master_mesh, i),
                                                               Cell(slave_mesh, j)))
        return true;
    }

  return false;
}
//-----------------------------------------------------------------------------
std::map<std::size_t, std::vector<std::size_t>>
  GeometricContact::contact_surface_map_volume_sweep_3d(Mesh& mesh, Function& u,
                                                        const std::vector<std::size_t>& master_facets, const std::vector<std::size_t>& slave_facets)
{
  // Construct a dictionary mapping master facets to their collided slave counterparts.

  // This algorithm find which slave facets are contained in the volume swept out by
  // the master surface and its displacement.

  // :param mesh: The mesh
  // :param u: The (3D) solution vector field
  // :param master_facets: list of master facet indices
  // :param slave_facets: list of slave facet indices
  // :return: mapping master facet index to slave facet indices

  std::map<std::size_t, std::vector<std::size_t>> master_to_slave;

  // Local mesh of master 'prisms', three tetrahedra created per facet
  Mesh master_mesh(MPI_COMM_SELF);
  MeshEditor master_ed;
  master_ed.open(master_mesh, mesh.topology().dim(), mesh.geometry().dim());
  master_ed.init_vertices(master_facets.size()*6);
  master_ed.init_cells(master_facets.size()*3);
  int c = 0;
  int v = 0;

  std::cout << "mf.size() = " << master_facets.size() << "\n";

  for (auto &mf : master_facets)
  {
    std::cout << "c = " << c << "\n";

    std::vector<Point> master_tet_set = create_deformed_segment_volume_3d(mesh, mf, u);
    for (int i = 0; i < 3; ++i)
      master_ed.add_cell(c+i, v+i, v+i+1, v+i+2, v+i+3);
    c += 3;
    for (int i = 0; i < 6; ++i)
      master_ed.add_vertex(v+i, master_tet_set[i]);
    v += 6;
  }
  master_ed.close();

  // Local mesh of slave 'prisms', three tetrahedra created per facet
  Mesh slave_mesh(MPI_COMM_SELF);
  MeshEditor slave_ed;
  slave_ed.open(slave_mesh, mesh.topology().dim(), mesh.geometry().dim());
  slave_ed.init_vertices(slave_facets.size()*6);
  slave_ed.init_cells(slave_facets.size()*3);
  c = 0;
  v = 0;
  for (auto &sf : slave_facets)
  {
    std::vector<Point> slave_tet_set = create_deformed_segment_volume_3d(mesh, sf, u);
    for (int i = 0; i < 3; ++i)
      slave_ed.add_cell(c+i, v+i, v+i+1, v+i+2, v+i+3);
    c += 3;
    for (int i = 0; i < 6; ++i)
      slave_ed.add_vertex(v+i, slave_tet_set[i]);
    v += 6;
  }
  slave_ed.close();

  // Check each master 'prism' against each slave 'prism'
  for (unsigned int i = 0; i < master_facets.size(); ++i)
    for (unsigned int j = 0; j < slave_facets.size(); ++j)
    {
      std::size_t mf = master_facets[i];
      std::size_t sf = slave_facets[j];

      if (check_tet_set_collision(master_mesh, i*3, slave_mesh, j*3))
        master_to_slave[mf].push_back(sf);
    }


  for (auto m : master_to_slave)
  {
    std::cout << m.first << ":";
    for (auto q : m.second)
      std::cout << q << " ";
    std::cout << "\n";
  }

   return master_to_slave;
}
