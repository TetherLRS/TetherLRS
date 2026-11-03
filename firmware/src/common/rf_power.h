//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
// OlliW @ www.olliw.eu
// TetherLRS changes: Copyright (c) TetherLRS project
//*******************************************************
// RF Power dynamic handling
//********************************************************
#ifndef RF_POWER_H
#define RF_POWER_H
#pragma once


#include "hal/hal.h"
#include "dynamic_power.h"


class tRfPower
{
  public:
    void Init(void);
    void Update(void);
    void Set(uint8_t power);
    void Dynamic(bool enabled, bool connected, int8_t peer_rssi_dbm, uint8_t peer_lq, int8_t sensitivity_dbm);
    int8_t Current_dbm(void) { return (rfpower_current_idx < RFPOWER_LIST_NUM) ? rfpower_list[rfpower_current_idx].dbm : 0; }
    uint8_t Code(void) { return (rfpower_current_idx < 7) ? rfpower_current_idx + 1 : 0; } // 3 bits in the Rx frames

  private:
    uint8_t rfpower_current_idx;
    uint8_t rfpower_new_idx; // the configured power, the ceiling for the dynamic one
    uint8_t dyn_idx;
    tDynPower dynpower;
    int8_t table_dbm[RFPOWER_LIST_NUM];
};


void tRfPower::Init(void)
{
    rfpower_current_idx = UINT8_MAX;  // to initialize properly
    rfpower_new_idx = rfpower_current_idx; // to prevent update before first Set
    dyn_idx = UINT8_MAX;
    for (uint8_t i = 0; i < RFPOWER_LIST_NUM; i++) table_dbm[i] = rfpower_list[i].dbm;
    dynpower.Init(table_dbm, RFPOWER_LIST_NUM);
}


// called once per frame cycle, after Set()
void tRfPower::Dynamic(bool enabled, bool connected, int8_t peer_rssi_dbm, uint8_t peer_lq, int8_t sensitivity_dbm)
{
    if (!enabled || rfpower_new_idx >= RFPOWER_LIST_NUM) {
        dyn_idx = UINT8_MAX;
        return;
    }
    dyn_idx = dynpower.Step(millis32(), connected, peer_rssi_dbm, peer_lq, sensitivity_dbm, rfpower_new_idx);
}


void tRfPower::Update(void)
{
    uint8_t idx = (dyn_idx < rfpower_new_idx) ? dyn_idx : rfpower_new_idx;

    if (idx == rfpower_current_idx) return; // no change required

    rfpower_current_idx = idx;

    if (rfpower_current_idx >= RFPOWER_LIST_NUM) { // should not happen, play it safe
        rfpower_current_idx = RFPOWER_LIST_NUM - 1;
        rfpower_new_idx = rfpower_current_idx;
    }

    Config.Sx.Power_dbm = rfpower_list[rfpower_current_idx].dbm;
    Config.Sx2.Power_dbm = Config.Sx.Power_dbm;

    sx.UpdateRfPower(&Config.Sx);
    sx2.UpdateRfPower(&Config.Sx2);
}


void tRfPower::Set(uint8_t power)
{
    rfpower_new_idx = (power > Config.PowerIdxMax) ? Config.PowerIdxMax : power; // the region caps it
}


#endif // RF_POWER_H



