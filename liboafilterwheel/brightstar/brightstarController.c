/*****************************************************************************
 *
 * brightstarController.c -- Brightstar filter wheel control functions
 *
 * Copyright 2018,2021
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
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>

#include <openastro/util.h>
#include <openastro/filterwheel.h>
#include <openastro/filterwheel/controls.h>

#include "oafwprivate.h"
#include "unimplemented.h"
#include "brightstarfw.h"


static int	_processSetControl ( PRIVATE_INFO*, OA_COMMAND* );
static int	_processGetControl ( PRIVATE_INFO*, OA_COMMAND* );


void*
oafwBrightstarcontroller ( void* param )
{
  oaFilterWheel*	wheel = param;
  PRIVATE_INFO*		wheelInfo = wheel->_private;
  OA_COMMAND*		command;
  int			exitThread = 0;
  int			resultCode;

  do {
    pthread_mutex_lock ( &wheelInfo->commandQueueMutex );
    exitThread = wheelInfo->stopControllerThread;
    pthread_mutex_unlock ( &wheelInfo->commandQueueMutex );
    if ( exitThread ) {
      break;
    } else {
      pthread_mutex_lock ( &wheelInfo->commandQueueMutex );
      // stop us busy-waiting
      if ( oaDLListIsEmpty ( wheelInfo->commandQueue )) {
        pthread_cond_wait ( &wheelInfo->commandQueued,
            &wheelInfo->commandQueueMutex );
      }
      pthread_mutex_unlock ( &wheelInfo->commandQueueMutex );
    }
    do {
      command = oaDLListRemoveFromHead ( wheelInfo->commandQueue );
      if ( command ) {
        switch ( command->commandType ) {
          case OA_CMD_CONTROL_SET:
            resultCode = _processSetControl ( wheelInfo, command );
            break;
          case OA_CMD_CONTROL_GET:
            resultCode = _processGetControl ( wheelInfo, command );
            break;
          default:
            oaLogError ( OA_LOG_FILTERWHEEL, "%s: Invalid command type %d",
                __func__, command->commandType );
            resultCode = -OA_ERR_INVALID_CONTROL;
            break;
        }
        if ( command->callback ) {
					oaLogWarning ( OA_LOG_FILTERWHEEL, "%s: command has callback",
							__func__ );
        } else {
          pthread_mutex_lock ( &wheelInfo->commandQueueMutex );
          command->completed = 1;
          command->resultCode = resultCode;
          pthread_mutex_unlock ( &wheelInfo->commandQueueMutex );
          pthread_cond_broadcast ( &wheelInfo->commandComplete );
        }
      }
    } while ( command );

  } while ( !exitThread );

  return 0;
}


static int
_processSetControl ( PRIVATE_INFO* wheelInfo, OA_COMMAND* command )
{
  int			control = command->controlId;
  oaControlValue*	val = command->commandData;

  oaLogDebug ( OA_LOG_FILTERWHEEL, "%s: Brightstar control:  %d", __func__,
			control );

  switch ( control ) {

    case OA_FW_CTRL_MOVE_ABSOLUTE_ASYNC:
    {
      int	slot;

      if ( val->valueType != OA_CTRL_TYPE_INT32 ) {
        oaLogError ( OA_LOG_FILTERWHEEL,
						"%s: invalid control type %d where int32 expected", __func__,
						val->valueType );
        return -OA_ERR_INVALID_CONTROL_TYPE;
      }
      slot = val->int32;
      oaBrightstarMoveTo ( wheelInfo, slot, 0 );
      break;
    }
    default:
      oaLogError ( OA_LOG_FILTERWHEEL, "%s: Unrecognised control %d", __func__,
					control );
      return -OA_ERR_INVALID_CONTROL;
      break;
  }

  return OA_ERR_NONE;
}


static int
_processGetControl ( PRIVATE_INFO* wheelInfo, OA_COMMAND* command )
{
  int			control = command->controlId;

  oaLogDebug ( OA_LOG_FILTERWHEEL, "%s: Brightstar control:  %d", __func__,
			control );

  oaLogError ( OA_LOG_FILTERWHEEL, "%s: Unrecognised control %d", __func__,
			control );
  return -OA_ERR_INVALID_CONTROL;
}
