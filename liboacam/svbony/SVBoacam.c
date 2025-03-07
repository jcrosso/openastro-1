/*****************************************************************************
 *
 * SVBoacam.c -- main entrypoint for SVBony Cameras
 *
 * Copyright 2020,2021
 *   James Fidell (james@openastroproject.org)
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

#include <SVBCameraSDK.h>

#include <openastro/camera.h>
#include <openastro/util.h>

#include "unimplemented.h"
#include "oacamprivate.h"
#include "SVBoacam.h"
#include "SVBprivate.h"


/**
 * Cycle through the cameras reported by the SVBony library
 */

int
oaSVBGetCameras ( CAMERA_LIST* deviceList, unsigned long featureFlags,
		int flags )
{
  unsigned int		numFound = 0, i;
  int			ret;
  const char*		currName;
  oaCameraDevice*	dev;
  DEVICE_INFO*		_private;
  SVB_CAMERA_INFO	camInfo;
  

	if (( ret = _svbInitLibraryFunctionPointers()) != OA_ERR_NONE ) {
		return ret;
	}

  if (( numFound = p_SVBGetNumOfConnectedCameras()) < 1 ) {
    return 0;
  }
  
  for ( i = 0; i < numFound; i++ ) {
    p_SVBGetCameraInfo ( &camInfo, i );
    currName = camInfo.FriendlyName;

    if (!( dev = malloc ( sizeof ( oaCameraDevice )))) {
      return -OA_ERR_MEM_ALLOC;
    }
    if (!( _private = malloc ( sizeof ( DEVICE_INFO )))) {
      ( void ) free (( void* ) dev );
      return -OA_ERR_MEM_ALLOC;
    }
		oaLogDebug ( OA_LOG_CAMERA, "%s: allocated @ %p for camera device",
				__func__, dev );
    _oaInitCameraDeviceFunctionPointers ( dev );
    dev->interface = OA_CAM_IF_SVB;
    if ( numFound == 1 ) {
      ( void ) strncpy ( dev->deviceName, currName, OA_MAX_NAME_LEN );
    } else {
      snprintf ( dev->deviceName, OA_MAX_NAME_LEN, "%s #%d", currName, i+1 );
    }
    _private->devIndex = i;
    dev->_private = _private;
    dev->initCamera = oaSVBInitCamera;
    if (( ret = _oaCheckCameraArraySize ( deviceList )) < 0 ) {
      ( void ) free (( void* ) dev );
      ( void ) free (( void* ) _private );
      return ret;
    }
    deviceList->cameraList[ deviceList->numCameras++ ] = dev;
  }

  return numFound;
}
