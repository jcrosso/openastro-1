/*****************************************************************************
 *
 * SXcontrol.c -- control functions for SX cameras
 *
 * Copyright 2014,2015,2017,2018,2019,2021
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

#include <pthread.h>
#include <openastro/camera.h>
#include <openastro/util.h>

#include "oacamprivate.h"
#include "SX.h"
#include "SXstate.h"
#include "SXoacam.h"


int
oaSXCameraTestControl ( oaCamera* camera, int control, oaControlValue* val )
{
  uint32_t	val_u32;
  int64_t	val_s64;
  COMMON_INFO*	commonInfo = camera->_common;

  if ( !camera->OA_CAM_CTRL_TYPE( control )) {
    return -OA_ERR_INVALID_CONTROL;
  }

  if ( camera->OA_CAM_CTRL_TYPE( control ) != val->valueType ) {
    return -OA_ERR_INVALID_CONTROL_TYPE;
  }

  switch ( control ) {

    case OA_CAM_CTRL_BRIGHTNESS:
    case OA_CAM_CTRL_CONTRAST:
    case OA_CAM_CTRL_SATURATION:
    case OA_CAM_CTRL_HUE:
    case OA_CAM_CTRL_SHARPNESS:
    case OA_CAM_CTRL_GAMMA:
    case OA_CAM_CTRL_WHITE_BALANCE_TEMP:
    case OA_CAM_CTRL_GAIN:
      // This should be unsigned 32-bit
      val_s64 = val->int64;
      if ( val_s64 < 0 ) {
        return -OA_ERR_OUT_OF_RANGE;
      }
      val_u32 = val_s64 & 0xffffffff;
      if ( val_u32 >= commonInfo->OA_CAM_CTRL_MIN( control ) &&
          val_u32 <= commonInfo->OA_CAM_CTRL_MAX( control ) &&
          ( 0 == ( val_u32 - commonInfo->OA_CAM_CTRL_MIN( control )) %
          commonInfo->OA_CAM_CTRL_STEP( control ))) {
        return OA_ERR_NONE;
      }
      break;

    case OA_CAM_CTRL_EXPOSURE_ABSOLUTE:
      val_s64 = val->int64;
      if ( val_s64 <= 0 ) {
        return -OA_ERR_OUT_OF_RANGE;
      }
      return OA_ERR_NONE;

    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_WHITE_BALANCE ):
    case OA_CAM_CTRL_AUTO_WHITE_BALANCE_TEMP:
    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_HUE ):
      // These just need to be boolean and we've already checked that
      return OA_ERR_NONE;
      break;

    case OA_CAM_CTRL_BINNING:
			if ( val->discrete == OA_BIN_MODE_NONE ||
					val->discrete == OA_BIN_MODE_2x2 ) {
				return OA_ERR_NONE;
			}
      return -OA_ERR_OUT_OF_RANGE;
      break;

    default:
      // If we reach here it's because we don't recognise the control
      oaLogError ( OA_LOG_CAMERA, "%s: Unrecognised control %d", __func__,
					control );
      return -OA_ERR_INVALID_CONTROL;
      break;
  }

  // And if we reach here it's because the value wasn't valid
  return -OA_ERR_OUT_OF_RANGE;
}


/*
int
oaSXCameraReset ( oaCamera* camera )
{
  unsigned char	buff[ SXUSB_REQUEST_BUFSIZE ];
  int		ret, transferred;

  OA_CLEAR( buff );
  buff[ SXUSB_REQ_CMD_TYPE ] = SXUSB_CMD_SEND;
  buff[ SXUSB_REQ_CMD ] = SXUSB_RESET;
  ret = libusb_bulk_transfer ( cameraInfo->usbHandle, SXUSB_BULK_ENDP_OUT,
      buff, SXUSB_REQUEST_BUFSIZE, &transferred, SXUSB_TIMEOUT );
  if ( ret || transferred != SXUSB_REQUEST_BUFSIZE ) {
    oaLogError ( OA_LOG_CAMERA, "%s: send RESET for SX failed: ret = %d, "
        "transferred = %d of %d", __func__, ret, transferred,
				SXUSB_REQUEST_BUFSIZE );
    return -OA_ERR_CAMERA_IO`;
  }

  return OA_ERR_NONE;
}

*/
