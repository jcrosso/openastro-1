/*****************************************************************************
 *
 * mainWindow.cc -- the main controlling window class
 *
 * Copyright 2013,2014,2015,2016,2017,2018,2019,2020,2021
 *     James Fidell (james@openastroproject.org)
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

#include <QtGui>

extern "C" {
#include <unistd.h>
#include <pwd.h>

#include <openastro/filterwheel.h>
#include <openastro/demosaic.h>
}

#include "commonState.h"
#include "commonConfig.h"
#include "targets.h"

#include "mainWindow.h"
#include "version.h"
#include "configuration.h"
#include "cameraControls.h"
#include "controlsWidget.h"
#include "settingsWidget.h"
#include "state.h"

CONFIG					config;
STATE						state;

#ifdef OACAPTURE
static const char* styleGroupBoxBorders =
  "QGroupBox { "
    "margin-top: 1ex;"
    "border: 2px groove grey;"
//  "border-radius: 5px;"
  "} "
  "QGroupBox::title { "
    "subcontrol-origin: margin;"
    "subcontrol-position: left top;"
    "left: 1em;"
  "}";
#endif


MainWindow::MainWindow ( QString configFile )
{
#ifdef OACAPTURE
  QString qtVer;
  unsigned int qtMajorVersion;
  int i;
  bool ok;
#endif

	commonState.localState = &state;
  userConfigFile = configFile;
  cameraSignalMapper = filterWheelSignalMapper = 0;
#ifdef OACAPTURE
  timerSignalMapper = 0;
  timerStatus = 0;
#endif
  wheelStatus = 0;
  advancedFilterWheelSignalMapper = 0;
  rescanCam = disconnectCam = 0;
  rescanWheel = disconnectWheel = warmResetWheel = coldResetWheel = 0;
#ifdef OACAPTURE
  rescanTimer = disconnectTimerDevice = resetTimerDevice = 0;
#endif
  connectedCameras = cameraMenuCreated = 0;
  connectedFilterWheels = filterWheelMenuCreated = 0;
#ifdef OACAPTURE
  connectedTimers = timerMenuCreated = 0;
#endif
  doingQuit = 0;
  cameraDevs = 0;
  filterWheelDevs = 0;
#ifdef OACAPTURE
  state.histogramOn = 0;
  state.histogramWidget = 0;
  state.needGroupBoxBorders = 0;
#endif
#ifdef OALIVE
	state.histogramCCSignalConnected = 0;
	state.histogramProcessingSignalConnected = 0;
#endif
  commonState.gpsValid = 0;
  commonState.cameraTempValid = 0;
  commonState.binningValid = 0;

#ifdef OACAPTURE
  // The gtk+ style doesn't enable group box borders by default, which makes
  // the display look confusing.
  //
  // Same thing with Qt5, so work out the version and add them if required

  qtVer = qVersion();
  if (( i = qtVer.indexOf ( '.' )) >= 0 ) {
    qtVer.truncate ( i );
  }
  qtMajorVersion = qtVer.toInt( &ok );

  QString currentStyle = QApplication::style()->objectName();
  if ( currentStyle.toLower() == "gtk+" || ( ok && qtMajorVersion > 4 )) {
    state.needGroupBoxBorders = 1;
    this->setStyleSheet ( styleGroupBoxBorders );
  }
#endif

  readConfig ( configFile );
  createStatusBar();
  createMenus();
  setWindowTitle( APPLICATION_NAME " " VERSION_STR );

  state.mainWindow = this;
#ifdef OACAPTURE
  state.controlWidget = 0;
#else
  state.cameraControls = 0;
#endif
  commonState.camera = new Camera;
  commonState.filterWheel = new FilterWheel ( &trampolines );
#ifdef OACAPTURE
  commonState.timer = new Timer;
  oldHistogramState = -1;
#endif
  state.lastRecordedFile = "";
  updateTemperatureLabel = 0;
  commonState.captureIndex = 0;
  state.settingsWidget = 0;
  state.advancedSettings = 0;
  colourDialog = 0;

  // need to do this to prevent access attempts before creation
#ifdef OACAPTURE
  previewWidget = 0;
#else
  viewWidget = 0;
#endif

  createControlWidgets();
#ifdef OACAPTURE
  createPreviewWindow();
#else
  createViewWindow();
	state.cameraControls->connectHistogramSignal();
	state.processingControls->connectHistogramSignal();
	state.controlsWidget->connectSignals();
#endif

#ifdef OACAPTURE
  connect ( state.previewWidget, SIGNAL( updateFrameCount ( unsigned int )),
      this, SLOT ( setCapturedFrames ( unsigned int )));
  connect ( state.previewWidget, SIGNAL( updateActualFrameRate (
      unsigned int )), this, SLOT ( setActualFrameRate ( unsigned int )));
  connect ( state.previewWidget, SIGNAL( updateProgress ( unsigned int )),
      this, SLOT ( setProgress ( unsigned int )));
  connect ( state.previewWidget, SIGNAL( stopRecording ( void )),
      state.captureWidget, SLOT ( doStopRecording ( void )));
  connect ( state.captureWidget, SIGNAL( writeStatusMessage ( QString )),
      this, SLOT ( showStatusMessage ( QString )));
  connect ( state.previewWidget, SIGNAL( frameWriteFailed ( void )),
      this, SLOT ( frameWriteFailedPopup ( void )));
#else
  connect ( state.viewWidget, SIGNAL( updateStackedFrameCount ( void )),
      this, SLOT ( setStackedFrames ( void )));
  connect ( state.processingControls, SIGNAL( redrawImage ( void )),
			state.viewWidget, SLOT ( redrawImage ( void )));

  // FIX ME -- make these work?
  // connect ( state.viewWidget, SIGNAL( updateDroppedFrames ( void )),
  //     this, SLOT ( setDroppedFrames ( void )));
  // connect ( state.captureWidget, SIGNAL( writeStatusMessage ( QString )),
  //     this, SLOT ( showStatusMessage ( QString )));
  // connect ( state.viewWidget, SIGNAL( frameWriteFailed ( void )),
  //     this, SLOT ( frameWriteFailedPopup ( void )));
#endif

  // update filters for matching filter wheels from config
  commonState.filterWheel->updateAllSearchFilters();

  char d[ PATH_MAX ];
#if HAVE_QT4
  commonState.currentDirectory = QString::fromAscii ( getcwd ( d, PATH_MAX ));
#else
  commonState.currentDirectory = QString::fromLatin1 ( getcwd ( d, PATH_MAX ));
#endif

  if ( connectedCameras == 1 && generalConf.connectSoleCamera ) {
    connectCamera ( 0 );
  }
  focusaid->setChecked ( config.showFocusAid );

	// Create timers for updating temperature, dropped frames etc.
	temperatureTimer = new QTimer ( this );
	droppedFrameTimer = new QTimer ( this );
	batteryLevelTimer = new QTimer ( this );
	timeRemainingTimer = new QTimer ( this );
	connect ( temperatureTimer, SIGNAL( timeout()), this,
			SLOT( setTemperature ( void )));
	connect ( droppedFrameTimer, SIGNAL( timeout()), this,
			SLOT ( setDroppedFrames ( void )));
	connect ( batteryLevelTimer, SIGNAL( timeout()), state.cameraControls,
      SLOT ( setBatteryLevel ( void )));
	connect ( timeRemainingTimer, SIGNAL( timeout()), this,
			SLOT ( setTimeRemaining ( void )));

	temperatureTimer->start ( 5000 );
	batteryLevelTimer->start ( 60000 );
	timeRemainingTimer->start ( 1000 );
	droppedFrameTimer->start ( 5000 );

	waitSpinner = new WaitingSpinnerWidget ( this, true, false );
	waitSpinner->setColor ( QColor ( 200, 200, 200 ));
	connect ( state.viewWidget, SIGNAL( enableSpinner ( int )), this,
			SLOT ( showSpinner ( int )));
}


MainWindow::~MainWindow()
{
  // FIX ME -- delete cameras[], filterWheels[]

#ifdef OACAPTURE
  state.histogramOn = 0;
#endif

	temperatureTimer->stop();
	//droppedFrameTimer->stop();
	//autoControlsTimer->stop();
	batteryLevelTimer->stop();
	timeRemainingTimer->stop();

  delete about;
#ifdef OACAPTURE
  delete histogram;
  delete autorun;
#endif
  delete demosaic;
  delete fits;
  delete filters;
  delete profiles;
  delete capture;
  delete general;
  delete focusaid;
  delete reticle;
  if ( cameraSignalMapper ) {
    delete cameraSignalMapper;
  }
  if ( filterWheelSignalMapper ) {
    delete filterWheelSignalMapper;
  }
  if ( advancedFilterWheelSignalMapper ) {
    delete advancedFilterWheelSignalMapper;
  }
#ifdef OACAPTURE
  if ( timerSignalMapper ) {
    delete timerSignalMapper;
  }
#endif
  delete exit;
  if ( rescanCam ) {
    delete rescanCam;
  }
  if ( disconnectCam ) {
    delete disconnectCam;
  }
  if ( rescanWheel ) {
    delete rescanWheel;
  }
  if ( disconnectWheel ) {
    delete disconnectWheel;
  }
  if ( warmResetWheel ) {
    delete warmResetWheel;
  }
  if ( coldResetWheel ) {
    delete coldResetWheel;
  }
#ifdef OACAPTURE
  if ( rescanTimer ) {
    delete rescanTimer;
  }
  if ( disconnectTimerDevice ) {
    delete disconnectTimerDevice;
  }
  if ( resetTimerDevice ) {
    delete resetTimerDevice;
  }
  delete saveConfig;
  delete loadConfig;
  delete capturedValue;
  delete capturedLabel;
#endif
  if ( commonState.camera ) {
    delete commonState.camera;
  }
  if ( commonState.filterWheel ) {
    delete commonState.filterWheel;
  }
#ifdef OACAPTURE
  if ( commonState.timer ) {
    delete commonState.timer;
  }
  if ( state.histogramWidget ) {
    delete state.histogramWidget;
  }
#endif
	delete temperatureTimer;
	//delete droppedFramesTimer;
	delete batteryLevelTimer;
	//delete autoControlsTimer;
	delete timeRemainingTimer;

	delete waitSpinner;
}


void
MainWindow::readConfig ( QString configFile )
{
  QSettings*  settings;
  const char* defaultDir = "";

#if USE_HOME_DEFAULT
  struct passwd*	pwd;

  pwd = getpwuid ( getuid());
  if ( pwd ) {
    defaultDir = pwd->pw_dir;
  }
#endif

  if ( configFile != "" ) {
    settings = new QSettings ( configFile, QSettings::IniFormat );
  } else {
#if defined(__APPLE__) && defined(__MACH__) && TARGET_OS_MAC == 1
    QSettings* iniSettings = new QSettings ( QSettings::IniFormat,
        QSettings::UserScope, ORGANISATION_NAME_SETTINGS, APPLICATION_NAME );
    settings = iniSettings;

    // The intention here is to allow a new ini-format file to override an
    // old plist file, but to use the plist file if no other exists
    if ( iniSettings->value ( "saveSettings", -1 ).toInt() == -1 ) {
      QSettings* plistSettings = new QSettings ( ORGANISATION_NAME_SETTINGS,
          APPLICATION_NAME );
      if ( plistSettings->value ( "saveSettings", -1 ).toInt() != -1 ) {
        delete iniSettings;
        settings = plistSettings;
      } else {
        delete plistSettings;
      }
    }
#else
    settings = new QSettings ( ORGANISATION_NAME_SETTINGS, APPLICATION_NAME );
#endif
  }

  // -1 means we don't have a config file.  We change it to 1 later in the
  // function
  generalConf.saveSettings = settings->value ( "saveSettings", -1 ).toInt();

  if ( !generalConf.saveSettings ) {

    generalConf.tempsInC = 1;
    generalConf.reticleStyle = 1;
#ifdef OACAPTURE
    config.displayFPS = 15;

    config.showHistogram = 0;
    config.autoAlign = 0;
    config.cutout = 0;
    config.darkFrame = 0;
    config.flipX = 0;
    config.flipY = 0;
    config.demosaic = 0;
#endif
    config.showReticle = 0;
    config.showFocusAid = 0;
#ifdef OALIVE
    config.showSpinner = 1;
#endif

    commonConfig.binning2x2 = 0;
    commonConfig.colourise = 0;

    commonConfig.useROI = 0;
    commonConfig.imageSizeX = 0;
    commonConfig.imageSizeY = 0;

    config.zoomButton1Option = 0;
    config.zoomButton2Option = 1;
    config.zoomButton3Option = 3;
    config.zoomValue = 100;

    cameraConf.CONTROL_VALUE( OA_CAM_CTRL_GAIN ) = 50;
    cameraConf.CONTROL_VALUE( OA_CAM_CTRL_EXPOSURE_UNSCALED ) = 10;
    cameraConf.CONTROL_VALUE( OA_CAM_CTRL_EXPOSURE_ABSOLUTE ) = 100;
    cameraConf.CONTROL_VALUE( OA_CAM_CTRL_GAMMA ) = -1;
    cameraConf.CONTROL_VALUE( OA_CAM_CTRL_BRIGHTNESS ) = -1;
    config.exposureMenuOption = 3;
    config.frameRateNumerator = 0;
    config.frameRateDenominator = 1;
    config.selectableControl[0] = OA_CAM_CTRL_GAMMA;
    config.selectableControl[1] = OA_CAM_CTRL_BRIGHTNESS;
    config.intervalMenuOption = 1;  // msec

    commonConfig.profileOption = 0;
    commonConfig.filterOption = 0;
    commonConfig.fileTypeOption = 1;
#ifdef OACAPTURE
    config.limitEnabled = 0;
    config.framesLimitValue = 0;
    config.secondsLimitValue = 0;
    config.limitType = 0;
    config.fileNameTemplate = QString ( "oaCapture-%DATE-%TIME" );
#else
    config.frameFileNameTemplate = QString ( "oalive-%INDEX" );
    config.processedFileNameTemplate = QString ( "oalive-processsed-%INDEX" );
    config.saveEachFrame = 0;
    config.saveProcessedImage = 0;
		config.stackKappa = 2.0;
		config.maxFramesToStack = 20;
#endif
    config.captureDirectory = QString ( defaultDir );

#ifdef OACAPTURE
    config.autorunCount = 0;
    config.autorunDelay = 0;
#endif
    config.saveCaptureSettings = 1;
#ifdef OACAPTURE
#if defined(__APPLE__) && defined(__MACH__) && TARGET_OS_MAC == 1
    config.windowsCompatibleAVI = 1;
#else
    config.windowsCompatibleAVI = 0;
#endif
    config.useUtVideo = 0;
#endif
    config.indexDigits = 6;

#ifdef OACAPTURE
    config.preview = 1;
    config.nightMode = 0;

    config.splitHistogram = 0;
    config.histogramOnTop = 1;

    config.demosaicPreview = 0;
    config.demosaicOutput = 0;
#endif
    demosaicConf.cfaPattern = OA_DEMOSAIC_AUTO;
    demosaicConf.demosaicMethod = OA_DEMOSAIC_BILINEAR;
    demosaicConf.monoIsRawColour = 0;

    profileConf.numProfiles = 0;
    filterConf.numFilters = 0;

    filterConf.promptForFilterChange = 0;
    filterConf.interFilterDelay = 0;

    config.currentColouriseColour.setRgb ( 255, 255, 255 );
    config.numCustomColours = 0;

    fitsConf.observer = "";
    fitsConf.instrument = "";
    fitsConf.object = "";
    fitsConf.comment = "";
    fitsConf.telescope = "";
    fitsConf.focalLength = "";
    fitsConf.apertureDia = "";
    fitsConf.apertureArea = "";
    fitsConf.pixelSizeX = "";
    fitsConf.pixelSizeY = "";
    fitsConf.subframeOriginX = "";
    fitsConf.subframeOriginY = "";
    fitsConf.siteLatitude = "";
    fitsConf.siteLongitude = "";
    fitsConf.filter = "";

#ifdef OACAPTURE
    config.timerMode = OA_TIMER_MODE_UNSET;
    config.timerEnabled = 0;
#endif
  } else {

    int version = settings->value ( "configVersion", CONFIG_VERSION ).toInt();

    restoreGeometry ( settings->value ( "geometry").toByteArray());

    // FIX ME -- how to handle this?
    // config.cameraDevice = settings->value ( "device/camera", -1 ).toInt();

    generalConf.tempsInC = settings->value ( "tempsInCentigrade", 1 ).toInt();
    generalConf.connectSoleCamera = settings->value ( "connectSoleCamera",
        0 ).toInt();
#ifdef OACAPTURE
    config.dockableControls = settings->value ( "dockableControls", 0 ).toInt();
    config.controlsOnRight = settings->value ( "controlsOnRight", 1 ).toInt();
    config.separateControls = settings->value ( "separateControls", 0 ).toInt();
#endif
    config.saveCaptureSettings = settings->value ( "saveCaptureSettings",
        1 ).toInt();
#ifdef OACAPTURE
    config.windowsCompatibleAVI = settings->value ( "windowsCompatibleAVI",
        0 ).toInt();
    config.useUtVideo = settings->value ( "useUtVideo", 0 ).toInt();
#endif
    config.indexDigits = settings->value ( "indexDigits", 6 ).toInt();

#ifdef OACAPTURE
    config.showHistogram = settings->value ( "options/showHistogram",
        0 ).toInt();
    config.autoAlign = settings->value ( "options/autoAlign", 0 ).toInt();
#endif
    config.showReticle = settings->value ( "options/showReticle", 0 ).toInt();
    config.showFocusAid = settings->value ( "options/showFocusAid", 0 ).toInt();
#ifdef OALIVE
    config.showSpinner = settings->value ( "options/showSpinner", 0 ).toInt();
#endif
#ifdef OACAPTURE
    config.cutout = settings->value ( "options/cutout", 0 ).toInt();
    config.darkFrame = settings->value ( "options/darkFrame", 0 ).toInt();
    config.flipX = settings->value ( "options/flipX", 0 ).toInt();
    config.flipY = settings->value ( "options/flipY", 0 ).toInt();
    config.demosaic = settings->value ( "options/demosaic", 0 ).toInt();
#endif

    commonConfig.binning2x2 = settings->value (
				"camera/binning2x2", 0 ).toInt();
    commonConfig.colourise = settings->value ( "camera/colourise", 0 ).toInt();
    // FIX ME -- reset these temporarily.  needs fixing properly
    commonConfig.binning2x2 = 0;
    commonConfig.colourise = 0;

    commonConfig.useROI = settings->value ( "image/useROI", 0 ).toInt();
    commonConfig.imageSizeX = settings->value ( "image/imageSizeX", 0 ).toInt();
    commonConfig.imageSizeY = settings->value ( "image/imageSizeY", 0 ).toInt();

    config.zoomButton1Option = settings->value ( "image/zoomButton1Option",
        0 ).toInt();
    config.zoomButton2Option = settings->value ( "image/zoomButton2Option",
        1 ).toInt();
    config.zoomButton3Option = settings->value ( "image/zoomButton3Option",
        3 ).toInt();
    config.zoomValue = settings->value ( "image/zoomValue", 100 ).toInt();

#ifdef OACAPTURE
    if ( version < 3 ) {
      cameraConf.CONTROL_VALUE( OA_CAM_CTRL_GAIN ) = settings->value (
          "control/gainValue", 50 ).toInt();
      cameraConf.CONTROL_VALUE( OA_CAM_CTRL_EXPOSURE_UNSCALED ) =
					settings->value ( "control/exposureValue", 10 ).toInt();
      cameraConf.CONTROL_VALUE( OA_CAM_CTRL_EXPOSURE_ABSOLUTE ) =
					settings->value ( "control/exposureAbsoluteValue", 10 ).toInt();
      cameraConf.CONTROL_VALUE( OA_CAM_CTRL_GAMMA ) = settings->value (
          "control/gammaValue", -1 ).toInt();
      cameraConf.CONTROL_VALUE( OA_CAM_CTRL_BRIGHTNESS ) = settings->value (
          "control/brightnessValue", -1 ).toInt();
    }
#endif

    config.exposureMenuOption = settings->value ( "control/exposureMenuOption",
        3 ).toInt();
    config.frameRateNumerator = settings->value ( "control/frameRateNumerator",
        0 ).toInt();
    config.frameRateDenominator = settings->value (
        "control/frameRateDenominator", 1 ).toInt();
#ifdef OACAPTURE
    config.selectableControl[0] = settings->value (
        "control/selectableControl1", OA_CAM_CTRL_GAMMA ).toInt();
    config.selectableControl[1] = settings->value (
        "control/selectableControl2", OA_CAM_CTRL_BRIGHTNESS ).toInt();
    if ( config.selectableControl[1] == config.selectableControl[0] ) {
      config.selectableControl[1] = -1;
    }
#endif
    config.intervalMenuOption = settings->value (
        "control/intervalMenuOption", 1 ).toInt(); // default = msec

    commonConfig.profileOption = settings->value ( "control/profileOption",
        0 ).toInt();
    commonConfig.filterOption = settings->value (
				"control/filterOption", 0 ).toInt();
#ifdef OACAPTURE
    commonConfig.fileTypeOption = settings->value ( "control/fileTypeOption",
        1 ).toInt();
    config.limitEnabled = settings->value ( "control/limitEnabled", 0 ).toInt();
    config.framesLimitValue = settings->value ( "control/framesLimitValue",
        0 ).toInt();
    config.secondsLimitValue = settings->value ( "control/secondsLimitValue",
        0 ).toInt();
    config.limitType = settings->value ( "control/limitType", 0 ).toInt();
    config.fileNameTemplate = settings->value ( "control/fileNameTemplate",
        "oaCapture-%DATE-%TIME" ).toString();
    config.captureDirectory = settings->value ( "control/captureDirectory",
        defaultDir ).toString();
#else
    commonConfig.fileTypeOption = settings->value ( "files/fileTypeOption",
        1 ).toInt();
    config.frameFileNameTemplate = settings->value (
        "files/frameFileNameTemplate", "oalive-%DATE-%TIME" ).toString();
    config.processedFileNameTemplate = settings->value (
        "files/processedFileNameTemplate",
        "oalive-processed-%DATE-%TIME" ).toString();
    config.saveEachFrame = settings->value ( "files/saveEachFrame", 0 ).toInt();
    config.saveProcessedImage = settings->value ( "files/saveProcessedImage",
        0 ).toInt();
    config.captureDirectory = settings->value ( "files/captureDirectory",
        "" ).toString();

    config.stackKappa = settings->value ( "stacking/kappa", 2.0 ).toDouble();
    config.maxFramesToStack = settings->value ( "stacking/maxFramesToStack",
				20 ).toInt();
#endif

#ifdef OACAPTURE
    config.autorunCount = settings->value ( "autorun/count", 0 ).toInt();
    config.autorunDelay = settings->value ( "autorun/delay", 0 ).toInt();
#endif
    filterConf.promptForFilterChange = settings->value (
        "autorun/filterPrompt", 0 ).toInt();
    filterConf.interFilterDelay = settings->value (
        "autorun/interFilterDelay", 0 ).toInt();

#ifdef OACAPTURE
    config.preview = settings->value ( "display/preview", 1 ).toInt();
    config.nightMode = settings->value ( "display/nightMode", 0 ).toInt();
    config.displayFPS = settings->value ( "display/displayFPS", 15 ).toInt();
    // fix a problem with existing configs
    if ( !config.displayFPS ) { config.displayFPS = 15; }

    config.splitHistogram = settings->value ( "histogram/split", 0 ).toInt();
    config.histogramOnTop = settings->value ( "histogram/onTop", 1 ).toInt();

    config.demosaicPreview = settings->value ( "demosaic/preview", 0 ).toInt();
    config.demosaicOutput = settings->value ( "demosaic/output", 0 ).toInt();
#endif
    demosaicConf.demosaicMethod = settings->value ( "demosaic/method",
        OA_DEMOSAIC_BILINEAR ).toInt();
    demosaicConf.cfaPattern = settings->value ( "demosaic/cfaPattern",
        OA_DEMOSAIC_AUTO ).toInt();
    demosaicConf.monoIsRawColour = settings->value ( "demosaic/monoIsRawColour",
        1 ).toInt();

    generalConf.reticleStyle = settings->value ( "reticle/style",
        RETICLE_CIRCLE ).toInt();

    // Give up on earlier versions of this data.  It's too complicated to
    // sort out
    if ( version >= 2 ) {
      int numControls = settings->beginReadArray ( "controls" );
      if ( numControls ) {
        for ( int j = 1; j <= numControls; j++ ) {
          settings->setArrayIndex ( j-1 );
          int numModifiers = settings->beginReadArray ( "modifiers" );
          if ( numModifiers )  {
            for ( int i = 0; i < numModifiers; i++ ) {
              settings->setArrayIndex ( i );
              cameraConf.controlValues[i][j] = settings->value (
									"controlValue", 0 ).toInt();
            }
          }
          settings->endArray();
        }
      }
      settings->endArray();
    }

    // For v1 config we may not have a "none" option here, or if
    // we do it may not be first, so it has to be added and the filter numbers
    // adjusted accordingly.

    if ( version > 1 ) {
      filterConf.numFilters = settings->beginReadArray ( "filters" );
      if ( filterConf.numFilters ) {
        for ( int i = 0; i < filterConf.numFilters; i++ ) {
          settings->setArrayIndex ( i );
          FILTER f;
          f.filterName = settings->value ( "name", "" ).toString();
          filterConf.filters.append ( f );
        }
      } else {
        // FIX ME -- these should probably be configured elsewhere
        QList<QString> defaults;
        defaults << "None" << "L" << "R" << "G" << "B" << "IR" << "UV" <<
            "Ha" << "Hb" << "S2" << "O3" << "CH4";
        filterConf.numFilters = defaults.count();
        for ( int i = 0; i < filterConf.numFilters; i++ ) {
          FILTER f;
          f.filterName = defaults[i];
          filterConf.filters.append ( f );
        }
      }
      settings->endArray();
    } else {
      int numFilters = settings->beginReadArray ( "filters" );
      int totalFilters = 0, renumberFrom = -1, renumberTo = -1;
      if ( numFilters ) {
        for ( int i = 0; i < numFilters; i++ ) {
          settings->setArrayIndex ( i );
          FILTER f;
          f.filterName = settings->value ( "name", "" ).toString();
          // see if this one is called "none"
          if ( !QString::compare ( f.filterName, "none",
              Qt::CaseInsensitive )) {
            // it is.  is it the first?
            if ( i ) {
              // no, then update renumberTo and skip this one as we've
              // already added it.  renumberFrom should already be set
              renumberTo = i - 1;
              continue;
            } // else carry on as normal
          } else {
            if ( !i ) {
              // current string is not "none", so add it if this is our first
              // time through
              FILTER fn;
              fn.filterName = "none";
              filterConf.filters.append ( fn );
              totalFilters++;
              renumberFrom = 0;
              renumberTo = numFilters;
            }
          }
          filterConf.filters.append ( f );
          totalFilters++;
        }
      } else {
        FILTER fn;
        fn.filterName = "none";
        filterConf.filters.append ( fn );
        totalFilters++;
      }
      settings->endArray();
      filterConf.numFilters = totalFilters;
      if ( commonConfig.filterOption >= renumberFrom &&
					commonConfig.filterOption <= renumberTo ) {
        if ( renumberTo < numFilters && commonConfig.filterOption ==
            ( renumberTo + 1 )) {
          // we saw "none" and were currently using it
          commonConfig.filterOption = 0;
        } else {
          commonConfig.filterOption++;
        }
      }
    }

    profileConf.numProfiles = settings->beginReadArray ( "profiles" );
    if ( profileConf.numProfiles ) {
      for ( int i = 0; i < profileConf.numProfiles; i++ ) {
        settings->setArrayIndex ( i );
        PROFILE p;
        p.profileName = settings->value ( "name", "" ).toString();
        p.binning2x2 = settings->value ( "binning2x2", 0 ).toInt();
        p.colourise = settings->value ( "colourise", 0 ).toInt();
        p.useROI = settings->value ( "useROI", 0 ).toInt();
        p.imageSizeX = settings->value ( "imageSizeX", 0 ).toInt();
        p.imageSizeY = settings->value ( "imageSizeY", 0 ).toInt();
          if ( version > 1 ) {
            int numFilters = settings->beginReadArray ( "filters" );
            if ( numFilters ) {
              for ( int k = 0; k < numFilters; k++ ) {
                settings->setArrayIndex ( k );
                if ( numFilters <= filterConf.numFilters ) {
                  FILTER_PROFILE fp;
                  fp.filterName = filterConf.filters[k].filterName;
                  p.filterProfiles.append ( fp );
                }
                // Give up on anything before version 7.  It's just too
                // messy
                if ( version >= 7 ) {
                  int numControls = settings->beginReadArray ( "controls" );
                  for ( int j = 1; j <= numControls; j++ ) {
                    settings->setArrayIndex ( j-1 );
                    int numModifiers = settings->beginReadArray ( "modifiers" );
                    if ( numModifiers )  {
                      for ( int i = 0; i < numModifiers; i++ ) {
                        settings->setArrayIndex ( i );
                        if ( numFilters <= filterConf.numFilters ) {
                          p.filterProfiles[ k ].controls[ i ][ j ] =
                              settings->value ( "controlValue", 0 ).toInt();
                        }
                      }
                    }
                    settings->endArray();
                  }
                  settings->endArray();
                }
                p.filterProfiles[ k ].intervalMenuOption = settings->value (
                    "intervalMenuOption", 1 ).toInt(); // default = msec
              }
            }
            settings->endArray();

          } else {
            if ( filterConf.numFilters ) {
              for ( int k = 0; k < filterConf.numFilters; k++ ) {
                FILTER_PROFILE fp;
                fp.filterName = filterConf.filters[k].filterName;
                p.filterProfiles.append ( fp );
                for ( int j = 0; j < OA_CAM_CTRL_LAST_P1; j++ ) {
                  for ( int i = 0; i < OA_CAM_CTRL_MODIFIERS_LAST_P1; i++ ) {
                    p.filterProfiles[ k ].controls[ i ][ j ] = 1;
                  }
                }
                p.filterProfiles[ k ].intervalMenuOption = 1; // msec
              }
            }
          }

          p.frameRateNumerator = settings->value ( "frameRateNumerator",
              0 ).toInt();
          p.frameRateDenominator = settings->value ( "frameRateDenominator",
              1 ).toInt();
          p.filterOption = settings->value ( "filterOption", 0 ).toInt();
          p.fileTypeOption = settings->value ( "fileTypeOption", 1 ).toInt();
#ifdef OACAPTURE
          p.fileNameTemplate = settings->value ( "fileNameTemplate",
              "oaCapture-%DATE-%TIME" ).toString();
          p.limitEnabled = settings->value ( "limitEnabled", 0 ).toInt();
          p.framesLimitValue = settings->value ( "framesLimitValue",
              0 ).toInt();
          p.secondsLimitValue = settings->value ( "secondsLimitValue",
              0 ).toInt();
          p.limitType = settings->value ( "limitType", -1 ).toInt();
#else
          p.frameFileNameTemplate = settings->value (
              "frameFileNameTemplate", "oalive-%DATE-%TIME" ).toString();
          p.processedFileNameTemplate = settings->value (
              "processedFileNameTemplate",
              "oalive-processed-%DATE-%TIME" ).toString();
#endif
          p.target = settings->value ( "target", 0 ).toInt();
          profileConf.profiles.append ( p );
      }
      settings->endArray();

    } else {
      // if we have no profiles we create a default one

      PROFILE p;
      p.profileName = "default";
      p.binning2x2 = commonConfig.binning2x2;
      p.colourise = commonConfig.colourise;
      p.useROI = commonConfig.useROI;
      p.imageSizeX = commonConfig.imageSizeX;
      p.imageSizeY = commonConfig.imageSizeY;
      if ( filterConf.numFilters ) {
        for ( int k = 0; k < filterConf.numFilters; k++ ) {
          FILTER_PROFILE fp;
          fp.filterName = filterConf.filters[k].filterName;
          fp.intervalMenuOption = 1; // msec
          p.filterProfiles.append ( fp );
        }
      }
      for ( int j = 1; j < OA_CAM_CTRL_LAST_P1; j++ ) {
        if ( filterConf.numFilters ) {
	  for ( int k = 0; k < filterConf.numFilters; k++ ) {
            for ( int i = 0; i < OA_CAM_CTRL_MODIFIERS_LAST_P1; i++ ) {
              p.filterProfiles[ k ].controls[ i ][ j ] =
                  cameraConf.controlValues[ i ][ j ];
            }
          }
        }
      }

      p.frameRateNumerator = config.frameRateNumerator;
      p.frameRateDenominator = config.frameRateDenominator;
      p.filterOption = commonConfig.filterOption;
      p.fileTypeOption = commonConfig.fileTypeOption;
#ifdef OACAPTURE
      p.fileNameTemplate = config.fileNameTemplate;
      p.limitEnabled = config.limitEnabled;
      p.framesLimitValue = config.framesLimitValue;
      p.secondsLimitValue = config.secondsLimitValue;
      p.limitType = config.limitType;
#else
      p.frameFileNameTemplate = config.frameFileNameTemplate;
      p.processedFileNameTemplate = config.processedFileNameTemplate;
#endif
      p.target = TGT_UNKNOWN;
      profileConf.profiles.append ( p );
      profileConf.numProfiles = 1;
    }

    if ( version > 1 ) {
      ( void ) settings->beginReadArray ( "filterSlots" );
      for ( int i = 0; i < MAX_FILTER_SLOTS; i++ ) {
        settings->setArrayIndex ( i );
        filterConf.filterSlots[i] = settings->value ( "slot", -1 ).toInt();
      }
      settings->endArray();

#ifdef OACAPTURE
      int numSeqs = settings->beginReadArray ( "filterSequence" );
      if ( numSeqs ) {
        for ( int i = 0; i < numSeqs; i++ ) {
          settings->setArrayIndex ( i );
          filterConf.autorunFilterSequence.append ( settings->value (
              "slot", -1 ).toInt());
        }
      }
      settings->endArray();
#endif
    }

    config.currentColouriseColour = QColor ( 255, 255, 255 );
    config.numCustomColours = 0;
    config.customColours.clear();

    if ( version > 1 ) {
      int r = settings->value ( "colourise/currentColour/red", 255 ).toInt();
      int g = settings->value ( "colourise/currentColour/green", 255 ).toInt();
      int b = settings->value ( "colourise/currentColour/blue", 255 ).toInt();
      config.currentColouriseColour = QColor ( r, g, b );
      config.numCustomColours = settings->beginReadArray (
          "colourise/customColours" );
      if ( config.numCustomColours ) {
        for ( int i = 0; i < config.numCustomColours; i++ ) {
          settings->setArrayIndex ( i );
          r = settings->value ( "red", 255 ).toInt();
          b = settings->value ( "blue", 255 ).toInt();
          g = settings->value ( "green", 255 ).toInt();
          config.customColours.append ( QColor ( r, g, b ));
        }
      }
      settings->endArray();
    }
  }

  commonConfig.filterWheelConfig.clear();
  for ( int i = 0; i < OA_FW_IF_COUNT; i++ ) {
    userConfigList	conf;
    conf.clear();
    commonConfig.filterWheelConfig.append ( conf );
  }
  int numInterfaces = settings->beginReadArray ( "filterWheelUserConfig" );
  if ( numInterfaces ) {
    for ( int i = 0; i < numInterfaces; i++ ) {
      settings->setArrayIndex ( i );
      int numMatches = settings->beginReadArray ( "matches" );
      if ( numMatches ) {
        for ( int j = 0; j < numMatches; j++ ) {
          settings->setArrayIndex ( j );
          userDeviceConfig c;
          c.vendorId = settings->value ( "vendorId", 0 ).toInt();
          c.productId = settings->value ( "productId", 0 ).toInt();
          ( void ) strcpy ( c.manufacturer, settings->value ( "manufacturer",
              0 ).toString().toStdString().c_str());
          ( void ) strcpy ( c.product, settings->value ( "product",
              0 ).toString().toStdString().c_str());
          ( void ) strcpy ( c.serialNo, settings->value ( "serialNo",
              0 ).toString().toStdString().c_str());
          ( void ) strcpy ( c.filesystemPath, settings->value ( "fsPath",
              0 ).toString().toStdString().c_str());
          commonConfig.filterWheelConfig[i].append ( c );
        }
      }
      settings->endArray();
    }
    settings->endArray();
  }

#ifdef OACAPTURE
  config.timerConfig.clear();
  for ( int i = 0; i < OA_TIMER_IF_COUNT; i++ ) {
    userConfigList      conf;
    conf.clear();
    config.timerConfig.append ( conf );
  }
  numInterfaces = settings->beginReadArray ( "ptrUserConfig" );
  if ( numInterfaces ) {
    for ( int i = 0; i < numInterfaces; i++ ) {
      settings->setArrayIndex ( i );
      int numMatches = settings->beginReadArray ( "matches" );
      if ( numMatches ) {
        for ( int j = 0; j < numMatches; j++ ) {
          settings->setArrayIndex ( j );
          userDeviceConfig c;
          c.vendorId = settings->value ( "vendorId", 0 ).toInt();
          c.productId = settings->value ( "productId", 0 ).toInt();
          ( void ) strcpy ( c.manufacturer, settings->value ( "manufacturer",
              0 ).toString().toStdString().c_str());
          ( void ) strcpy ( c.product, settings->value ( "product",
              0 ).toString().toStdString().c_str());
          ( void ) strcpy ( c.serialNo, settings->value ( "serialNo",
              0 ).toString().toStdString().c_str());
          ( void ) strcpy ( c.filesystemPath, settings->value ( "fsPath",
              0 ).toString().toStdString().c_str());
          config.timerConfig[i].append ( c );
        }
      }
      settings->endArray();
    }
    settings->endArray();
  }
#endif

  if ( !generalConf.saveSettings || generalConf.saveSettings == -1 ) {
    generalConf.saveSettings = -generalConf.saveSettings;
  }

  fitsConf.observer = settings->value ( "fits/observer", "" ).toString();
  fitsConf.instrument = settings->value ( "fits/instrument", "" ).toString();
  fitsConf.object = settings->value ( "fits/object", "" ).toString();
  fitsConf.comment = settings->value ( "fits/comment", "" ).toString();
  fitsConf.telescope = settings->value ( "fits/telescope", "" ).toString();
  fitsConf.focalLength = settings->value ( "fits/focalLength", "" ).toString();
  fitsConf.apertureDia = settings->value ( "fits/apertureDia", "" ).toString();
  fitsConf.apertureArea = settings->value (
      "fits/apertureArea", "" ).toString();
  fitsConf.pixelSizeX = settings->value ( "fits/pixelSizeX", "" ).toString();
  fitsConf.pixelSizeY = settings->value ( "fits/pixelSizeY", "" ).toString();
  fitsConf.subframeOriginX = settings->value (
      "fits/subframeOriginX", "" ).toString();
  fitsConf.subframeOriginY = settings->value (
      "fits/subframeOriginY", "" ).toString();
  fitsConf.siteLatitude = settings->value (
      "fits/siteLatitude", "" ).toString();
  fitsConf.siteLongitude = settings->value (
      "fits/siteLongitude", "" ).toString();
  fitsConf.filter = settings->value ( "fits/filter", "" ).toString();

#ifdef OACAPTURE
  config.timerMode = settings->value ( "timer/mode",
      OA_TIMER_MODE_UNSET ).toInt();
  config.timerEnabled = settings->value ( "timer/enabled", 0 ).toInt();
  config.triggerInterval = settings->value ( "timer/triggerInterval",
      1 ).toInt();
  config.userDrainDelayEnabled = settings->value ( "timer/drainDelayEnabled",
      0 ).toInt();
  config.drainDelay = settings->value ( "timer/drainDelay", 500 ).toInt();
  config.timestampDelay = settings->value ( "timer/timestampDelay",
      50 ).toInt();
  config.queryGPSForEachCapture = settings->value (
      "timer/queryGPSForEachCapture", 0 ).toInt();
#endif
}


void
MainWindow::writeConfig ( QString configFile )
{
  QSettings*  settings;

  if ( !generalConf.saveSettings ) {
    return;
  }

  if ( configFile != "" ) {
    settings = new QSettings ( configFile, QSettings::IniFormat );
  } else {
#if defined(__APPLE__) && defined(__MACH__) && TARGET_OS_MAC == 1
    settings = new QSettings ( QSettings::IniFormat, QSettings::UserScope,
                            ORGANISATION_NAME_SETTINGS, APPLICATION_NAME );
#else
    settings = new QSettings ( ORGANISATION_NAME_SETTINGS, APPLICATION_NAME );
#endif
  }

  settings->clear();

  settings->setValue ( "saveSettings", generalConf.saveSettings );

  settings->setValue ( "configVersion", CONFIG_VERSION );
  settings->setValue ( "geometry", geometry());

  settings->setValue ( "tempsInCentigrade", generalConf.tempsInC );
  settings->setValue ( "connectSoleCamera", generalConf.connectSoleCamera );
#ifdef OACAPTURE
  settings->setValue ( "dockableControls", config.dockableControls );
  settings->setValue ( "controlsOnRight", config.controlsOnRight );
  settings->setValue ( "separateControls", config.separateControls );
#endif
  settings->setValue ( "saveCaptureSettings", config.saveCaptureSettings );
#ifdef OACAPTURE
  settings->setValue ( "windowsCompatibleAVI", config.windowsCompatibleAVI );
  settings->setValue ( "useUtVideo", config.useUtVideo );
#endif
  settings->setValue ( "indexDigits", config.indexDigits );

  // FIX ME -- how to handle this?
  // settings->setValue ( "device/camera", -1 ).toInt();

#ifdef OACAPTURE
  settings->setValue ( "options/showHistogram", config.showHistogram );
  settings->setValue ( "options/autoAlign", config.autoAlign );
  settings->setValue ( "options/cutout", config.cutout );
  settings->setValue ( "options/darkFrame", config.darkFrame );
  settings->setValue ( "options/flipX", config.flipX );
  settings->setValue ( "options/flipY", config.flipY );
#endif
  settings->setValue ( "options/showReticle", config.showReticle );
  settings->setValue ( "options/showFocusAid", config.showFocusAid );
  settings->setValue ( "options/demosaic", config.demosaic );
#ifdef OALIVE
  settings->setValue ( "options/showSpinner", config.showSpinner );
#endif

  settings->setValue ( "camera/binning2x2", commonConfig.binning2x2 );
  settings->setValue ( "camera/colourise", commonConfig.colourise );
  settings->setValue ( "camera/inputFrameFormat", config.inputFrameFormat );
  settings->setValue ( "camera/forceInputFrameFormat",
      cameraConf.forceInputFrameFormat );

  settings->setValue ( "image/useROI", commonConfig.useROI );
  settings->setValue ( "image/imageSizeX", commonConfig.imageSizeX );
  settings->setValue ( "image/imageSizeY", commonConfig.imageSizeY );

  settings->setValue ( "image/zoomButton1Option", config.zoomButton1Option );
  settings->setValue ( "image/zoomButton2Option", config.zoomButton2Option );
  settings->setValue ( "image/zoomButton3Option", config.zoomButton3Option );
  settings->setValue ( "image/zoomValue", config.zoomValue );

  settings->setValue ( "control/exposureMenuOption",
      config.exposureMenuOption );
  settings->setValue ( "control/frameRateNumerator",
      config.frameRateNumerator );
  settings->setValue ( "control/frameRateDenominator",
      config.frameRateDenominator );
#ifdef OACAPTURE
  settings->setValue ( "control/selectableControl1",
      config.selectableControl[0] );
  settings->setValue ( "control/selectableControl2",
      config.selectableControl[1] );
#endif
  settings->setValue ( "control/intervalMenuOption",
      config.intervalMenuOption );

  settings->setValue ( "control/profileOption", commonConfig.profileOption );
  settings->setValue ( "control/filterOption", commonConfig.filterOption );
#ifdef OACAPTURE
  settings->setValue ( "control/fileTypeOption", commonConfig.fileTypeOption );
  settings->setValue ( "control/limitEnabled", config.limitEnabled );
  settings->setValue ( "control/framesLimitValue", config.framesLimitValue );
  settings->setValue ( "control/secondsLimitValue", config.secondsLimitValue );
  settings->setValue ( "control/limitType", config.limitType );
  settings->setValue ( "control/fileNameTemplate", config.fileNameTemplate );
  settings->setValue ( "control/captureDirectory", config.captureDirectory );

  settings->setValue ( "autorun/count", config.autorunCount );
  settings->setValue ( "autorun/delay", config.autorunDelay );
  settings->setValue ( "autorun/filterPrompt",
			filterConf.promptForFilterChange );
  settings->setValue ( "autorun/interFilterDelay",
      filterConf.interFilterDelay );

  settings->setValue ( "display/preview", config.preview );
  settings->setValue ( "display/nightMode", config.nightMode );
  settings->setValue ( "display/displayFPS", config.displayFPS );

  settings->setValue ( "histogram/split", config.splitHistogram );
  settings->setValue ( "histogram/onTop", config.histogramOnTop );
#else
  settings->setValue ( "files/fileTypeOption", commonConfig.fileTypeOption );
  settings->setValue ( "files/frameFileNameTemplate",
      config.frameFileNameTemplate );
  settings->setValue ( "files/processedFileNameTemplate",
      config.processedFileNameTemplate );
  settings->setValue ( "files/saveEachFrame", config.saveEachFrame );
  settings->setValue ( "files/saveProcessedImage", config.saveProcessedImage );
  settings->setValue ( "files/captureDirectory", config.captureDirectory );

  settings->setValue ( "stacking/kappa", config.stackKappa );
  settings->setValue ( "stacking/maxFramesToStack", config.maxFramesToStack );
#endif

#ifdef OACAPTURE
  settings->setValue ( "demosaic/preview", config.demosaicPreview );
  settings->setValue ( "demosaic/output", config.demosaicOutput );
#endif
  settings->setValue ( "demosaic/method", demosaicConf.demosaicMethod );
  settings->setValue ( "demosaic/monoIsRawColour",
			demosaicConf.monoIsRawColour );
  settings->setValue ( "demosaic/cfaPattern", demosaicConf.cfaPattern );

  settings->setValue ( "reticle/style", generalConf.reticleStyle );

  settings->beginWriteArray ( "controls" );
  for ( int i = 1; i < OA_CAM_CTRL_LAST_P1; i++ ) {
    settings->setArrayIndex ( i-1 );
		settings->beginWriteArray ( "modifiers" );
		for ( int j = 0; j < OA_CAM_CTRL_MODIFIERS_LAST_P1; j++ ) {
			settings->setArrayIndex ( j );
			settings->setValue ( "controlValue",
					static_cast<qlonglong> ( cameraConf.controlValues[j][i] ));
		}
		settings->endArray();
  }
  settings->endArray();

  settings->beginWriteArray ( "filters" );
  if ( filterConf.numFilters ) {
    for ( int i = 0; i < filterConf.numFilters; i++ ) {
      settings->setArrayIndex ( i );
      settings->setValue ( "name", filterConf.filters[i].filterName );
    }
  }
  settings->endArray();

  settings->beginWriteArray ( "profiles" );
  if ( profileConf.numProfiles ) {
    for ( int i = 0; i < profileConf.numProfiles; i++ ) {
      settings->setArrayIndex ( i );
      settings->setValue ( "name", profileConf.profiles[i].profileName );
      settings->setValue ( "binning2x2", profileConf.profiles[i].binning2x2 );
      settings->setValue ( "colourise", profileConf.profiles[i].colourise );
      settings->setValue ( "useROI", profileConf.profiles[i].useROI );
      settings->setValue ( "imageSizeX", profileConf.profiles[i].imageSizeX );
      settings->setValue ( "imageSizeY", profileConf.profiles[i].imageSizeY );

      if ( filterConf.numFilters &&
          !profileConf.profiles[ i ].filterProfiles.isEmpty()) {
        settings->beginWriteArray ( "filters" );
        for ( int j = 0; j < filterConf.numFilters; j++ ) {
          settings->setArrayIndex ( j );
          settings->setValue ( "intervalMenuOption",
              profileConf.profiles[ i ].filterProfiles[ j ].intervalMenuOption );
          settings->beginWriteArray ( "controls" );
          for ( int k = 1; k < OA_CAM_CTRL_LAST_P1; k++ ) {
            settings->setArrayIndex ( k );
            settings->beginWriteArray ( "modifiers" );
            for ( int l = 0; l < OA_CAM_CTRL_MODIFIERS_LAST_P1; l++ ) {
              settings->setArrayIndex ( l );
              settings->setValue ( "controlValue",
                  profileConf.profiles[ i ].filterProfiles[ j ].controls[ l ][ k ]);
            }
            settings->endArray();
          }
          settings->endArray();
        }
        settings->endArray();
      }
      settings->setValue ( "frameRateNumerator",
          profileConf.profiles[i].frameRateNumerator );
      settings->setValue ( "frameRateDenominator",
          profileConf.profiles[i].frameRateDenominator );
      settings->setValue ( "filterOption", profileConf.profiles[i].filterOption );
      settings->setValue ( "fileTypeOption",
          profileConf.profiles[i].fileTypeOption );
#ifdef OACAPTURE
      settings->setValue ( "fileNameTemplate",
          profileConf.profiles[i].fileNameTemplate );
      settings->setValue ( "limitEnabled", profileConf.profiles[i].limitEnabled );
      settings->setValue ( "framesLimitValue",
          profileConf.profiles[i].framesLimitValue );
      settings->setValue ( "secondsLimitValue",
          profileConf.profiles[i].secondsLimitValue );
      settings->setValue ( "limitType", profileConf.profiles[i].limitType );
#else
      settings->setValue ( "frameFileNameTemplate",
          profileConf.profiles[i].frameFileNameTemplate );
      settings->setValue ( "processedFileNameTemplate",
          profileConf.profiles[i].processedFileNameTemplate );
#endif
      settings->setValue ( "target", profileConf.profiles[i].target );
    }
  }
  settings->endArray();

  settings->beginWriteArray ( "filterSlots" );
  for ( int i = 0; i < MAX_FILTER_SLOTS; i++ ) {
    settings->setArrayIndex ( i );
    settings->setValue ( "slot", filterConf.filterSlots[i] );
  }
  settings->endArray();

#ifdef OACAPTURE
  settings->beginWriteArray ( "filterSequence" );
  int numSeqs;
  if (( numSeqs = filterConf.autorunFilterSequence.count())) {
    for ( int i = 0; i < numSeqs; i++ ) {
      settings->setArrayIndex ( i );
      settings->setValue ( "slot", filterConf.autorunFilterSequence[i] );
    }
  }
  settings->endArray();
#endif

  settings->beginWriteArray ( "filterWheelUserConfig" );
  int numInterfaces = commonConfig.filterWheelConfig.count();
  for ( int i = 0; i < numInterfaces; i++ ) {
    settings->setArrayIndex ( i );
    settings->beginWriteArray ( "matches" );
    int numMatches = commonConfig.filterWheelConfig[i].count();
    userConfigList confList = commonConfig.filterWheelConfig[i];
    for ( int j = 0; j < numMatches; j++ ) {
      settings->setArrayIndex ( j );
      settings->setValue ( "vendorId", confList[j].vendorId );
      settings->setValue ( "productId", confList[j].productId );
      settings->setValue ( "manufacturer", confList[j].manufacturer );
      settings->setValue ( "product", confList[j].product );
      settings->setValue ( "serialNo", confList[j].serialNo );
      settings->setValue ( "fsPath", confList[j].filesystemPath );
    }
    settings->endArray();
  }
  settings->endArray();

#ifdef OACAPTURE
  settings->beginWriteArray ( "ptrUserConfig" );
  numInterfaces = config.timerConfig.count();
  for ( int i = 0; i < numInterfaces; i++ ) {
    settings->setArrayIndex ( i );
    settings->beginWriteArray ( "matches" );
    int numMatches = config.timerConfig[i].count();
    userConfigList confList = config.timerConfig[i];
    for ( int j = 0; j < numMatches; j++ ) {
      settings->setArrayIndex ( j );
      settings->setValue ( "vendorId", confList[j].vendorId );
      settings->setValue ( "productId", confList[j].productId );
      settings->setValue ( "manufacturer", confList[j].manufacturer );
      settings->setValue ( "product", confList[j].product );
      settings->setValue ( "serialNo", confList[j].serialNo );
      settings->setValue ( "fsPath", confList[j].filesystemPath );
    }
    settings->endArray();
  }
  settings->endArray();
#endif

  int r = config.currentColouriseColour.red();
  int g = config.currentColouriseColour.green();
  int b = config.currentColouriseColour.blue();
  settings->setValue ( "colourise/currentColour/red", r );
  settings->setValue ( "colourise/currentColour/green", g );
  settings->setValue ( "colourise/currentColour/blue", b );
  settings->beginWriteArray ( "colourise/customColours" );
  for ( int i = 0; i < config.numCustomColours; i++ ) {
    settings->setArrayIndex ( i );
    r = config.customColours[i].red();
    g = config.customColours[i].green();
    b = config.customColours[i].blue();
    settings->setValue ( "red", r );
    settings->setValue ( "green", g );
    settings->setValue ( "blue", b );
  }
  settings->endArray();

  settings->setValue ( "fits/observer", fitsConf.observer );
  settings->setValue ( "fits/instrument", fitsConf.instrument );
  settings->setValue ( "fits/object", fitsConf.object );
  settings->setValue ( "fits/comment", fitsConf.comment );
  settings->setValue ( "fits/telescope", fitsConf.telescope );
  settings->setValue ( "fits/focalLength", fitsConf.focalLength );
  settings->setValue ( "fits/apertureDia", fitsConf.apertureDia );
  settings->setValue ( "fits/apertureArea", fitsConf.apertureArea );
  settings->setValue ( "fits/pixelSizeX", fitsConf.pixelSizeX );
  settings->setValue ( "fits/pixelSizeY", fitsConf.pixelSizeY );
  settings->setValue ( "fits/subframeOriginX", fitsConf.subframeOriginX );
  settings->setValue ( "fits/subframeOriginY", fitsConf.subframeOriginY );
  settings->setValue ( "fits/siteLatitude", fitsConf.siteLatitude );
  settings->setValue ( "fits/siteLongitude", fitsConf.siteLongitude );
  settings->setValue ( "fits/filter", fitsConf.filter );

#ifdef OACAPTURE
  settings->setValue ( "timer/mode", config.timerMode );
  settings->setValue ( "timer/enabled", config.timerEnabled );
  settings->setValue ( "timer/triggerInterval", config.triggerInterval );
  settings->setValue ( "timer/drainDelayEnabled",
      config.userDrainDelayEnabled );
  settings->setValue ( "timer/drainDelay", config.drainDelay );
  settings->setValue ( "timer/timestampDelay", config.timestampDelay );
  settings->setValue ( "timer/queryGPSForEachCapture",
      config.queryGPSForEachCapture );
#endif

  settings->sync();
}


void
MainWindow::createStatusBar ( void )
{
  statusLine = statusBar();
  setStatusBar ( statusLine );

  tempLabel = new QLabel();
  if ( generalConf.tempsInC ) {
    tempLabel->setText ( tr ( "Temp (C)" ));
  } else {
    tempLabel->setText ( tr ( "Temp (F)" ));
  }
  tempLabel->setFixedWidth ( 60 );
#ifdef OACAPTURE
  fpsMaxLabel = new QLabel ( tr ( "FPS (max)" ));
  fpsMaxLabel->setFixedWidth ( 65 );
  fpsActualLabel = new QLabel ( tr ( "FPS (actual)" ));
  fpsActualLabel->setFixedWidth ( 80 );
  capturedLabel = new QLabel ( tr ( "Captured" ));
  capturedLabel->setFixedWidth ( 60 );
#endif
  droppedLabel = new QLabel ( tr ( "Dropped" ));
  droppedLabel->setFixedWidth ( 55 );
	stackedLabel = new QLabel ( tr ( "Stacked frames:" ));
  stackedLabel->setFixedWidth ( 100 );
  timeRemainingLabel = new QLabel ( tr ( "Time remaining: " ));
  timeRemainingLabel->setFixedWidth ( 100 );

  tempValue = new QLabel ( "" );
  tempValue->setFixedWidth ( 30 );
#ifdef OACAPTURE
  fpsMaxValue = new QLabel ( "0" );
  fpsMaxValue->setFixedWidth ( 30 );
  fpsActualValue = new QLabel ( "0" );
  fpsActualValue->setFixedWidth ( 30 );
  capturedValue = new QLabel ( "0" );
  capturedValue->setFixedWidth ( 40 );
#endif
  droppedValue = new QLabel ( "0" );
  droppedValue->setFixedWidth ( 40 );
	stackedValue = new QLabel ( "0" );
  stackedValue->setFixedWidth ( 40 );
  timeRemainingValue = new QLabel ( "0" );
  timeRemainingValue->setFixedWidth ( 40 );

  statusLine->addPermanentWidget ( timeRemainingLabel );
  statusLine->addPermanentWidget ( timeRemainingValue );
  statusLine->addPermanentWidget ( stackedLabel );
  statusLine->addPermanentWidget ( stackedValue );
  statusLine->addPermanentWidget ( tempLabel );
  statusLine->addPermanentWidget ( tempValue );
#ifdef OACAPTURE
  statusLine->addPermanentWidget ( fpsMaxLabel );
  statusLine->addPermanentWidget ( fpsMaxValue );
  statusLine->addPermanentWidget ( fpsActualLabel );
  statusLine->addPermanentWidget ( fpsActualValue );
  statusLine->addPermanentWidget ( capturedLabel );
  statusLine->addPermanentWidget ( capturedValue );
#endif
  statusLine->addPermanentWidget ( droppedLabel );
  statusLine->addPermanentWidget ( droppedValue );

  statusLine->showMessage ( tr ( "started" ));
}


void
MainWindow::createMenus ( void )
{
  // FIX ME -- add "restore program defaults" option

#ifdef OACAPTURE
  // File menu
  loadConfig = new QAction ( tr ( "Re&load Config" ), this );
  loadConfig->setStatusTip ( tr ( "Load default configuration" ));
  // FIX ME - set up slots

  saveConfig = new QAction ( tr ( "&Save Config" ), this );
  saveConfig->setShortcut ( QKeySequence::Save );
  saveConfig->setStatusTip ( tr ( "Save default configuration" ));
  // FIX ME - set up slots
#endif

  exit = new QAction ( tr ( "&Quit" ), this );
  exit->setShortcut ( QKeySequence::Quit );
  connect ( exit, SIGNAL( triggered()), this, SLOT( quit()));

  fileMenu = menuBar()->addMenu( tr ( "&File" ));
#ifdef OACAPTURE
  fileMenu->addAction ( loadConfig );
  fileMenu->addAction ( saveConfig );
#endif
  fileMenu->addSeparator();
  fileMenu->addAction ( exit );

  // Camera device menu

  cameraMenu = menuBar()->addMenu ( tr ( "&Camera" ));
  doCameraMenu(0);

  // Filter wheel menu

  filterWheelMenu = menuBar()->addMenu ( tr ( "&Filter Wheel" ));
  doFilterWheelMenu(0);

#ifdef OACAPTURE
  // Timer menu

  timerMenu = menuBar()->addMenu ( tr ( "&Timer" ));
  doTimerMenu(0);
#endif

  // Options menu

#ifdef OACAPTURE
  histogramOpt = new QAction ( QIcon ( ":/qt-icons/barchart.png" ),
      tr ( "Histogram" ), this );
  histogramOpt->setStatusTip ( tr ( "Open window for image histogram" ));
  histogramOpt->setCheckable ( true );
  connect ( histogramOpt, SIGNAL( changed()), this, SLOT( enableHistogram()));
  histogramOpt->setChecked ( config.showHistogram );

  autoalign = new QAction ( tr ( "Auto Align" ), this );
  autoalign->setCheckable ( true );
  // FIX ME - set up slots

  alignbox = new QAction ( tr ( "Align Box" ), this );
  alignbox->setCheckable ( true );
  // FIX ME - set up slots

  autoguide = new QAction ( tr ( "Auto Guide" ), this );
  autoguide->setCheckable ( true );
  // FIX ME - set up slots
#endif

  reticle = new QAction ( QIcon ( ":/qt-icons/reticle.png" ),
      tr ( "Reticle" ), this );
  reticle->setStatusTip ( tr ( "Overlay a reticle on the preview image" ));
  reticle->setCheckable ( true );
  connect ( reticle, SIGNAL( changed()), this, SLOT( enableReticle()));
  reticle->setChecked ( config.showReticle );

#ifdef OALIVE
  spinner = new QAction ( /* QIcon ( ":/qt-icons/spinner.png" ), */
      tr ( "Spinner" ), this );
  spinner->setStatusTip ( tr ( "Show spinner when processing images" ));
  spinner->setCheckable ( true );
  connect ( spinner, SIGNAL( changed()), this, SLOT( enableSpinner()));
  spinner->setChecked ( config.showSpinner );
#endif

#ifdef OACAPTURE
  cutout = new QAction ( tr ( "Cut Out" ), this );
  cutout->setCheckable ( true );
  // FIX ME - set up slots
#endif

  focusaid = new QAction ( tr ( "Focus Aid" ), this );
  focusaid->setCheckable ( true );
  connect ( focusaid, SIGNAL( changed()), this, SLOT( enableFocusAid()));

#ifdef OACAPTURE
  darkframe = new QAction ( tr ( "Dark Frame" ), this );
  darkframe->setCheckable ( true );
  // FIX ME - set up slots

  flipX = new QAction ( QIcon ( ":/qt-icons/object-flip-horizontal.png" ),
      tr ( "Flip X" ), this );
  flipX->setStatusTip ( tr ( "Flip image left<->right" ));
  flipX->setCheckable ( true );
  flipX->setChecked ( config.flipX );
  connect ( flipX, SIGNAL( changed()), this, SLOT( enableFlipX()));

  flipY = new QAction ( QIcon ( ":/qt-icons/object-flip-vertical.png" ),
      tr ( "Flip Y" ), this );
  flipY->setStatusTip ( tr ( "Flip image top<->bottom" ));
  flipY->setCheckable ( true );
  flipY->setChecked ( config.flipY );
  connect ( flipY, SIGNAL( changed()), this, SLOT( enableFlipY()));

  demosaicOpt = new QAction ( QIcon ( ":/qt-icons/mosaic.png" ),
      tr ( "Demosaic" ), this );
  demosaicOpt->setCheckable ( true );
  demosaicOpt->setChecked ( config.demosaic );
  connect ( demosaicOpt, SIGNAL( changed()), this, SLOT( enableDemosaic()));
#endif

  optionsMenu = menuBar()->addMenu ( tr ( "&Options" ));
#ifdef OACAPTURE
  optionsMenu->addAction ( histogramOpt );
#ifdef ENABLE_AUTOALIGN
  optionsMenu->addAction ( autoalign );
#endif
#ifdef ENABLE_ALIGNBOX
  optionsMenu->addAction ( alignbox );
#endif
#ifdef ENABLE_AUTOGUIDE
  optionsMenu->addAction ( autoguide );
#endif
#endif
  optionsMenu->addAction ( reticle );
#ifdef OACAPTURE
#ifdef ENABLE_CUTOUT
  optionsMenu->addAction ( cutout );
#endif
#endif
  optionsMenu->addAction ( focusaid );
#if OALIVE
  optionsMenu->addAction ( spinner );
#endif
#ifdef OACAPTURE
#ifdef ENABLE_DARKFRAME
  optionsMenu->addAction ( darkframe );
#endif
  optionsMenu->addAction ( flipX );
  optionsMenu->addAction ( flipY );
  optionsMenu->addAction ( demosaicOpt );
#endif

  // settings menu

  general = new QAction ( QIcon ( ":/qt-icons/cog.png" ),
      tr ( "General" ), this );
  general->setStatusTip ( tr ( "General configuration" ));
  connect ( general, SIGNAL( triggered()), this, SLOT( doGeneralSettings()));

  capture = new QAction ( QIcon ( ":/qt-icons/capture.png" ),
      tr ( "Capture" ), this );
  connect ( capture, SIGNAL( triggered()), this, SLOT( doCaptureSettings()));

#ifdef OACAPTURE
  cameraOpt = new QAction ( QIcon ( ":/qt-icons/planetary-camera.png" ),
      tr ( "Camera" ), this );
  connect ( cameraOpt, SIGNAL( triggered()), this, SLOT( doCameraSettings()));
  // not enabled until we connect a camera
  cameraOpt->setEnabled ( 0 );
#endif

  profiles = new QAction ( QIcon ( ":/qt-icons/jupiter.png" ),
      tr ( "Profiles" ), this );
  profiles->setStatusTip ( tr ( "Edit saved profiles" ));
  connect ( profiles, SIGNAL( triggered()), this, SLOT( doProfileSettings()));

  filters = new QAction ( QIcon ( ":/qt-icons/filter-wheel.png" ),
      tr ( "Filters" ), this );
  filters->setStatusTip ( tr ( "Configuration for filters" ));
  connect ( filters, SIGNAL( triggered()), this, SLOT( doFilterSettings()));

  demosaic = new QAction ( QIcon ( ":/qt-icons/mosaic.png" ),
      tr ( "Demosaic" ), this );
  demosaic->setStatusTip ( tr ( "Configuration for demosaicking" ));
  connect ( demosaic, SIGNAL( triggered()), this, SLOT( doDemosaicSettings()));

  fits = new QAction ( QIcon ( ":/qt-icons/fits.png" ),
      tr ( "FITS/SER Metadata" ), this );
  fits->setStatusTip ( tr ( "Configuration for FITS/SER metadata keywords" ));
  connect ( fits, SIGNAL( triggered()), this, SLOT( doFITSSettings()));

#ifdef OACAPTURE
  autorun = new QAction ( QIcon ( ":/qt-icons/clicknrun.png" ),
      tr ( "Autorun" ), this );
  autorun->setStatusTip ( tr ( "Configuration for repeat captures" ));
  connect ( autorun, SIGNAL( triggered()), this, SLOT( doAutorunSettings()));

  histogram = new QAction ( QIcon ( ":/qt-icons/barchart.png" ),
      tr ( "Histogram" ), this );
  histogram->setStatusTip ( tr ( "Configuration for histogram" ));
  connect ( histogram, SIGNAL( triggered()), this,
      SLOT( doHistogramSettings()));
#endif

  colourise = new QAction ( QIcon ( ":/qt-icons/sun.png" ),
      tr ( "False Colour" ), this );
  connect ( colourise, SIGNAL( triggered()), this,
      SLOT( doColouriseSettings()));

#ifdef OACAPTURE
  timer = new QAction ( QIcon ( ":/qt-icons/timer.png" ),
      tr ( "Timer" ), this );
  timer->setStatusTip ( tr ( "Configuration for Timer unit" ));
  connect ( timer, SIGNAL( triggered()), this, SLOT( doTimerSettings()));
#endif

  settingsMenu = menuBar()->addMenu ( tr ( "&Settings" ));
  settingsMenu->addAction ( general );
  settingsMenu->addAction ( capture );
#ifdef OACAPTURE
  settingsMenu->addAction ( cameraOpt );
#endif
  settingsMenu->addAction ( profiles );
  settingsMenu->addAction ( filters );
  settingsMenu->addAction ( demosaic );
  settingsMenu->addAction ( fits );
#ifdef OACAPTURE
  settingsMenu->addAction ( autorun );
  settingsMenu->addAction ( histogram );
#endif
  settingsMenu->addAction ( colourise );
#ifdef OACAPTURE
  settingsMenu->addAction ( timer );
#endif

  // For the moment we only add the advanced menu if there are filter
  // wheels with user-configurable interfaces

  int requireAdvanced = 0;
  for ( int i = 1; i < OA_FW_IF_COUNT && !requireAdvanced; i++ ) {
    if ( oaFilterWheelInterfaces[ i ].userConfigFlags ) {
      requireAdvanced = 1;
    }
  }

  if ( requireAdvanced ) {
    advancedMenu = menuBar()->addMenu ( tr ( "&Advanced" ));
    doAdvancedMenu();
  }

  // help menu

  about = new QAction ( tr ( "About" ), this );
  connect ( about, SIGNAL( triggered()), this, SLOT( aboutDialog()));

  helpMenu = menuBar()->addMenu ( tr ( "&Help" ));
  helpMenu->addAction ( about );
}


void
MainWindow::connectCamera ( int deviceIndex )
{
  int v, ret, attempt;

#ifdef OACAPTURE
  int format;

  if ( -1 == oldHistogramState ) {
    oldHistogramState = state.histogramOn;
  }
  state.histogramOn = 0;
#endif
  doDisconnectCam();

  for ( attempt = 0, ret = 1; ret == 1 && attempt < 2; attempt++ ) {
    if (( ret = commonState.camera->initialise ( cameraDevs[ deviceIndex ],
				APPLICATION_NAME, TOP_WIDGET ))) {
      if ( !attempt && ret == 1 ) {
        if ( connectedCameras == 1 ) {
          // we think a rescan should be sufficient to identify the camera
          // as there's only one that we know about.  deviceIndex should
          // be 0 by definition for this
          int retries = 5, haveCamera = 0;
          do {
            doCameraMenu(1);
            if ( connectedCameras ) {
              haveCamera = 1;
            } else {
              sleep(1);
            }
            retries--;
          } while ( !connectedCameras && retries );
          if ( haveCamera && connectedCameras == 1 ) {
            continue;
          }
          QMessageBox::warning ( TOP_WIDGET, APPLICATION_NAME,
              tr ( "The firmware has loaded, but a rescan is required "
              "and the camera must be selected again." ));
        } else {
          QMessageBox::warning ( TOP_WIDGET, APPLICATION_NAME,
              tr ( "The firmware has loaded, but a rescan is required "
              "and the camera must be selected again." ));
        }
      } else {
        QMessageBox::warning ( TOP_WIDGET, APPLICATION_NAME,
            tr ( "Unable to connect camera" ));
      }
#ifdef OACAPTURE
      state.histogramOn = oldHistogramState;
#endif
      return;
    }
  }

  disconnectCam->setEnabled( 1 );
  rescanCam->setEnabled( 0 );
  // Now it gets a bit messy.  The camera should get the settings from
  // the current profile, but the configure() functions take the current
  // values, set them in the camera and write them to the current
  // profile.
  if ( commonConfig.profileOption >= 0 && commonConfig.profileOption <
      profileConf.numProfiles && commonConfig.filterOption >= 0 &&
			commonConfig.filterOption < filterConf.numFilters ) {
    for ( uint8_t c = 1; c < OA_CAM_CTRL_LAST_P1; c++ ) {
      for ( uint8_t m = 1; m < OA_CAM_CTRL_MODIFIERS_LAST_P1; m++ ) {
        cameraConf.controlValues[ m ][ c ] =
          profileConf.profiles[ commonConfig.profileOption ].filterProfiles[
              commonConfig.filterOption ].controls[ m ][ c ];
      }
    }
    config.intervalMenuOption = profileConf.profiles[
				commonConfig.profileOption ].filterProfiles[
				commonConfig.filterOption ].intervalMenuOption;
  }
  configure();
  statusLine->showMessage ( commonState.camera->name() +
			tr ( " connected" ), 5000 );
  clearTemperature();
  clearDroppedFrames();
#ifdef OACAPTURE
  state.captureWidget->enableStartButton ( 1 );
  state.captureWidget->enableProfileSelect ( 1 );
  // FIX ME -- should these happen in the "configure" functions for each
  // widget?
  state.previewWidget->setVideoFramePixelFormat (
      commonState.camera->videoFramePixelFormat());
  state.cameraWidget->enableBinningControl (
			commonState.camera->hasBinning ( 2 ));
  v = commonState.camera->hasControl ( OA_CAM_CTRL_TEMPERATURE );
  state.previewWidget->enableTempDisplay ( v );
  styleStatusBarTemp ( v );
  v = commonState.camera->hasControl ( OA_CAM_CTRL_DROPPED );
  state.previewWidget->enableDroppedDisplay ( v );
  styleStatusBarDroppedFrames ( v );
  if ( state.settingsWidget ) {
    state.settingsWidget->enableTab ( commonState.cameraSettingsIndex, 1 );
  }
  cameraOpt->setEnabled ( 1 );

  enableFlipX();
  enableFlipY();
#else
  controlsWidget->enableButtons ( 1 );
  // FIX ME -- Enable profile selection
  // state.captureWidget->enableProfileSelect ( 1 );

  // FIX ME -- should these happen in the "configure" functions for each
  // widget?
  state.viewWidget->setVideoFramePixelFormat (
      commonState.camera->videoFramePixelFormat());

  // FIX ME -- enable binning
  // state.cameraWidget->enableBinningControl ( commonState.camera->hasBinning ( 2 ));
  v = commonState.camera->hasControl ( OA_CAM_CTRL_TEMPERATURE );
  styleStatusBarTemp ( v );
#endif

#ifdef OACAPTURE
  // start regardless of whether we're displaying or capturing the
  // data
  commonState.camera->startStreaming();
  state.controlWidget->disableAutoControls();
  state.histogramOn = oldHistogramState;
  oldHistogramState = -1;
#endif

#ifdef OACAPTURE
  format = commonState.camera->videoFramePixelFormat();
  state.captureWidget->enableTIFFCapture (
      ( !oaFrameFormats[ format ].rawColour ||
      ( config.demosaic && config.demosaicOutput )) ? 1 : 0 );
  state.captureWidget->enablePNGCapture (
      ( !oaFrameFormats[ format ].rawColour ||
      ( config.demosaic && config.demosaicOutput )) ? 1 : 0 );
  state.captureWidget->enableMOVCapture (( QUICKTIME_OK( format ) ||
      ( oaFrameFormats[ format ].rawColour && config.demosaic &&
      config.demosaicOutput )) ? 1 : 0 );
#endif
}


void
MainWindow::disconnectCamera ( void )
{
#ifdef OALIVE
  if ( state.controlsWidget ) {
    state.controlsWidget->disableAllButtons();
  }
#endif
  commonState.cameraTempValid = 0;
  commonState.binningValid = 0;
#ifdef OACAPTURE
  if ( state.settingsWidget ) {
    state.settingsWidget->enableTab ( commonState.cameraSettingsIndex, 0 );
  }
  cameraOpt->setEnabled ( 0 );
  oldHistogramState = state.histogramOn;
  state.histogramOn = 0;
  state.captureWidget->enableStartButton ( 0 );
  state.captureWidget->enableProfileSelect ( 0 );
#endif
  doDisconnectCam();
  statusLine->showMessage ( tr ( "Camera disconnected" ), 5000 );
}


void
MainWindow::doDisconnectCam ( void )
{
  if ( commonState.camera && commonState.camera->isInitialised()) {
#ifdef OACAPTURE
    if ( state.captureWidget ) {
      state.captureWidget->closeOutputHandler();
#else
    if ( state.controlsWidget ) {
      state.controlsWidget->closeOutputHandlers();
#endif
    }
    commonState.camera->stop();
    commonState.camera->disconnect();
    disconnectCam->setEnabled( 0 );
    rescanCam->setEnabled( 1 );
  }
}


void
MainWindow::rescanCameras ( void )
{
  doCameraMenu(0);
}


void
MainWindow::connectFilterWheel ( int deviceIndex )
{
  doDisconnectFilterWheel();
  if ( commonState.filterWheel->initialise ( filterWheelDevs[ deviceIndex ] )) {
    QMessageBox::warning ( TOP_WIDGET, APPLICATION_NAME,
        tr ( "Unable to connect filter wheel" ));
    return;
  }

  disconnectWheel->setEnabled( 1 );
  warmResetWheel->setEnabled( commonState.filterWheel->hasWarmReset());
  coldResetWheel->setEnabled( commonState.filterWheel->hasColdReset());
  rescanWheel->setEnabled( 0 );
  if ( !wheelStatus ) {
    wheelStatus = new QLabel();
    wheelStatus->setPixmap ( QPixmap ( QString::fromUtf8 (
        ":/qt-icons/filter-wheel.png" )));
    statusLine->insertWidget( 0, wheelStatus );
  }
  statusLine->addWidget ( wheelStatus );
  wheelStatus->show();
  statusLine->showMessage ( commonState.filterWheel->name() +
      tr ( " connected" ), 5000 );
  if ( commonState.filterWheel->hasSpeedControl()) {
    unsigned int speed;
    commonState.filterWheel->getSpeed ( &speed );
    if ( !speed ) {
      commonState.filterWheel->setSpeed ( 100, 0 );
    }
  }

  // FIX ME
  // need to set filter wheel position and filter setting in
  // capture widget to the same?
}


void
MainWindow::disconnectFilterWheel ( void )
{
  doDisconnectFilterWheel();
  statusLine->removeWidget ( wheelStatus );
  statusLine->showMessage ( tr ( "Filter wheel disconnected" ), 5000 );
}


void
MainWindow::warmResetFilterWheel ( void )
{
  if ( commonState.filterWheel && commonState.filterWheel->isInitialised()) {
    commonState.filterWheel->warmReset();
  }
  statusLine->showMessage ( tr ( "Filter wheel reset" ), 5000 );
}


void
MainWindow::coldResetFilterWheel ( void )
{
  if ( commonState.filterWheel && commonState.filterWheel->isInitialised()) {
    commonState.filterWheel->coldReset();
  }
  statusLine->showMessage ( tr ( "Filter wheel reset" ), 5000 );
}


void
MainWindow::rescanFilterWheels ( void )
{
  doFilterWheelMenu ( 0 );
}


void
MainWindow::doDisconnectFilterWheel ( void )
{
  if ( commonState.filterWheel && commonState.filterWheel->isInitialised()) {
    commonState.filterWheel->disconnect();
    disconnectWheel->setEnabled( 0 );
    warmResetWheel->setEnabled( 0 );
    coldResetWheel->setEnabled( 0 );
    rescanWheel->setEnabled( 1 );
  }
}


#ifdef OACAPTURE
void
MainWindow::connectTimer ( int deviceIndex )
{
  doDisconnectTimer();
  if ( commonState.timer->initialise ( timerDevs[ deviceIndex ] )) {
    QMessageBox::warning ( TOP_WIDGET, APPLICATION_NAME,
        tr ( "Unable to connect timer" ));
    return;
  }

  disconnectTimerDevice->setEnabled( 1 );
  resetTimerDevice->setEnabled( commonState.timer->hasReset());
  rescanTimer->setEnabled( 0 );
  if ( !timerStatus ) {
    timerStatus = new QLabel();
    timerStatus->setPixmap ( QPixmap ( QString::fromUtf8 (
        ":/qt-icons/timer.png" )));
    statusLine->insertWidget( 0, timerStatus );
  }
  statusLine->addWidget ( timerStatus );
  timerStatus->show();
  if ( commonState.timer->hasGPS()) {
    if ( commonState.timer->readGPS ( &state.latitude, &state.longitude,
        &state.altitude ) == OA_ERR_NONE ) {
      state.gpsValid = 1;
    }
  }
  statusLine->showMessage ( commonState.timer->name() +
			tr ( " connected" ), 5000 );
}


void
MainWindow::disconnectTimer ( void )
{
  state.gpsValid = 0;
  doDisconnectTimer();
  statusLine->removeWidget ( timerStatus );
  statusLine->showMessage ( tr ( "Timer disconnected" ), 5000 );
}


void
MainWindow::resetTimer ( void )
{
  if ( commonState.timer && commonState.timer->isInitialised()) {
    commonState.timer->reset();
  }
  statusLine->showMessage ( tr ( "Timer reset" ));
}


void
MainWindow::rescanTimers ( void )
{
  doTimerMenu ( 0 );
}


void
MainWindow::doDisconnectTimer ( void )
{
  if ( commonState.timer && commonState.timer->isInitialised()) {
    commonState.timer->disconnect();
    disconnectTimerDevice->setEnabled( 0 );
    resetTimerDevice->setEnabled( 0 );
    rescanTimer->setEnabled( 1 );
  }
}


void
MainWindow::setCapturedFrames ( unsigned int newVal )
{
  QString stringVal;

  stringVal.setNum ( newVal );
  capturedValue->setText ( stringVal );
}


void
MainWindow::setActualFrameRate ( unsigned int count )
{
  QString stringVal;

  stringVal.setNum ( count );
  fpsActualValue->setText ( stringVal );
  state.currentFPS = count;
}
#endif /* OACAPTURE */


void
MainWindow::setTemperature()
{
  float temp;
  QString stringVal;

	if ( !commonState.camera->isInitialised() ||
			!commonState.camera->hasControl ( OA_CAM_CTRL_TEMPERATURE )) {
		return;
	}

  temp = commonState.camera->getTemperature();
  commonState.cameraTempValid = 1;
  commonState.cameraTemp = temp;

  if ( updateTemperatureLabel == 1 ) {
    if ( generalConf.tempsInC ) {
      tempLabel->setText ( tr ( "Temp (C)" ));
    } else {
      tempLabel->setText ( tr ( "Temp (F)" ));
    }
    updateTemperatureLabel = 0;
  }

  if ( !generalConf.tempsInC ) {
    temp = temp * 9 / 5 + 32;
  }
  stringVal.setNum ( temp, 'g', 3 );
  tempValue->setText ( stringVal );
}


void
MainWindow::setStackedFrames()
{
  QString stringVal;
	int			num;

  num = state.viewWidget->getStackedFrames();
  stringVal.setNum ( num );
  stackedValue->setText ( stringVal );
}


void
MainWindow::setDroppedFrames()
{
  uint64_t dropped;
  QString stringVal;

	if ( !commonState.camera->isInitialised() ||
			!commonState.camera->hasControl ( OA_CAM_CTRL_DROPPED )) {
		return;
	}

  dropped = commonState.camera->readControl ( OA_CAM_CTRL_DROPPED );
  stringVal.setNum ( dropped );
  droppedValue->setText ( stringVal );
}


void
MainWindow::resetTemperatureLabel()
{
  updateTemperatureLabel = 1;
}


void
MainWindow::clearTemperature ( void )
{
  tempValue->setText ( "" );
}


void
MainWindow::clearDroppedFrames ( void )
{
  droppedValue->setText ( "" );
}


void
MainWindow::quit ( void )
{
  doingQuit = 1;
  doDisconnectCam();
  doDisconnectFilterWheel();
  writeConfig ( userConfigFile );
  qApp->quit();
}


#ifdef OACAPTURE
void
MainWindow::showFPSMaxValue ( int value )
{
  fpsMaxValue->setText ( QString::number ( value ));
}


void
MainWindow::clearFPSMaxValue ( void )
{
  fpsMaxValue->setText ( "" );
}
#endif


void
MainWindow::showStatusMessage ( QString message )
{
  statusLine->showMessage ( message );
}


#ifdef OACAPTURE
void
MainWindow::setNightMode ( int mode )
{
  // FIX ME -- need to set flag so subwindows can be started with the
  // correct stylesheet
  if ( mode ) {
    setNightStyleSheet ( this );
    if ( state.histogramWidget ) {
      setNightStyleSheet ( state.histogramWidget );
    }
    config.nightMode = 1;
  } else {
    clearNightStyleSheet ( this );
    if ( state.histogramWidget ) {
      clearNightStyleSheet ( state.histogramWidget );
    }
    config.nightMode = 0;
  }
  update();
}


void
MainWindow::enableHistogram ( void )
{
  if ( histogramOpt->isChecked()) {
    if ( !state.histogramWidget ) {
      state.histogramWidget = new HistogramWidget();
      // need to do this to be able to uncheck the menu item on closing
      state.histogramWidget->setAttribute ( Qt::WA_DeleteOnClose );
      if ( config.histogramOnTop ) {
        state.histogramWidget->setWindowFlags ( Qt::WindowStaysOnTopHint );
      }
      connect ( state.histogramWidget, SIGNAL( destroyed ( QObject* )), this,
          SLOT ( histogramClosed()));
    }
    state.histogramWidget->show();
    state.histogramOn = 1;
    config.showHistogram = 1;
    oldHistogramState = -1;
  } else {
    if ( state.histogramWidget ) {
      state.histogramWidget->hide();
    }
    state.histogramOn = 0;
    config.showHistogram = 0;
  }
}


void
MainWindow::histogramClosed ( void )
{
  state.histogramWidget = 0;
  state.histogramOn = 0;
  if ( !doingQuit ) {
    histogramOpt->setChecked ( 0 );
  }
  oldHistogramState = 0;
  // We don't want to change this if the histogram window is closing because
  // we exited
  if ( !doingQuit ) {
    config.showHistogram = 0;
  }
}


void
MainWindow::setNightStyleSheet ( QWidget* window )
{
  QString style = QString (
      "background: rgb( 92, 0, 0 ); "
      "border-color: rgb( 192, 0, 0 ); "
  );

  if ( state.needGroupBoxBorders ) {
    style += styleGroupBoxBorders;
  }

  window->setStyleSheet ( style );
}


void
MainWindow::clearNightStyleSheet ( QWidget* window )
{
  window->setStyleSheet ( state.needGroupBoxBorders ?
      styleGroupBoxBorders : "" );
}
#endif /* OACAPTURE */


void
MainWindow::enableReticle ( void )
{
  config.showReticle = reticle->isChecked() ? 1 : 0;
}


void
MainWindow::enableSpinner ( void )
{
  config.showSpinner = spinner->isChecked() ? 1 : 0;
	if ( !config.showSpinner ) {
		waitSpinner->stop();
	}
}


void
MainWindow::enableFocusAid ( void )
{
  config.showFocusAid = focusaid->isChecked() ? 1 : 0;
  if ( config.showFocusAid ) {
    focusOverlay->show();
  } else {
    focusOverlay->hide();
  }
}


#ifdef OACAPTURE
void
MainWindow::setFlipX ( int state )
{
  flipX->setChecked ( state );
}


void
MainWindow::setFlipY ( int state )
{
  flipY->setChecked ( state );
}


void
MainWindow::enableFlipX ( void )
{
  int flipState = flipX->isChecked() ? 1 : 0;

  config.flipX = flipState;
  if ( commonState.camera->isInitialised() &&
      commonState.camera->hasControl ( OA_CAM_CTRL_HFLIP )) {
    commonState.camera->setControl ( OA_CAM_CTRL_HFLIP, flipState );
    cameraConf.CONTROL_VALUE( OA_CAM_CTRL_HFLIP ) = flipState;
    SET_PROFILE_CONTROL( OA_CAM_CTRL_HFLIP, flipState );
    if ( state.settingsWidget ) {
      state.settingsWidget->enableFlipX ( flipState );
    }
    return;
  }
  state.previewWidget->enableFlipX ( flipState );
  if ( flipState ) {
    mosaicFlipWarning();
  }
}


void
MainWindow::enableFlipY ( void )
{
  int flipState = flipY->isChecked() ? 1 : 0;

  config.flipY = flipState;
  if ( commonState.camera->isInitialised() &&
      commonState.camera->hasControl ( OA_CAM_CTRL_VFLIP )) {
    commonState.camera->setControl ( OA_CAM_CTRL_VFLIP, flipState );
    cameraConf.CONTROL_VALUE( OA_CAM_CTRL_VFLIP ) = flipState;
    SET_PROFILE_CONTROL( OA_CAM_CTRL_VFLIP, flipState );
    if ( state.settingsWidget ) {
      state.settingsWidget->enableFlipY ( flipState );
    }
    return;
  }
  state.previewWidget->enableFlipY ( flipState );
  if ( flipState ) {
    mosaicFlipWarning();
  }
}


void
MainWindow::mosaicFlipWarning ( void )
{
  int format = commonState.camera->videoFramePixelFormat();

  if ( oaFrameFormats[ format ].rawColour ) {
    QMessageBox::warning ( TOP_WIDGET, APPLICATION_NAME,
        tr ( "Flipping a raw camera image may "
        "require a different colour mask to be used for demosaicking " ));
  }
}


void
MainWindow::enableDemosaic ( void )
{
  int demosaicState = demosaicOpt->isChecked() ? 1 : 0;
  int format;

  config.demosaic = demosaicState;
  state.previewWidget->enableDemosaic ( demosaicState );
  if ( commonState.camera->isInitialised()) {
    format = commonState.camera->videoFramePixelFormat();
    state.captureWidget->enableTIFFCapture (
        ( !oaFrameFormats[ format ].rawColour ||
        ( config.demosaic && config.demosaicOutput )) ? 1 : 0 );
    state.captureWidget->enablePNGCapture (
        ( !oaFrameFormats[ format ].rawColour ||
        ( config.demosaic && config.demosaicOutput )) ? 1 : 0 );
    state.captureWidget->enableMOVCapture (( QUICKTIME_OK( format ) ||
        ( oaFrameFormats[ format ].rawColour && config.demosaic &&
        config.demosaicOutput )) ? 1 : 0 );
  }
}
#endif


void
MainWindow::aboutDialog ( void )
{
  QMessageBox::about ( TOP_WIDGET, tr ( "About " APPLICATION_NAME ),
      tr ( "<h2>" APPLICATION_NAME " " VERSION_STR "</h2>"
      "<p>Copyright &copy; " COPYRIGHT_YEARS " " AUTHOR_NAME "<br/>"
      "&lt;" AUTHOR_EMAIL "&gt;</p>"
      "<p>" APPLICATION_NAME " is an open source video capture application "
      "for electronically-assisted astronomy."
      "<p>Thanks are due to numerous forum members for testing and "
      "encouragement, and to those manufacturers including ZW Optical, "
      "Celestron, The Imaging Source, First Light Optics, QHY and Xagyl "
      "who have provided documentation, Linux SDKs and other help without "
      "which this application would have taken much longer to create.</p>"
      "<p>Kudos is also due to the libusb project, "
      "libuvc and libhidapi, which I have hacked without mercy, as well as "
      "to many other open source projects that have provided inspiration, "
      "documentation and enlightenment where there was precious little "
      "otherwise.</p>" ));
}


void
MainWindow::setTimeRemaining ( void )
{
	uint64_t			timeRemaining;
	float					secsRemaining;
	QString				stringVal;

	if ( !commonState.camera->isInitialised() ||
			!commonState.camera->hasSingleShot() ||
			cameraConf.CONTROL_VALUE( OA_CAM_CTRL_EXPOSURE_ABSOLUTE ) < 3000000 ) {
		return;
	}

	// this value will be in usec.
	timeRemaining = commonState.camera->exposureTimeLeft();
	secsRemaining = timeRemaining / 1000000.0;

  stringVal.setNum ( secsRemaining, 'g', 2 );
  timeRemainingValue->setText ( stringVal );
}


void
MainWindow::doGeneralSettings ( void )
{
  createSettingsWidget();
  state.settingsWidget->setActiveTab ( commonState.generalSettingsIndex );
  state.settingsWidget->show();
}


void
MainWindow::doCaptureSettings ( void )
{
  createSettingsWidget();
  state.settingsWidget->setActiveTab ( commonState.captureSettingsIndex );
  state.settingsWidget->show();
}


#ifdef OACAPTURE
void
MainWindow::doCameraSettings ( void )
{
  createSettingsWidget();
  state.settingsWidget->setActiveTab ( commonState.cameraSettingsIndex );
  state.settingsWidget->show();
}
#endif


void
MainWindow::doProfileSettings ( void )
{
  createSettingsWidget();
  state.settingsWidget->setActiveTab ( commonState.profileSettingsIndex );
  state.settingsWidget->show();
}


void
MainWindow::doFilterSettings ( void )
{
  createSettingsWidget();
  state.settingsWidget->setActiveTab ( commonState.filterSettingsIndex );
  state.settingsWidget->show();
}


#ifdef OACAPTURE
void
MainWindow::doAutorunSettings ( void )
{
  createSettingsWidget();
  state.settingsWidget->setActiveTab ( commonState.autorunSettingsIndex );
  state.settingsWidget->show();
}


void
MainWindow::doHistogramSettings ( void )
{
  createSettingsWidget();
  state.settingsWidget->setActiveTab ( commonState.histogramSettingsIndex );
  state.settingsWidget->show();
}
#endif


void
MainWindow::doDemosaicSettings ( void )
{
  createSettingsWidget();
  state.settingsWidget->setActiveTab ( commonState.demosaicSettingsIndex );
  state.settingsWidget->show();
}


void
MainWindow::doFITSSettings ( void )
{
  createSettingsWidget();
  state.settingsWidget->setActiveTab ( commonState.fitsSettingsIndex );
  state.settingsWidget->show();
}


#ifdef OACAPTURE
void
MainWindow::doTimerSettings ( void )
{
  createSettingsWidget();
  state.settingsWidget->setActiveTab ( commonState.timerSettingsIndex );
  state.settingsWidget->show();
}
#endif


void
MainWindow::createSettingsWidget ( void )
{
  if ( !state.settingsWidget ) {
    state.settingsWidget = new SettingsWidget ( this, APPLICATION_NAME,
			OALIVE_SETTINGS, 0, 0, &trampolines );
    state.settingsWidget->setWindowFlags ( Qt::WindowStaysOnTopHint );
    state.settingsWidget->setAttribute ( Qt::WA_DeleteOnClose );
#ifdef OACAPTURE
    state.settingsWidget->enableTab ( commonState.cameraSettingsIndex,
        commonState.camera->isInitialised() ? 1 : 0 );
#endif
    connect ( state.settingsWidget, SIGNAL( destroyed ( QObject* )), this,
        SLOT ( settingsClosed()));
  }
}


void
MainWindow::settingsClosed ( void )
{
  state.settingsWidget = 0;
}


void
MainWindow::doCameraMenu ( int replaceSingleItem )
{
  int numDevs;
  int i;

  if ( rescanCam ) {
    rescanCam->setEnabled( 0 );
  }
  if ( connectedCameras && !replaceSingleItem ) {
    for ( i = 0; i < connectedCameras; i++ ) {
      cameraMenu->removeAction ( cameras[i] );
      delete cameras[i];
    }
    delete cameraSignalMapper;
  }

  if ( cameraDevs ) {
    commonState.camera->releaseInfo ( cameraDevs );
  }
  numDevs = commonState.camera->listConnected ( &cameraDevs,
			OA_CAM_FEATURE_NONE );

  if ( !replaceSingleItem ) {
    if ( numDevs > 0 ) {
      cameraSignalMapper = new QSignalMapper ( this );
      for ( i = 0; i < numDevs && cameraDevs[i]; i++ ) {
        QString iface (
            oaCameraInterfaces [ cameraDevs[i]->interface ].shortName );
        QString name ( cameraDevs[i]->deviceName );
        cameraMenuEntry[i] = "(" + iface + ") " + name;
        cameras[i] = new QAction ( cameraMenuEntry[i], this );
        if ( cameraMenuCreated ) {
          cameraMenu->insertAction ( cameraMenuSeparator, cameras[i] );
        } else {
          cameraMenu->addAction ( cameras[i] );
        }
        cameraSignalMapper->setMapping ( cameras[i], i );
        connect ( cameras[i], SIGNAL( triggered()), cameraSignalMapper,
            SLOT( map()));
      }
      connect ( cameraSignalMapper, SIGNAL( mapped ( int )), this,
          SLOT( connectCamera ( int )));
    }

    if ( !cameraMenuCreated ) {
      cameraMenuSeparator = cameraMenu->addSeparator();
      rescanCam = new QAction ( tr ( "Rescan" ), this );
      rescanCam->setStatusTip ( tr ( "Scan for newly connected devices" ));
      connect ( rescanCam, SIGNAL( triggered()), this, SLOT( rescanCameras() ));
      disconnectCam = new QAction ( tr ( "Disconnect" ), this );
      connect ( disconnectCam, SIGNAL( triggered()), this,
          SLOT( disconnectCamera()));
      disconnectCam->setEnabled( 0 );
      cameraMenu->addAction ( rescanCam );
      cameraMenu->addAction ( disconnectCam );
    }
  } else {
    if ( numDevs == 1 ) { // this should hopefully be true
      QString iface (
          oaCameraInterfaces [ cameraDevs[0]->interface ].shortName );
      QString name ( cameraDevs[0]->deviceName );
      cameraMenuEntry[0] = "(" + iface + ") " + name;
      cameras[0]->setText ( cameraMenuEntry[0] );
    } else {
      for ( i = 0; i < connectedCameras; i++ ) {
        cameras[i]->setEnabled(0);
      }
    }
  }

  cameraMenuCreated = 1;
  connectedCameras = numDevs;
  if ( rescanCam ) {
    rescanCam->setEnabled( 1 );
  }
}


void
MainWindow::doFilterWheelMenu ( int replaceSingleItem )
{
  int numFilterWheels = 0;
  int i;

  if ( rescanWheel ) {
    rescanWheel->setEnabled( 0 );
  }
  if ( connectedFilterWheels && !replaceSingleItem ) {
    for ( i = 0; i < connectedFilterWheels; i++ ) {
      filterWheelMenu->removeAction ( filterWheels[i] );
      delete filterWheels[i];
    }
    delete filterWheelSignalMapper;
  }

  if ( filterWheelDevs ) {
    commonState.filterWheel->releaseInfo ( filterWheelDevs );
  }
  numFilterWheels = commonState.filterWheel->listConnected ( &filterWheelDevs );

  if ( !replaceSingleItem ) {
    if ( numFilterWheels > 0 ) {
      filterWheelSignalMapper = new QSignalMapper ( this );
      for ( i = 0; i < numFilterWheels && filterWheelDevs[i]; i++ ) {
        QString name ( filterWheelDevs[i]->deviceName );
        filterWheelMenuEntry[i] = name;
        filterWheels[i] = new QAction ( filterWheelMenuEntry[i], this );
        if ( filterWheelMenuCreated ) {
          filterWheelMenu->insertAction ( filterWheelMenuSeparator,
              filterWheels[i] );
        } else {
          filterWheelMenu->addAction ( filterWheels[i] );
        }
        filterWheelSignalMapper->setMapping ( filterWheels[i], i );
        connect ( filterWheels[i], SIGNAL( triggered()),
            filterWheelSignalMapper, SLOT( map()));
      }
      connect ( filterWheelSignalMapper, SIGNAL( mapped ( int )), this,
          SLOT( connectFilterWheel ( int )));
    }

    if ( !filterWheelMenuCreated ) {
      filterWheelMenuSeparator = filterWheelMenu->addSeparator();

      warmResetWheel = new QAction ( tr ( "Warm Reset" ), this );
      warmResetWheel->setStatusTip ( tr (
          "Send a 'warm reset' command to the wheel" ));
      connect ( warmResetWheel, SIGNAL( triggered()), this, SLOT(
          warmResetFilterWheel()));
      warmResetWheel->setEnabled( 0 );

      coldResetWheel = new QAction ( tr ( "Cold Reset" ), this );
      coldResetWheel->setStatusTip ( tr (
          "Send a 'cold reset' command to the wheel" ));
      connect ( coldResetWheel, SIGNAL( triggered()), this, SLOT(
          coldResetFilterWheel()));
      coldResetWheel->setEnabled( 0 );

      rescanWheel = new QAction ( tr ( "Rescan" ), this );
      rescanWheel->setStatusTip ( tr ( "Scan for newly connected wheels" ));
      connect ( rescanWheel, SIGNAL( triggered()), this, SLOT(
          rescanFilterWheels()));

      disconnectWheel = new QAction ( tr ( "Disconnect" ), this );
      connect ( disconnectWheel, SIGNAL( triggered()), this,
          SLOT( disconnectFilterWheel()));
      disconnectWheel->setEnabled( 0 );

      filterWheelMenu->addAction ( warmResetWheel );
      filterWheelMenu->addAction ( coldResetWheel );
      filterWheelMenu->addAction ( rescanWheel );
      filterWheelMenu->addAction ( disconnectWheel );
    }
  } else {
    if ( numFilterWheels == 1 ) { // this should hopefully be true
      QString name ( filterWheelDevs[0]->deviceName );
      filterWheelMenuEntry[0] = name;
      filterWheels[0]->setText ( filterWheelMenuEntry[0] );
    } else {
      for ( i = 0; i < connectedFilterWheels; i++ ) {
        filterWheels[i]->setEnabled(0);
      }
    }
  }

  filterWheelMenuCreated = 1;
  connectedFilterWheels = numFilterWheels;
  if ( rescanWheel ) {
    rescanWheel->setEnabled( 1 );
  }
}


#ifdef OACAPTURE
void
MainWindow::doTimerMenu ( int replaceSingleItem )
{
  int numTimers = 0;
  int i;

  if ( rescanTimer ) {
    rescanTimer->setEnabled( 0 );
  }
  if ( connectedTimers && !replaceSingleItem ) {
    for ( i = 0; i < connectedTimers; i++ ) {
      timerMenu->removeAction ( timers[i] );
      delete timers[i];
    }
    delete timerSignalMapper;
  }

  if ( timerDevs ) {
    commonState.timer->releaseInfo ( timerDevs );
  }
  numTimers = commonState.timer->listConnected ( &timerDevs );

  if ( !replaceSingleItem ) {
    if ( numTimers > 0 ) {
      timerSignalMapper = new QSignalMapper ( this );
      for ( i = 0; i < numTimers && timerDevs[i]; i++ ) {
        QString name ( timerDevs[i]->deviceName );
        timerMenuEntry[i] = name;
        timers[i] = new QAction ( timerMenuEntry[i], this );
        if ( timerMenuCreated ) {
          timerMenu->insertAction ( timerMenuSeparator,
              timers[i] );
        } else {
          timerMenu->addAction ( timers[i] );
        }
        timerSignalMapper->setMapping ( timers[i], i );
        connect ( timers[i], SIGNAL( triggered()),
            timerSignalMapper, SLOT( map()));
      }
      connect ( timerSignalMapper, SIGNAL( mapped ( int )), this,
          SLOT( connectTimer ( int )));
    }

    if ( !timerMenuCreated ) {
      timerMenuSeparator = timerMenu->addSeparator();

      resetTimerDevice = new QAction ( tr ( "Reset" ), this );
      resetTimerDevice->setStatusTip ( tr ( "Reset the timer device" ));
      connect ( resetTimerDevice, SIGNAL( triggered()), this,
          SLOT( resetTimer()));
      resetTimerDevice->setEnabled( 0 );

      rescanTimer = new QAction ( tr ( "Rescan" ), this );
      rescanTimer->setStatusTip ( tr ( "Scan for newly-connected timers" ));
      connect ( rescanTimer, SIGNAL( triggered()), this, SLOT(
          rescanTimers()));

      disconnectTimerDevice = new QAction ( tr ( "Disconnect" ), this );
      connect ( disconnectTimerDevice, SIGNAL( triggered()), this,
          SLOT( disconnectTimer()));
      disconnectTimerDevice->setEnabled( 0 );

      timerMenu->addAction ( resetTimerDevice );
      timerMenu->addAction ( rescanTimer );
      timerMenu->addAction ( disconnectTimerDevice );
    }
  } else {
    if ( numTimers == 1 ) { // this should hopefully be true
      QString name ( timerDevs[0]->deviceName );
      timerMenuEntry[0] = name;
      timers[0]->setText ( timerMenuEntry[0] );
    } else {
      for ( i = 0; i < connectedTimers; i++ ) {
        timers[i]->setEnabled(0);
      }
    }
  }

  timerMenuCreated = 1;
  connectedTimers = numTimers;
  if ( rescanTimer ) {
    rescanTimer->setEnabled( 1 );
  }
}
#endif /* OACAPTURE */


void
MainWindow::closeSettingsWindow ( void )
{
  if ( state.settingsWidget ) {
    state.settingsWidget->close();
    state.settingsWidget = 0;
  }
}


void
MainWindow::destroyLayout ( QLayout* layout )
{
  QLayoutItem* item;
  QLayout*     sublayout;
  QWidget*     widget;

  if ( !layout ) { return; }

  while (( item = layout->takeAt(0))) {
    if (( sublayout = item->layout())) {
      destroyLayout ( sublayout );
    }
    else if (( widget = item->widget())) {
      widget->hide();
      delete widget;
    } else {
      delete item;
    }
  }

  delete layout;
}


void
MainWindow::styleStatusBarTemp ( int state )
{
  tempLabel->setEnabled ( state );
  tempValue->setEnabled ( state );
}


#ifdef OACAPTURE
void
MainWindow::styleStatusBarDroppedFrames ( int state )
{
  droppedLabel->setEnabled ( state );
  droppedValue->setEnabled ( state );
}
#endif


void
#ifdef OACAPTURE
MainWindow::createPreviewWindow()
#else
MainWindow::createViewWindow()
#endif
{
#ifdef OACAPTURE
  int height, width;
  int minHeight = 600, minWidth = 800;

  previewScroller = new QScrollArea ( this );
  focusOverlay = new FocusOverlay ( previewScroller );
  state.focusOverlay = focusOverlay;
  previewWidget = new PreviewWidget ( previewScroller );
  state.previewWidget = previewWidget;
#else
  viewScroller = new QScrollArea ( this );
  focusOverlay = new FocusOverlay ( viewScroller );
  viewWidget = new ViewWidget ( viewScroller );
  state.viewWidget = viewWidget;
	commonState.viewerWidget = dynamic_cast<QWidget*>( viewWidget );
#endif

#ifdef OACAPTURE
  // These figures are a bit arbitrary, but give a size that should work
  // initially on small displays
  QRect rec = QApplication::desktop()->availableGeometry (
      QApplication::desktop()->primaryScreen());
  height = rec.height();
  width = rec.width();
  if ( height < 1024 || width < 1280 ) {
    if ( height < 600 || width < 800 ) {
      minWidth = 640;
      minHeight = 480;
    } else {
      minWidth = 800;
      minHeight = 600;
    }
  } else {
    height *= 2.0/3.0;
    width *= 2.0/3.0;
    if ( minHeight > height ) {
      minHeight = height;
    }
    if ( minWidth > width ) {
      minWidth = width;
    }
  }

  previewScroller->setMinimumSize ( minWidth, minHeight );
  previewScroller->setSizePolicy( QSizePolicy::Expanding,
      QSizePolicy::Expanding );
  previewScroller->setFocusPolicy( Qt::NoFocus );
  previewScroller->setContentsMargins( 0, 0, 0, 0 );
  previewScroller->setWidget ( previewWidget );
#else
  viewScroller->setMinimumSize ( 800, 600 );
  viewScroller->setSizePolicy( QSizePolicy::Expanding,
      QSizePolicy::Expanding );
  viewScroller->setFocusPolicy( Qt::NoFocus );
  viewScroller->setContentsMargins( 0, 0, 0, 0 );
  viewScroller->setWidget ( viewWidget );
#endif

#ifdef OACAPTURE
  if ( !config.preview ) {
    previewScroller->hide();
  }

  if ( config.dockableControls || config.separateControls ) {
    setCentralWidget ( previewScroller );
  } else {
    if ( config.controlsOnRight ) {
      dummyWidget = new QWidget ( this );
      dummyWidget->setLayout ( vertControlsBox );
      splitter->addWidget ( previewScroller );
      splitter->addWidget ( dummyWidget );
    } else {
      dummyWidget = new QWidget ( this );
      dummyWidget->setLayout ( horizControlsBox );
      splitter->setOrientation ( Qt::Vertical );
      splitter->addWidget ( dummyWidget );
      splitter->addWidget ( previewScroller );
    }
  }
#else
  splitter->addWidget ( viewScroller );
  splitter->addWidget ( controlsWidget );
#endif
}


#ifdef OACAPTURE
void
MainWindow::changePreviewState ( int newState )
{
  if ( newState == Qt::Unchecked ) {
    config.preview = 0;
    previewScroller->hide();
    if ( previewWidget ) {
      previewWidget->setEnabled ( 0 );
    }
  } else {
    config.preview = 1;
    previewScroller->show();
    if ( previewWidget ) {
      previewWidget->setEnabled ( 1 );
    }
  }
}
#endif


void
MainWindow::configure ( void )
{
#ifdef OACAPTURE
  cameraWidget->configure();
  imageWidget->configure();
  controlWidget->configure();
  previewWidget->configure();
#else
  viewWidget->configure();
  controlsWidget->configure();
#endif
}


void
MainWindow::createControlWidgets ( void )
{
#ifdef OACAPTURE
  cameraWidget = new CameraWidget ( this );
  imageWidget = new ImageWidget ( this );
  zoomWidget = new ZoomWidget ( this );
  controlWidget = new ControlWidget ( this );
  captureWidget = new CaptureWidget ( this );

  imageZoomBox = new QVBoxLayout();
  imageZoomBox->addWidget ( imageWidget );
  imageZoomBox->addWidget ( zoomWidget );

  if ( config.dockableControls && !config.separateControls ) {
    cameraDock = new QDockWidget ( tr ( "Camera" ), this );
    cameraDock->setFeatures ( QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable );
    cameraDock->setWidget ( cameraWidget );
    cameraWidget->show();

    imageZoomDock = new QDockWidget ( tr ( "Image/Zoom" ), this );
    imageZoomDock->setFeatures ( QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable );
    imageZoomWidget = new QWidget ( this );
    imageZoomWidget->setLayout ( imageZoomBox );
    imageZoomDock->setWidget ( imageZoomWidget );
    imageZoomWidget->show();

    controlDock = new QDockWidget ( tr ( "Control" ), this );
    controlDock->setFeatures ( QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable );
    controlDock->setWidget ( controlWidget );
    controlWidget->show();

    captureDock = new QDockWidget ( tr ( "Capture" ), this );
    captureDock->setFeatures ( QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable );
    captureDock->setWidget ( captureWidget );
    captureWidget->show();

    enum Qt::DockWidgetArea where = config.controlsOnRight ?
        Qt::RightDockWidgetArea : Qt::TopDockWidgetArea;
    addDockWidget ( where, cameraDock );
    addDockWidget ( where, imageZoomDock );
    addDockWidget ( where, controlDock );
    addDockWidget ( where, captureDock );

  } else {

    // we can't set a MainWindow layout directly, so we need to add a
    // new widget as the central widget and add the layout to that

    if ( config.separateControls ) {

      controlWindow = new QMainWindow;
      dummyWidget = new QWidget ( controlWindow );
      controlWindow->setCentralWidget ( dummyWidget );
      controlWindow->setWindowTitle ( tr ( "oaCapture controls" ));
      Qt::WindowFlags flags = controlWindow->windowFlags();
      flags &= ~Qt::WindowCloseButtonHint;
      controlWindow->setWindowFlags ( flags );
      gridBox = new QGridLayout();
      gridBox->addWidget ( cameraWidget, 0, 0 );
      gridBox->addLayout ( imageZoomBox, 0, 1 );
      gridBox->addWidget ( controlWidget, 1, 0 );
      gridBox->addWidget ( captureWidget, 1, 1 );
      dummyWidget->setLayout ( gridBox );
      controlWindow->show();

    } else {

      splitter = new QSplitter ( this );
      setCentralWidget ( splitter );

      if ( config.controlsOnRight ) {
        vertControlsBox = new QVBoxLayout();
        vertControlsBox->addWidget ( cameraWidget );
        vertControlsBox->addLayout ( imageZoomBox );
        vertControlsBox->addWidget ( controlWidget );
        vertControlsBox->addWidget ( captureWidget );
        vertControlsBox->addStretch ( 1 );
      } else {
        horizControlsBox = new QHBoxLayout();
        horizControlsBox->addWidget ( cameraWidget );
        horizControlsBox->addLayout ( imageZoomBox );
        horizControlsBox->addWidget ( controlWidget );
        horizControlsBox->addWidget ( captureWidget );
        horizControlsBox->addStretch ( 1 );
      }
    }
  }

  state.controlWidget = controlWidget;
  state.imageWidget = imageWidget;
  state.zoomWidget = zoomWidget;
  state.cameraWidget = cameraWidget;
  state.captureWidget = captureWidget;

#else

  controlsWidget = new ControlsWidget ( this );
  state.controlsWidget = controlsWidget;
  splitter = new QSplitter ( this );
  setCentralWidget ( splitter );

#endif
}


void
MainWindow::doAdvancedMenu( void )
{
  int i, numFilterWheelActions, totalActions;

  if ( advancedFilterWheelSignalMapper ) {
    totalActions = advancedActions.count();
    if ( totalActions ) {
      for ( i = 0; i < totalActions; i++ ) {
        advancedMenu->removeAction ( advancedActions[i] );
      }
    }
    delete advancedFilterWheelSignalMapper;
    advancedFilterWheelSignalMapper = 0;
  }

  advancedActions.clear();
  advancedFilterWheelSignalMapper = new QSignalMapper ( this );

  numFilterWheelActions = totalActions = 0;
  for ( i = 1; i < OA_FW_IF_COUNT; i++ ) {
    if ( oaFilterWheelInterfaces[ i ].userConfigFlags ) {
      QString label = QString ( oaFilterWheelInterfaces[ i ].name );
      label += " " + QString ( tr ( "filter wheels" ));
      advancedActions.append ( new QAction ( label, this ));
      advancedMenu->addAction ( advancedActions[ totalActions ]);
      advancedFilterWheelSignalMapper->setMapping (
          advancedActions[ totalActions ],
          oaFilterWheelInterfaces[ i ].interfaceType );
      connect ( advancedActions[ totalActions ], SIGNAL( triggered()),
            advancedFilterWheelSignalMapper, SLOT( map()));
      totalActions++;
      numFilterWheelActions++;
    }
  }

  if ( numFilterWheelActions ) {
    connect ( advancedFilterWheelSignalMapper, SIGNAL( mapped ( int )),
        this, SLOT( advancedFilterWheelHandler ( int )));
  }

#ifdef OACAPTURE
  QString label = QString ( "PTR" );
  label += " " + QString ( tr ( "timers" ));
  advancedActions.append ( new QAction ( label, this ));
  advancedMenu->addAction ( advancedActions[ totalActions ]);
  connect ( advancedActions[ totalActions ], SIGNAL( triggered()),
      this, SLOT( advancedPTRHandler()));
#endif
}


void
MainWindow::advancedFilterWheelHandler ( int interfaceType )
{
  if ( !state.advancedSettings ) {
    state.advancedSettings = new AdvancedSettings ( this,
				OA_DEVICE_FILTERWHEEL, interfaceType, &trampolines );
    state.advancedSettings->setAttribute ( Qt::WA_DeleteOnClose );
    connect ( state.advancedSettings, SIGNAL( destroyed ( QObject* )), this,
        SLOT ( advancedClosed()));
  }

  state.advancedSettings->show();
}


#ifdef OACAPTURE
void
MainWindow::advancedPTRHandler ( void )
{
  if ( !state.advancedSettings ) {
    state.advancedSettings = new AdvancedSettings ( this,
				OA_DEVICE_PTR, 0, &trampolines );
    state.advancedSettings->setAttribute ( Qt::WA_DeleteOnClose );
    connect ( state.advancedSettings, SIGNAL( destroyed ( QObject* )), this,
        SLOT ( advancedClosed()));
  }

  state.advancedSettings->show();
}
#endif


void
MainWindow::advancedClosed ( void )
{
  state.advancedSettings = 0;
}


void
MainWindow::closeAdvancedWindow ( void )
{
  if ( state.advancedSettings ) {
    state.advancedSettings->close();
    state.advancedSettings = 0;
  }
}


void
MainWindow::doColouriseSettings ( void )
{
  if ( config.numCustomColours ) {
    for ( int i = 0; i < config.numCustomColours; i++ ) {
      QColorDialog::setCustomColor ( i, config.customColours[i].rgb());
    }
  }

  QColor chosenColour = QColorDialog::getColor();
  if ( chosenColour.isValid()) {
    config.currentColouriseColour = chosenColour;
#ifdef OACAPTURE
    if ( state.previewWidget ) {
      state.previewWidget->setMonoPalette ( config.currentColouriseColour );
#else
    if ( state.viewWidget ) {
      state.viewWidget->setMonoPalette ( config.currentColouriseColour );
#endif
    }
  }
  config.numCustomColours = colourDialog->customCount();
  if ( config.numCustomColours ) {
    config.customColours.clear();
    for ( int i = 0; i < config.numCustomColours; i++ ) {
#if HAVE_QT4
      QRgb custCol = colourDialog->customColor ( i );
      config.customColours.append ( QColor ( custCol ));
#else
      QColor custCol = colourDialog->customColor ( i );
      config.customColours.append ( custCol );
#endif
    }
  }
}


void
MainWindow::reveal ( void )
{
  show();
}


void
MainWindow::frameWriteFailedPopup ( void )
{
  QMessageBox::warning ( TOP_WIDGET, APPLICATION_NAME,
      tr ( "Error saving captured frame" ));
}


void
MainWindow::updateConfig ( void )
{
  writeConfig ( userConfigFile );
}


void
MainWindow::outputUnwritable ( void )
{
  QMessageBox::warning ( TOP_WIDGET, tr ( "Start Recording" ),
      tr ( "Output is not writable" ));
}


int
MainWindow::outputExists ( void )
{
  return QMessageBox::question ( TOP_WIDGET, tr ( "Start Recording" ),
      tr ( "Output file exists.  OK to overwrite?" ), QMessageBox::No |
      QMessageBox::Yes, QMessageBox::No );
}


void
MainWindow::outputExistsUnwritable ( void )
{
  QMessageBox::warning ( TOP_WIDGET, tr ( "Start Recording" ),
      tr ( "Output file exists and is not writable" ));
}


void
MainWindow::createFileFailed ( void )
{
  QMessageBox::warning ( TOP_WIDGET, APPLICATION_NAME,
      tr ( "Unable to create file for output" ));
}


void
MainWindow::showSpinner ( int enable )
{
	if ( enable ) {
		if ( config.showSpinner ) {
			waitSpinner->start();
		}
	} else {
		waitSpinner->stop();
	}
}
