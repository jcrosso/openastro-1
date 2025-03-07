/*****************************************************************************
 *
 * FC2connect.c -- Initialise Point Grey Gig-E cameras
 *
 * Copyright 2015,2016,2017,2018,2019,2020,2021
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
#include <pthread.h>
#include <openastro/camera.h>
#include <openastro/util.h>
#include <openastro/demosaic.h>

#include "unimplemented.h"
#include "oacamprivate.h"
#include "FC2oacam.h"
#include "FC2private.h"
#include "FC2.h"
#include "FC2state.h"


static void _FC2InitFunctionPointers ( oaCamera* );

struct pgeCtrl pgeControls[] = {
  { FC2_BRIGHTNESS, OA_CAM_CTRL_BRIGHTNESS,
      OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_BRIGHTNESS )},
  { FC2_AUTO_EXPOSURE, OA_CAM_CTRL_EXPOSURE_VALUE,
      OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_EXPOSURE_VALUE )},
  { FC2_SHARPNESS, OA_CAM_CTRL_SHARPNESS,
      OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_SHARPNESS )},
  { FC2_WHITE_BALANCE, OA_CAM_CTRL_WHITE_BALANCE,
      OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_WHITE_BALANCE )},
  { FC2_HUE, OA_CAM_CTRL_HUE, OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_HUE )},
  { FC2_SATURATION, OA_CAM_CTRL_SATURATION, 
      OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_SATURATION )},
  { FC2_GAMMA, OA_CAM_CTRL_GAMMA, OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_GAMMA )},
  { FC2_IRIS, OA_CAM_CTRL_IRIS_ABSOLUTE,
      OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_IRIS_ABSOLUTE )},
  { FC2_FOCUS, OA_CAM_CTRL_FOCUS_ABSOLUTE,
      OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_FOCUS_ABSOLUTE )},
  { FC2_ZOOM, OA_CAM_CTRL_ZOOM_ABSOLUTE, 
      OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_ZOOM_ABSOLUTE )},
  { FC2_PAN, OA_CAM_CTRL_PAN_ABSOLUTE,
      OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_PAN_ABSOLUTE )},
  { FC2_TILT, OA_CAM_CTRL_TILT_ABSOLUTE, 
      OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_TILT_ABSOLUTE )},
  { FC2_SHUTTER, -1, -1 },
  { FC2_GAIN, OA_CAM_CTRL_GAIN, OA_CAM_CTRL_MODE_AUTO( OA_CAM_CTRL_GAIN )},
  { FC2_TRIGGER_MODE, OA_CAM_CTRL_TRIGGER_MODE, 0 },
  { FC2_TRIGGER_DELAY, OA_CAM_CTRL_TRIGGER_DELAY, 0 },
  { FC2_FRAME_RATE, -1, -1 },
  { FC2_TEMPERATURE, OA_CAM_CTRL_TEMPERATURE, 0 },
};

unsigned int numFC2Controls = sizeof ( pgeControls ) /
    sizeof ( struct pgeCtrl );

struct pgeFrameRate pgeFrameRates[] = {
  { FC2_FRAMERATE_1_875, 8, 15 },
  { FC2_FRAMERATE_3_75, 4, 15 },
  { FC2_FRAMERATE_7_5, 2, 15 },
  { FC2_FRAMERATE_15, 1, 15 },
  { FC2_FRAMERATE_30, 1, 30 },
  { FC2_FRAMERATE_60, 1, 60 },
  { FC2_FRAMERATE_120, 1, 120 },
  { FC2_FRAMERATE_240, 1, 240 }
};

unsigned int numFC2FrameRates = sizeof ( pgeFrameRates ) /
    sizeof ( struct pgeFrameRate );

/**
 * Initialise a given camera device
 */

oaCamera*
oaFC2InitCamera ( oaCameraDevice* device )
{
  oaCamera*			camera;
  FC2_STATE*			cameraInfo;
  COMMON_INFO*			commonInfo;
  fc2Context			pgeContext;
  DEVICE_INFO*			devInfo;
  int				oaControl, oaAutoControl, onOffControl;
  int				mode, firstMode;
  fc2Property			property;
  fc2PropertyInfo		propertyInfo;
  fc2GigEImageSettings		settings;
  fc2GigEImageSettingsInfo	imageInfo;
  fc2TriggerModeInfo		triggerInfo;
  fc2TriggerMode		triggerMode;
  fc2TriggerDelayInfo		delayInfo;
  fc2TriggerDelay		triggerDelay;
  fc2StrobeInfo			strobeInfo;
  fc2StrobeControl		strobeControl;
  fc2CameraInfo			camInfo;
	fc2EmbeddedImageInfo	embeddedInfo;
  unsigned int			i, j, numResolutions, found, xbin = 1;
  BOOL				supported;
  uint16_t			mask16;
  unsigned int			numberOfSources, numberOfModes;
  unsigned int			dataFormat, format;
  int				ret, numBinModes, maxBinMode;
	void*			tmpPtr;

  if ( _oaInitCameraStructs ( &camera, ( void* ) &cameraInfo,
      sizeof ( FC2_STATE ), &commonInfo ) != OA_ERR_NONE ) {
    return 0;
  }

  _FC2InitFunctionPointers ( camera );

  ( void ) strcpy ( camera->deviceName, device->deviceName );
  cameraInfo->initialised = 0;
  devInfo = device->_private;

  if (( *p_fc2CreateGigEContext )( &pgeContext ) != FC2_ERROR_OK ) {
    oaLogError ( OA_LOG_CAMERA, "%s: Can't get FC2 context", __func__ );
    FREE_DATA_STRUCTS;
    return 0;
  }

  if (( *p_fc2Connect )( pgeContext, &devInfo->pgeGuid ) != FC2_ERROR_OK ) {
    oaLogError ( OA_LOG_CAMERA, "%s: Can't connect to FC2 GUID", __func__ );
    ( *p_fc2DestroyContext )( pgeContext );
    FREE_DATA_STRUCTS;
    return 0;
  }

  if (( *p_fc2GetCameraInfo )( pgeContext, &camInfo ) != FC2_ERROR_OK ) {
    oaLogError ( OA_LOG_CAMERA, "%s: Can't get camera info for FC2 camera",
				__func__ );
    ( *p_fc2DestroyContext )( pgeContext );
    FREE_DATA_STRUCTS;
    return 0;
  }

  oaLogDebug ( OA_LOG_CAMERA, "%s: GigE major version - %u", __func__,
      camInfo.gigEMajorVersion );
  oaLogDebug ( OA_LOG_CAMERA, "%s: GigE minor version - %u", __func__,
      camInfo.gigEMinorVersion );
  oaLogDebug ( OA_LOG_CAMERA, "%s: User-defined name - %s", __func__,
      camInfo.userDefinedName );
  oaLogDebug ( OA_LOG_CAMERA, "%s: Model name - %s", __func__,
      camInfo.modelName );
  oaLogDebug ( OA_LOG_CAMERA, "%s: XML URL1 - %s", __func__, camInfo.xmlURL1 );
  oaLogDebug ( OA_LOG_CAMERA, "%s: XML URL2 - %s", __func__, camInfo.xmlURL2 );
  oaLogDebug ( OA_LOG_CAMERA, "%s: Firmware version - %s", __func__,
      camInfo.firmwareVersion );
  oaLogDebug ( OA_LOG_CAMERA, "%s: IIDC version - %1.2f", __func__,
      camInfo.iidcVer / 100.0f );
  oaLogDebug ( OA_LOG_CAMERA,
			"%s: MAC address - %02X:%02X:%02X:%02X:%02X:%02X", __func__,
      camInfo.macAddress.octets[0], camInfo.macAddress.octets[1],
      camInfo.macAddress.octets[2], camInfo.macAddress.octets[3],
      camInfo.macAddress.octets[4], camInfo.macAddress.octets[5] );
  oaLogDebug ( OA_LOG_CAMERA, "%s: IP address - %u.%u.%u.%u", __func__,
      camInfo.ipAddress.octets[0], camInfo.ipAddress.octets[1],
      camInfo.ipAddress.octets[2], camInfo.ipAddress.octets[3] );
  oaLogDebug ( OA_LOG_CAMERA, "%s: Subnet mask - %u.%u.%u.%u", __func__,
      camInfo.subnetMask.octets[0], camInfo.subnetMask.octets[1],
      camInfo.subnetMask.octets[2], camInfo.subnetMask.octets[3] );
  oaLogDebug ( OA_LOG_CAMERA, "%s: Default gateway - %u.%u.%u.%u", __func__,
      camInfo.defaultGateway.octets[0], camInfo.defaultGateway.octets[1],
      camInfo.defaultGateway.octets[2], camInfo.defaultGateway.octets[3]);

  camera->interface = device->interface;

  cameraInfo->colour = devInfo->colour;
  cameraInfo->cfaPattern = devInfo->cfaPattern;

  cameraInfo->runMode = CAM_RUN_MODE_STOPPED;

  // FIX ME -- Frame rate is awkward because the maximum exposure time
  // varies with the frame rate.  For the time being if the frame rate
  // can be changed we'll turn it off and pretend it doesn't exist.

  OA_CLEAR ( propertyInfo );
  propertyInfo.type = FC2_FRAME_RATE;
  if (( *p_fc2GetPropertyInfo )( pgeContext, &propertyInfo ) != FC2_ERROR_OK ) {
    oaLogError ( OA_LOG_CAMERA,
				"%s: Can't get property info for PGR frame rate", __func__ );
    ( *p_fc2DestroyContext )( pgeContext );
    FREE_DATA_STRUCTS;
    return 0;
  }
  if ( propertyInfo.present ) {
    OA_CLEAR ( property );
    property.type = FC2_FRAME_RATE;
    if (( *p_fc2GetProperty )( pgeContext, &property ) != FC2_ERROR_OK ) {
      oaLogError ( OA_LOG_CAMERA, "%s: Can't get property for PGR frame rate",
					__func__ );
      ( *p_fc2DestroyContext )( pgeContext );
      FREE_DATA_STRUCTS;
      return 0;
    }
    if ( propertyInfo.onOffSupported ) {
      property.onOff = 0;
      property.autoManualMode = 0;
      if (( *p_fc2SetProperty )( pgeContext, &property ) != FC2_ERROR_OK ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: Can't set property for PGR frame rate", __func__ );
        ( *p_fc2DestroyContext )( pgeContext );
        FREE_DATA_STRUCTS;
        return 0;
      }
    } else {
      oaLogWarning ( OA_LOG_CAMERA,
					"%s: FC2 frame rate exists, but cannot be turned off", __func__ );
    }
  }

  // There's probably a lot of work still to be done here.

  camera->features.flags |= OA_CAM_FEATURE_READABLE_CONTROLS;
  camera->features.flags |= OA_CAM_FEATURE_STREAMING;

  for ( i = 0; i < FC2_UNSPECIFIED_PROPERTY_TYPE; i++ ) {
    OA_CLEAR ( propertyInfo );
    propertyInfo.type = i;
    if (( *p_fc2GetPropertyInfo )( pgeContext, &propertyInfo ) !=
        FC2_ERROR_OK ) {
      oaLogError ( OA_LOG_CAMERA,
					"%s: Can't get property info %d for FC2 GUID", __func__, i );
      ( *p_fc2DestroyContext )( pgeContext );
      FREE_DATA_STRUCTS;
      return 0;
    }
    if ( !propertyInfo.present ) {
      continue;
    }
    OA_CLEAR ( property );
    property.type = i;
    if (( *p_fc2GetProperty )( pgeContext, &property ) != FC2_ERROR_OK ) {
      oaLogError ( OA_LOG_CAMERA, "%s: Can't get property %d for FC2 GUID",
					__func__, i );
      ( *p_fc2DestroyContext )( pgeContext );
      FREE_DATA_STRUCTS;
      return 0;
    }

		oaLogDebug ( OA_LOG_CAMERA, "%s: property %d, units: %s, abbrev: %s",
				__func__, i, propertyInfo.pUnits, propertyInfo.pUnitAbbr );
		oaLogDebug ( OA_LOG_CAMERA, "%s:   on/off: %d, value %d", __func__,
				propertyInfo.onOffSupported, property.onOff  );
		oaLogDebug ( OA_LOG_CAMERA, "%s:   min: %d, max %d", __func__,
				propertyInfo.min, propertyInfo.max );
		oaLogDebug ( OA_LOG_CAMERA, "%s:   abs: %d, absmin: %f, absmax: %f",
				__func__, propertyInfo.absValSupported, propertyInfo.absMin,
				propertyInfo.absMax );
		oaLogDebug ( OA_LOG_CAMERA, "%s:   auto: %d, manual %d, state: %d",
				__func__, propertyInfo.autoSupported, propertyInfo.manualSupported,
				property.autoManualMode );

    oaControl = pgeControls[ i ].oaControl;
    oaAutoControl = pgeControls[ i ].oaAutoControl;
    onOffControl = OA_CAM_CTRL_MODE_ON_OFF( oaControl );

    switch ( i + FC2_BRIGHTNESS ) {

      case FC2_BRIGHTNESS:
      case FC2_SHARPNESS:
      case FC2_HUE:
      case FC2_SATURATION:
      case FC2_GAMMA:
      case FC2_GAIN:
      case FC2_IRIS:
      case FC2_FOCUS:
      case FC2_ZOOM:
      case FC2_PAN:
      case FC2_TILT:
      case FC2_AUTO_EXPOSURE:
        if ( propertyInfo.manualSupported ) {
          camera->OA_CAM_CTRL_TYPE( oaControl ) = OA_CTRL_TYPE_INT32;
          commonInfo->OA_CAM_CTRL_MIN( oaControl ) = propertyInfo.min;
          commonInfo->OA_CAM_CTRL_MAX( oaControl ) = propertyInfo.max;
          commonInfo->OA_CAM_CTRL_STEP( oaControl ) = 1; // arbitrary
          commonInfo->OA_CAM_CTRL_DEF( oaControl ) = property.valueA;
        }
        if ( propertyInfo.autoSupported ) {
          if ( oaAutoControl ) {
            camera->OA_CAM_CTRL_TYPE( oaAutoControl ) = OA_CTRL_TYPE_BOOLEAN;
            commonInfo->OA_CAM_CTRL_MIN( oaAutoControl ) = 0;
            commonInfo->OA_CAM_CTRL_MAX( oaAutoControl ) = 1;
            commonInfo->OA_CAM_CTRL_STEP( oaAutoControl ) = 1;
            commonInfo->OA_CAM_CTRL_DEF( oaAutoControl ) =
                ( property.autoManualMode ) ? 1 : 0;
          } else {
            oaLogWarning ( OA_LOG_CAMERA,
								"%s: have auto for control %d, but liboacam does not",
								__func__, oaControl );
          }
        }
        if ( propertyInfo.onOffSupported ) {
          camera->OA_CAM_CTRL_TYPE( onOffControl ) = OA_CTRL_TYPE_BOOLEAN;
          commonInfo->OA_CAM_CTRL_MIN( onOffControl ) = 0;
          commonInfo->OA_CAM_CTRL_MAX( onOffControl ) = 1;
          commonInfo->OA_CAM_CTRL_STEP( onOffControl ) = 1;
          commonInfo->OA_CAM_CTRL_DEF( onOffControl ) = 1; // a guess
        }
        break;

      case FC2_SHUTTER:
      {
        unsigned int min, max, step, def;
        // FIX ME -- should really handle both absolute and unscaled
        // exposure times here
        // shutter is actually exposure time.  exposure is something
        // else
        if ( propertyInfo.absValSupported ) {
          oaControl = OA_CAM_CTRL_EXPOSURE_ABSOLUTE;
          camera->OA_CAM_CTRL_TYPE( oaControl ) = OA_CTRL_TYPE_INT64;
          // On the Blackfly at least, these values appear to be in seconds
          // rather than as the units string suggests
          min = propertyInfo.absMin * 1000.0;
          max = propertyInfo.absMax * 1000.0;
          step = 1000; // arbitrary
          def = property.absValue * 1000.0;
        } else {
          oaControl = OA_CAM_CTRL_EXPOSURE_UNSCALED;
          camera->OA_CAM_CTRL_TYPE( oaControl ) = OA_CTRL_TYPE_INT32;
          min = propertyInfo.min;
          max = propertyInfo.max;
          step = 1; // arbitrary
          def = property.valueA;
        }
        commonInfo->OA_CAM_CTRL_MIN( oaControl ) = min;
        commonInfo->OA_CAM_CTRL_MAX( oaControl ) = max;
        commonInfo->OA_CAM_CTRL_STEP( oaControl ) = step;
        commonInfo->OA_CAM_CTRL_DEF( oaControl ) = def;
        if ( propertyInfo.autoSupported ) {
          oaAutoControl = OA_CAM_CTRL_MODE_AUTO( oaControl );
          camera->OA_CAM_CTRL_TYPE( oaAutoControl ) = OA_CTRL_TYPE_BOOLEAN;
          commonInfo->OA_CAM_CTRL_MIN( oaAutoControl ) = 0;
          commonInfo->OA_CAM_CTRL_MAX( oaAutoControl ) = 1;
          commonInfo->OA_CAM_CTRL_STEP( oaAutoControl ) = 1;
          commonInfo->OA_CAM_CTRL_DEF( oaAutoControl ) =
							( property.autoManualMode ) ?  1 : 0;
        }
        if ( propertyInfo.onOffSupported ) {
          camera->OA_CAM_CTRL_TYPE( onOffControl ) = OA_CTRL_TYPE_BOOLEAN;
          commonInfo->OA_CAM_CTRL_MIN( onOffControl ) = 0;
          commonInfo->OA_CAM_CTRL_MAX( onOffControl ) = 1;
          commonInfo->OA_CAM_CTRL_STEP( onOffControl ) = 1;
          commonInfo->OA_CAM_CTRL_DEF( onOffControl ) = 1; // a guess
        }
        break;
      }
      case FC2_WHITE_BALANCE:
        // This is more complex.
        // The manual white balance control is actually achieved by
        // setting the red and blue balances independently (though they
        // both have to be set at the same time).  Auto mode however
        // controls both at once.
        // So, we need manual red and blue balance controls and an auto
        // white balance control

        if ( propertyInfo.manualSupported ) {
          camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_BLUE_BALANCE ) =
              camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_RED_BALANCE ) =
              OA_CTRL_TYPE_INT32;
          commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_BLUE_BALANCE ) =
              commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_RED_BALANCE ) =
              propertyInfo.min;
          commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_BLUE_BALANCE ) =
              commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_RED_BALANCE ) =
              propertyInfo.max;
          commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_BLUE_BALANCE ) =
              commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_RED_BALANCE ) = 1;//arbitrary
          commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_RED_BALANCE ) =
              cameraInfo->currentRedBalance = property.valueA;
          commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_BLUE_BALANCE ) =
              cameraInfo->currentBlueBalance = property.valueB;
        }
        if ( propertyInfo.autoSupported ) {
          if ( oaAutoControl ) {
            camera->OA_CAM_CTRL_TYPE( oaAutoControl ) = OA_CTRL_TYPE_BOOLEAN;
            commonInfo->OA_CAM_CTRL_MIN( oaAutoControl ) = 0;
            commonInfo->OA_CAM_CTRL_MAX( oaAutoControl ) = 1;
            commonInfo->OA_CAM_CTRL_STEP( oaAutoControl ) = 1;
            commonInfo->OA_CAM_CTRL_DEF( oaAutoControl ) =
                ( property.autoManualMode ) ? 1 : 0;
          } else {
            oaLogError ( OA_LOG_CAMERA,
								"%s: have auto for control %d, but liboacam does not",
								__func__, oaControl );
          }
        }
        if ( propertyInfo.onOffSupported ) {
          camera->OA_CAM_CTRL_TYPE( onOffControl ) = OA_CTRL_TYPE_BOOLEAN;
          commonInfo->OA_CAM_CTRL_MIN( onOffControl ) = 0;
          commonInfo->OA_CAM_CTRL_MAX( onOffControl ) = 1;
          commonInfo->OA_CAM_CTRL_STEP( onOffControl ) = 1;
          commonInfo->OA_CAM_CTRL_DEF( onOffControl ) = 1; // a guess
        }
        break;

      case FC2_FRAME_RATE:
				// FIX ME
        oaLogInfo ( OA_LOG_CAMERA,
						"%s: Need to set up frame rates for FC2 camera", __func__ );
        break;

      case FC2_TEMPERATURE:
        camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_TEMPERATURE ) = OA_CTRL_TYPE_READONLY;
        break;

      // FIX ME -- not sure how to handle these next two because the trigger
      // control is all done with separate API calls.  It's not clear to me
      // why these exist
      case FC2_TRIGGER_MODE:
      case FC2_TRIGGER_DELAY:
        break;

      default:
        oaLogError ( OA_LOG_CAMERA, "%s: unknown FC2 control %d", __func__,
            i + FC2_BRIGHTNESS );
        break;
    }
  }

  // Now sort out whether trigger mode is supported or not

  if (( p_fc2GetTriggerModeInfo )( pgeContext, &triggerInfo ) !=
      FC2_ERROR_OK ) {
    oaLogError ( OA_LOG_CAMERA,
				"%s: Can't get trigger mode info %d for FC2 GUID", __func__, i );
    ( *p_fc2DestroyContext )( pgeContext );
    FREE_DATA_STRUCTS;
    return 0;
  }

  oaLogDebug ( OA_LOG_CAMERA, "%s: trigger mode:", __func__ );
  oaLogDebug ( OA_LOG_CAMERA, "%s:   present  : %d", __func__,
			triggerInfo.present ? 1 : 0 );
  oaLogDebug ( OA_LOG_CAMERA, "%s:   readout  : %d", __func__,
			triggerInfo.readOutSupported ? 1 : 0 );
  oaLogDebug ( OA_LOG_CAMERA, "%s:   on/off   : %d", __func__,
			triggerInfo.onOffSupported ? 1 : 0 );
  oaLogDebug ( OA_LOG_CAMERA, "%s:   polarity : %d", __func__,
			triggerInfo.polaritySupported ? 1 : 0 );
  oaLogDebug ( OA_LOG_CAMERA, "%s:   readable : %d", __func__,
			triggerInfo.valueReadable ? 1 : 0 );
  oaLogDebug ( OA_LOG_CAMERA, "%s:   src mask : %08x", __func__,
			triggerInfo.sourceMask );
  oaLogDebug ( OA_LOG_CAMERA, "%s:   sw trig  : %d", __func__,
			triggerInfo.softwareTriggerSupported ? 1 : 0 );
  oaLogDebug ( OA_LOG_CAMERA, "%s:   mode mask: %08x", __func__,
			triggerInfo.modeMask );

  if ( triggerInfo.present ) {
		camera->features.flags |= OA_CAM_FEATURE_EXTERNAL_TRIGGER;
	}

  // FIX ME -- need to handle readOutSupported ?
  // FIX ME -- need to handle valueReadable ?
  // FIX ME -- need to handle softwareTriggerSupported ?

  if ( triggerInfo.present ) {

    cameraInfo->triggerEnable = triggerInfo.onOffSupported ? 1 : 0;
    if ( triggerInfo.onOffSupported ) {
      if ( !triggerInfo.valueReadable ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: Trigger info is not readable. This will break", __func__ );
      }
      camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_TRIGGER_ENABLE ) =
					OA_CTRL_TYPE_BOOLEAN;
      commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_TRIGGER_ENABLE ) = 0;
      commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_TRIGGER_ENABLE ) = 1;
      commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_TRIGGER_ENABLE ) = 1;
      // off appears to be the case for the Blackfly.  I'll assume for all
      commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_TRIGGER_ENABLE ) = 0;
    }

    if ( triggerInfo.polaritySupported ) {
      camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_TRIGGER_POLARITY ) =
					OA_CTRL_TYPE_MENU;
      commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_TRIGGER_POLARITY ) = 0;
      commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_TRIGGER_POLARITY ) = 1;
      commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_TRIGGER_POLARITY ) = 1;
      // trailing edge appears to be the case for the Blackfly.  I'll
      // assume for all
      commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_TRIGGER_POLARITY ) = 0;
    }

    // We're going to asssume that the list of sources will be contiguous
    // otherwise it won't work as a menu and we'll have to swap to discrete
    // values.
    // My current interpretation of sourceMask is that bits 0 to 3
    // correspond to GPIO3 to GPIO0 (the camera appears to use a different
    // endianness to x86 as far as I can see).

    numberOfSources = 0;
    if ( triggerInfo.sourceMask ) {
      mask16 = triggerInfo.sourceMask & 0x0f; // only bits 0 to 3
      while ( mask16 ) {
        if ( mask16 & 0x8 ) {
          cameraInfo->triggerGPIO = numberOfSources;
          numberOfSources++;
        } else {
          if ( numberOfSources ) {
            // At this point we don't have a source for the current GPIO pin,
            // but there appear to be sources for higher-numbered pins.  This
            // is not going to lead to happiness at the moment
            oaLogError ( OA_LOG_CAMERA, "%s: Available source GPIO pins "
								"appear to be non-contiguous.  This will lead to pain and "
								"needs fixing.", __func__ );
          }
        }
        mask16 = ( mask16 << 1 ) & 0x0f;
      }

      if ( numberOfSources > 1 ) {
				// FIX ME -- not sure 0 is the correct minimum value here, or if so,
				// numberOfSources - 1 should perhaps be the correct maximum
        camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_TRIGGER_SOURCE ) = OA_CTRL_TYPE_MENU;
        commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_TRIGGER_SOURCE ) = 0;
        commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_TRIGGER_SOURCE ) = numberOfSources;
        commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_TRIGGER_SOURCE ) = 1;
        oaLogWarning ( OA_LOG_CAMERA,
						"%s: Need to set default trigger source value", __func__ );
        commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_TRIGGER_SOURCE ) = 0;
      }
    }

    if ( triggerInfo.modeMask ) {
      numberOfModes = 0;
      mask16 = cameraInfo->modeMask = triggerInfo.modeMask;
      while ( mask16 ) {
        if ( mask16 & 0x8000 ) {
          numberOfModes++;
        }
        mask16 <<= 1;
      }

      if ( numberOfModes > 1 ) {
        camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_TRIGGER_MODE ) =
            OA_CTRL_TYPE_DISC_MENU;
        commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_TRIGGER_MODE ) = 0;
        commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_TRIGGER_MODE ) = numberOfModes;
        commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_TRIGGER_MODE ) = 1;
        // 0 appears to be the case for the Blackfly.  I'll assume for all
        commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_TRIGGER_MODE ) = 0;

        // Now we know how many values there are, cycle through them again to
        // create the discrete values list

        cameraInfo->triggerModeCount = numberOfModes;
        if (!( cameraInfo->triggerModes = calloc ( numberOfModes,
            sizeof ( int64_t )))) {
          oaLogError ( OA_LOG_CAMERA,
							"%s: Can't calloc space for trigger mode list", __func__ );
          ( *p_fc2DestroyContext )( pgeContext );
          FREE_DATA_STRUCTS;
          return 0;
        }
        mask16 = cameraInfo->modeMask;
        int64_t* modep = cameraInfo->triggerModes;
        int modeNumber = 0;
        while ( mask16 ) {
          if ( mask16 & 0x8000 ) {
            *modep++ = modeNumber;
          }
          modeNumber++;
          mask16 <<= 1;
        }
      }
    }

    if ( numberOfSources ) {
      triggerMode.source = cameraInfo->triggerGPIO;
      if (( ret = ( *p_fc2GetTriggerMode )( pgeContext, &triggerMode )) !=
          FC2_ERROR_OK ) {
        oaLogError ( OA_LOG_CAMERA, "%s: Can't get trigger mode for FC2 GUID",
						__func__ );
        ( *p_fc2DestroyContext )( pgeContext );
        if ( cameraInfo->triggerModes ) {
					free (( void* ) cameraInfo->triggerModes );
				}
        FREE_DATA_STRUCTS;
        return 0;
      }

      oaLogDebug ( OA_LOG_CAMERA, "%s: trigger %d:", __func__,
					cameraInfo->triggerGPIO );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   on/off   : %d", __func__,
					triggerMode.onOff ? 1 : 0 );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   polarity : %d", __func__,
					triggerMode.polarity ? 1 : 0 );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   mode     : %d", __func__,
					triggerMode.mode );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   param    : %d", __func__,
					triggerMode.parameter );

      cameraInfo->triggerEnabled = triggerMode.onOff;
      cameraInfo->triggerCurrentPolarity = triggerMode.polarity;
      cameraInfo->triggerCurrentMode = triggerMode.mode;

      if (( ret = ( *p_fc2GetTriggerDelayInfo )( pgeContext, &delayInfo )) !=
          FC2_ERROR_OK ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: Can't get trigger delay info for FC2 GUID", __func__ );
        ( *p_fc2DestroyContext )( pgeContext );
        if ( cameraInfo->triggerModes ) {
					free (( void* ) cameraInfo->triggerModes );
				}
        FREE_DATA_STRUCTS;
        return 0;
      }

      oaLogDebug ( OA_LOG_CAMERA, "%s: trigger delay info:", __func__ );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   present  : %d", __func__,
					delayInfo.present ? 1 : 0 );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   auto     : %d", __func__,
					delayInfo.autoSupported ? 1 : 0 );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   manual   : %d", __func__,
					delayInfo.manualSupported ? 1 : 0 );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   on/off   : %d", __func__,
					delayInfo.onOffSupported ? 1 : 0 );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   one push : %d", __func__,
					delayInfo.onePushSupported ? 1 : 0 );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   absolute : %d", __func__,
					delayInfo.absValSupported ? 1 : 0 );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   readout  : %d", __func__,
					delayInfo.readOutSupported ? 1 : 0 );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   min      : %d", __func__,
					delayInfo.min );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   max      : %d", __func__,
					delayInfo.max );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   min      : %f", __func__,
					delayInfo.absMin );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   max      : %f", __func__,
					delayInfo.absMax );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   units    : %s", __func__,
					delayInfo.pUnits );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   units    : %s", __func__,
					delayInfo.pUnitAbbr );

      if ( delayInfo.present ) {
        if (( ret = ( *p_fc2GetTriggerDelay )( pgeContext, &triggerDelay )) !=
            FC2_ERROR_OK ) {
          oaLogError ( OA_LOG_CAMERA,
							"%s: Can't get trigger delay for FC2 GUID", __func__ );
          ( *p_fc2DestroyContext )( pgeContext );
					if ( cameraInfo->triggerModes ) {
						free (( void* ) cameraInfo->triggerModes );
					}
          FREE_DATA_STRUCTS;
          return 0;
        }

        oaLogDebug ( OA_LOG_CAMERA, "%s: trigger delay:", __func__ );
        oaLogDebug ( OA_LOG_CAMERA, "%s:   present  : %d", __func__,
						triggerDelay.present ? 1 : 0 );
        oaLogDebug ( OA_LOG_CAMERA, "%s:   absolute : %d", __func__,
						triggerDelay.absControl ? 1 : 0 );
        oaLogDebug ( OA_LOG_CAMERA, "%s:   one push : %d", __func__,
						triggerDelay.onePush ? 1 : 0 );
        oaLogDebug ( OA_LOG_CAMERA, "%s:   on/off   : %d", __func__,
						triggerDelay.onOff ? 1 : 0 );
        oaLogDebug ( OA_LOG_CAMERA, "%s:   auto/man : %d", __func__,
						triggerDelay.autoManualMode ? 1 : 0 );
        oaLogDebug ( OA_LOG_CAMERA, "%s:   valueA   : %d", __func__,
						triggerDelay.valueA ? 1 : 0 );
        oaLogDebug ( OA_LOG_CAMERA, "%s:   absValue : %f", __func__,
						triggerDelay.absValue );

        cameraInfo->triggerDelayEnable = delayInfo.onOffSupported ? 1 : 0;
        if ( delayInfo.onOffSupported ) {
          camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_TRIGGER_DELAY_ENABLE ) =
              OA_CTRL_TYPE_BOOLEAN;
          commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_TRIGGER_DELAY_ENABLE ) = 0;
          commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_TRIGGER_DELAY_ENABLE ) = 1;
          commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_TRIGGER_DELAY_ENABLE ) = 1;
          commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_TRIGGER_DELAY_ENABLE ) = 0;
        }

        camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_TRIGGER_DELAY ) =
            OA_CTRL_TYPE_INT64;
        commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_TRIGGER_DELAY ) =
            delayInfo.min * 1000000;
        commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_TRIGGER_DELAY ) =
            delayInfo.max * 1000000;
        commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_TRIGGER_DELAY ) = 1;
        commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_TRIGGER_DELAY ) = 0;
      }
    }
  }

  // And now it's the turn of strobe mode

  // Multiple pins can be configured as strobe outputs.  The documentation
  // suggests that there are only three bits available for the strobe source,
  // so hopefully we can assume that there can be no more than 8 GPIO lines
  // available.

  for ( i = 0; i < 8; i++ ) {

    strobeInfo.source = i;
    if (( ret = ( *p_fc2GetStrobeInfo )( pgeContext, &strobeInfo )) !=
        FC2_ERROR_OK ) {
      // not an error if this isn't a strobe line
      if ( ret != FC2_ERROR_INVALID_PARAMETER ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: Can't get strobe mode info for FC2 GUID", __func__ );
        ( *p_fc2DestroyContext )( pgeContext );
				if ( cameraInfo->triggerModes ) {
					free (( void* ) cameraInfo->triggerModes );
				}
        FREE_DATA_STRUCTS;
        return 0;
      }
    }

    if ( strobeInfo.present ) {
      oaLogDebug ( OA_LOG_CAMERA, "%s: GPIO %d strobe mode:", __func__ , i );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   present  : %d", __func__ ,
					strobeInfo.present ? 1 : 0 );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   on/off   : %d", __func__ ,
					strobeInfo.onOffSupported ?
          1 : 0 );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   polarity : %d", __func__ ,
					strobeInfo.polaritySupported ?
          1 : 0 );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   min val  : %f", __func__ ,
					strobeInfo.minValue );
      oaLogDebug ( OA_LOG_CAMERA, "%s:   max val  : %f", __func__ ,
					strobeInfo.maxValue );
      if ( camera->features.flags & OA_CAM_FEATURE_STROBE_OUTPUT ) {
        oaLogWarning ( OA_LOG_CAMERA,
						"%s: More than one strobe output found", __func__ );
      }
      if ( !i ) {
        oaLogError ( OA_LOG_CAMERA,
						"%s: Strobe output may be the same as the trigger input",
						__func__ );
      }
 
      camera->features.flags |= OA_CAM_FEATURE_STROBE_OUTPUT;
      cameraInfo->strobeGPIO = i;

      cameraInfo->strobeEnable = strobeInfo.onOffSupported ? 1 : 0;
      if ( strobeInfo.onOffSupported ) {
        camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_STROBE_ENABLE ) =
            OA_CTRL_TYPE_BOOLEAN;
        commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_STROBE_ENABLE ) = 0;
        commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_STROBE_ENABLE ) = 1;
        commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_STROBE_ENABLE ) = 1;
        // on appears to be the case for the Blackfly.  I'll assume for all
        commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_STROBE_ENABLE ) = 1;
      }

      if ( strobeInfo.polaritySupported ) {
        camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_STROBE_POLARITY ) =
            OA_CTRL_TYPE_MENU;
        commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_STROBE_POLARITY ) = 0;
        commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_STROBE_POLARITY ) = 1;
        commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_STROBE_POLARITY ) = 1;
        // trailing edge appears to be the case for the Blackfly.  I'll
        // assume for all
        commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_STROBE_POLARITY ) = 0;
      }

      camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_STROBE_DELAY ) =
            OA_CTRL_TYPE_INT64;
      commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_STROBE_DELAY ) =
          strobeInfo.minValue * 1000000;
      commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_STROBE_DELAY ) =
          strobeInfo.maxValue * 1000000;
      commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_STROBE_DELAY ) = 1;
      commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_STROBE_DELAY ) = 0;

      camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_STROBE_DURATION ) =
            OA_CTRL_TYPE_INT64;
      commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_STROBE_DURATION ) =
          strobeInfo.minValue * 1000000;
      commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_STROBE_DURATION ) =
          strobeInfo.maxValue * 1000000;
      commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_STROBE_DURATION ) = 1;
      commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_STROBE_DURATION ) = 0;
    }
  }

  if ( camera->features.flags & OA_CAM_FEATURE_STROBE_OUTPUT ) {
    strobeControl.source = cameraInfo->strobeGPIO;
    if (( ret = ( *p_fc2GetStrobe )( pgeContext, &strobeControl )) !=
        FC2_ERROR_OK ) {
      oaLogError ( OA_LOG_CAMERA, "%s: Can't get strobe control for FC2 GUID",
					__func__ );
      ( *p_fc2DestroyContext )( pgeContext );
			if ( cameraInfo->triggerModes ) {
				free (( void* ) cameraInfo->triggerModes );
			}
      FREE_DATA_STRUCTS;
      return 0;
    }

    cameraInfo->strobeEnabled = strobeControl.onOff ? 1 : 0;
    cameraInfo->strobeCurrentPolarity = strobeControl.polarity ? 1 : 0;
    cameraInfo->strobeCurrentDelay = strobeControl.delay * 1000000;
    cameraInfo->strobeCurrentDuration = strobeControl.duration * 1000000;

    oaLogDebug ( OA_LOG_CAMERA, "%s: source %d:", __func__,
				cameraInfo->strobeGPIO );
    oaLogDebug ( OA_LOG_CAMERA, "%s:   on/off   : %d", __func__,
				strobeControl.onOff ? 1 : 0 );
    oaLogDebug ( OA_LOG_CAMERA, "%s:   polarity : %d", __func__,
				strobeControl.polarity ? 1 : 0 );
    oaLogDebug ( OA_LOG_CAMERA, "%s:   delay    : %f", __func__,
				strobeControl.delay );
    oaLogDebug ( OA_LOG_CAMERA, "%s:   duration : %f", __func__,
				strobeControl.duration );
  }

  // There are problems here if not all colour modes are supported in
  // all resolutions

  cameraInfo->currentVideoFormat = 0;
  cameraInfo->currentMode = 0;

  // It appears to be true for the Flea3 and Blackfly, so perhaps it's
  // true for others, that:
  //
  // Mode 0: standard image
  // Mode 1: 2x binning
  // Mode 2: 2x subsampling
  // Mode 4: looks messy :)
  // Mode 5: 4x binning
  // Mode 6: 4x binning for colour cameras, monochrome out
  // Mode 7: standard image, slower pixel clock, preferred for longer exposures
  //
  // For the time being I'll ignore all but 0, 1 and 5.

  numResolutions = 0;
  firstMode = 0; // by definition
  camera->features.flags |= OA_CAM_FEATURE_FIXED_FRAME_SIZES;
  for ( mode = FC2_MODE_0; mode < FC2_NUM_MODES; mode++ ) {

    // skip modes unsupported by code
    if ( mode != FC2_MODE_0 && mode != FC2_MODE_1 && mode != FC2_MODE_5 ) {
      continue;
    }

    if (( *p_fc2QueryGigEImagingMode )( pgeContext, mode, &supported ) !=
        FC2_ERROR_OK ) {
      oaLogError ( OA_LOG_CAMERA, "%s: Can't get mode info %d for FC2 GUID",
					__func__, i );
      ( *p_fc2DestroyContext )( pgeContext );
			if ( cameraInfo->triggerModes ) {
				free (( void* ) cameraInfo->triggerModes );
			}
      FREE_DATA_STRUCTS;
      return 0;
    }
    if ( supported ) {
      if (( *p_fc2SetGigEImagingMode )( pgeContext, mode ) != FC2_ERROR_OK ) {
        oaLogError ( OA_LOG_CAMERA, "%s: Can't set mode %d for FC2 GUID",
						__func__, mode );
        ( *p_fc2DestroyContext )( pgeContext );
				if ( cameraInfo->triggerModes ) {
					free (( void* ) cameraInfo->triggerModes );
				}
        FREE_DATA_STRUCTS;
        return 0;
      }
      if (( *p_fc2GetGigEImageSettingsInfo )( pgeContext, &imageInfo ) !=
          FC2_ERROR_OK ) {
        oaLogError ( OA_LOG_CAMERA, "%s: Can't get image info %d for FC2 GUID",
						__func__, i );
        ( *p_fc2DestroyContext )( pgeContext );
				if ( cameraInfo->triggerModes ) {
					free (( void* ) cameraInfo->triggerModes );
				}
        FREE_DATA_STRUCTS;
        return 0;
      }

      switch ( mode ) {
        case FC2_MODE_0:
          xbin = 1;
          break;
        case FC2_MODE_1:
          xbin = 2;
          break;
        case FC2_MODE_5:
          xbin = 4;
          break;
      }

      if ( xbin > 1 ) {
        if (( *p_fc2SetGigEImageBinningSettings )( pgeContext, xbin,
            xbin ) != FC2_ERROR_OK ) {
          // looks like this binning level isn't available in this mode
          continue;
        }
      }

      cameraInfo->availableBinModes |= ( 1 << ( xbin - 1 ));
      if ( imageInfo.imageHStepSize || imageInfo.imageVStepSize ) {
				camera->features.flags |= OA_CAM_FEATURE_ROI;
				camera->features.flags &= ~OA_CAM_FEATURE_FIXED_FRAME_SIZES;
      }

      found = 0;
      numResolutions = cameraInfo->frameSizes[xbin].numSizes;
      if ( numResolutions ) {
        for ( j = 0; j < numResolutions && !found; j++ ) {
          if ( cameraInfo->frameSizes[xbin].sizes[j].x ==
              imageInfo.maxWidth &&
              cameraInfo->frameSizes[xbin].sizes[j].y ==
              imageInfo.maxHeight ) {
            found = 1;
          }
        }
      }
      if ( !found ) {
        if (!( tmpPtr = realloc ( cameraInfo->frameSizes[ xbin ].sizes,
							( numResolutions + 1 ) * sizeof ( FRAMESIZE )))) {
          oaLogError ( OA_LOG_CAMERA, "%s: realloc for frame sizes failed",
							__func__ );
					( *p_fc2DestroyContext )( pgeContext );
					for ( j = 1; j <= OA_MAX_BINNING; j++ ) {
						if ( cameraInfo->frameSizes[ j ].numSizes ) {
							free (( void* ) cameraInfo->frameSizes[ j ].sizes );
							if ( cameraInfo->frameModes[ j ] ) {
								free (( void* ) cameraInfo->frameModes[ j ]);
							}
						}
					}
					if ( cameraInfo->triggerModes ) {
						free (( void* ) cameraInfo->triggerModes );
					}
					FREE_DATA_STRUCTS;
          return 0;
        }
				cameraInfo->frameSizes[ xbin ].sizes = tmpPtr;
        if (!( tmpPtr = realloc ( cameraInfo->frameModes[ xbin ],
            sizeof ( struct modeInfo ) * ( numResolutions + 1 )))) {
          oaLogError ( OA_LOG_CAMERA, "%s: realloc for frame modes failed",
							__func__ );
					( *p_fc2DestroyContext )( pgeContext );
					for ( j = 1; j <= OA_MAX_BINNING; j++ ) {
						if ( cameraInfo->frameSizes[ j ].numSizes ) {
							free (( void* ) cameraInfo->frameSizes[ j ].sizes );
							if ( cameraInfo->frameModes[ j ] ) {
								free (( void* ) cameraInfo->frameModes[ j ]);
							}
						}
					}
					if ( cameraInfo->triggerModes ) {
						free (( void* ) cameraInfo->triggerModes );
					}
					FREE_DATA_STRUCTS;
          return 0;
        }
				cameraInfo->frameModes[ xbin ] = ( struct modeInfo* ) tmpPtr;

        cameraInfo->frameSizes[xbin].sizes[ numResolutions ].x =
            imageInfo.maxWidth;
        cameraInfo->frameSizes[xbin].sizes[ numResolutions ].y =
            imageInfo.maxHeight;
        cameraInfo->frameModes[xbin][ numResolutions ].mode = mode;
        if ( imageInfo.maxWidth > cameraInfo->xSize || imageInfo.maxHeight >
            cameraInfo->ySize ) {
          cameraInfo->xSize = imageInfo.maxWidth;
          cameraInfo->ySize = imageInfo.maxHeight;
        }
        cameraInfo->frameSizes[xbin].numSizes++;
      }
    }
  }
  maxBinMode = numBinModes = 0;
  i = cameraInfo->availableBinModes;
  while ( i ) {
    maxBinMode++;
    if ( i & 1 ) {
      numBinModes++;
    }
    i >>= 1;
  }
  if ( numBinModes > 1 ) {
    camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_BINNING ) = OA_CTRL_TYPE_INT32;
    commonInfo->OA_CAM_CTRL_MIN( OA_CAM_CTRL_BINNING ) = 1;
    commonInfo->OA_CAM_CTRL_MAX( OA_CAM_CTRL_BINNING ) = maxBinMode;
    commonInfo->OA_CAM_CTRL_STEP( OA_CAM_CTRL_BINNING ) = 1; // arbitrary
    commonInfo->OA_CAM_CTRL_DEF( OA_CAM_CTRL_BINNING ) = 1;
  }

  cameraInfo->maxResolutionX = cameraInfo->xSize;
  cameraInfo->maxResolutionY = cameraInfo->ySize;
  cameraInfo->binMode = OA_BIN_MODE_NONE;
  cameraInfo->pixelFormats = imageInfo.pixelFormatBitField;

  // Put the camera into a known state
  // FIX ME -- probably should just handle whatever is already set?
  if (( *p_fc2SetGigEImagingMode )( pgeContext, firstMode ) != FC2_ERROR_OK ) {
    oaLogError ( OA_LOG_CAMERA, "%s: Can't set mode %d for FC2 GUID",
				__func__, i );
    ( *p_fc2DestroyContext )( pgeContext );
		for ( j = 1; j <= OA_MAX_BINNING; j++ ) {
			if ( cameraInfo->frameSizes[ j ].numSizes ) {
				free (( void* ) cameraInfo->frameSizes[ j ].sizes );
				if ( cameraInfo->frameModes[ j ] ) {
					free (( void* ) cameraInfo->frameModes[ j ]);
				}
			}
		}
    FREE_DATA_STRUCTS;
    return 0;
  }
  if (( *p_fc2GetGigEImageSettings )( pgeContext, &settings ) !=
      FC2_ERROR_OK ) {
    oaLogError ( OA_LOG_CAMERA, "%s: Can't get settings %d for FC2 GUID",
				__func__, i );
    ( *p_fc2DestroyContext )( pgeContext );
		for ( j = 1; j <= OA_MAX_BINNING; j++ ) {
			if ( cameraInfo->frameSizes[ j ].numSizes ) {
				free (( void* ) cameraInfo->frameSizes[ j ].sizes );
				if ( cameraInfo->frameModes[ j ] ) {
					free (( void* ) cameraInfo->frameModes[ j ]);
				}
			}
		}
		if ( cameraInfo->triggerModes ) {
			free (( void* ) cameraInfo->triggerModes );
		}
    FREE_DATA_STRUCTS;
    return 0;
  }
  if ( cameraInfo->pixelFormats & FC2_PIXEL_FORMAT_MONO8 ) {
    settings.pixelFormat = FC2_PIXEL_FORMAT_MONO8;
    cameraInfo->currentBytesPerPixel = 1;
  } else {
    // FIX ME
    oaLogWarning ( OA_LOG_CAMERA,
				"%s: Don't know what to set default camera format to", __func__ );
  }
  if (( *p_fc2SetGigEImageSettings )( pgeContext, &settings ) !=
      FC2_ERROR_OK ) {
    oaLogError ( OA_LOG_CAMERA, "%s: Can't set settings %d for FC2 GUID",
				__func__, i );
    ( *p_fc2DestroyContext )( pgeContext );
		for ( j = 1; j <= OA_MAX_BINNING; j++ ) {
			if ( cameraInfo->frameSizes[ j ].numSizes ) {
				free (( void* ) cameraInfo->frameSizes[ j ].sizes );
				if ( cameraInfo->frameModes[ j ] ) {
					free (( void* ) cameraInfo->frameModes[ j ]);
				}
			}
		}
		if ( cameraInfo->triggerModes ) {
			free (( void* ) cameraInfo->triggerModes );
		}
    FREE_DATA_STRUCTS;
    return 0;
  }
  if (( *p_fc2SetGigEImageBinningSettings )( pgeContext, 1, 1 ) !=
      FC2_ERROR_OK ) {
    oaLogError ( OA_LOG_CAMERA, "%s: Can't set binmode 1 for FC2 GUID",
				__func__ );
    ( *p_fc2DestroyContext )( pgeContext );
		for ( j = 1; j <= OA_MAX_BINNING; j++ ) {
			if ( cameraInfo->frameSizes[ j ].numSizes ) {
				free (( void* ) cameraInfo->frameSizes[ j ].sizes );
				if ( cameraInfo->frameModes[ j ] ) {
					free (( void* ) cameraInfo->frameModes[ j ]);
				}
			}
		}
		if ( cameraInfo->triggerModes ) {
			free (( void* ) cameraInfo->triggerModes );
		}
    FREE_DATA_STRUCTS;
    return 0;
  }

  cameraInfo->currentMode = firstMode;
  cameraInfo->currentVideoFormat = settings.pixelFormat;
  cameraInfo->currentFrameFormat = 0;

  // Endianness for 12- and 16-bit images can apparently be
  // forced to little-endian as follows:
  //
  //  const unsigned int k_imageDataFmtReg = 0x1048;
  //  unsigned int value = 0;
  //  error = m_pCamera->ReadRegister( k_imageDataFmtReg, &value );
  //  value &= ~(0x1 << 0);
  //  error = m_pCamera->WriteRegister( k_imageDataFmtReg, value );
  //
  // However, that's not much use if the camera doesn't support it, so
  // perhaps we just need to cope with whatever mode the camera claims to
  // be in

  if ( camInfo.iidcVer >= 132 ) {
    if (( *p_fc2ReadRegister )( pgeContext, FC2_REG_DATA_DEPTH,
        &dataFormat ) != FC2_ERROR_OK ) {
      oaLogError ( OA_LOG_CAMERA, "%s: Can't read FC2 register 0x%04x",
          __func__, FC2_REG_DATA_DEPTH );
      ( *p_fc2DestroyContext )( pgeContext );
			for ( j = 1; j <= OA_MAX_BINNING; j++ ) {
				if ( cameraInfo->frameSizes[ j ].numSizes ) {
					free (( void* ) cameraInfo->frameSizes[ j ].sizes );
					if ( cameraInfo->frameModes[ j ] ) {
						free (( void* ) cameraInfo->frameModes[ j ]);
					}
				}
			}
			if ( cameraInfo->triggerModes ) {
				free (( void* ) cameraInfo->triggerModes );
			}
      FREE_DATA_STRUCTS;
      return 0;
    }
    // FIX ME
    // This is allegedly the other way around, but only this way works
    // for me
    cameraInfo->bigEndian = (( dataFormat >> 16 ) & 0x80 ) ? 0 : 1;
  } else {
    if (( *p_fc2ReadRegister )( pgeContext, FC2_REG_IMAGE_DATA_FORMAT,
        &dataFormat ) != FC2_ERROR_OK ) {
      oaLogError ( OA_LOG_CAMERA, "%s: Can't read FC2 register 0x%04x",
          __func__, FC2_REG_IMAGE_DATA_FORMAT );
      ( *p_fc2DestroyContext )( pgeContext );
			for ( j = 1; j <= OA_MAX_BINNING; j++ ) {
				if ( cameraInfo->frameSizes[ j ].numSizes ) {
					free (( void* ) cameraInfo->frameSizes[ j ].sizes );
					if ( cameraInfo->frameModes[ j ] ) {
						free (( void* ) cameraInfo->frameModes[ j ]);
					}
				}
			}
			if ( cameraInfo->triggerModes ) {
				free (( void* ) cameraInfo->triggerModes );
			}
      FREE_DATA_STRUCTS;
      return 0;
    }
    if (( dataFormat & 0x80000000 ) == 0 ) {
      oaLogWarning ( OA_LOG_CAMERA,
					"%s: Image Data Format register unsupported", __func__ );
    }
    cameraInfo->bigEndian = ( dataFormat & 0xff ) ? 1 : 0;
  }

  cameraInfo->maxBytesPerPixel = 0;
  if (( imageInfo.pixelFormatBitField & FC2_PIXEL_FORMAT_MONO8 ) ==
      FC2_PIXEL_FORMAT_MONO8 ) {
    camera->frameFormats[ OA_PIX_FMT_GREY8 ] = 1;
    if ( cameraInfo->maxBytesPerPixel < 1 ) {
      cameraInfo->maxBytesPerPixel = 1;
    }
    if ( cameraInfo->currentVideoFormat == FC2_PIXEL_FORMAT_MONO8 ) {
      cameraInfo->currentFrameFormat = OA_PIX_FMT_GREY8;
    }
  }
  if (( imageInfo.pixelFormatBitField & FC2_PIXEL_FORMAT_411YUV8 ) ==
      FC2_PIXEL_FORMAT_411YUV8 ) {
    camera->frameFormats[ OA_PIX_FMT_YUV411 ] = 1;
    if ( cameraInfo->maxBytesPerPixel < 2 ) {
      cameraInfo->maxBytesPerPixel = 2;
    }
    if ( cameraInfo->currentVideoFormat == FC2_PIXEL_FORMAT_411YUV8 ) {
      cameraInfo->currentFrameFormat = OA_PIX_FMT_YUV411;
    }
  }
  if (( imageInfo.pixelFormatBitField & FC2_PIXEL_FORMAT_422YUV8 ) ==
      FC2_PIXEL_FORMAT_422YUV8 ) {
    camera->frameFormats[ OA_PIX_FMT_YUV422 ] = 1;
    if ( cameraInfo->maxBytesPerPixel < 2 ) {
      cameraInfo->maxBytesPerPixel = 2;
    }
    if ( cameraInfo->currentVideoFormat == FC2_PIXEL_FORMAT_422YUV8 ) {
      cameraInfo->currentFrameFormat = OA_PIX_FMT_YUV422;
    }
  }
  if (( imageInfo.pixelFormatBitField & FC2_PIXEL_FORMAT_444YUV8 ) ==
      FC2_PIXEL_FORMAT_444YUV8 ) {
    camera->frameFormats[ OA_PIX_FMT_YUV444 ] = 1;
    if ( cameraInfo->maxBytesPerPixel < 3 ) {
      cameraInfo->maxBytesPerPixel = 3;
    }
    if ( cameraInfo->currentVideoFormat == FC2_PIXEL_FORMAT_444YUV8 ) {
      cameraInfo->currentFrameFormat = OA_PIX_FMT_YUV444;
    }
  }
  if (( imageInfo.pixelFormatBitField & FC2_PIXEL_FORMAT_RGB8 ) ==
      FC2_PIXEL_FORMAT_RGB8 ) {
    camera->frameFormats[ OA_PIX_FMT_RGB24 ] = 1;
    if ( cameraInfo->maxBytesPerPixel < 3 ) {
      cameraInfo->maxBytesPerPixel = 3;
    }
    if ( cameraInfo->currentVideoFormat == FC2_PIXEL_FORMAT_RGB8 ) {
      cameraInfo->currentFrameFormat = OA_PIX_FMT_RGB24;
    }
  }
  if (( imageInfo.pixelFormatBitField & FC2_PIXEL_FORMAT_MONO16 ) ==
      FC2_PIXEL_FORMAT_MONO16 ) {
    format = cameraInfo->bigEndian ? OA_PIX_FMT_GREY16BE : OA_PIX_FMT_GREY16LE;
    camera->frameFormats[ format ] = 1;
    if ( cameraInfo->maxBytesPerPixel < 2 ) {
      cameraInfo->maxBytesPerPixel = 2;
    }
    if ( cameraInfo->currentVideoFormat == FC2_PIXEL_FORMAT_MONO16 ) {
      cameraInfo->currentFrameFormat = format;
    }
  }
  if (( imageInfo.pixelFormatBitField & FC2_PIXEL_FORMAT_RGB16 ) ==
      FC2_PIXEL_FORMAT_RGB16 ) {
    format = cameraInfo->bigEndian ? OA_PIX_FMT_RGB48BE : OA_PIX_FMT_RGB48LE;
    camera->frameFormats[ format ] = 1;
    if ( cameraInfo->maxBytesPerPixel < 6 ) {
      cameraInfo->maxBytesPerPixel = 6;
    }
    if ( cameraInfo->currentVideoFormat == FC2_PIXEL_FORMAT_RGB16 ) {
      cameraInfo->currentFrameFormat = format;
    }
  }
  if (( imageInfo.pixelFormatBitField & FC2_PIXEL_FORMAT_RAW8 ) ==
      FC2_PIXEL_FORMAT_RAW8 ) {
    switch ( cameraInfo->cfaPattern ) {
      case OA_DEMOSAIC_RGGB:
        format = OA_PIX_FMT_RGGB8;
        break;
      case OA_DEMOSAIC_BGGR:
        format = OA_PIX_FMT_BGGR8;
        break;
      case OA_DEMOSAIC_GRBG:
        format = OA_PIX_FMT_GRBG8;
        break;
      case OA_DEMOSAIC_GBRG:
        format = OA_PIX_FMT_GBRG8;
        break;
      default: // a random selection really.  Just ensures initialisation.
        format = OA_PIX_FMT_RGGB8;
        oaLogError ( OA_LOG_CAMERA,
						"%s: Unrecognised CFA pattern.  Should not happen", __func__ );
        break;
    }
    camera->frameFormats[ format ] = 1;
		camera->features.flags |= OA_CAM_FEATURE_RAW_MODE;
    if ( cameraInfo->maxBytesPerPixel < 1 ) {
      cameraInfo->maxBytesPerPixel = 1;
    }
    if ( cameraInfo->currentVideoFormat == FC2_PIXEL_FORMAT_RAW8 ) {
      cameraInfo->currentFrameFormat = format;
    }
  }
  if (( imageInfo.pixelFormatBitField & FC2_PIXEL_FORMAT_RAW16 ) ==
      FC2_PIXEL_FORMAT_RAW16 ) {
    switch ( cameraInfo->cfaPattern ) {
      case OA_DEMOSAIC_RGGB:
        format = cameraInfo->bigEndian ? OA_PIX_FMT_RGGB16BE :
            OA_PIX_FMT_RGGB16LE;
        break;
      case OA_DEMOSAIC_BGGR:
        format = cameraInfo->bigEndian ? OA_PIX_FMT_BGGR16BE :
            OA_PIX_FMT_BGGR16LE;
        break;
      case OA_DEMOSAIC_GRBG:
        format = cameraInfo->bigEndian ? OA_PIX_FMT_GRBG16BE :
            OA_PIX_FMT_GRBG16LE;
        break;
      case OA_DEMOSAIC_GBRG:
        format = cameraInfo->bigEndian ? OA_PIX_FMT_GBRG16BE :
            OA_PIX_FMT_GBRG16LE;
        break;
      default: // a random selection really.  Just ensures initialisation.
        format = OA_PIX_FMT_RGGB16LE;
        oaLogError ( OA_LOG_CAMERA,
						"%s: Unrecognised CFA pattern.  Should not happen", __func__ );
        break;
    }
    camera->frameFormats[ format ] = 1;
		camera->features.flags |= OA_CAM_FEATURE_RAW_MODE;
    if ( cameraInfo->maxBytesPerPixel < 2 ) {
      cameraInfo->maxBytesPerPixel = 2;
    }
    if ( cameraInfo->currentVideoFormat == FC2_PIXEL_FORMAT_RAW16 ) {
      cameraInfo->currentFrameFormat = format;
    }
  }
  if (( imageInfo.pixelFormatBitField & FC2_PIXEL_FORMAT_MONO12 ) ==
      FC2_PIXEL_FORMAT_MONO12 ) {
    camera->frameFormats[ OA_PIX_FMT_GREY12P ] = 1;
    if ( cameraInfo->maxBytesPerPixel < 2 ) {
      cameraInfo->maxBytesPerPixel = 2;
    }
  }
  if (( imageInfo.pixelFormatBitField & FC2_PIXEL_FORMAT_RAW12 ) ==
      FC2_PIXEL_FORMAT_RAW12 ) {
    switch ( cameraInfo->cfaPattern ) {
      case OA_DEMOSAIC_RGGB:
        format = OA_PIX_FMT_RGGB12;
        break;
      case OA_DEMOSAIC_BGGR:
        format = OA_PIX_FMT_BGGR12;
        break;
      case OA_DEMOSAIC_GRBG:
        format = OA_PIX_FMT_GRBG12;
        break;
      case OA_DEMOSAIC_GBRG:
        format = OA_PIX_FMT_GBRG12;
        break;
      default: // a random selection really.  Just ensures initialisation.
        format = OA_PIX_FMT_RGGB12;
        oaLogError ( OA_LOG_CAMERA,
						"%s: Unrecognised CFA pattern.  Should not happen", __func__ );
        break;
    }
    camera->frameFormats[ format ] = 1;
		camera->features.flags |= OA_CAM_FEATURE_RAW_MODE;
    if ( cameraInfo->maxBytesPerPixel < 2 ) {
      cameraInfo->maxBytesPerPixel = 2;
    }
    if ( cameraInfo->currentVideoFormat == FC2_PIXEL_FORMAT_RAW12 ) {
      cameraInfo->currentFrameFormat = format;
    }
  }
  if (( imageInfo.pixelFormatBitField & FC2_PIXEL_FORMAT_BGR ) ==
      FC2_PIXEL_FORMAT_BGR ) {
    camera->frameFormats[ OA_PIX_FMT_BGR24 ] = 1;
    if ( cameraInfo->maxBytesPerPixel < 3 ) {
      cameraInfo->maxBytesPerPixel = 3;
    }
    if ( cameraInfo->currentVideoFormat == FC2_PIXEL_FORMAT_BGR ) {
      cameraInfo->currentFrameFormat = OA_PIX_FMT_BGR24;
    }
  }
  if (( imageInfo.pixelFormatBitField & FC2_PIXEL_FORMAT_BGR16 ) ==
      FC2_PIXEL_FORMAT_BGR16 ) {
    format = cameraInfo->bigEndian ? OA_PIX_FMT_BGR48BE : OA_PIX_FMT_BGR48LE;
    camera->frameFormats[ format ] = 1;
    if ( cameraInfo->maxBytesPerPixel < 6 ) {
      cameraInfo->maxBytesPerPixel = 6;
    }
    if ( cameraInfo->currentVideoFormat == FC2_PIXEL_FORMAT_BGR16 ) {
      cameraInfo->currentFrameFormat = format;
    }
  }

  if ( !cameraInfo->maxBytesPerPixel ) {
    oaLogError ( OA_LOG_CAMERA, "%s: Unsupported pixel formats exist: 0x%04x",
        __func__, imageInfo.pixelFormatBitField );
  }

  if (( *p_fc2GetEmbeddedImageInfo )( pgeContext, &embeddedInfo ) !=
      FC2_ERROR_OK ) {
		oaLogError ( OA_LOG_CAMERA, "%s: fc2GetEmbeddedImageInfo failed",
				__func__ );
    ( *p_fc2DestroyContext )( pgeContext );
		for ( j = 1; j <= OA_MAX_BINNING; j++ ) {
			if ( cameraInfo->frameSizes[ j ].numSizes ) {
				free (( void* ) cameraInfo->frameSizes[ j ].sizes );
				if ( cameraInfo->frameModes[ j ] ) {
					free (( void* ) cameraInfo->frameModes[ j ]);
				}
			}
		}
		if ( cameraInfo->triggerModes ) {
			free (( void* ) cameraInfo->triggerModes );
		}
    FREE_DATA_STRUCTS;
    return 0;
	}
	if ( embeddedInfo.frameCounter.available ) {
		cameraInfo->haveFrameCounter = 1;
		if ( !embeddedInfo.frameCounter.onOff ) {
			embeddedInfo.frameCounter.onOff = 1;
			if (( *p_fc2SetEmbeddedImageInfo )( pgeContext, &embeddedInfo ) !=
					FC2_ERROR_OK ) {
				oaLogError ( OA_LOG_CAMERA, "%s: fc2SetEmbeddedImageInfo failed",
						__func__ );
				( *p_fc2DestroyContext )( pgeContext );
				for ( j = 1; j <= OA_MAX_BINNING; j++ ) {
					if ( cameraInfo->frameSizes[ j ].numSizes ) {
						free (( void* ) cameraInfo->frameSizes[ j ].sizes );
						if ( cameraInfo->frameModes[ j ] ) {
							free (( void* ) cameraInfo->frameModes[ j ]);
						}
					}
				}
				if ( cameraInfo->triggerModes ) {
					free (( void* ) cameraInfo->triggerModes );
				}
				FREE_DATA_STRUCTS;
				return 0;
			}
		}
	}

  camera->OA_CAM_CTRL_TYPE( OA_CAM_CTRL_FRAME_FORMAT ) = OA_CTRL_TYPE_DISCRETE;

  // The largest buffer size we should need

  cameraInfo->buffers = 0;
  cameraInfo->imageBufferLength = cameraInfo->maxResolutionX *
      cameraInfo->maxResolutionY * cameraInfo->maxBytesPerPixel;
  cameraInfo->buffers = calloc ( OA_CAM_BUFFERS, sizeof ( frameBuffer ));
  cameraInfo->metadataBuffers = calloc ( OA_CAM_BUFFERS,
			sizeof ( FRAME_METADATA ));
  for ( i = 0; i < OA_CAM_BUFFERS; i++ ) {
    void* m = malloc ( cameraInfo->imageBufferLength );
    if ( m ) {
      cameraInfo->buffers[i].start = m;
      cameraInfo->configuredBuffers++;
    } else {
      oaLogError ( OA_LOG_CAMERA, "%s: malloc of camera buffers failed",
					__func__ );
      if ( i ) {
        for ( j = 0; j < i; j++ ) {
          free (( void* ) cameraInfo->buffers[j].start );
        }
      }
			free (( void* ) cameraInfo->metadataBuffers );
			free (( void* ) cameraInfo->buffers );
      ( *p_fc2DestroyContext )( pgeContext );
			for ( j = 1; j <= OA_MAX_BINNING; j++ ) {
				if ( cameraInfo->frameSizes[ j ].numSizes ) {
					free (( void* ) cameraInfo->frameSizes[ j ].sizes );
					if ( cameraInfo->frameModes[ j ] ) {
						free (( void* ) cameraInfo->frameModes[ j ]);
					}
				}
			}
			if ( cameraInfo->triggerModes ) {
				free (( void* ) cameraInfo->triggerModes );
			}
      free (( void* ) cameraInfo->metadataBuffers );
      FREE_DATA_STRUCTS;
      return 0;
    }
  }

  cameraInfo->stopControllerThread = cameraInfo->stopCallbackThread = 0;
  cameraInfo->commandQueue = oaDLListCreate();
  cameraInfo->callbackQueue = oaDLListCreate();
  cameraInfo->nextBuffer = 0;
  cameraInfo->configuredBuffers = OA_CAM_BUFFERS;
  cameraInfo->buffersFree = OA_CAM_BUFFERS;

  if ( pthread_create ( &( cameraInfo->controllerThread ), 0,
      oacamFC2controller, ( void* ) camera )) {
    ( *p_fc2DestroyContext )( pgeContext );
		for ( j = 1; j <= OA_MAX_BINNING; j++ ) {
			if ( cameraInfo->frameSizes[ j ].numSizes ) {
				free (( void* ) cameraInfo->frameSizes[ j ].sizes );
				if ( cameraInfo->frameModes[ j ] ) {
					free (( void* ) cameraInfo->frameModes[ j ]);
				}
			}
		}
    for ( j = 0; j < OA_CAM_BUFFERS; j++ ) {
      free (( void* ) cameraInfo->buffers[j].start );
    }
    free (( void* ) cameraInfo->metadataBuffers );
		free (( void* ) cameraInfo->buffers );
		if ( cameraInfo->triggerModes ) {
			free (( void* ) cameraInfo->triggerModes );
		}
    oaDLListDelete ( cameraInfo->commandQueue, 0 );
    oaDLListDelete ( cameraInfo->callbackQueue, 0 );
    FREE_DATA_STRUCTS;
    return 0;
  }
  if ( pthread_create ( &( cameraInfo->callbackThread ), 0,
      oacamFC2callbackHandler, ( void* ) camera )) {

    void* dummy;
    cameraInfo->stopControllerThread = 1;
    pthread_cond_broadcast ( &cameraInfo->commandQueued );
    pthread_join ( cameraInfo->controllerThread, &dummy );
    ( *p_fc2DestroyContext )( pgeContext );
		for ( j = 1; j <= OA_MAX_BINNING; j++ ) {
			if ( cameraInfo->frameSizes[ j ].numSizes ) {
				free (( void* ) cameraInfo->frameSizes[ j ].sizes );
				if ( cameraInfo->frameModes[ j ] ) {
					free (( void* ) cameraInfo->frameModes[ j ]);
				}
			}
		}
    for ( j = 0; j < OA_CAM_BUFFERS; j++ ) {
      free (( void* ) cameraInfo->buffers[j].start );
    }
    free (( void* ) cameraInfo->metadataBuffers );
		free (( void* ) cameraInfo->buffers );
		if ( cameraInfo->triggerModes ) {
			free (( void* ) cameraInfo->triggerModes );
		}
    oaDLListDelete ( cameraInfo->commandQueue, 0 );
    oaDLListDelete ( cameraInfo->callbackQueue, 0 );
    FREE_DATA_STRUCTS;
    return 0;
  }

  cameraInfo->pgeContext = pgeContext;
  cameraInfo->initialised = 1;
  return camera;
}


static void
_FC2InitFunctionPointers ( oaCamera* camera )
{
  camera->funcs.initCamera = oaFC2InitCamera;
  camera->funcs.closeCamera = oaFC2CloseCamera;

  camera->funcs.testControl = oaFC2CameraTestControl;
  camera->funcs.getControlRange = oaFC2CameraGetControlRange;
  camera->funcs.getControlDiscreteSet = oaFC2CameraGetControlDiscreteSet;

  camera->funcs.testROISize = oaFC2CameraTestROISize;

  camera->funcs.hasAuto = oacamHasAuto;
  // camera->funcs.isAuto = _isAuto;

  camera->funcs.enumerateFrameSizes = oaFC2CameraGetFrameSizes;
  camera->funcs.getFramePixelFormat = oaFC2CameraGetFramePixelFormat;

  camera->funcs.enumerateFrameRates = oaFC2CameraGetFrameRates;

  camera->funcs.getMenuString = oaFC2CameraGetMenuString;
}


int
oaFC2CloseCamera ( oaCamera* camera )
{
  void*		dummy;
  FC2_STATE*	cameraInfo;
	int			j;

  if ( camera ) {

    cameraInfo = camera->_private;

    cameraInfo->stopControllerThread = 1;
    pthread_cond_broadcast ( &cameraInfo->commandQueued );
    pthread_join ( cameraInfo->controllerThread, &dummy );
  
    cameraInfo->stopCallbackThread = 1;
    pthread_cond_broadcast ( &cameraInfo->callbackQueued );
    pthread_join ( cameraInfo->callbackThread, &dummy );

    ( *p_fc2DestroyContext )( cameraInfo->pgeContext );

    for ( j = 0; j < OA_CAM_BUFFERS; j++ ) {
      free (( void* ) cameraInfo->buffers[j].start );
    }

    if ( cameraInfo->frameRates.numRates ) {
     free (( void* ) cameraInfo->frameRates.rates );
    }

		for ( j = 1; j <= OA_MAX_BINNING; j++ ) {
			if ( cameraInfo->frameSizes[ j ].numSizes ) {
				free (( void* ) cameraInfo->frameSizes[ j ].sizes );
				if ( cameraInfo->frameModes[ j ] ) {
					free (( void* ) cameraInfo->frameModes[ j ]);
				}
			}
		}

    oaDLListDelete ( cameraInfo->commandQueue, 1 );
    oaDLListDelete ( cameraInfo->callbackQueue, 0 );

    free (( void* ) cameraInfo->metadataBuffers );
		free (( void* ) cameraInfo->buffers );
		if ( cameraInfo->triggerModes ) {
			free (( void* ) cameraInfo->triggerModes );
		}
    free (( void* ) camera->_common );
    free (( void* ) cameraInfo );
    free (( void* ) camera );

  } else {
    return -OA_ERR_INVALID_CAMERA;
  }
  return OA_ERR_NONE;
}
