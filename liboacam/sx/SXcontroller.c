/*****************************************************************************
 *
 * SXcontroller.c -- Main camera controller thread
 *
 * Copyright 2015,2018,2019,2021
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
#include <sys/time.h>

#include "oacamprivate.h"
#include "unimplemented.h"
#include "SX.h"
#include "SXstate.h"
#include "SXoacam.h"


static int	_processSetControl ( SX_STATE*, OA_COMMAND* );
static int	_processGetControl ( SX_STATE*, OA_COMMAND* );
static int	_processSetResolution ( oaCamera*, OA_COMMAND* );
static int	_processStreamingStart ( oaCamera*, OA_COMMAND* );
static int	_processStreamingStop ( SX_STATE*, OA_COMMAND* );
static int	_doStartExposure ( SX_STATE* );
static int	_doReadExposure ( SX_STATE* );

static int	_clearFrame ( SX_STATE*, unsigned int );
static int	_latchFrame ( SX_STATE*, unsigned int, unsigned int,
			unsigned int, unsigned int, unsigned int );
static int	_readFrame ( SX_STATE*, unsigned char*, int );
static int	_readTemperature ( SX_STATE* );


void*
oacamSXcontroller ( void* param )
{
  oaCamera*		camera = param;
  SX_STATE*		cameraInfo = camera->_private;
  OA_COMMAND*		command;
  int			exitThread = 0;
  int			resultCode;
  int			streaming = 0;
  int			maxWaitTime, frameWait;
  int			nextBuffer, buffersFree;
  unsigned char*	evenFrame;
  unsigned char*	oddFrame;
  unsigned char*	tgt;
  unsigned int		halfFrameSize, rowLength, i, numRows;

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
            resultCode = _processSetControl ( cameraInfo, command );
            break;
          case OA_CMD_CONTROL_GET:
            resultCode = _processGetControl ( cameraInfo, command );
            break;
          case OA_CMD_RESOLUTION_SET:
            resultCode = _processSetResolution ( camera, command );
            break;
          case OA_CMD_START_STREAMING:
            resultCode = _processStreamingStart ( camera, command );
            break;
          case OA_CMD_STOP_STREAMING:
            resultCode = _processStreamingStop ( cameraInfo, command );
            break;
          default:
            oaLogError ( OA_LOG_CAMERA,
								"%s: Invalid command type %d in controller", __func__,
                command->commandType );
            resultCode = -OA_ERR_INVALID_CONTROL;
            break;
        }
        if ( command->callback ) {
					oaLogWarning ( OA_LOG_CAMERA, "%s: CONT: command has callback",
							__func__ );
        } else {
          pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
          command->completed = 1;
          command->resultCode = resultCode;
          pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
          pthread_cond_broadcast ( &cameraInfo->commandComplete );
        }
      }
    } while ( command );

    pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
    streaming = ( cameraInfo->runMode == CAM_RUN_MODE_STREAMING ) ? 1 : 0;
    pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );

    if ( streaming ) {
      pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
      maxWaitTime = frameWait = cameraInfo->currentExposure;
      pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );

      _doStartExposure ( cameraInfo );

      while ( !exitThread && maxWaitTime > 0 ) {
        usleep ( frameWait );
        maxWaitTime -= frameWait;
      }

      if ( !exitThread ) {
        if ( !_doReadExposure ( cameraInfo )) {
          pthread_mutex_lock ( &cameraInfo->callbackQueueMutex );
          buffersFree = cameraInfo->buffersFree;
          pthread_mutex_unlock ( &cameraInfo->callbackQueueMutex );
          pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
					streaming = ( cameraInfo->runMode == CAM_RUN_MODE_STREAMING ) ? 1 : 0;
          pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );
          if ( buffersFree && streaming ) {
            nextBuffer = cameraInfo->nextBuffer;
            if ( cameraInfo->isInterlaced ) {
							if ( OA_BIN_MODE_NONE == cameraInfo->binMode ) {
								rowLength = cameraInfo->xImageSize * cameraInfo->bytesPerPixel;
								numRows = cameraInfo->yImageSize / 2;
								halfFrameSize = rowLength * numRows;
								evenFrame = cameraInfo->xferBuffer;
								oddFrame = cameraInfo->xferBuffer + halfFrameSize;

								tgt = cameraInfo->buffers[ nextBuffer ].start;
								for ( i = 0; i < numRows; i++ ) {
									memcpy ( tgt, oddFrame, rowLength );
									tgt += rowLength;
									oddFrame += rowLength;
									memcpy ( tgt, evenFrame, rowLength );
									tgt += rowLength;
									evenFrame += rowLength;
								}
							} else {
								memcpy ( cameraInfo->buffers[ nextBuffer ].start,
										cameraInfo->xferBuffer, cameraInfo->actualImageLength );
							}
						}
            cameraInfo->frameCallbacks[ nextBuffer ].callbackType =
                OA_CALLBACK_NEW_FRAME;
            cameraInfo->frameCallbacks[ nextBuffer ].callback =
                cameraInfo->streamingCallback.callback;
            cameraInfo->frameCallbacks[ nextBuffer ].callbackArg =
                cameraInfo->streamingCallback.callbackArg;
            cameraInfo->frameCallbacks[ nextBuffer ].buffer =
                cameraInfo->buffers[ nextBuffer ].start;
            cameraInfo->frameCallbacks[ nextBuffer ].bufferLen =
								cameraInfo->actualImageLength;
            pthread_mutex_lock ( &cameraInfo->callbackQueueMutex );
            oaDLListAddToTail ( cameraInfo->callbackQueue,
                &cameraInfo->frameCallbacks[ nextBuffer ]);
            cameraInfo->buffersFree--;
            cameraInfo->nextBuffer = ( nextBuffer + 1 ) %
                cameraInfo->configuredBuffers;
            pthread_mutex_unlock ( &cameraInfo->callbackQueueMutex );
            pthread_cond_broadcast ( &cameraInfo->callbackQueued );
          }
        }
      }
    }
  } while ( !exitThread );

  return 0;
}


static int
_processSetControl ( SX_STATE* cameraInfo, OA_COMMAND* command )
{
  int			control = command->controlId;
  oaControlValue*	val = command->commandData;

	oaLogInfo ( OA_LOG_CAMERA, "%s ( %p, %p ): entered", __func__, cameraInfo,
			command );
	oaLogDebug ( OA_LOG_CAMERA, "%s: control = %d", __func__, control );

  switch ( control ) {

    case OA_CAM_CTRL_EXPOSURE_ABSOLUTE:
      if ( val->valueType != OA_CTRL_TYPE_INT64 ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where int64 expected", __func__,
            val->valueType );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      cameraInfo->currentExposure = val->int64;
      // Probably don't need to do anything else with this because we're
      // effectively free-running.  The camera waits for us to say that
      // the exposure is done.  But I'll set it just in case.
      _SXsetTimer ( cameraInfo, cameraInfo->currentExposure );
      break;

    case OA_CAM_CTRL_BINNING:
      if ( val->valueType != OA_CTRL_TYPE_DISCRETE ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: invalid control type %d where discrete expected", __func__,
						val->valueType );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
			if ( val->discrete == OA_BIN_MODE_NONE ||
					val->discrete == OA_BIN_MODE_2x2 ) {
				cameraInfo->binMode = val->discrete;
				switch ( val->discrete ) {
					case OA_BIN_MODE_NONE:
						cameraInfo->xImageSize = cameraInfo->xSubframeSize;
						cameraInfo->yImageSize = cameraInfo->ySubframeSize;
						break;
					case OA_BIN_MODE_2x2:
						cameraInfo->xImageSize = cameraInfo->xSubframeSize / 2;
						cameraInfo->yImageSize = cameraInfo->ySubframeSize / 2;
						break;
				}
				cameraInfo->actualImageLength = cameraInfo->xImageSize *
						cameraInfo->yImageSize * cameraInfo->bytesPerPixel;
			} else {
				return -OA_ERR_OUT_OF_RANGE;
			}
      break;

    default:
      oaLogError ( OA_LOG_CAMERA, "%s: Unrecognised control %d", __func__,
					control );
      return -OA_ERR_INVALID_CONTROL;
      break;
  }

	oaLogInfo ( OA_LOG_CAMERA, "%s: exiting", __func__ );

  return OA_ERR_NONE;
}


static int
_processGetControl ( SX_STATE* cameraInfo, OA_COMMAND* command )
{
  int			control = command->controlId;
  oaControlValue*	val = command->resultData;

	oaLogInfo ( OA_LOG_CAMERA, "%s ( %p, %p ): entered", __func__, cameraInfo,
			command );
	oaLogDebug ( OA_LOG_CAMERA, "%s: control = %d", __func__, control );

  switch ( control ) {

    case OA_CAM_CTRL_EXPOSURE_ABSOLUTE:
      val->valueType = OA_CTRL_TYPE_INT64;
      val->int64 = cameraInfo->currentExposure;
      break;

    case OA_CAM_CTRL_BINNING:
      val->valueType = OA_CTRL_TYPE_DISCRETE;
      val->discrete = cameraInfo->binMode;
      break;

    case OA_CAM_CTRL_TEMPERATURE:
    {
      val->valueType = OA_CTRL_TYPE_READONLY;
      val->readonly = _readTemperature ( cameraInfo );
      break;
    }
    case OA_CAM_CTRL_DROPPED:
      val->valueType = OA_CTRL_TYPE_READONLY;
      val->readonly = cameraInfo->droppedFrames;
      break;

    default:
      oaLogError ( OA_LOG_CAMERA, "%s: Unrecognised control %d in %s",
					__func__, control );
      return -OA_ERR_INVALID_CONTROL;
      break;
  }

	oaLogInfo ( OA_LOG_CAMERA, "%s: exiting", __func__ );

  return OA_ERR_NONE;
}


static int
_processSetResolution ( oaCamera* camera, OA_COMMAND* command )
{
  SX_STATE*	cameraInfo = camera->_private;
  FRAMESIZE*	size = command->commandData;

  cameraInfo->xImageSize = size->x;
  cameraInfo->yImageSize = size->y;
	cameraInfo->xSubframeSize = cameraInfo->xImageSize * cameraInfo->binMode;
	cameraInfo->ySubframeSize = cameraInfo->yImageSize * cameraInfo->binMode;
	cameraInfo->xSubframeOffset = ( cameraInfo->maxResolutionX -
			cameraInfo->xSubframeSize ) / 2;
	cameraInfo->ySubframeOffset = ( cameraInfo->maxResolutionY -
			cameraInfo->ySubframeSize ) / 2;

  cameraInfo->actualImageLength = cameraInfo->xImageSize *
			cameraInfo->yImageSize * cameraInfo->bytesPerPixel;
  return OA_ERR_NONE;
}


static int
_processStreamingStart ( oaCamera* camera, OA_COMMAND* command )
{
  SX_STATE*	cameraInfo = camera->_private;
  CALLBACK*	cb = command->commandData;

  if ( cameraInfo->runMode != CAM_RUN_MODE_STOPPED ) {
    return -OA_ERR_INVALID_COMMAND;
  }

  cameraInfo->streamingCallback.callback = cb->callback;
  cameraInfo->streamingCallback.callbackArg = cb->callbackArg;

  pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
  cameraInfo->runMode = CAM_RUN_MODE_STREAMING;
  pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );

  return OA_ERR_NONE;
}


static int
_processStreamingStop ( SX_STATE* cameraInfo, OA_COMMAND* command )
{
  int		queueEmpty;

  if ( cameraInfo->runMode != CAM_RUN_MODE_STREAMING ) {
    return -OA_ERR_INVALID_COMMAND;
  }

  pthread_mutex_lock ( &cameraInfo->commandQueueMutex );
  cameraInfo->runMode = CAM_RUN_MODE_STOPPED;
  pthread_mutex_unlock ( &cameraInfo->commandQueueMutex );

  // We wait here until the callback queue has drained otherwise a future
  // close of the camera could rip the image frame out from underneath the
  // callback

  queueEmpty = 0;
  do {
    pthread_mutex_lock ( &cameraInfo->callbackQueueMutex );
    queueEmpty = ( OA_CAM_BUFFERS == cameraInfo->buffersFree ) ? 1 : 0;
    pthread_mutex_unlock ( &cameraInfo->callbackQueueMutex );
    if ( !queueEmpty ) {
      usleep ( 100 );
    }
  } while ( !queueEmpty );

  return OA_ERR_NONE;
}


static int
_doStartExposure ( SX_STATE* cameraInfo )
{
  if ( cameraInfo->isInterlaced && cameraInfo->binMode == OA_BIN_MODE_NONE ) {
    _clearFrame ( cameraInfo, CCD_EXP_FLAGS_FIELD_EVEN );
		// FIX ME -- arbitrary figure here?
    usleep ( 100 );
    _clearFrame ( cameraInfo, CCD_EXP_FLAGS_FIELD_ODD );
  } else {
    _clearFrame ( cameraInfo, CCD_EXP_FLAGS_FIELD_BOTH );
  }

  return OA_ERR_NONE;
}


static int
_doReadExposure ( SX_STATE* cameraInfo )
{
  unsigned char*	evenFrame;
  unsigned char*	oddFrame;
  int			halfFrameSize, rowLength, numRows;

  _clearFrame ( cameraInfo, CCD_EXP_FLAGS_NOWIPE_FRAME );
 
  usleep ( 3000 );

  if ( cameraInfo->isInterlaced ) {
		if ( OA_BIN_MODE_NONE == cameraInfo->binMode ) {
			rowLength = cameraInfo->xImageSize * cameraInfo->bytesPerPixel;
			numRows = cameraInfo->yImageSize / 2;
			halfFrameSize = rowLength * numRows;
			evenFrame = cameraInfo->xferBuffer;
			oddFrame = cameraInfo->xferBuffer + halfFrameSize;

			_latchFrame ( cameraInfo, CCD_EXP_FLAGS_FIELD_EVEN,
					cameraInfo->xImageSize, numRows, cameraInfo->xSubframeOffset,
					cameraInfo->ySubframeOffset );
			_readFrame ( cameraInfo, evenFrame, halfFrameSize );
			_latchFrame ( cameraInfo, CCD_EXP_FLAGS_FIELD_ODD,
					cameraInfo->xImageSize, numRows, cameraInfo->xSubframeOffset,
					cameraInfo->ySubframeOffset );
			_readFrame ( cameraInfo, oddFrame, halfFrameSize );
		} else {
			_latchFrame ( cameraInfo, CCD_EXP_FLAGS_FIELD_BOTH,
					cameraInfo->xSubframeSize, cameraInfo->ySubframeSize / 2,
					cameraInfo->xSubframeOffset, cameraInfo->ySubframeOffset /
					cameraInfo->binMode );
			_readFrame ( cameraInfo, cameraInfo->xferBuffer,
					cameraInfo->actualImageLength );
		}
	} else {
		oaLogError ( OA_LOG_CAMERA, "%s: trying to read non-interlaced camera?!",
				__func__ );
	}
  return OA_ERR_NONE;
}


static int
_readTemperature ( SX_STATE* cameraInfo )
{
  int			temp, ret, transferred;
  unsigned char		buff [ SXUSB_REQUEST_BUFSIZE ];

  OA_CLEAR( buff );
  buff[ SXUSB_REQ_CMD_TYPE ] = SXUSB_CMD_REQUEST;
  buff[ SXUSB_REQ_CMD ] = SXUSB_COOLER_TEMPERATURE;
  buff[ SXUSB_REQ_LENGTH_L ] = 2;
  buff[ SXUSB_REQ_LENGTH_H ] = 0;

  if (( ret = libusb_bulk_transfer ( cameraInfo->usbHandle,
      SXUSB_BULK_ENDP_OUT, buff, SXUSB_REQUEST_BUFSIZE, &transferred,
      SXUSB_TIMEOUT )) || transferred != SXUSB_REQUEST_BUFSIZE ) {
    oaLogError ( OA_LOG_CAMERA,
				"%s: request TEMPERATURE for SX failed: ret = %d, "
        "transferred = %d of %d", __func__, ret, transferred,
				SXUSB_REQUEST_BUFSIZE );
    return 0;
  }
  if (( ret = libusb_bulk_transfer ( cameraInfo->usbHandle,
      SXUSB_BULK_ENDP_IN, buff, 2, &transferred,
      SXUSB_TIMEOUT )) || transferred != 2 ) {
    oaLogError ( OA_LOG_CAMERA,
				"%s: request TEMPERATURE for SX failed: ret = %d, "
        "transferred = %d of %d", __func__, ret, transferred, 2 );
    return 0;
  }

  temp = ( buff[0] | ( buff[1] << 8 )) - 273;
  return temp;
}


static int
_clearFrame ( SX_STATE* cameraInfo, unsigned int flags )
{
  unsigned char	buff [ SXUSB_REQUEST_BUFSIZE ];
  int		ret, transferred;

  OA_CLEAR( buff );
  buff[ SXUSB_REQ_CMD_TYPE ] = SXUSB_CMD_SEND;
  buff[ SXUSB_REQ_CMD ] = SXUSB_CLEAR_PIXELS;
  buff[ SXUSB_REQ_VALUE_L ] = flags & 0xff;
  buff[ SXUSB_REQ_VALUE_H ] = ( flags >> 8 ) & 0xff;
  ret = libusb_bulk_transfer ( cameraInfo->usbHandle, SXUSB_BULK_ENDP_OUT,
      buff, SXUSB_REQUEST_BUFSIZE, &transferred, SXUSB_TIMEOUT );
  if ( ret || transferred != SXUSB_REQUEST_BUFSIZE ) {
    oaLogError ( OA_LOG_CAMERA,
				"%s: send CLEAR PIXELS for SX failed: ret = %d, xferred = %d of %d",
				__func__, ret, transferred, SXUSB_REQUEST_BUFSIZE );
    return -OA_ERR_CAMERA_IO;
  }

  return OA_ERR_NONE;
}


static int
_latchFrame ( SX_STATE* cameraInfo, unsigned int flags, unsigned int x,
    unsigned int y, unsigned int xoff, unsigned int yoff )
{
  unsigned char	buff [ SXUSB_READ_BUFSIZE ];
  int		ret, transferred, xbin, ybin;

  if ( OA_BIN_MODE_2x2 == cameraInfo->binMode ) {
    xbin = 2;
    ybin = 1;
  } else {
    xbin = ybin = 1;
  }

  OA_CLEAR( buff );
  buff[ SXUSB_REQ_CMD_TYPE ] = SXUSB_CMD_SEND;
  buff[ SXUSB_REQ_CMD ] = SXUSB_READ_PIXELS;
  buff[ SXUSB_REQ_VALUE_L ] = flags & 0xff;
  buff[ SXUSB_REQ_VALUE_H ] = ( flags >> 8 ) & 0xff;
  buff[ SXUSB_REQ_INDEX_L ] = 0;
  buff[ SXUSB_REQ_INDEX_H ] = 0;
  buff[ SXUSB_REQ_LENGTH_L ] = 10;
  buff[ SXUSB_REQ_LENGTH_H ] = 0;
  buff[ SXUSB_REQ_DATA + 0] = xoff & 0xff;
  buff[ SXUSB_REQ_DATA + 1] = ( xoff >> 8 ) & 0xff;
  buff[ SXUSB_REQ_DATA + 2] = yoff & 0xff;
  buff[ SXUSB_REQ_DATA + 3] = ( yoff >> 8 ) & 0xff;
  buff[ SXUSB_REQ_DATA + 4] = x & 0xff;
  buff[ SXUSB_REQ_DATA + 5] = ( x >> 8 ) & 0xff;
  buff[ SXUSB_REQ_DATA + 6] = y & 0xff;
  buff[ SXUSB_REQ_DATA + 7] = ( y >> 8 ) & 0xff;
  buff[ SXUSB_REQ_DATA + 8] = xbin;
  buff[ SXUSB_REQ_DATA + 9] = ybin;

  if (( ret = libusb_bulk_transfer ( cameraInfo->usbHandle,
      SXUSB_BULK_ENDP_OUT, buff, SXUSB_READ_BUFSIZE, &transferred,
      SXUSB_TIMEOUT )) || transferred != SXUSB_READ_BUFSIZE ) {
    oaLogError ( OA_LOG_CAMERA,
				"%s: request READ for SX failed: ret = %d, xferred = %d of %d",
				__func__, ret, transferred, SXUSB_READ_BUFSIZE );
    return -OA_ERR_CAMERA_IO;
  }

  return OA_ERR_NONE;
}


static int
_readFrame ( SX_STATE* cameraInfo, unsigned char* buffer, int length )
{
  int		transferred;
  int		ret;

	while ( length ) {
		if (( ret = libusb_bulk_transfer ( cameraInfo->usbHandle,
				SXUSB_BULK_ENDP_IN, buffer, length, &transferred,
				SXUSB_FRAME_TIMEOUT ))) {
			oaLogError ( OA_LOG_CAMERA,
					"%s: receive READ for SX failed: ret = %d, xferred = %d of %d",
					__func__, ret, transferred, length );
			return -OA_ERR_CAMERA_IO;
		}
		/*
		if ( length != transferred ) {
			oaLogError ( OA_LOG_CAMERA, "%s: length %d != transferred %d",
			__func__, length, transferred );
			return -OA_ERR_CAMERA_IO;
		}
		 */
		length -= transferred;
		buffer += transferred;
	}
  return OA_ERR_NONE;
}


int
_SXsetTimer ( SX_STATE* cameraInfo, unsigned int microseconds )
{
  unsigned char buff [ SXUSB_TIMER_BUFSIZE ];
  int           ret, transferred;
  unsigned int	milliseconds = microseconds / 1000;

  OA_CLEAR( buff );
  buff[ SXUSB_REQ_CMD_TYPE ] = SXUSB_CMD_SEND;
  buff[ SXUSB_REQ_CMD ] = SXUSB_SET_TIMER;
  buff[ SXUSB_REQ_LENGTH_L ] = 4;
  buff[ SXUSB_REQ_LENGTH_H ] = 0;
  buff[ SXUSB_REQ_DATA + 0] = milliseconds & 0xff;
  buff[ SXUSB_REQ_DATA + 1] = ( milliseconds >> 8 ) & 0xff;
  buff[ SXUSB_REQ_DATA + 2] = ( milliseconds >> 16 ) & 0xff;
  buff[ SXUSB_REQ_DATA + 3] = ( milliseconds >> 24 ) & 0xff;

  ret = libusb_bulk_transfer ( cameraInfo->usbHandle, SXUSB_BULK_ENDP_OUT,
      buff, SXUSB_TIMER_BUFSIZE, &transferred, SXUSB_TIMEOUT );
  if ( ret || transferred != SXUSB_TIMER_BUFSIZE ) {
    oaLogError ( OA_LOG_CAMERA,
				"%s: send TIMER for SX failed: ret = %d, xferred = %d of %d",
				__func__, ret, transferred, SXUSB_TIMER_BUFSIZE );
    return -OA_ERR_CAMERA_IO;
  }

  return OA_ERR_NONE;
}
