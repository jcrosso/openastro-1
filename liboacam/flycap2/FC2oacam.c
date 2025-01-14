/*****************************************************************************
 *
 * FC2oacam.c -- main entrypoint for Point Grey Gig-E Cameras
 *
 * Copyright 2015,2016,2018,2019,2020,2021
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

#include <flycapture/C/FlyCapture2_C.h>

#include <openastro/camera.h>
#include <openastro/util.h>
#include <openastro/demosaic.h>

#include "oacamprivate.h"
#include "unimplemented.h"
#include "FC2oacam.h"
#include "FC2private.h"


/**
 * Cycle through the list of cameras returned by the Flycapture library
 */

int
oaFC2GetCameras ( CAMERA_LIST* deviceList, unsigned long featureFlags,
		int flags )
{
  fc2Context		pgeContext;
  fc2CameraInfo*	devList;
  fc2PGRGuid		guid;
  fc2InterfaceType	interfaceType;
  unsigned int		numCameras, slotsAvailable;
  unsigned int		i;
  oaCameraDevice*       dev;
  DEVICE_INFO*		_private;
  int                   numFound, ret;
	char						buffer[61];
	FILE*						fd;

	if (( ret = _fc2InitLibraryFunctionPointers()) != OA_ERR_NONE ) {
    return ret;
  }

  if (( *p_fc2CreateGigEContext )( &pgeContext ) != FC2_ERROR_OK ) {
    oaLogError ( OA_LOG_CAMERA, "%s: Can't get FC2 context", __func__ );
    return -OA_ERR_SYSTEM_ERROR;
  }

  slotsAvailable = 0;
  devList = 0;
  do {
    slotsAvailable += 4;
    if (!( devList = realloc ( devList, sizeof ( fc2CameraInfo ) *
        slotsAvailable ))) {
      return -OA_ERR_MEM_ALLOC;
    }
    numCameras = slotsAvailable;
    ret = ( *p_fc2DiscoverGigECameras )( pgeContext, devList, &numCameras );
    if ( ret != FC2_ERROR_OK && ret != FC2_ERROR_BUFFER_TOO_SMALL ) {
      ( *p_fc2DestroyContext )( pgeContext );
      oaLogError ( OA_LOG_CAMERA, "%s: Can't enumerate FC2 devices", __func__ );
      ( void ) free (( void* ) devList );
      return -OA_ERR_SYSTEM_ERROR;
    }
  } while ( ret != FC2_ERROR_OK );

  if ( !numCameras ) {
    ( *p_fc2DestroyContext )( pgeContext );
    ( void ) free (( void* ) devList );
    return 0;
  }

/*
  if (( * p_fc2GetNumOfCameras )( pgeContext, &numCameras ) != FC2_ERROR_OK ) {
    ( *p_fc2DestroyContext )( pgeContext );
    free (( void* ) devList );
    oaLogError ( OA_LOG_CAMERA, "%s: Error fetching number of cameras",
		__func__ );
    return -OA_ERR_SYSTEM_ERROR;
  }

  if ( !numCameras ) {
    ( *p_fc2DestroyContext )( pgeContext );
    free (( void* ) devList );
    return 0;
  }
*/

  numFound = 0;
  for ( i = 0; i < numCameras; i++ ) {
    // if (( *p_fc2GetCameraFromIndex )( pgeContext, i, &guid ) != FC2_ERROR_OK ) {
    if (( *p_fc2GetCameraFromIPAddress )( pgeContext, devList[i].ipAddress,
        &guid ) != FC2_ERROR_OK ) {
      ( *p_fc2DestroyContext )( pgeContext );
      ( void ) free (( void* ) devList );
      oaLogError ( OA_LOG_CAMERA, "%s: Error fetching details for camera %d",
					__func__, i );
      return -OA_ERR_SYSTEM_ERROR;
    }

    if (( *p_fc2GetInterfaceTypeFromGuid )( pgeContext, &guid,
        &interfaceType ) != FC2_ERROR_OK ) {
      ( *p_fc2DestroyContext )( pgeContext );
      oaLogError ( OA_LOG_CAMERA,
					"%s: Error getting interface type for camera %d", __func__, i );
      ( void ) free (( void* ) devList );
      return -OA_ERR_SYSTEM_ERROR;
    }

    if ( interfaceType != FC2_INTERFACE_GIGE ) {
      continue;
    }

    oaLogDebug ( OA_LOG_CAMERA, "%s: GigE major version - %u", __func__,
        devList[i].gigEMajorVersion );
    oaLogDebug ( OA_LOG_CAMERA, "%s: GigE minor version - %u", __func__,
        devList[i].gigEMinorVersion );
    oaLogDebug ( OA_LOG_CAMERA, "%s: User-defined name - %s", __func__,
        devList[i].userDefinedName );
    oaLogDebug ( OA_LOG_CAMERA, "%s: Model name - %s", __func__,
        devList[i].modelName );
    oaLogDebug ( OA_LOG_CAMERA, "%s: XML URL1 - %s", __func__,
        devList[i].xmlURL1 );
    oaLogDebug ( OA_LOG_CAMERA, "%s: XML URL2 - %s", __func__,
        devList[i].xmlURL2 );
    oaLogDebug ( OA_LOG_CAMERA, "%s: Firmware version - %s", __func__,
        devList[i].firmwareVersion );
    oaLogDebug ( OA_LOG_CAMERA, "%s: IIDC version - %1.2f", __func__,
        devList[i].iidcVer / 100.0f );
    oaLogDebug ( OA_LOG_CAMERA,
				"%s: MAC address - %02X:%02X:%02X:%02X:%02X:%02X", __func__,
        devList[i].macAddress.octets[0], devList[i].macAddress.octets[1],
        devList[i].macAddress.octets[2], devList[i].macAddress.octets[3],
        devList[i].macAddress.octets[4], devList[i].macAddress.octets[5] );
    oaLogDebug ( OA_LOG_CAMERA, "%s: IP address - %u.%u.%u.%u", __func__,
        devList[i].ipAddress.octets[0], devList[i].ipAddress.octets[1],
        devList[i].ipAddress.octets[2], devList[i].ipAddress.octets[3] );
    oaLogDebug ( OA_LOG_CAMERA, "%s: Subnet mask - %u.%u.%u.%u", __func__,
        devList[i].subnetMask.octets[0], devList[i].subnetMask.octets[1],
        devList[i].subnetMask.octets[2], devList[i].subnetMask.octets[3] );
    oaLogDebug ( OA_LOG_CAMERA, "%s: Default gateway - %u.%u.%u.%u", __func__,
        devList[i].defaultGateway.octets[0],
        devList[i].defaultGateway.octets[1],
        devList[i].defaultGateway.octets[2],
        devList[i].defaultGateway.octets[3]);

    if (!( dev = malloc ( sizeof ( oaCameraDevice )))) {
			// FIX ME -- does this need to happen?
      _oaFreeCameraDeviceList ( deviceList );
      ( *p_fc2DestroyContext )( pgeContext );
      ( void ) free (( void* ) devList );
      return -OA_ERR_MEM_ALLOC;
    }

    if (!( _private = malloc ( sizeof ( DEVICE_INFO )))) {
      ( void ) free (( void* ) dev );
      ( *p_fc2DestroyContext )( pgeContext );
      ( void ) free (( void* ) devList );
      return -OA_ERR_MEM_ALLOC;
    }
		oaLogDebug ( OA_LOG_CAMERA, "%s: allocated @ %p for camera device",
				__func__, dev );

    _oaInitCameraDeviceFunctionPointers ( dev );
    dev->interface = OA_CAM_IF_FC2;
		( void ) strncpy ( buffer, devList[i].modelName, 60 );
    ( void ) snprintf ( dev->deviceName, OA_MAX_NAME_LEN+1,
        "%s (%d.%d.%d.%d)", buffer,
        devList[i].ipAddress.octets[0], devList[i].ipAddress.octets[1],
        devList[i].ipAddress.octets[2], devList[i].ipAddress.octets[3] );
    memcpy (( void* ) &_private->pgeGuid, ( void* ) &guid, sizeof ( guid ));
    dev->_private = _private;
    dev->initCamera = oaFC2InitCamera;
    if ((( _private->colour = devList[i].isColorCamera ) ? 1 : 0 )) {
      switch ( devList[i].bayerTileFormat ) {
        case FC2_BT_RGGB:
          _private->cfaPattern = OA_DEMOSAIC_RGGB;
          break;
        case FC2_BT_GRBG:
          _private->cfaPattern = OA_DEMOSAIC_GRBG;
          break;
        case FC2_BT_GBRG:
          _private->cfaPattern = OA_DEMOSAIC_GBRG;
          break;
        case FC2_BT_BGGR:
          _private->cfaPattern = OA_DEMOSAIC_BGGR;
          break;
        default:
          break;
      }
    }
    if (( ret = _oaCheckCameraArraySize ( deviceList )) < 0 ) {
      ( void ) free (( void* ) dev );
      ( void ) free (( void* ) _private );
      ( *p_fc2DestroyContext )( pgeContext );
      ( void ) free (( void* ) devList );
      return ret;
    }
    deviceList->cameraList[ deviceList->numCameras++ ] = dev;
    numFound++;
  }

  ( *p_fc2DestroyContext )( pgeContext );
  ( void ) free (( void* ) devList );

	if (( fd = fopen ( "/proc/sys/net/core/rmem_default", "r" ))) {
		unsigned long	val;
		if ( fscanf ( fd, "%ld", &val ) == 1 ) {
			if ( val <= 10485760 ) {
				oaLogWarning ( OA_LOG_CAMERA,
						"%s: It may be necessary to raise rmem_default and rmem_max",
						__func__ );
				oaLogWarning ( OA_LOG_CAMERA,
						"%s: to a larger value (for example, 10000000) for", __func__ );
				oaLogWarning ( OA_LOG_CAMERA,
						"%s: best performance with GigE cameras.", __func__ );
			}
		}
		fclose ( fd );
	}

  return numFound;
}
