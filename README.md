# openastro
Open Astro Project - With support for SVBony SV605MC

- Changes to make SVBony SV605MC work with oacapture:
    - Fixed initialization of camera
        - Added proper sequence of commands to initialize the camera
        - Used correct image modes (Y8/Y16 vs RAW8/RAW16)
    - ROI and binning now work (even together)
    - Added support for temperature / cooler control
- Histogram: can now show logarithmic scale (configurable)
- Implemented missing functions for saving and loading configuration files
- Fixed problem with profile targets (Earth is now included to avoid them)
- Added checkboxes to store capture files in directories with the profile name and/or date (created if necessary)

Notes:
- Only tested with SV605MC, have no other camera
- Only tested on Linux
- configure.ac has been modified to compile only with SVBony support
- Modified some QT object sizes to let text show properly
- There is a scaling issue on my 1600x900 screen. I fix it by starting the app with: QT_FONT_DPI=96 ./oacapture
- Not working/missing/todo:
    - Profiles do not restore previous exposure range
    - Target temperature is not restored
    - Selectable controls are not restored
    - Histogram window position is not restored
    - Previous profile is not used when starting (default is always used)
