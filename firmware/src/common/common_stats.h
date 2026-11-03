//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
// OlliW @ www.olliw.eu
// TetherLRS changes: Copyright (c) TetherLRS project
//*******************************************************
// Common Statistics
//*******************************************************
#ifndef COMMON_STATS_H
#define COMMON_STATS_H
#pragma once

#include <stdint.h>
#include "common_conf.h"
#include "hal/device_conf.h"
#include "libs/filters.h"
#include "lq_counter.h"
#include "common_types.h"


extern bool connected(void);


//-------------------------------------------------------
// Common stats
//-------------------------------------------------------

class tStats
{
  public:
    void Init(uint8_t _maverage_period, uint16_t _frame_rate_hz, uint16_t _frame_rate_ms);

    void Update1Hz(void);             // called at 1 Hz
    void Next(void);                  // called at each cycle, is called when transmit starts, or shortly after
    void Clear(void);                 // called then not connected
    void JustConnected(void);         // called upon first connection

    void doFrameReceived(void);       // counts received frames, practically not very relevant
    void doValidFrameReceived(void);  // counts received frames, which also passed main crc check

    uint8_t GetTransmitBandwidthUsage(void);
    uint8_t GetReceiveBandwidthUsage(void);
    int8_t GetLastRssi(void);
    int8_t GetLastSnr(void);

#ifdef DEVICE_IS_RECEIVER
    uint8_t GetLQ_link(void);         // the Rx's link LQ, reported to the Tx and to the autopilot
#endif
    uint8_t GetLQ_serial(void);
#ifdef DEVICE_IS_TRANSMITTER
    uint8_t GetReceivedLQ_link(void); // the Rx's link LQ, as it reported it
#endif

    void doMavlinkCnt(bool valid);
    uint8_t GetMavlinkLQ(void);

    // statistics for our device

    tStatsLQ frames_received;         // number of frames received, practically not very relevant
    tStatsLQ valid_frames_received;   // received frames which also passed crc check

    tStatsLQ serial_data_transmitted; // frames with serial data transmitted, retransmissions are not counted
    tStatsLQ serial_data_received;    // frames with serial data received, retransmissions are not counted

    tStatsBytes bytes_transmitted;    // retransmissions are not counted
    tStatsBytes bytes_received;       // retransmissions are not counted

    tStatsMavlinkLQ mav_packets_received;   // number of MAVLink packets received

    // RF statistics for our device

    int8_t last_rssi1;
    int8_t last_rssi2;
    int8_t last_snr1;
    int8_t last_snr2;
    uint8_t last_antenna;
    uint8_t last_transmit_antenna;

    // statistics received from the other end

    int8_t received_rssi;
#ifdef DEVICE_IS_TRANSMITTER
    uint8_t received_LQ_link;
    uint8_t received_power_code; // the Rx's power index + 1, 0 = not known
#endif
#ifdef DEVICE_IS_RECEIVER
    uint8_t received_fhss_index_band;
    uint8_t received_fhss_index;
#endif
    uint8_t received_LQ_serial;
    uint8_t received_antenna;
    uint8_t received_transmit_antenna;

    // statistics for ARQ, LPF filter for the number of frames with fresh payload

    tLpFilter frame_cnt;
    void cntFrameTransmitted(void);
    void cntFrameSkipped(void);
    int32_t GetFrameCnt(void);

    // seq no in the last transmitted frame, only tx

    uint8_t transmit_seq_no;

    // moving average fields

//    tLqCounterBase LQma_received;
//    tLqCounterBase LQma_valid;

    // to avoid fractional LQ upon first connection, report 100% for 2 secs
    uint8_t just_connected_cnt;
};


#endif // COMMON_STATS_H
