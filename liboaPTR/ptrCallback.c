/*****************************************************************************
 *
 * ptrCallback.c -- Thread for handling callbacks to user code
 *
 * Copyright 2015,2017,2021 James Fidell (james@openastroproject.org)
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

#include <openastro/util.h>
#include <openastro/timer.h>

#include "oaptrprivate.h"
#include "unimplemented.h"
#include "ptr.h"


void*
oaPTRcallbackHandler ( void* param )
{
  oaPTR*		ptr = param;
  PRIVATE_INFO*		ptrInfo = ptr->_private;
  int			exitThread = 0;
  CALLBACK*		callback;

  do {
    pthread_mutex_lock ( &ptrInfo->callbackQueueMutex );
    exitThread = ptrInfo->stopCallbackThread;
    pthread_mutex_unlock ( &ptrInfo->callbackQueueMutex );

    if ( exitThread ) {
      break;
    } else {
      // try to prevent busy-waiting
      if ( oaDLListIsEmpty ( ptrInfo->callbackQueue )) {
        pthread_mutex_lock ( &ptrInfo->callbackQueueMutex );
        pthread_cond_wait ( &ptrInfo->callbackQueued,
            &ptrInfo->callbackQueueMutex );
        pthread_mutex_unlock ( &ptrInfo->callbackQueueMutex );
      }
    }

    callback = oaDLListRemoveFromHead ( ptrInfo->callbackQueue );
    if ( callback ) {
      switch ( callback->callbackType ) {
        default:
          oaLogError ( OA_LOG_TIMER, "%s: unexpected callback type %d",
							__func__, callback->callbackType );
          break;
      }
    }
  } while ( 1 );

  return 0;
}
