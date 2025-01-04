/*****************************************************************************
 *
 * SVBroi.c -- region of interest management for SVBony cameras
 *
 * Copyright 2020 James Fidell (james@openastroproject.org)
 *
 * License:
 *
 * This file is part of the Open Astro Project.
 *
 * The Open Astro Project is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The Open Astro Project is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the Open Astro Project.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include <oa_common.h>
#include <openastro/camera.h>
#include <openastro/errno.h>

#include "oacamprivate.h"
#include "SVBoacam.h"
#include "SVBstate.h"


int
oaSVBCameraTestROISize ( oaCamera* camera, unsigned int tryX,
    unsigned int tryY, unsigned int* suggX, unsigned int* suggY )
{
  int		done;
  unsigned int	n;
  SVB_STATE*	cameraInfo = camera->_private;

  // USB3 cameras have different rules
  if ( cameraInfo->usb3Cam ) {
    if ( tryX % 8 == 0 && tryY % 2 == 0 ) {
      if (( tryX * cameraInfo->binMode ) <= cameraInfo->frameSizes[1].sizes[0].x
          && ( tryY * cameraInfo->binMode ) <=
          cameraInfo->frameSizes[1].sizes[0].y ) {
        oaLogDebug(OA_LOG_CAMERA, "tryX = %d / tryY = %d", tryX, tryY);
        return OA_ERR_NONE;
      }
    }
    *suggX = ( tryX & ~0x7 );
    if (( *suggX * cameraInfo->binMode ) >
        cameraInfo->frameSizes[1].sizes[0].x ) {
      *suggX = cameraInfo->frameSizes[ cameraInfo->binMode ].sizes[0].x;
    }
    *suggY = ( tryX & ~0x1 );
    if (( *suggY * cameraInfo->binMode ) >
        cameraInfo->frameSizes[1].sizes[0].y ) {
      *suggY = cameraInfo->frameSizes[ cameraInfo->binMode ].sizes[0].y;
    }
    oaLogDebug(OA_LOG_CAMERA, "suggX = %d / suggY = %d", *suggX, *suggY);
    return -OA_ERR_INVALID_SIZE;
  }


  if ( tryX > 0 && tryY > 0 && ( tryX * tryY ) % 1024 == 0 ) {
    if (( tryX * cameraInfo->binMode ) <= cameraInfo->frameSizes[1].sizes[0].x
        && ( tryY * cameraInfo->binMode ) <=
        cameraInfo->frameSizes[1].sizes[0].y ) {
      return OA_ERR_NONE;
    }
  }

  for ( n = 0, done = -1; n <
      cameraInfo->frameSizes[ cameraInfo->binMode].numSizes && done < 0; n++ ) {
    if ( cameraInfo->frameSizes[ cameraInfo->binMode ].sizes[n].x <= tryX ) {
      done = n;
    }
  }
  if ( done < 0 ) {
    done = cameraInfo->frameSizes[ cameraInfo->binMode ].numSizes;
  }

  *suggX = cameraInfo->frameSizes[ cameraInfo->binMode ].sizes[done - 1].x;
  *suggY = cameraInfo->frameSizes[ cameraInfo->binMode ].sizes[done - 1].y;
  oaLogDebug(OA_LOG_CAMERA, "suggX = %d / suggY = %d", *suggX, *suggY);
  return -OA_ERR_INVALID_SIZE;
}
