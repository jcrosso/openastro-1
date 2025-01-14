/*****************************************************************************
 *
 * oacam.c -- main entrypoint for Touptek cameras
 *
 * Copyright 2019,2020,2021
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

#include <openastro/camera.h>
#include <openastro/util.h>

#include "touptek-conf.h"
#include "oacamprivate.h"
#include "unimplemented.h"
#include "touptekoacam.h"
#include "touptekprivate.h"


/**
 * Cycle through the list of cameras returned by the touptek library
 */

int
TT_FUNC( oa, GetCameras )( CAMERA_LIST* deviceList, unsigned long featureFlags,
		int flags )
{
  TT_VAR_TYPE( DeviceV2 )		devList[ TT_DEFINE( MAX )];
  unsigned int		numCameras;
  unsigned int		i;
  oaCameraDevice*       dev;
  DEVICE_INFO*		_private;
  int                   ret;

	oaLogInfo ( OA_LOG_CAMERA, "%s ( %p, %ld, %d ): entered", __func__,
			deviceList, featureFlags, flags );

	if (( ret = TT_FUNC( _, InitLibraryFunctionPointers )()) != OA_ERR_NONE ) {
		oaLogError ( OA_LOG_CAMERA,
				"%s: _...InitLibraryFunctionPointers() failed, exiting", __func__ );
		return ret;
	}

  numCameras = ( TT_LIB_PTR( EnumV2 ))( devList );
  if ( numCameras < 1 ) {
		oaLogInfo ( OA_LOG_CAMERA, "%s: No cameras found", __func__ );
    return 0;
  }

  for ( i = 0; i < numCameras; i++ ) {

    if (!( dev = malloc ( sizeof ( oaCameraDevice )))) {
			oaLogError ( OA_LOG_CAMERA,
					"%s: Failed to allocate memory for oaCameraDevice", __func__ );
      return -OA_ERR_MEM_ALLOC;
    }

    if (!( _private = malloc ( sizeof ( DEVICE_INFO )))) {
			oaLogError ( OA_LOG_CAMERA,
					"%s: Failed to allocate memory for DEVICE_INFO", __func__ );
      ( void ) free (( void* ) dev );
      return -OA_ERR_MEM_ALLOC;
    }
		oaLogDebug ( OA_LOG_CAMERA, "%s: allocated @ %p for camera device",
				__func__, dev );

    _oaInitCameraDeviceFunctionPointers ( dev );
    dev->interface = TT_INTERFACE;
    ( void ) strncpy ( dev->deviceName, devList[i].displayname,
        OA_MAX_NAME_LEN+1 );
    _private->devIndex = i;
    ( void ) strncpy ( _private->deviceId, devList[i].id, OA_MAX_DEVICEID_LEN );
    dev->_private = _private;
    dev->initCamera = TT_FUNC( oa, InitCamera );
    if (( ret = _oaCheckCameraArraySize ( deviceList )) < 0 ) {
			oaLogError ( OA_LOG_CAMERA, "%s: _oaCheckCameraArraySize() failed",
					__func__ );
      ( void ) free (( void* ) dev );
      ( void ) free (( void* ) _private );
      return ret;
    }
    deviceList->cameraList[ deviceList->numCameras++ ] = dev;
  }

	oaLogInfo ( OA_LOG_CAMERA, "%s: exiting.  Found %d cameras", __func__,
			numCameras );

  return numCameras;
}
