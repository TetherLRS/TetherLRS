//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// Antenna Switch Diversity
//********************************************************
// One radio with an RF switch between two antennas (ELRS ant_ctrl), as on the DAKEFPV 900 MAX 1W.
// Ported from ExpressLRS (GPL-3.0): updateDiversity() and switchAntenna() in src/src/rx_main.cpp, the LPF class in
// src/include/LowPassFilter.h. ELRS runs it on receivers; here both ends receive every frame period, so the Rx and
// the Tx use it alike.
//********************************************************
#ifndef ANTENNA_SWITCH_H
#define ANTENNA_SWITCH_H
#pragma once


#include <inttypes.h>


#define ANTSW_INTERVAL            5 // frame periods, ELRS DIVERSITY_ANTENNA_INTERVAL
#define ANTSW_RSSI_TRIGGER        5 // dB, ELRS DIVERSITY_ANTENNA_RSSI_TRIGGER


// ELRS LPF with Beta 5 and 5 fractional bits
class tAntennaSwitchLpf
{
  public:
    void Reset(void) { need_reset = true; }
    int32_t Value(void) { return value_int; }

    void Update(int32_t in)
    {
        if (need_reset) {
            need_reset = false;
            value_int = in;
            value_fp = in << 5;
            return;
        }
        value_fp = (value_fp << 5) - value_fp;
        value_fp += in << 5;
        value_fp >>= 5;
        value_int = value_fp >> 5;
    }

  private:
    bool need_reset = true;
    int32_t value_int = 0;
    int32_t value_fp = 0;
};


class tAntennaSwitch
{
  public:
    void Init(void)
    {
        antenna = 0;
        prev_rssi = 0;
        lq_drop_trigger = 0;
        rssi_drop_trigger = 0;
        lpf[0].Reset();
        lpf[1].Reset();
        sx_antenna_switch_set(antenna);
    }

    // once per frame period, after a frame was or should have been received; rssi only counts with a valid frame
    void Update(bool frame_received, int8_t rssi)
    {
        if (frame_received) lpf[antenna].Update(rssi);

        int32_t cur_rssi = lpf[antenna].Value();
        int32_t other_rssi = lpf[1 - antenna].Value();

        if ((cur_rssi < (prev_rssi - ANTSW_RSSI_TRIGGER)) && rssi_drop_trigger >= ANTSW_INTERVAL) {
            Switch();
            lq_drop_trigger = 1;
            rssi_drop_trigger = 0;
        } else if (cur_rssi > prev_rssi || rssi_drop_trigger < ANTSW_INTERVAL) {
            prev_rssi = cur_rssi;
            rssi_drop_trigger++;
        }

        if (!frame_received && lq_drop_trigger == 0) {
            Switch();
            lq_drop_trigger = 1;
            rssi_drop_trigger = 0;
        } else if (lq_drop_trigger >= ANTSW_INTERVAL) {
            // switched ANTSW_INTERVAL periods ago: both antennas have a fresh RSSI, go back if it got worse
            if (cur_rssi < other_rssi) {
                Switch();
                lq_drop_trigger = 1;
                rssi_drop_trigger = 0;
            } else {
                lq_drop_trigger = 0;
            }
        } else if (lq_drop_trigger > 0) {
            lq_drop_trigger++;
        }
    }

    uint8_t Antenna(void) { return antenna; }

  private:
    void Switch(void)
    {
        antenna = 1 - antenna;
        lpf[antenna].Reset(); // discard the value this antenna had before the other one was in use
        sx_antenna_switch_set(antenna);
    }

    uint8_t antenna;
    tAntennaSwitchLpf lpf[2];
    int32_t prev_rssi;
    int32_t lq_drop_trigger;
    int32_t rssi_drop_trigger;
};


#endif // ANTENNA_SWITCH_H
