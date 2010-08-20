/* Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009

	M Roberts (original release)
	Robin Birch <robinb@ruffnready.co.uk>
	Samuel Gisiger <samuel.gisiger@triadis.ch>
	Jeff Goodenough <jeff@enborne.f2s.com>
	Alastair Harrison <aharrison@magic.force9.co.uk>
	Scott Penrose <scottp@dd.com.au>
	John Wharington <jwharington@gmail.com>
	Lars H <lars_hn@hotmail.com>
	Rob Dunning <rob@raspberryridgesheepfarm.com>
	Russell King <rmk@arm.linux.org.uk>
	Paolo Ventafridda <coolwind@email.it>
	Tobias Lohner <tobias@lohner-net.de>
	Mirek Jezek <mjezek@ipplc.cz>
	Max Kellermann <max@duempel.org>
	Tobias Bieniek <tobias.bieniek@gmx.de>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "Math/Earth.hpp"

#include <assert.h>

static inline bool
is_zero(const fixed value)
{
  return (long)fabs(value * 10000) == 0;
}

static inline bool
is_one(const fixed value)
{
  return is_zero(value - fixed_one);
}

static inline bool
equals(const fixed a, const fixed b)
{
  if (is_zero(a) || is_zero(b))
    return is_zero(a) && is_zero(b);

  return is_one(a / b);
}

static inline bool
equals(const fixed a, int b)
{
  return equals(a, fixed(b));
}

static inline bool
equals(const Angle a, int b)
{
  return equals(a.value_degrees(), fixed(b));
}

int main(int argc, char **argv)
{
  const GEOPOINT a(Angle::degrees(fixed(7.7061111111111114)),
                   Angle::degrees(fixed(51.051944444444445)));
  const GEOPOINT b(Angle::degrees(fixed(7.599444444444444)),
                   Angle::degrees(fixed(51.099444444444444)));

  fixed distance = Distance(a, b);
  assert(distance > fixed(9130) && distance < fixed(9140));

  Angle bearing = Bearing(a, b);
  assert(bearing.value_degrees() > fixed(304));
  assert(bearing.value_degrees() < fixed(306));

  bearing = Bearing(b, a);
  assert(bearing.value_degrees() > fixed(124));
  assert(bearing.value_degrees() < fixed(126));

  distance = ProjectedDistance(a, b, a);
  assert(is_zero(distance));
  distance = ProjectedDistance(a, b, b);
  assert(distance > fixed(9120) && distance < fixed(9140));

  const GEOPOINT middle(a.Longitude.Fraction(b.Longitude, fixed_half),
                        a.Latitude.Fraction(b.Latitude, fixed_half));
  distance = ProjectedDistance(a, b, middle);
  assert(distance > fixed(9100/2) && distance < fixed(9140/2));

  return 0;
}
