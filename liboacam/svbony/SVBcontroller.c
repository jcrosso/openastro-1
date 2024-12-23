/*****************************************************************************
 *
 * SVBcontroller.c -- Main camera controller thread
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

#include <pthread.h>

#include <openastro/camera.h>
#include <sys/time.h>
#include <SVBCameraSDK.h>

#include "oacamprivate.h"
#include "unimplemented.h"
#include "SVBoacam.h"
#include "SVBstate.h"
#include "SVBprivate.h"


static int	_processSetControl ( oaCamera*, OA_COMMAND* );
static int	_processGetControl ( oaCamera*, OA_COMMAND* );
static int	_processSetResolution ( SVB_STATE*, OA_COMMAND* );
static int	_processStreamingStart ( SVB_STATE*, OA_COMMAND* );
static int	_processStreamingStop ( SVB_STATE*, OA_COMMAND* );
//static int	_processExposureStart ( SVB_STATE*, OA_COMMAND* );
//static int	_processAbortExposure ( SVB_STATE* );
//static void	_timerCallback ( void* );
static void	_doFrameReconfiguration ( SVB_STATE* );


void*
oacamSVBcontroller ( void* param )
{
  oaCamera*		camera = param;
  SVB_STATE*		cameraInfo = camera->_private;
  OA_COMMAND*		command;
  int			exitThread = 0;
  int			resultCode, nextBuffer, buffersFree, frameWait;
  int			imageBufferLength, haveFrame;
//int			maxWaitTime;
  int			streaming = 0;

  do {
    pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
    exitThread = cameraInfo->stopControllerThread;
    pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
    if ( exitThread ) {
      break;
    } else {
      pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
      // stop us busy-waiting
      streaming = ( cameraInfo->runMode == CAM_RUN_MODE_STREAMING ) ? 1 : 0;
      if ( !streaming && oaDLListIsEmpty ( cameraInfo->commandQueue )) {
        pthread_cond_wait ( &cameraInfo->commandQueued,
            &cameraInfo->commandQueueMutex );
      }
      pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
    }

    do {
      command = oaDLListRemoveFromHead ( cameraInfo->commandQueue );
      if ( command ) {
        switch ( command->commandType ) {
          case OA_CMD_CONTROL_SET:
            resultCode = _processSetControl ( camera, command );
            break;
          case OA_CMD_CONTROL_GET:
            resultCode = _processGetControl ( camera, command );
            break;
          case OA_CMD_RESOLUTION_SET:
            resultCode = _processSetResolution ( cameraInfo, command );
            break;
          case OA_CMD_START_STREAMING:
            resultCode = _processStreamingStart ( cameraInfo, command );
            break;
          case OA_CMD_STOP_STREAMING:
            resultCode = _processStreamingStop ( cameraInfo, command );
            break;
/*
          case OA_CMD_START_EXPOSURE:
            resultCode = _processExposureStart ( cameraInfo, command );
            break;
          case OA_CMD_ABORT_EXPOSURE:
            resultCode = _processAbortExposure ( cameraInfo );
            break;
*/
          default:
            resultCode = -OA_ERR_INVALID_CONTROL;
            break;
        }
        if ( command->callback ) {
					oaLogWarning ( OA_LOG_CAMERA, "%s: command has callback", __func__ );
        } else {
          pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
          command->completed = 1;
          command->resultCode = resultCode;
          pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
          pthread_cond_broadcast ( &cameraInfo->commandComplete );
        }
      }
    } while ( command );

    if ( streaming ) {

      pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
      imageBufferLength = cameraInfo->imageBufferLength;
      frameWait = cameraInfo->currentAbsoluteExposure;
      pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );

      // convert frameWait from microseconds to milliseconds
      // if it is more than 100ms then set it to 100ms and that
      // is the longest we will wait before seeing the thread
      // killed

      frameWait /= 1000;
//    maxWaitTime = frameWait * 2;
      if ( frameWait > 100 ) {
        frameWait = 100;
      }

      pthread_mutex_lock ( &cameraInfo->callbackQueueMutex );
      buffersFree = cameraInfo->buffersFree;
      pthread_mutex_unlock ( &cameraInfo->callbackQueueMutex );

      if ( buffersFree ) {
        nextBuffer = cameraInfo->nextBuffer;
        haveFrame = 0;
//      do {
          if ( !p_SVBGetVideoData ( cameraInfo->cameraId,
              cameraInfo->buffers[ nextBuffer ].start, imageBufferLength,
              frameWait )) {
            haveFrame = 1;
          }
//        maxWaitTime -= frameWait;

          pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
          exitThread = cameraInfo->stopControllerThread;
          pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );

          if ( !exitThread && haveFrame ) {
            cameraInfo->frameCallbacks[ nextBuffer ].callbackType =
                OA_CALLBACK_NEW_FRAME;
            cameraInfo->frameCallbacks[ nextBuffer ].callback =
                cameraInfo->streamingCallback.callback;
            cameraInfo->frameCallbacks[ nextBuffer ].callbackArg =
                cameraInfo->streamingCallback.callbackArg;
            cameraInfo->frameCallbacks[ nextBuffer ].buffer =
                cameraInfo->buffers[ nextBuffer ].start;
            cameraInfo->frameCallbacks[ nextBuffer ].bufferLen =
                imageBufferLength;
            oaDLListAddToTail ( cameraInfo->callbackQueue,
                &cameraInfo->frameCallbacks[ nextBuffer ]);
            pthread_mutex_lock ( &cameraInfo->callbackQueueMutex );
            cameraInfo->buffersFree--;
            cameraInfo->nextBuffer = ( nextBuffer + 1 ) %
                cameraInfo->configuredBuffers;
            pthread_mutex_unlock ( &cameraInfo->callbackQueueMutex );
            pthread_cond_broadcast ( &cameraInfo->callbackQueued );
          }
//      } while ( !exitThread && !haveFrame && maxWaitTime > 0 );
      }
    }
  } while ( !exitThread );

  return 0;
}


static int
_processSetControl ( oaCamera* camera, OA_COMMAND* command )
{
  SVB_STATE*		cameraInfo = camera->_private;
  oaControlValue	*val = command->commandData;

  switch ( command->controlId ) {

#if 0
    case OA_CAM_CTRL_BRIGHTNESS:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_BRIGHTNESS, val->int32,
          cameraInfo->autoBrightness );
      cameraInfo->currentBrightness = val->int32;
      break;
#endif

    case OA_CAM_CTRL_BLUE_BALANCE:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_WB_B, val->int32,
          cameraInfo->autoBlueBalance );
      cameraInfo->currentBlueBalance = val->int32;
      break;

    case OA_CAM_CTRL_RED_BALANCE:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_WB_R, val->int32,
          cameraInfo->autoRedBalance );
      cameraInfo->currentRedBalance = val->int32;
      break;

    case OA_CAM_CTRL_GAMMA:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_GAMMA, val->int32,
          cameraInfo->autoGamma );
      cameraInfo->currentGamma = val->int32;
      break;

    case OA_CAM_CTRL_CONTRAST:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_CONTRAST, val->int32,
          cameraInfo->autoContrast );
      cameraInfo->currentContrast = val->int32;
      break;

    case OA_CAM_CTRL_SATURATION:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_SATURATION, val->int32,
          cameraInfo->autoSaturation );
      cameraInfo->currentSaturation = val->int32;
      break;

    case OA_CAM_CTRL_BLACKLEVEL:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_BLACK_LEVEL, val->int32,
          cameraInfo->autoBlackLevel );
      cameraInfo->currentBlackLevel = val->int32;
      break;

    case OA_CAM_CTRL_SHARPNESS:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_SHARPNESS, val->int32,
          cameraInfo->autoSharpness );
      cameraInfo->currentSharpness = val->int32;
      break;

          case OA_CAM_CTRL_GAIN:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_GAIN, val->int32,
          cameraInfo->autoGain );
      cameraInfo->currentGain = val->int32;
      break;

    case OA_CAM_CTRL_EXPOSURE_ABSOLUTE:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_EXPOSURE, val->int32,
          cameraInfo->autoExposure );
      pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
      cameraInfo->currentAbsoluteExposure = val->int32;
      pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
      break;

#if 0
    case OA_CAM_CTRL_USBTRAFFIC:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_BANDWIDTHOVERLOAD,
          val->int32, cameraInfo->autoUSBTraffic );
      cameraInfo->currentUSBTraffic = val->int32;
      break;
#endif

#if 0
    case OA_CAM_CTRL_OVERCLOCK:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_OVERCLOCK, val->int32,
          cameraInfo->autoOverclock );
      cameraInfo->currentOverclock = val->int32;
      break;
#endif

#if 0
    case OA_CAM_CTRL_HIGHSPEED:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_HIGH_SPEED_MODE,
          val->boolean, cameraInfo->autoHighSpeed );
      cameraInfo->currentHighSpeed = val->boolean;
      break;
#endif

    case OA_CAM_CTRL_BINNING:
      cameraInfo->binMode = val->discrete;
/*
      if ( camera->colour ) {
        if ( val->discrete < OA_BIN_MODE_2x2 &&
            8 == cameraInfo->currentBitDepth ) {
          if ( 3 == cameraInfo->FSMState ) {
            cameraInfo->currentMode = SVB_IMG_RAW8;
          } else {
            cameraInfo->currentMode = SVB_IMG_RGB24;
          }
        }
      }
*/
      _doFrameReconfiguration ( cameraInfo );
      break;

    case OA_CAM_CTRL_HFLIP:
    case OA_CAM_CTRL_VFLIP:
    {
      int flip = SVB_FLIP_NONE;
      if ( command->controlId == OA_CAM_CTRL_HFLIP ) {
        cameraInfo->currentHFlip = val->boolean;
      } else {
        cameraInfo->currentVFlip = val->boolean;
      }
      if ( cameraInfo->currentHFlip ) {
        flip = SVB_FLIP_HORIZ;
        if ( cameraInfo->currentVFlip ) {
          flip = SVB_FLIP_BOTH;
        }
      } else {
        if ( cameraInfo->currentVFlip ) {
          flip = SVB_FLIP_VERT;
        }
      }
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_FLIP, flip, 0 );
      break;
    }

    case OA_CAM_CTRL_FRAME_FORMAT:
    {
      int format = val->discrete;

      switch ( format ) {
        case OA_PIX_FMT_BGR24:
          cameraInfo->currentMode = SVB_IMG_RGB24;
          break;
        case OA_PIX_FMT_RGGB8:
        case OA_PIX_FMT_BGGR8:
        case OA_PIX_FMT_GRBG8:
        case OA_PIX_FMT_GBRG8:
          cameraInfo->currentMode = SVB_IMG_Y8;
          break;
        case OA_PIX_FMT_GREY8:
          cameraInfo->currentMode = cameraInfo->greyscaleMode;
          cameraInfo->currentMode = SVB_IMG_Y8;
          break;
        case OA_PIX_FMT_RGGB16LE:
        case OA_PIX_FMT_BGGR16LE:
        case OA_PIX_FMT_GRBG16LE:
        case OA_PIX_FMT_GBRG16LE:
          cameraInfo->currentMode = SVB_IMG_Y16;
          break;
        case OA_PIX_FMT_GREY16LE:
          cameraInfo->currentMode = cameraInfo->greyscaleMode;
          cameraInfo->currentMode = SVB_IMG_Y16;
          break;
      }
      cameraInfo->currentFormat = format;
      cameraInfo->currentBitDepth = oaFrameFormats[ format ].bitsPerPixel;
      _doFrameReconfiguration ( cameraInfo );
      break;
    }

#if 0
    case OA_CAM_CTRL_MONO_BIN_COLOUR:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_MONO_BIN,
          val->boolean, 0 );
      cameraInfo->monoBinning = val->boolean;
      break;
#endif

#if 0
    case OA_CAM_CTRL_FAN:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_FAN_ON,
          val->boolean, 0 );
      cameraInfo->fanEnabled = val->boolean;
      break;
#endif

#if 0
    case OA_CAM_CTRL_PATTERN_ADJUST:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_PATTERN_ADJUST,
          val->boolean, 0 );
      cameraInfo->patternAdjust = val->boolean;
      break;
#endif

#if 0
    case OA_CAM_CTRL_DEW_HEATER:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_ANTI_DEW_HEATER,
          val->boolean, 0 );
      cameraInfo->dewHeater = val->boolean;
      break;
#endif

    case OA_CAM_CTRL_COOLER:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_COOLER_ENABLE,
          val->boolean, 0 );
      cameraInfo->currentCoolerEnabled = val->boolean;
      break;

    case OA_CAM_CTRL_TEMP_SETPOINT:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_TARGET_TEMPERATURE, val->int32,
          0 );
      cameraInfo->currentSetPoint = val->int32;
      break;

    case OA_CAM_CTRL_COOLER_POWER:
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_COOLER_POWER,
          val->int32, 0 );
      cameraInfo->currentCoolerPower = val->int32;
      break;

    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_GAIN ):
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_GAIN,
          cameraInfo->currentGain, val->boolean );
      cameraInfo->autoGain = val->boolean;
      break;

    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_GAMMA ):
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_GAMMA,
          cameraInfo->currentGamma, val->boolean );
      cameraInfo->autoGamma = val->boolean;
      break;

#if 0
    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_BRIGHTNESS ):
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_BRIGHTNESS,
          cameraInfo->currentBrightness, val->boolean );
      cameraInfo->autoBrightness = val->boolean;
      break;
#endif

    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_EXPOSURE_ABSOLUTE ):
    {
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_EXPOSURE,
          cameraInfo->currentAbsoluteExposure, val->boolean );
      cameraInfo->autoExposure = val->boolean;
      break;
    }

    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_RED_BALANCE ):
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_WB_R,
          cameraInfo->currentRedBalance, val->boolean );
      cameraInfo->autoRedBalance = val->boolean;
      break;

    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_BLUE_BALANCE ):
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_WB_B,
          cameraInfo->currentBlueBalance, val->boolean );
      cameraInfo->autoBlueBalance = val->boolean;
      break;

#if 0
    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_USBTRAFFIC ):
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_BANDWIDTHOVERLOAD,
          cameraInfo->currentUSBTraffic, val->boolean );
      cameraInfo->autoUSBTraffic = val->boolean;
      break;
#endif

#if 0
    case OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_OVERCLOCK ):
      p_SVBSetControlValue ( cameraInfo->cameraId, SVB_OVERCLOCK,
          cameraInfo->currentOverclock, val->boolean );
      cameraInfo->autoOverclock = val->boolean;
      break;
#endif

    default:
      return -OA_ERR_INVALID_CONTROL;
      break;
  }
  return OA_ERR_NONE;
}


static int
_processGetControl ( oaCamera* camera, OA_COMMAND* command )
{
  SVB_STATE*		cameraInfo = camera->_private;
  oaControlValue	*val = command->resultData;
	long						ctrlVal;
	bool            boolVal;

  switch ( command->controlId ) {

#if 0
    case OA_CAM_CTRL_BRIGHTNESS:
    case OA_CAM_CTRL_MODE_AUTO ( OA_CAM_CTRL_BRIGHTNESS ):
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_BRIGHTNESS, &ctrlVal,
          &cameraInfo->autoBrightness );
			if ( OA_CAM_CTRL_BRIGHTNESS == command->controlId ) {
				val->valueType = OA_CTRL_TYPE_INT32;
				val->int32 = ctrlVal;
			} else {
				val->valueType = OA_CTRL_TYPE_BOOLEAN;
				val->boolean = cameraInfo->autoBrightness;
			}
      cameraInfo->currentBrightness = ctrlVal;
      break;
#endif

    case OA_CAM_CTRL_BLUE_BALANCE:
    case OA_CAM_CTRL_MODE_AUTO ( OA_CAM_CTRL_BLUE_BALANCE ):
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_WB_B, &ctrlVal,
          &cameraInfo->autoBlueBalance );
			if ( OA_CAM_CTRL_BLUE_BALANCE == command->controlId ) {
				val->valueType = OA_CTRL_TYPE_INT32;
				val->int32 = ctrlVal;
			} else {
				val->valueType = OA_CTRL_TYPE_BOOLEAN;
				val->boolean = cameraInfo->autoBlueBalance;
			}
      cameraInfo->currentBlueBalance = ctrlVal;
      break;

    case OA_CAM_CTRL_RED_BALANCE:
    case OA_CAM_CTRL_MODE_AUTO ( OA_CAM_CTRL_RED_BALANCE ):
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_WB_R, &ctrlVal,
          &cameraInfo->autoRedBalance );
			if ( OA_CAM_CTRL_RED_BALANCE == command->controlId ) {
				val->valueType = OA_CTRL_TYPE_INT32;
				val->int32 = ctrlVal;
			} else {
				val->valueType = OA_CTRL_TYPE_BOOLEAN;
				val->boolean = cameraInfo->autoRedBalance;
			}
      cameraInfo->currentRedBalance = ctrlVal;
      break;

    case OA_CAM_CTRL_GAMMA:
    case OA_CAM_CTRL_MODE_AUTO ( OA_CAM_CTRL_GAMMA ):
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_GAMMA, &ctrlVal,
          &cameraInfo->autoGamma );
			if ( OA_CAM_CTRL_GAMMA == command->controlId ) {
				val->valueType = OA_CTRL_TYPE_INT32;
				val->int32 = ctrlVal;
			} else {
				val->valueType = OA_CTRL_TYPE_BOOLEAN;
				val->boolean = cameraInfo->autoGamma;
			}
      cameraInfo->currentGamma = ctrlVal;
      break;

    case OA_CAM_CTRL_GAIN:
    case OA_CAM_CTRL_MODE_AUTO ( OA_CAM_CTRL_GAIN ):
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_GAIN, &ctrlVal,
          &cameraInfo->autoGain );
			if ( OA_CAM_CTRL_GAIN == command->controlId ) {
				val->valueType = OA_CTRL_TYPE_INT32;
				val->int32 = ctrlVal;
			} else {
				val->valueType = OA_CTRL_TYPE_BOOLEAN;
				val->boolean = cameraInfo->autoGain;
			}
      cameraInfo->currentGain = ctrlVal;
      break;

    case OA_CAM_CTRL_EXPOSURE_ABSOLUTE:
    case OA_CAM_CTRL_MODE_AUTO ( OA_CAM_CTRL_EXPOSURE_ABSOLUTE ):
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_EXPOSURE, &ctrlVal,
          &cameraInfo->autoExposure );
			if ( OA_CAM_CTRL_EXPOSURE_ABSOLUTE == command->controlId ) {
				val->valueType = OA_CTRL_TYPE_INT32;
				val->int32 = ctrlVal;
			} else {
				val->valueType = OA_CTRL_TYPE_BOOLEAN;
				val->boolean = cameraInfo->autoExposure;
			}
      cameraInfo->currentAbsoluteExposure = ctrlVal;
      break;

#if 0
    case OA_CAM_CTRL_USBTRAFFIC:
    case OA_CAM_CTRL_MODE_AUTO ( OA_CAM_CTRL_USBTRAFFIC ):
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_BANDWIDTHOVERLOAD,
					&ctrlVal, &cameraInfo->autoUSBTraffic );
			if ( OA_CAM_CTRL_USBTRAFFIC == command->controlId ) {
				val->valueType = OA_CTRL_TYPE_INT32;
				val->int32 = ctrlVal;
			} else {
				val->valueType = OA_CTRL_TYPE_BOOLEAN;
				val->boolean = cameraInfo->autoUSBTraffic;
			}
      cameraInfo->currentUSBTraffic = ctrlVal;
      break;
#endif

#if 0
    case OA_CAM_CTRL_OVERCLOCK:
    case OA_CAM_CTRL_MODE_AUTO ( OA_CAM_CTRL_OVERCLOCK ):
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_OVERCLOCK, &ctrlVal,
					&cameraInfo->autoOverclock );
			if ( OA_CAM_CTRL_OVERCLOCK == command->controlId ) {
				val->valueType = OA_CTRL_TYPE_INT32;
				val->int32 = ctrlVal;
			} else {
				val->valueType = OA_CTRL_TYPE_BOOLEAN;
				val->boolean = cameraInfo->autoOverclock;
			}
      cameraInfo->currentOverclock = ctrlVal;
      break;
#endif

#if 0
    case OA_CAM_CTRL_HIGHSPEED:
    case OA_CAM_CTRL_MODE_AUTO ( OA_CAM_CTRL_HIGHSPEED ):
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_HIGH_SPEED_MODE,
					&ctrlVal, &cameraInfo->autoHighSpeed );
			if ( OA_CAM_CTRL_HIGHSPEED == command->controlId ) {
				val->valueType = OA_CTRL_TYPE_INT32;
				val->int32 = ctrlVal;
			} else {
				val->valueType = OA_CTRL_TYPE_BOOLEAN;
				val->boolean = cameraInfo->autoHighSpeed;
			}
      cameraInfo->currentHighSpeed = ctrlVal;
      break;
#endif

    case OA_CAM_CTRL_BINNING:
			// Not ideal, but we can't read this one from the camera
			val->valueType = OA_CTRL_TYPE_INT32;
			val->int32 = cameraInfo->binMode;
      break;

    case OA_CAM_CTRL_HFLIP:
    case OA_CAM_CTRL_VFLIP:
    {
      long flip;
			SVB_BOOL autoflip;

      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_FLIP, &flip,
					&autoflip );
			val->valueType = OA_CTRL_TYPE_BOOLEAN;
      if ( command->controlId == OA_CAM_CTRL_HFLIP ) {
        val->boolean = ( flip == SVB_FLIP_HORIZ || flip == SVB_FLIP_BOTH ) ?
						1 : 0;
			} else {
        val->boolean = ( flip == SVB_FLIP_VERT || flip == SVB_FLIP_BOTH ) ?
						1 : 0;
			}
      break;
    }

#if 0
    case OA_CAM_CTRL_MONO_BIN_COLOUR:
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_MONO_BIN, &ctrlVal,
					&boolVal );
			val->valueType = OA_CTRL_TYPE_BOOLEAN;
			val->boolean = ctrlVal;
      break;
#endif

#if 0
    case OA_CAM_CTRL_FAN:
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_FAN_ON, &ctrlVal,
					&boolVal );
			val->valueType = OA_CTRL_TYPE_BOOLEAN;
			val->boolean = ctrlVal;
      break;
#endif

#if 0
    case OA_CAM_CTRL_PATTERN_ADJUST:
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_PATTERN_ADJUST, &ctrlVal,
					&boolVal );
			val->valueType = OA_CTRL_TYPE_BOOLEAN;
			val->boolean = ctrlVal;
      break;
#endif

#if 0
    case OA_CAM_CTRL_DEW_HEATER:
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_ANTI_DEW_HEATER,
					&ctrlVal, &boolVal );
			val->valueType = OA_CTRL_TYPE_BOOLEAN;
			val->boolean = ctrlVal;
      break;
#endif

    case OA_CAM_CTRL_COOLER:
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_COOLER_ENABLE, &ctrlVal,	&boolVal );
			val->valueType = OA_CTRL_TYPE_BOOLEAN;
			val->boolean = ctrlVal;
      break;

    case OA_CAM_CTRL_TEMP_SETPOINT:
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_TARGET_TEMPERATURE,
					&ctrlVal, &boolVal );
			val->valueType = OA_CTRL_TYPE_INT32;
			val->int32 = ctrlVal;
      break;

    case OA_CAM_CTRL_TEMPERATURE:
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_CURRENT_TEMPERATURE, &ctrlVal,
					&boolVal );
			val->valueType = OA_CTRL_TYPE_INT32;
			val->int32 = ctrlVal;
      break;

     case OA_CAM_CTRL_COOLER_POWER:
      p_SVBGetControlValue ( cameraInfo->cameraId, SVB_COOLER_POWER,
					&ctrlVal, &boolVal );
			val->valueType = OA_CTRL_TYPE_INT32;
			val->int32 = ctrlVal;
      break;

	 case OA_CAM_CTRL_DROPPED:
		{
			int	dropped;

      p_SVBGetDroppedFrames ( cameraInfo->cameraId, &dropped );
			val->valueType = OA_CTRL_TYPE_INT32;
			val->int32 = dropped;
      break;
		}
    default:
      return -OA_ERR_INVALID_CONTROL;
      break;
  }
  return OA_ERR_NONE;
}


static int
_processSetResolution ( SVB_STATE* cameraInfo, OA_COMMAND* command )
{
  FRAMESIZE*	size = command->commandData;

  cameraInfo->xSize = size->x;
  cameraInfo->ySize = size->y;
  _doFrameReconfiguration ( cameraInfo );
  return OA_ERR_NONE;
}


static void
_doFrameReconfiguration ( SVB_STATE* cameraInfo )
{
  int		multiplier;
  int		restartStreaming = 0;
  unsigned int	actualX, actualY;

  pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
  if ( cameraInfo->runMode == CAM_RUN_MODE_STREAMING ) {
    cameraInfo->runMode = CAM_RUN_MODE_STOPPED;
    restartStreaming = 1;
  }
  pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );

  if ( restartStreaming ) {
    p_SVBStopVideoCapture ( cameraInfo->cameraId );
  }
  actualX = cameraInfo->xSize;
  actualY = cameraInfo->ySize;
  if (( actualX * cameraInfo->binMode ) > cameraInfo->maxResolutionX ) {
    actualX = cameraInfo->maxResolutionX / cameraInfo->binMode;
  }
  if (( actualY * cameraInfo->binMode ) > cameraInfo->maxResolutionY ) {
    actualY = cameraInfo->maxResolutionY / cameraInfo->binMode;
  }
  p_SVBSetROIFormat ( cameraInfo->cameraId,
			( cameraInfo->maxResolutionX - cameraInfo->xSize ) / 2, cameraInfo->xSize,
			( cameraInfo->maxResolutionY - cameraInfo->ySize ) / 2, cameraInfo->ySize,
			cameraInfo->binMode );
  p_SVBSetOutputImageType ( cameraInfo->cameraId, cameraInfo->currentMode );

  if ( OA_BIN_MODE_NONE == cameraInfo->binMode &&
      ( cameraInfo->xSize != cameraInfo->maxResolutionX ||
      cameraInfo->ySize != cameraInfo->maxResolutionY )) {
/*
    p_SVBSetStartPos ( cameraInfo->cameraId,
        ( cameraInfo->maxResolutionX - cameraInfo->xSize ) / 2,
        ( cameraInfo->maxResolutionY - cameraInfo->ySize ) / 2 );
*/
  }

  // RGB colour is 3 bytes per pixel, mono one for 8-bit, two for 16-bit,
  // RAW is one for 8-bit, 2 for 16-bit
  multiplier = ( SVB_IMG_RGB24 == cameraInfo->currentMode ) ? 3 :
      ( SVB_IMG_Y16 == cameraInfo->currentMode ) ? 2 : 1;
  pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
  cameraInfo->imageBufferLength = actualX * actualY * multiplier;
  if ( restartStreaming ) {
    usleep ( 300000 );
    p_SVBStartVideoCapture ( cameraInfo->cameraId );
    cameraInfo->runMode = CAM_RUN_MODE_STREAMING;
  }
  pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
}


static int
_processStreamingStart ( SVB_STATE* cameraInfo, OA_COMMAND* command )
{
  CALLBACK*	cb = command->commandData;

  if ( cameraInfo->runMode != CAM_RUN_MODE_STOPPED ) {
    return -OA_ERR_INVALID_COMMAND;
  }

  /*
   * This is now done by assigning the largest possible buffers when
   * the camera is initialised, but could be changed back to here to
   * use meory more efficiently.
   *
  cameraInfo->configuredBuffers = 0;
  cameraInfo->buffers = calloc ( OA_CAM_BUFFERS, sizeof ( struct SVBbuffer ));
  for ( i = 0; i < OA_CAM_BUFFERS; i++ ) {
    void* m = malloc ( cameraInfo->imageBufferLength );
    if ( m ) {
      cameraInfo->buffers[i].start = m;
      cameraInfo->configuredBuffers++;
    } else {
      oaLogError ( OA_LOG_CAMERA, "%s: malloc of buffers failed", __func__ );
      if ( i ) {
        for ( j = 0; i < i; j++ ) {
          free (( void* ) cameraInfo->buffers[j].start );
        }
      }
      return -OA_ERR_MEM_ALLOC;
    }
  }
   */

  cameraInfo->streamingCallback.callback = cb->callback;
  cameraInfo->streamingCallback.callbackArg = cb->callbackArg;
  p_SVBStartVideoCapture ( cameraInfo->cameraId );
  pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
  cameraInfo->runMode = CAM_RUN_MODE_STREAMING;
  pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
  return OA_ERR_NONE;
}


static int
_processStreamingStop ( SVB_STATE* cameraInfo, OA_COMMAND* command )
{
  if ( cameraInfo->runMode != CAM_RUN_MODE_STREAMING ) {
    return -OA_ERR_INVALID_COMMAND;
  }

  /*
   * This will be needed if the buffer assignment is restored in
   * _processStreamingStart()
   *
  for ( i = 0; i < cameraInfo->configuredBuffers; i++ ) {
    free (( void* ) cameraInfo->buffers[i].start );
  }
  cameraInfo->configuredBuffers = 0;
  cameraInfo->lastUsedBuffer = -1;
  free (( void* ) cameraInfo->buffers );
  cameraInfo->buffers = 0;
   */

  p_SVBStopVideoCapture ( cameraInfo->cameraId );
  pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
  cameraInfo->runMode = CAM_RUN_MODE_STOPPED;
  pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
  return OA_ERR_NONE;
}


#if 0
static int
_processExposureStart ( SVB_STATE* cameraInfo, OA_COMMAND* command )
{
  CALLBACK*		cb = command->commandData;
	int					ret;

  if ( cameraInfo->runMode == CAM_RUN_MODE_STREAMING ) {
    return -OA_ERR_INVALID_COMMAND;
  }

	if ( cameraInfo->exposureInProgress ) {
		return OA_ERR_NONE;
	}

	if (( ret = p_SVBStartExposure ( cameraInfo->cameraId, 0 )) < 0 ) {
		oaLogError ( OA_LOG_CAMERA, "%s: SVBStartExposure failed, error %d",
				__func__, ret );
    return -OA_ERR_CAMERA_IO;
	}
	cameraInfo->streamingCallback.callback = cb->callback;
	cameraInfo->streamingCallback.callbackArg = cb->callbackArg;
	cameraInfo->runMode = CAM_RUN_MODE_SINGLE_SHOT;
	cameraInfo->timerCallback = _timerCallback;
	oacamStartTimer ( cameraInfo->currentAbsoluteExposure + 500000, cameraInfo );

  pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
  cameraInfo->exposureInProgress = 1;
  cameraInfo->abortExposure = 0;
  pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );

  return OA_ERR_NONE;
}


static int
_processAbortExposure ( SVB_STATE* cameraInfo )
{
	int			ret;

  pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
  cameraInfo->abortExposure = 1;
  cameraInfo->exposureInProgress = 0;
  pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );

	oacamAbortTimer ( cameraInfo );

  if (( ret = p_SVBStopExposure ( cameraInfo->cameraId )) < 0 ) {
    oaLogError ( OA_LOG_CAMERA, "%s: SVBStopExposure failed: %d", __func__,
				ret );
    return -OA_ERR_CAMERA_IO;
  }

	cameraInfo->runMode = CAM_RUN_MODE_STOPPED;
  return OA_ERR_NONE;
}


static void
_timerCallback ( void* param )
{
	SVB_STATE*				cameraInfo = param;
	int									ret, buffersFree, nextBuffer;
	SVB_EXPOSURE_STATUS	status;

retry:
	if (( ret = p_SVBGetExpStatus ( cameraInfo->cameraId, &status )) < 0 ) {
		oaLogError ( OA_LOG_CAMERA, "%s: SVBGetExpStatus failed, error %d",
				__func__, ret );
		pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
		cameraInfo->exposureInProgress = 0;
		pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
		return;
	}

	if ( status != SVB_EXP_SUCCESS ) {
		if ( status == SVB_EXP_WORKING ) {
			// FIX ME -- busy waiting here is pretty ugly :(
			usleep(100000);
			goto retry;
		}
		oaLogWarning ( OA_LOG_CAMERA, "%s: SVBGetExpStatus returned status %d",
				__func__, status );
		pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
		cameraInfo->exposureInProgress = 0;
		pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
		return;
	}

	pthread_mutex_lock ( &cameraInfo->callbackQueueMutex );
	buffersFree = cameraInfo->buffersFree;
	pthread_mutex_unlock ( &cameraInfo->callbackQueueMutex );

  if ( buffersFree ) {
    nextBuffer = cameraInfo->nextBuffer;
		if (( ret = p_SVBGetDataAfterExp ( cameraInfo->cameraId,
					cameraInfo->buffers[ nextBuffer ].start,
					cameraInfo->imageBufferLength )) < 0 ) {
			oaLogError ( OA_LOG_CAMERA, "%s: SVBGetDataAfterExp failed, error %d",
					__func__, ret );
			pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
			cameraInfo->exposureInProgress = 0;
			pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
			return;
		}

		cameraInfo->exposureInProgress = 0;
    cameraInfo->frameCallbacks[ nextBuffer ].callbackType =
        OA_CALLBACK_NEW_FRAME;
    cameraInfo->frameCallbacks[ nextBuffer ].callback =
        cameraInfo->streamingCallback.callback;
    cameraInfo->frameCallbacks[ nextBuffer ].callbackArg =
        cameraInfo->streamingCallback.callbackArg;
    cameraInfo->frameCallbacks[ nextBuffer ].buffer =
        cameraInfo->buffers[ nextBuffer ].start;
    cameraInfo->frameCallbacks[ nextBuffer ].bufferLen =
        cameraInfo->imageBufferLength;
    oaDLListAddToTail ( cameraInfo->callbackQueue,
        &cameraInfo->frameCallbacks[ nextBuffer ]);
    pthread_mutex_lock ( &cameraInfo->callbackQueueMutex );
    cameraInfo->buffersFree--;
    cameraInfo->nextBuffer = ( nextBuffer + 1 ) %
        cameraInfo->configuredBuffers;
    pthread_mutex_unlock ( &cameraInfo->callbackQueueMutex );
    pthread_cond_broadcast ( &cameraInfo->callbackQueued );
  } else {
		cameraInfo->exposureInProgress = 0;
	}
}
#endif
