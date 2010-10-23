/*
Copyright_License {

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

#include "MapWindowProjection.hpp"
#include "Math/FastRotation.hpp"
#include "Screen/Layout.hpp"
#include "SettingsComputer.hpp"
#include "NMEA/Info.hpp"
#include "NMEA/Derived.hpp"
#include "Waypoint/Waypoint.hpp"

#include <stdlib.h>
#include <math.h>

MapWindowProjection::MapWindowProjection():
  WindowProjection(),
  DisplayMode(dmCruise),
  DisplayAircraftAngle(Angle::native(fixed_zero)),
  MapScale(5),
  _RequestedMapScale(5),
  _origin_centered(false),
  ScaleListCount (0) {}

void
MapWindowProjection::Initialize(const SETTINGS_MAP &settings_map,
                                         const RECT &rc)
{
  MapRect = rc;

  ScaleList[0] = fixed_half;
  ScaleList[1] = fixed_one;
  ScaleList[2] = fixed_two;
  ScaleList[3] = fixed(5);
  ScaleList[4] = fixed_ten;
  ScaleList[5] = fixed(20);
  ScaleList[6] = fixed(50);
  ScaleList[7] = fixed(100);
  ScaleList[8] = fixed(200);
  ScaleList[9] = fixed(500);
  ScaleList[10] = fixed(1000);
  ScaleListCount = 11;

  _RequestedMapScale = LimitMapScale(_RequestedMapScale, settings_map);
}

bool
MapWindowProjection::WaypointInScaleFilter(const Waypoint &way_point) const
{
  return (MapScale <= (way_point.is_landable() ? fixed_int_constant(20) :
                                                 fixed_ten));
}

void
MapWindowProjection::CalculateOrientationNormal(const NMEA_INFO &basic,
    const DERIVED_INFO &derived, const SETTINGS_MAP &settings)
{
  Angle trackbearing = basic.TrackBearing;

  if ((settings.DisplayOrientation == NORTHUP)
      || ((settings.DisplayOrientation == NORTHTRACK)
          && (DisplayMode != dmCircling))
      || (((settings.DisplayOrientation == NORTHCIRCLE)
           || (settings.DisplayOrientation == TRACKCIRCLE))
          && (DisplayMode == dmCircling))) {
    _origin_centered = true;

    if (settings.DisplayOrientation == TRACKCIRCLE) {
      SetScreenAngle(derived.task_stats.current_leg.
                     solution_remaining.Vector.Bearing);
      DisplayAircraftAngle = trackbearing - GetScreenAngle();
    } else {
      SetScreenAngle(Angle::native(fixed_zero));
      DisplayAircraftAngle = trackbearing;
    }

    DisplayAircraftAngle = DisplayAircraftAngle.as_bearing();
  } else {
    // normal, glider forward
    _origin_centered = false;
    SetScreenAngle(trackbearing);
    DisplayAircraftAngle = Angle::native(fixed_zero);
  }
}

void
MapWindowProjection::CalculateOrientationTargetPan(const NMEA_INFO &basic,
    const DERIVED_INFO &derived, const SETTINGS_MAP &settings)
{
  if (derived.common_stats.active_taskpoint_index ==
      settings.TargetPanIndex) {
    CalculateOrientationNormal(basic, derived, settings);
  } else {
    SetScreenAngle(Angle::native(fixed_zero));
    DisplayAircraftAngle = basic.TrackBearing;
  }
}

void
MapWindowProjection::CalculateOrigin(const RECT rc, const NMEA_INFO &DrawInfo,
    const DERIVED_INFO &DerivedDrawInfo,
    const SETTINGS_COMPUTER &settings_computer,
    const SETTINGS_MAP &settings_map)
{
  MapRect = rc;

  if (settings_map.TargetPan)
    CalculateOrientationTargetPan(DrawInfo, DerivedDrawInfo, settings_map);
  else
    CalculateOrientationNormal(DrawInfo, DerivedDrawInfo, settings_map);

  if (_origin_centered || settings_map.EnablePan) {
    SetScreenOrigin((rc.left + rc.right) / 2, (rc.bottom + rc.top) / 2);
  } else {
    SetScreenOrigin((rc.left + rc.right) / 2,
                    ((rc.top - rc.bottom) *
                     settings_map.GliderScreenPosition / 100) + rc.bottom);
  }

  if (settings_map.EnablePan)
    SetGeoLocation(settings_map.PanLocation);
  else
    // Pan is off
    SetGeoLocation(DrawInfo.Location);

  Orig_Aircraft = GeoToScreen(DrawInfo.Location);

  UpdateScreenBounds();
}

fixed
MapWindowProjection::GetScreenDistanceMeters() const
{
  return DistancePixelsToMeters(max_dimension(GetMapRect()));
}

fixed
MapWindowProjection::CalculateMapScale(int scale) const
{
  assert(scale >= 0 && scale < ScaleListCount);

  return ScaleList[scale] * GetMapResolutionFactor() /
         IBLSCALE(GetScreenWidth());
}

fixed
MapWindowProjection::LimitMapScale(fixed value,
                                   const SETTINGS_MAP& settings_map)
{
  fixed minreasonable = fixed(0.05);

  if (settings_map.AutoZoom && DisplayMode != dmCircling) {
#ifdef OLD_TASK // auto zoom 
    if (Calculated().common_stats.active_has_previous &&
        Calculated().common_stats.ordered_has_targets) {
      minreasonable = 0.88;
    } else {
      minreasonable = 0.44;
    }
#else
    minreasonable = fixed(0.44);
#endif
  }

  value = max(minreasonable, min(fixed_int_constant(160), value));
  if (HaveScaleList()) {
    int scale = FindMapScale(value);
    if (scale >= 0)
      value = CalculateMapScale(scale);
  }

  return value;
}

fixed
MapWindowProjection::StepMapScale(fixed scale, int Step) const
{
  int i = FindMapScale(scale);
  if (i < 0)
    return scale;

  if (abs(Step) >= 4)
    i += Step / 4;
  else
    i += Step;

  i = max(0, min(ScaleListCount - 1, i));
  return CalculateMapScale(i);
}

int
MapWindowProjection::FindMapScale(const fixed Value) const
{
  fixed BestFit(99999);
  int BestFitIdx = -1;
  fixed DesiredScale = Value * IBLSCALE(MapRect.right) /
                       GetMapResolutionFactor();

  for (int i = 0; i < ScaleListCount; i++) {
    fixed err = fabs(DesiredScale - ScaleList[i]) / DesiredScale;
    if (err < BestFit) {
      BestFit = err;
      BestFitIdx = i;
    }
  }

  return BestFitIdx;
}

void
MapWindowProjection::ModifyMapScale(const SETTINGS_MAP &settings_map)
{
  // limit zoomed in so doesn't reach silly levels
  _RequestedMapScale = LimitMapScale(_RequestedMapScale, settings_map);
  MapScale = _RequestedMapScale;

  SetScale(Units::ToUserDistance(fixed(GetMapResolutionFactor()) /
                                               MapScale));
}

void
MapWindowProjection::UpdateMapScale(const DERIVED_INFO &DerivedDrawInfo,
                                    const SETTINGS_MAP &settings_map)
{
  static fixed StartingAutoMapScale(fixed_zero);
  fixed AutoZoomFactor;
  static DisplayMode_t DisplayModeLast = DisplayMode;
  static bool TargetPanLast = false;
  static fixed TargetPanUnZoom = fixed_one;

  // if there is user intervention in the scale
  if (positive(settings_map.MapScale)) {
    fixed ext_mapscale = LimitMapScale(fixed(settings_map.MapScale),
                                       settings_map);
    if (positive(ext_mapscale) && DisplayMode == DisplayModeLast)
      _RequestedMapScale = ext_mapscale;
  }

  if(MapScale != _RequestedMapScale)
    ModifyMapScale(settings_map);

  DisplayModeLast = DisplayMode;

  fixed wpd;
  if (settings_map.TargetPan)
    wpd = fixed(settings_map.TargetZoomDistance);
  else
    wpd = DerivedDrawInfo.ZoomDistance;

  if (settings_map.TargetPan) {
    if (!TargetPanLast) { // just entered targetpan so save zoom
      TargetPanLast = true;
      TargetPanUnZoom = MapScale;
    }
    // set scale exactly so that waypoint distance is the zoom factor
    // across the screen
    _RequestedMapScale = LimitMapScale(Units::ToUserDistance(wpd / 4),
                                       settings_map);
    ModifyMapScale(settings_map);
    return;
  }

  if (settings_map.AutoZoom && positive(wpd)) {
    if (((settings_map.DisplayOrientation == NORTHTRACK)
          && (DisplayMode != dmCircling))
         || (settings_map.DisplayOrientation == NORTHUP)
         || (((settings_map.DisplayOrientation == NORTHCIRCLE)
             || (settings_map.DisplayOrientation == TRACKCIRCLE))
            && (DisplayMode == dmCircling))) {
      AutoZoomFactor = fixed(2.5);
    } else {
      AutoZoomFactor = fixed_four;
    }

    if ((wpd < Units::ToSysDistance(AutoZoomFactor * MapScale))
        || (StartingAutoMapScale == fixed_zero)) {
      // waypoint is too close, so zoom in
      // OR just turned waypoint

      // this is the first time this waypoint has gotten close,
      // so save original map scale

      if (StartingAutoMapScale == fixed_zero)
        StartingAutoMapScale = MapScale;

      // set scale exactly so that waypoint distance is the zoom factor
      // across the screen
      _RequestedMapScale = LimitMapScale(
          Units::ToUserDistance(wpd / AutoZoomFactor), settings_map);
      ModifyMapScale(settings_map);
    }
  } else if (TargetPanLast) {
    _RequestedMapScale = TargetPanUnZoom;
    ModifyMapScale(settings_map);
  }

  if (!settings_map.TargetPan && TargetPanLast)
    TargetPanLast = false;
}

fixed 
MapWindowProjection::GetMapScaleUser() const 
{
  return MapScale;
}
