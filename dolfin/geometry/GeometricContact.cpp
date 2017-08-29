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
#include <dolfin/function/FunctionSpace.h>
#include <dolfin/fem/GenericDofMap.h>
#include <dolfin/mesh/Mesh.h>
#include <dolfin/mesh/MeshEditor.h>
#include <dolfin/mesh/Facet.h>
#include <dolfin/mesh/Vertex.h>
#include <dolfin/common/Timer.h>

#include "BoundingBoxTree.h"
#include "CollisionDetection.h"
#include "Point.h"

#include "GeometricContact.h"

using namespace dolfin;

//-----------------------------------------------------------------------------
std::vector<Point> GeometricContact::create_deformed_segment_volume(const Mesh& mesh,
                                                                    std::size_t facet_index,
                                                                    const Function& u,
                                                                    std::size_t gdim)
{
  if (gdim != 3 and gdim != 2)
  {
    dolfin_error("GeometricContact.cpp",
                 "calculate deformation volume",
                 "Must be 2D or 3D");
  }

  Facet facet(mesh, facet_index);

  // Get id of attached cell
  std::size_t id = facet.entities(mesh.topology().dim())[0];

  // Vector value of Function
  // FIXME: if gdim == 2 then does Point have it's z component set to 0?
  Array<double> uval(u.value_size());

  const Cell cell(mesh, id);
  ufc::cell ufc_cell;
  cell.get_cell_data(ufc_cell);

  Point X1 = Vertex(mesh, facet.entities(0)[0]).point();
  const Array<double> _x1(3, X1.coordinates());
  u.eval(uval, _x1, cell, ufc_cell);
  Point x1 = X1 + Point(uval);

  Point X2 = Vertex(mesh, facet.entities(0)[1]).point();
  const Array<double> _x2(3, X2.coordinates());
  u.eval(uval, _x2, cell, ufc_cell);
  Point x2 = X2 + Point(uval);

  if (gdim == 2)
    return {X1, X2, x1, x2};

  Point X3 = Vertex(mesh, facet.entities(0)[2]).point();
  const Array<double> _x3(3, X3.coordinates());
  u.eval(uval, _x3, cell, ufc_cell);
  Point x3 = X3 + Point(uval);

  return {X1, X2, X3, x1, x2, x3};
}
//-----------------------------------------------------------------------------
bool GeometricContact::check_tri_set_collision(const Mesh& master_mesh, std::size_t mindex,
                                               const Mesh& slave_mesh, std::size_t sindex)
{

  for (std::size_t i = mindex; i < mindex + 8; ++i)
    for (std::size_t j = sindex; j < sindex + 8; ++j)
    {
      if (CollisionDetection::collides_triangle_triangle(Cell(master_mesh, i),
                                                         Cell(slave_mesh, j)))
        return true;
    }

  return false;
}
//-----------------------------------------------------------------------------
bool GeometricContact::check_edge_set_collision(const Mesh& master_mesh, std::size_t mindex,
                                               const Mesh& slave_mesh, std::size_t sindex)
{
  const auto mconn = master_mesh.topology()(1, 0);
  const auto sconn = slave_mesh.topology()(1, 0);

  for (std::size_t i = mindex; i < mindex + 4; ++i)
    for (std::size_t j = sindex; j < sindex + 4; ++j)
    {
      const Point p0 = Vertex(master_mesh, mconn(i)[0]).point();
      const Point p1 = Vertex(master_mesh, mconn(i)[1]).point();
      const Point p2 = Vertex(slave_mesh, sconn(j)[0]).point();
      const Point p3 = Vertex(slave_mesh, sconn(j)[1]).point();

      if (CollisionDetection::collides_edge_edge(p0, p1, p2, p3))
        return true;
    }

  return false;
}
//-----------------------------------------------------------------------------
void GeometricContact::create_displacement_volume_mesh(Mesh& displacement_mesh,
                                                       const Mesh& mesh,
                                                       const std::vector<std::size_t> contact_facets,
                                                       const Function& u)
{
  // Precalculate triangles making the surface of a prism
  const std::size_t triangles[8][3] = {{0, 1, 2},
                                       {0, 1, 3},
                                       {1, 4, 3},
                                       {1, 2, 4},
                                       {2, 5, 4},
                                       {2, 0, 5},
                                       {0, 3, 5},
                                       {3, 4, 5}};

  // Edges of surface of prism in 2D
  const std::size_t edges[4][2] = {{0, 1},
                                       {1, 2},
                                       {2, 3},
                                       {3, 0}};

  const std::size_t tdim = mesh.topology().dim();

  // Find number of cells/vertices in projected prism in 2D or 3D
  const std::size_t c_per_f = GeometricContact::cells_per_facet(tdim);
  const std::size_t v_per_f = GeometricContact::vertices_per_facet(tdim);

  // Local mesh of master 'prisms', eight triangles are created per facet in 3D
  // Four edges in 2D
  MeshEditor mesh_ed;
  mesh_ed.open(displacement_mesh, mesh.topology().dim() - 1, mesh.geometry().dim());
  std::size_t nf_local = contact_facets.size();
  std::size_t nf_global = MPI::sum(mesh.mpi_comm(), nf_local);
  mesh_ed.init_cells_global(nf_local*c_per_f, nf_global*c_per_f);
  mesh_ed.init_vertices_global(nf_local*v_per_f, nf_global*v_per_f);

  std::size_t c = 0;
  std::size_t v = 0;

  for (const auto& mf : contact_facets)
  {
    std::vector<Point> master_point_set = create_deformed_segment_volume(mesh, mf, u, tdim);

    if (tdim == 3)
    {
      // Add eight triangles
      for (std::size_t i = 0; i < c_per_f; ++i)
        mesh_ed.add_cell(c + i, v + triangles[i][0],
                         v + triangles[i][1],
                         v + triangles[i][2]);
    }
    else
    {
      // Add four edges
      for (std::size_t i = 0; i < c_per_f; ++i)
        mesh_ed.add_cell(c + i, v + edges[i][0],
                         v + edges[i][1]);
    }
    c += c_per_f;
    for (std::size_t i = 0; i < v_per_f; ++i)
      mesh_ed.add_vertex(v + i, master_point_set[i]);
    v += v_per_f;
  }
  mesh_ed.close();
}
//-----------------------------------------------------------------------------
void GeometricContact::create_communicated_prism_mesh(Mesh& prism_mesh,
                                                      const Mesh& mesh,
                                                      const std::vector<double>& coord,
                                                      std::size_t local_facet_idx)
{
  // Precalculate triangles making the surface of a prism
  const std::size_t triangles[8][3] = {{0, 1, 2},
                                       {0, 1, 3},
                                       {1, 4, 3},
                                       {1, 2, 4},
                                       {2, 5, 4},
                                       {2, 0, 5},
                                       {0, 3, 5},
                                       {3, 4, 5}};

  // Edges of surface of prism in 2D
  const std::size_t edges[4][2] = {{0, 1},
                                       {1, 2},
                                       {2, 3},
                                       {3, 0}};

  const std::size_t tdim = mesh.topology().dim();
  const std::size_t gdim = mesh.geometry().dim();

  // Find number of cells/vertices in projected prism in 2D or 3D
  const std::size_t c_per_f = GeometricContact::cells_per_facet(tdim);
  const std::size_t v_per_f = GeometricContact::vertices_per_facet(tdim);

  MeshEditor m_ed;
  m_ed.open(prism_mesh, tdim - 1, gdim);
  m_ed.init_cells(c_per_f);
  if (tdim == 3)
  {
    for (std::size_t i = 0; i < c_per_f; ++i)
      m_ed.add_cell(i, triangles[i][0], triangles[i][1], triangles[i][2]);
  }
  else
  {
    for (std::size_t i = 0; i < c_per_f; ++i)
      m_ed.add_cell(i, edges[i][0], edges[i][1]);
  }

  m_ed.init_vertices(v_per_f);
  for (std::size_t vert = 0; vert < v_per_f; ++vert)
    m_ed.add_vertex(vert, Point(gdim, coord.data() + (local_facet_idx*v_per_f + vert)*gdim));
  m_ed.close();
}
//-----------------------------------------------------------------------------
void GeometricContact::tabulate_off_process_displacement_volume_mesh_pairs(const Mesh& mesh,
                                                                           const Mesh& slave_mesh,
                                                                           const Mesh& master_mesh,
                                                                           const std::vector<std::size_t>& slave_facets,
                                                                           const std::vector<std::size_t>& master_facets,
                                                                           std::map<std::size_t, std::vector<std::size_t>>& contact_facet_map)
{
  std::vector<std::vector<std::size_t>> send_facets;
  std::vector<std::vector<double>> send_coordinates;

  const std::size_t mpi_size = MPI::size(mesh.mpi_comm());
  const std::size_t mpi_rank = MPI::rank(mesh.mpi_comm());

  const std::size_t tdim = mesh.topology().dim();
  const std::size_t gdim = mesh.geometry().dim();

  const std::size_t c_per_f = GeometricContact::cells_per_facet(tdim);
  const std::size_t v_per_f = GeometricContact::vertices_per_facet(tdim);

  auto slave_bb = slave_mesh.bounding_box_tree();
  auto master_bb = master_mesh.bounding_box_tree();

  Timer t1("YYYY GC:: compute_process_entity_collisions");
  // Find which master processes collide with which local slave cells
  auto overlap = master_bb->compute_process_entity_collisions(*slave_bb);
  auto master_procs = overlap.first;
  auto slave_cells = overlap.second;
  t1.stop();

  // Get slave facet indices to send
  // The slave mesh consists of repeated units of eight triangles
  // making the prisms which are projected forward. The "prism index"
  // can be obtained by integer division of the cell index.
  // Each prism corresponds to a slave facet of the original mesh
  Timer t2("YYYY GC:: populate send facets");
  send_facets.resize(mpi_size);
  for (std::size_t i = 0; i < master_procs.size(); ++i)
  {
    const std::size_t master_rank = master_procs[i];
    // Ignore local (already done)
    if (master_rank != mpi_rank)
    {
      // Get facet from cell index (2D: 4 edges per prism, 3D: eight triangles per prism)
      std::size_t facet = slave_cells[i]/c_per_f;
      send_facets[master_rank].push_back(facet);
    }
  }
  t2.stop();

  // Get unique set of facets to send to each process
  Timer t3("YYYY GC:: sort send facets");
  for (std::size_t p = 0; p != mpi_size; ++p)
  {
    std::vector<std::size_t>& v = send_facets[p];
    std::sort(v.begin(), v.end());
    auto last = std::unique(v.begin(), v.end());
    v.erase(last, v.end());
  }
  t3.stop();

  // Get coordinates of prism (18 doubles in 3D, 8 in 2D)
  // and convert index of slave mesh back to index of main mesh
  Timer t4("YYYY GC:: tabulate send coords");
  send_coordinates.resize(mpi_size);
  for (std::size_t p = 0; p != mpi_size; ++p)
  {
    std::vector<double>& coords_p = send_coordinates[p];
    for (auto& q : send_facets[p])
    {
      const double* coord_vals = slave_mesh.geometry().x(q*v_per_f);
      coords_p.insert(coords_p.end(), coord_vals, coord_vals + gdim*v_per_f);
      // Convert from local prism index to main mesh local facet indexing
      q = slave_facets[q];
    }
  }
  t4.stop();

  Timer t5("YYYY GC:: all_to_all all the things");
  std::vector<std::vector<std::size_t>> recv_facets(mpi_size);
  MPI::all_to_all(mesh.mpi_comm(), send_facets, recv_facets);

  std::vector<std::vector<double>> recv_coordinates(mpi_size);
  MPI::all_to_all(mesh.mpi_comm(), send_coordinates, recv_coordinates);
  t5.stop();

  Timer t6("YYYY GC:: Unpack everything");
  for (std::size_t proc = 0; proc != mpi_size; ++proc)
  {
    auto& rfacet = recv_facets[proc];
    auto& coord = recv_coordinates[proc];
    dolfin_assert(coord.size() == gdim*v_per_f*rfacet.size());

    for (std::size_t j = 0; j < rfacet.size(); ++j)
    {
      // FIXME: inefficient? but difficult to use BBT with primitives
      // so create a small Mesh for each received prism
      Mesh prism_mesh(MPI_COMM_SELF);
      GeometricContact::create_communicated_prism_mesh(prism_mesh, mesh, coord, j);

      // Check all local master facets against received slave data
      for (std::size_t i = 0; i < master_facets.size(); ++i)
      {
        bool collision;
        if (tdim == 3)
          collision = check_tri_set_collision(master_mesh, i*c_per_f, prism_mesh, 0);
        else
          collision = check_edge_set_collision(master_mesh, i*c_per_f, prism_mesh, 0);

        if (collision)
        {
          const std::size_t mf = master_facets[i];
          contact_facet_map[mf].push_back(proc);
          contact_facet_map[mf].push_back(rfacet[j]);
        }
      }
    }
  }
  t6.stop();
}
//-----------------------------------------------------------------------------
void GeometricContact::contact_surface_map_volume_sweep(Mesh& mesh, Function& u,
const std::vector<std::size_t>& master_facets, const std::vector<std::size_t>& slave_facets)
{
  // Construct a dictionary mapping master facets to their collided slave counterparts.

  const std::size_t tdim = mesh.topology().dim();
  if (tdim != 3 and tdim != 2)
  {
    dolfin_error("GeometricContact.cpp",
                 "find contact surface",
                 "Only implemented in 2D/3D");
  }

  const std::size_t gdim = mesh.geometry().dim();
  if (gdim != tdim)
  {
    dolfin_error("GeometricContact.cpp",
                 "find contact surface",
                 "Manifold meshes not supported");
  }

  // Ensure BBT built
  auto mesh_bb = mesh.bounding_box_tree();

  // Make sure facet->cell connections are made
  mesh.init(tdim - 1, tdim);

  Timer t("XXXX GC:: create_displacement_volume_mesh");
  // Make the displacement volume mesh of the master facets
  Mesh master_mesh(mesh.mpi_comm());
  GeometricContact::create_displacement_volume_mesh(master_mesh, mesh, master_facets, u);

  // Make the displacement volume mesh of the slave facets
  Mesh slave_mesh(mesh.mpi_comm());
  GeometricContact::create_displacement_volume_mesh(slave_mesh, mesh, slave_facets, u);
  t.stop();

  _master_to_slave.clear();
  _slave_to_master.clear();

  std::size_t mpi_rank = MPI::rank(mesh.mpi_comm());
  std::size_t mpi_size = MPI::size(mesh.mpi_comm());

  // Find number of cells/vertices in projected prism in 2D or 3D
  const std::size_t c_per_f = GeometricContact::cells_per_facet(tdim);

  // Check each master 'prism' against each slave 'prism'
  // Map is stored as local_master_facet -> [mpi_rank, local_index, mpi_rank, local_index ...]
  // First check locally
  Timer t1("XXXX GC:: collision detec");
  for (std::size_t i = 0; i < master_facets.size(); ++i)
    for (std::size_t j = 0; j < slave_facets.size(); ++j)
    {
      // FIXME: for efficiency, use BBT here
      bool collision;
      if (tdim == 3)
        collision = check_tri_set_collision(master_mesh, i*c_per_f, slave_mesh, j*c_per_f);
      else
        collision = check_edge_set_collision(master_mesh, i*c_per_f, slave_mesh, j*c_per_f);

      if (collision)
      {
        const std::size_t mf = master_facets[i];
        const std::size_t sf = slave_facets[j];
        _master_to_slave[mf].push_back(mpi_rank);
        _master_to_slave[mf].push_back(sf);
        _slave_to_master[sf].push_back(mpi_rank);
        _slave_to_master[sf].push_back(mf);
      }
    }
  t1.stop();

  // Find which [master global/slave entity] BBs overlap in parallel
  if (mpi_size > 1)
  {
    std::vector<std::vector<std::size_t>> send_facets;
    std::vector<std::vector<double>> send_coordinates;

    Timer t("XXXX GC:: tabulate_off_process_displacement_volume_mesh_pairs");
    GeometricContact::tabulate_off_process_displacement_volume_mesh_pairs(mesh, slave_mesh, master_mesh, slave_facets,
                                                                          master_facets, _master_to_slave);

    GeometricContact::tabulate_off_process_displacement_volume_mesh_pairs(mesh, master_mesh, slave_mesh, master_facets,
                                                                          slave_facets, _slave_to_master);
    t.stop();

  }
}
//-----------------------------------------------------------------------------
void GeometricContact::tabulate_collided_cell_dofs(const Mesh& mesh, const GenericDofMap& dofmap,
                                                   const std::map<std::size_t, std::vector<std::size_t>>& master_to_slave,
                                                   std::map<std::size_t, std::vector<std::size_t>>& facet_to_contacted_dofs,
                                                   std::map<std::size_t, std::vector<std::size_t>>& facet_to_off_proc_contacted_dofs)
{
  const std::size_t mpi_rank = MPI::rank(mesh.mpi_comm());
  const std::size_t mpi_size = MPI::size(mesh.mpi_comm());

  // tabulate global DoFs required for off-process insertion
  std::vector<std::size_t> local_to_global_dofs;
  dofmap.tabulate_local_to_global_dofs(local_to_global_dofs);

  const std::size_t tdim = mesh.topology().dim();

  // Send the master cell's dofs to the slave.
  // [proc: [local_slave, contact master dofs, local slave, contact master dofs, ...]]
  std::vector<std::vector<std::size_t>> send_master_dofs(mpi_size);

  for (const auto& m2s : master_to_slave)
  {
    const std::size_t mi = m2s.first;  // master facet indices
    const std::vector<std::size_t> sis = m2s.second;  // [proc, slave facet idx, proc, slave facet idx, ...]

    // Cell to which the master facet belongs and its DoFs
    const Cell m_cell(mesh, Facet(mesh, mi).entities(tdim)[0]);
    const auto& m_cell_dofs_eigen_map
        = dofmap.cell_dofs(m_cell.index());
    const auto m_cell_dofs = ArrayView<const dolfin::la_index>(
        m_cell_dofs_eigen_map.size(), m_cell_dofs_eigen_map.data());

    for (std::size_t j=0; j<sis.size(); j+=2)
    {
      const std::size_t& slave_proc = sis[j];
      const std::size_t& si = sis[j+1];

      // If the slave is on the current process, then append the dofs.
      if (mpi_rank == slave_proc)
      {
        // Cell to which the slave facet belongs and its DoFs
        const Cell s_cell(mesh, Facet(mesh, si).entities(tdim)[0]);
        const auto& s_cell_dofs_eigen_map
            = dofmap.cell_dofs(s_cell.index());
        const auto s_cell_dofs = ArrayView<const dolfin::la_index>(
            s_cell_dofs_eigen_map.size(), s_cell_dofs_eigen_map.data());

        // Insert slave dofs into the map
//        facet_to_contacted_dofs[mi].insert(std::end(facet_to_contacted_dofs[mi]),
//                                           std::begin(s_cell_dofs),
//                                           std::end(s_cell_dofs));

        facet_to_contacted_dofs[si].insert(std::end(facet_to_contacted_dofs[si]),
                                           std::begin(m_cell_dofs),
                                           std::end(m_cell_dofs));
      }
      else // schedule master dofs to be dispatched to the slaves
      {
        // Convert to global dofs for column dof entry
        std::vector<std::size_t> global_master_dofs(std::begin(m_cell_dofs),
                                                    std::end(m_cell_dofs));
        for (auto& dof : global_master_dofs)
          dof = local_to_global_dofs[dof];

        send_master_dofs[slave_proc].push_back(si);
        send_master_dofs[slave_proc].insert(std::end(send_master_dofs[slave_proc]),
                                            std::begin(global_master_dofs),
                                            std::end(global_master_dofs));
      }
    }

  }

  // Running in serial. So we're done.
  if (mpi_size == 1)
    return;

  std::vector<std::vector<std::size_t>> recv_master_dofs;
  MPI::all_to_all(mesh.mpi_comm(), send_master_dofs, recv_master_dofs);

  const std::size_t num_dofs_per_cell = dofmap.max_element_dofs();

  // Tabulate the communicated dofs belonging to the master cells. This requires
  // saving the global DoFs of the master cells to _local_cell_to_off_proc_contact_dofs
  // at the index of the local on process slave cell index.
  for (const auto& global_master_dofs : recv_master_dofs)
  {
    if (global_master_dofs.empty())
      continue;

    for (std::size_t j = 0; j < global_master_dofs.size(); j += num_dofs_per_cell + 1)
    {
      // global_master_dofs[j] is the slave facet local index. global_master_dofs[j+1:j+num_dofs_per_cell]
      // are the communicated master cell dofs.
      std::vector<std::size_t>& dof_list = facet_to_off_proc_contacted_dofs[global_master_dofs[j]];
      for (std::size_t i = 0; i < num_dofs_per_cell; ++i)
        dof_list.push_back(global_master_dofs[j + i + 1]);
    }
  }
}
//-----------------------------------------------------------------------------
void GeometricContact::tabulate_contact_cell_to_shared_dofs(Mesh& mesh, Function& u,
                                                            const std::vector<std::size_t>& master_facets,
                                                            const std::vector<std::size_t>& slave_facets)
{
  const auto V = u.function_space();
  const auto dofmap = V->dofmap();

  // Start from fresh
  _local_cell_to_contact_dofs.clear();
  _local_cell_to_off_proc_contact_dofs.clear();

  GeometricContact::tabulate_collided_cell_dofs(mesh, *dofmap, _master_to_slave, _local_cell_to_contact_dofs, _local_cell_to_off_proc_contact_dofs);
  GeometricContact::tabulate_collided_cell_dofs(mesh, *dofmap, _slave_to_master, _local_cell_to_contact_dofs, _local_cell_to_off_proc_contact_dofs);
}
//-----------------------------------------------------------------------------
void
GeometricContact::tabulate_contact_shared_cells(Mesh& mesh, Function& u,
                                                const std::vector<std::size_t>& master_facets,
                                                const std::vector<std::size_t>& slave_facets)
{
//  const std::size_t mpi_rank = MPI::rank(mesh.mpi_comm());
  const std::size_t mpi_size = MPI::size(mesh.mpi_comm());

//  const auto& m2s = master_to_slave();
  const auto& s2m = slave_to_master();

  const auto V = u.function_space();
  const auto dofmap = V->dofmap();

  // tabulate global DoFs required for off-process insertion
  std::vector<std::size_t> local_to_global_dofs;
  dofmap->tabulate_local_to_global_dofs(local_to_global_dofs);

  const std::size_t tdim = mesh.topology().dim();

  // Send the master cell's dofs to the slave.
  // [proc: [local_slave, contact master dofs, local slave, contact master dofs, ...]]
//  std::vector<std::vector<std::size_t>> send_master_dofs(mpi_size);

  // Communicate the slave cells' meta data to the master.
  // First we tabulate the slave cells' information for dispatch
  std::vector<std::vector<std::size_t>> slave_facet_infos_send(mpi_size);
  std::vector<std::vector<std::size_t>> slave_cell_global_dofs_send(mpi_size);
  std::vector<std::vector<double>> slave_cell_dof_coords_send(mpi_size);
  std::vector<std::vector<double>> slave_cell_dof_coeffs_send(mpi_size);

  std::vector<double> dof_coords;

  const FiniteElement element = *V->element();
  std::vector<double> dof_coeffs(element.space_dimension());
  ufc::cell ufc_cell;

  for (const auto& slave : s2m)
  {
    const std::size_t slave_idx = slave.first; // slave facet indices
    const std::vector<std::size_t> master_procs_idxs = slave.second;  // [proc, master facet idx, proc, master facet idx, ...

    const Facet slave_facet(mesh, slave_idx);
    const Cell slave_cell = Cell(mesh, slave_facet.entities(tdim)[0]);
    const std::size_t slave_facet_local_idx = slave_cell.index(slave_facet);

    // Tabulate dof coords
    slave_cell.get_coordinate_dofs(dof_coords);

    // Tabulate cell dofs
    const auto& s_cell_dofs_eigen_map
        = dofmap->cell_dofs(slave_cell.index());
    const auto s_cell_dofs = ArrayView<const dolfin::la_index>(
        s_cell_dofs_eigen_map.size(), s_cell_dofs_eigen_map.data());
    // Convert to global dofs
    std::vector<std::size_t> global_s_cell_dofs(std::begin(s_cell_dofs),
                                                std::end(s_cell_dofs));
    for (auto& dof : global_s_cell_dofs)
      dof = local_to_global_dofs[dof];

    // Tabulate cell dof coefficients (local finite element vector on the slave element)

    // Update to current cell
    slave_cell.get_cell_data(ufc_cell);
    u.restrict(dof_coeffs.data(), element, slave_cell, dof_coords.data(),
             ufc_cell);

    for (std::size_t j=0; j<master_procs_idxs.size(); j+=2)
    {
      const std::size_t master_proc = master_procs_idxs[j];
      const std::size_t master_facet_idx = master_procs_idxs[j+1];

      // [slave_facet_idx, slave_facet_local_idx, master_facet_idx]
      slave_facet_infos_send[master_proc].push_back(slave_facet.index());
      slave_facet_infos_send[master_proc].push_back(slave_facet_local_idx);
      slave_facet_infos_send[master_proc].push_back(master_facet_idx);

      slave_cell_global_dofs_send[master_proc].insert(std::end(slave_cell_global_dofs_send[master_proc]),
                                                      std::begin(global_s_cell_dofs), std::end(global_s_cell_dofs));

      slave_cell_dof_coords_send[master_proc].insert(std::end(slave_cell_dof_coords_send[master_proc]),
                                                     std::begin(dof_coords), std::end(dof_coords));

      slave_cell_dof_coeffs_send[master_proc].insert(std::end(slave_cell_dof_coeffs_send[master_proc]),
                                                      std::begin(dof_coeffs), std::end(dof_coeffs));
    }
  }

  // Receive the cell metadata
  std::vector<std::vector<std::size_t>> slave_facet_infos_recv;
  std::vector<std::vector<std::size_t>> slave_cell_global_dofs_recv;
  std::vector<std::vector<double>> slave_cell_dof_coords_recv;
  std::vector<std::vector<double>> slave_cell_dof_coeffs_recv;

  // FIXME: This is too much MP....?
  MPI::all_to_all(mesh.mpi_comm(), slave_facet_infos_send, slave_facet_infos_recv);
  MPI::all_to_all(mesh.mpi_comm(), slave_cell_global_dofs_send, slave_cell_global_dofs_recv);
  MPI::all_to_all(mesh.mpi_comm(), slave_cell_dof_coords_send, slave_cell_dof_coords_recv);
  MPI::all_to_all(mesh.mpi_comm(), slave_cell_dof_coeffs_send, slave_cell_dof_coeffs_recv);

  const std::size_t num_coords_per_cell = Cell(mesh, 0).num_vertices()*mesh.geometry().dim();
  const std::size_t num_dofs_per_cell = dofmap->max_element_dofs();

  for (std::size_t proc_source=0; proc_source<mpi_size; ++proc_source)
  {
    if (slave_facet_infos_recv[proc_source].empty())
      continue;

    for (std::size_t j=0; j<slave_facet_infos_recv[proc_source].size()/3; ++j)
    {
      // [slave_facet_idx, slave_facet_local_idx, master_facet_idx]
      const std::size_t slave_facet_idx = slave_facet_infos_recv[proc_source][3*j];
      const std::size_t slave_facet_local_idx = slave_facet_infos_recv[proc_source][3*j + 1];
      const std::size_t master_idx = slave_facet_infos_recv[proc_source][3*j + 2];

      const std::vector<std::size_t> cell_dofs(slave_cell_global_dofs_recv[proc_source].begin() + j*num_dofs_per_cell,
                                               slave_cell_global_dofs_recv[proc_source].begin() + (j + 1)*num_dofs_per_cell);
      const std::vector<double> cell_dof_coords(slave_cell_dof_coords_recv[proc_source].begin() + j*num_coords_per_cell,
                                                slave_cell_dof_coords_recv[proc_source].begin() + (j + 1)*num_coords_per_cell);

      // FIXME: Contact on manifolds will not have the same number of coeffs as coords
      const std::vector<double> cell_dof_coeffs(slave_cell_dof_coeffs_recv[proc_source].begin() + j*num_coords_per_cell,
                                                slave_cell_dof_coeffs_recv[proc_source].begin() + (j + 1)*num_coords_per_cell);

      auto cell_md = std::make_shared<CellMetaData>(slave_facet_idx, slave_facet_local_idx,
                           cell_dof_coords, cell_dofs, cell_dof_coeffs);
      _master_facet_to_contacted_cells[master_idx].push_back(cell_md);
    }
  }

}
//-----------------------------------------------------------------------------
