// Copyright (C) 2015 Ettie Unwin
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

#ifndef __PLANE_H
#define __PLANE_H

#include <cmath>
#include "Point.h"

namespace dolfin
{
  /// A Plane represents a plane in :math:`\mathbb{R}^3` with
  /// normal vector to the plane :math: `n = x, y, z,` and a distance
  /// to the plane :math:`x.n = d`.  It supports inputs of a normal
  /// vector and a distance, a point on the plane and a normal vector
  /// to the plane or three points.

  class Plane
  {
  public:

  /// Constructs a plane using a normal vector n and a distance to
  /// the plane d.
  Plane(Point n, double d) : _n(n), _d(d)
    { }

  /// Constructs a plane using a normal vector n and a point on the plane
  /// a.
  Plane(Point a, Point n) : _n(n), _d(a.dot(n))
    { }

  /// Constructs a plane using three points on the plane.
  Plane(Point a, Point b, Point c)
  { _n = (b-a).cross(c-a); _d = a.dot(_n); }

  /// Returns the normal vector to the plane
  Point normal()
    { return _n; }

  private:

    Point _n;
    double _d;
  };


}

#endif
