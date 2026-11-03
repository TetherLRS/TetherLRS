# TetherLRS - ArduPilot Tools #

This folder contains a collection of scripts for use with ArduPilot systems.

## Link Statistics Logging: mlrs_mavlink_link_stats.lua

This Lua script is intended to be installed on an ArduPilot flight controller. It logs the TetherLRS link statistics (the MLRS_RADIO_LINK_STATS and MLRS_RADIO_LINK_INFORMATION messages the receiver sends to ArduPilot 4.7 or later) to the DataFlash log.

Works with TetherLRS v2.0 and later, and with mLRS v1.3.04 and later.

Installation:
- set SCR_ENABLE = 1
- copy the Lua script to the APM/SCRIPTS/ directory on the flight controller's microSD card
- restart the flight controller

Required receiver settings:
- 'Rx Ser Link Mode' = 'mavlink' or 'mavlinkX' ('mavlinkX' should be preferred)

The script creates two ArduPilot parameters:
- MLRS_SCR_ENABLE: enables or disables logging. 0: disabled, >= 1: enabled (default is 1)
- MLRS_SCR_DBG:    sets the debug level. 0: disabled. 1: level 1, 2: level2, 3: all (default is 0)

The script adds five new DataFlash log message types:
- MLR1: rx_lq_rc, rx_lq_ser, tx_lq_ser, flags
- MLR2: rx_rssi1, rx_snr1, tx_rssi1, tx_snr1, f1
- MLR3: rx_rssi2, rx_snr2, tx_rssi2, tx_snr2, f2
- MLR4: fr_rate, mode, tx_pwr, rx_pwr
- MLR5: tx_ser_rate, rx_ser_rate, tx_sen, rx_sen
