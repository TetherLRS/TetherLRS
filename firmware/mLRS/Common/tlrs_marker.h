//*******************************************************
// TetherLRS
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// The text every image carries for the tool, read from its flash: role, board, version and the board's bands
//********************************************************
#ifndef TLRS_MARKER_H
#define TLRS_MARKER_H

#ifdef FREQUENCY_BAND_2P4_GHZ
  #define TLRS_BAND_2P4 "2.4 GHz,"
#else
  #define TLRS_BAND_2P4 ""
#endif
#ifdef FREQUENCY_BAND_915_MHZ_FCC
  #define TLRS_BAND_915 "915 MHz,"
#else
  #define TLRS_BAND_915 ""
#endif
#ifdef FREQUENCY_BAND_868_MHZ
  #define TLRS_BAND_868 "868 MHz,"
#else
  #define TLRS_BAND_868 ""
#endif
#ifdef FREQUENCY_BAND_866_MHZ_IN
  #define TLRS_BAND_866 "866 MHz,"
#else
  #define TLRS_BAND_866 ""
#endif
#ifdef FREQUENCY_BAND_433_MHZ
  #define TLRS_BAND_433 "433 MHz,"
#else
  #define TLRS_BAND_433 ""
#endif
#if defined FREQUENCY_BAND_70_CM || defined FREQUENCY_BAND_70_CM_HAM
  #define TLRS_BAND_70CM "70 cm,"
#else
  #define TLRS_BAND_70CM ""
#endif

#define TLRS_MARKER(role) \
    "TetherLRS:" role ":" DEVICE_NAME ":" VERSIONONLYSTR ":" \
    TLRS_BAND_2P4 TLRS_BAND_915 TLRS_BAND_868 TLRS_BAND_866 TLRS_BAND_433 TLRS_BAND_70CM ":"

#endif // TLRS_MARKER_H
