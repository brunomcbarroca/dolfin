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

#ifndef __GEOMETRIC_CONTACT_H
#define __GEOMETRIC_CONTACT_H

#include <map>
#include <utility>
#include <vector>

namespace dolfin
{

  // Forward declarations
  class Mesh;
  class Point;
  class Facet;
  class Function;

  /// This class implements ...

  class GeometricContact
  {
  public:
    GeometricContact()
    {
      // Constructor
    }

    ~GeometricContact()
    {
      // Destructor
    }

    /// Calculate map from master facets to possible colliding slave facets
    void
      contact_surface_map_volume_sweep(Mesh& mesh, Function& u,
                                          const std::vector<std::size_t>& master_facets,
                                          const std::vector<std::size_t>& slave_facets);

    /// For each of the local cells on this process. Compute the DoFs of the cells on the
    /// contact process.
    void
    tabulate_contact_cell_to_shared_dofs(Mesh& mesh, Function& u,
                                         const std::vector<std::size_t>& master_facets,
                                         const std::vector<std::size_t>& slave_facets);


    /// Tabulate the mapping of local cells in contact with their shared cells' metadata.
    void
    tabulate_contact_shared_cells(Mesh& mesh, Function& u,
                                  const std::vector<std::size_t>& master_facets,
                                  const std::vector<std::size_t>& slave_facets);

    /// Get master to slave mapping
    const std::map<std::size_t, std::vector<std::size_t>>& master_to_slave() const
    {
      return _master_to_slave;
    }

    /// Get slave to master mapping
    const std::map<std::size_t, std::vector<std::size_t>>& slave_to_master() const
    {
      return _slave_to_master;
    }

    /// Get dof matchup
    const std::map<std::size_t, std::vector<std::size_t>>& local_cells_to_contact_dofs() const
    {
      return _local_cell_to_contact_dofs;
    };

    /// Get dof matchup
    const std::map<std::size_t, std::vector<std::size_t>>& local_cell_to_off_proc_contact_dofs() const
    {
      return _local_cell_to_off_proc_contact_dofs;
    };

//    const std::vector<std::vector<std::size_t>> slave_cell_dofs(std::size_t master_idx) const
//    {
//      std::vector<std::vector<std::size_t>> contact_cell_dofs;
//      const auto list_of_cells = _master_facet_to_contacted_cells.find(master_idx);
//      dolfin_assert(list_of_cells != _master_facet_to_contacted_cells.end());
//      for (const auto& cell_md : list_of_cells->second)
//        contact_cell_dofs.push_back(cell_md.cell_dofs);
//      return contact_cell_dofs;
//    };

    const std::map<std::size_t, std::vector<double>>& master_to_candidate_slave_coords()
    {
      auto& data = _master_facet_to_contacted_cells_coords;
      for (const auto& entry : _master_facet_to_contacted_cells)
      {
        const std::size_t master_idx = entry.first;
        for (const auto& slave_cell_metadata : entry.second)
        {
          data[master_idx].insert(std::end(data[master_idx]),
                                  std::begin(slave_cell_metadata.dof_coords),
                                  std::end(slave_cell_metadata.dof_coords));
        }
      }
      return data;
    };

    const std::map<std::size_t, std::vector<std::size_t>>& master_to_candidate_slave_dofs()
    {
      auto& data = _master_facet_to_contacted_cells_dofs;
      for (const auto& entry : _master_facet_to_contacted_cells)
      {
        const std::size_t master_idx = entry.first;
        for (const auto& slave_cell_metadata : entry.second)
        {
          data[master_idx].insert(std::end(data[master_idx]),
                                  std::begin(slave_cell_metadata.cell_dofs),
                                  std::end(slave_cell_metadata.cell_dofs));
        }
      }
      return data;
    };

    const std::map<std::size_t, std::vector<double>>& master_to_candidate_slave_coeffs()
    {
      auto& data = _master_facet_to_contacted_cells_coeffs;
      for (const auto& entry : _master_facet_to_contacted_cells)
      {
        const std::size_t master_idx = entry.first;
        for (const auto& slave_cell_metadata : entry.second)
        {
          data[master_idx].insert(std::end(data[master_idx]),
                                  std::begin(slave_cell_metadata.dof_coeffs),
                                  std::end(slave_cell_metadata.dof_coeffs));
        }
      }
      return data;
    };

  private:

    struct CellMetaData
    {
      CellMetaData(const std::vector<double> dof_coords,
                   const std::vector<std::size_t> cell_dofs,
                   const std::vector<double> dof_coeffs) :
          dof_coords(dof_coords), cell_dofs(cell_dofs), dof_coeffs(dof_coeffs) {}
      const std::vector<double> dof_coords;
      const std::vector<std::size_t> cell_dofs;
      const std::vector<double> dof_coeffs;
    };

    std::map<std::size_t, std::vector<CellMetaData>> _master_facet_to_contacted_cells;
    std::map<std::size_t, std::vector<double>> _master_facet_to_contacted_cells_coords;
    std::map<std::size_t, std::vector<std::size_t>> _master_facet_to_contacted_cells_dofs;
    std::map<std::size_t, std::vector<double>> _master_facet_to_contacted_cells_coeffs;

    // Check whether two sets of triangles collide in 3D
    static bool check_tri_set_collision(const Mesh& mmesh, std::size_t mi,
                                        const Mesh& smesh, std::size_t si);

    // Check whether two sets of edges collide in 2D
    static bool check_edge_set_collision(const Mesh& mmesh, std::size_t mi,
                                         const Mesh& smesh, std::size_t si);

    // Project surface forward from a facet using 'u', creating a prismoidal volume in 2D or 3D
    static std::vector<Point> create_deformed_segment_volume(const Mesh& mesh,
                                                             std::size_t facet_index,
                                                             const Function& u,
                                                             std::size_t gdim);

    // Make a mesh of the displacement volume
    static void create_displacement_volume_mesh(Mesh& displacement_mesh,
                                                const Mesh& mesh,
                                                const std::vector<std::size_t> contact_facets,
                                                const Function& u);

    // Make a mesh of a communicated facet
    static void create_communicated_prism_mesh(Mesh& prism_mesh,
                                               const Mesh& mesh,
                                               const std::vector<double>& coord,
                                               std::size_t local_facet_idx);

    // Tabulate pairings between collided displacement volume meshes.
    static void tabulate_off_process_displacement_volume_mesh_pairs(const Mesh& mesh,
                                                                    const Mesh& slave_mesh,
                                                                    const Mesh& master_mesh,
                                                                    const std::vector<std::size_t>& slave_facets,
                                                                    const std::vector<std::size_t>& master_facets,
                                                                    std::map<std::size_t, std::vector<std::size_t>>& contact_facet_map);

    // Tabulate pairings between facet index and collided cell DoFs
    static void tabulate_collided_cell_dofs(const Mesh& mesh, const GenericDofMap& dofmap,
                                            const std::map<std::size_t, std::vector<std::size_t>>& master_to_slave,
                                            std::map<std::size_t, std::vector<std::size_t>>& facet_to_contacted_dofs,
                                            std::map<std::size_t, std::vector<std::size_t>>& facet_to_off_proc_contacted_dofs);

    // Find number of cells in projected prism in 2D or 3D
    static std::size_t cells_per_facet(std::size_t tdim) { return (tdim - 1)*4; };

    // Find number of cells in projected prism in 2D or 3D
    static std::size_t vertices_per_facet(std::size_t tdim) { return tdim*2; };

    std::map<std::size_t, std::vector<std::size_t>> _master_to_slave;
    std::map<std::size_t, std::vector<std::size_t>> _slave_to_master;
    std::map<std::size_t, std::vector<std::size_t>> _local_cell_to_contact_dofs;
    std::map<std::size_t, std::vector<std::size_t>> _local_cell_to_off_proc_contact_dofs;

  };

}

#endif
