//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// MAVLink Coalesce
//********************************************************
// TetherLRS: the receiver's queue of MAVLink frames towards the link, in place of a plain byte fifo.
// When the link is slower than the autopilot, a state message (attitude, position, ...) waiting in the queue is
// dropped as soon as a newer copy from the same sender arrives: the ground gets fresher telemetry instead of an
// old backlog. Messages which carry something that must arrive (parameters, missions, text, acks, FTP) are never
// dropped, and nothing is reordered. A frame the link has started to send is never dropped.
// The GCS sees the dropped copies as gaps in the MAVLink sequence numbers.
//********************************************************
#ifndef MAVLINK_COALESCE_H
#define MAVLINK_COALESCE_H
#pragma once


#include <inttypes.h>
#include <string.h>


#define MAVLINK_COALESCE_BUF_SIZE   2048
#define MAVLINK_COALESCE_FRAMES     64


// periodic state messages, where only the newest one per sender matters
// excluded on purpose: multi-instance messages keyed by a payload field (BATTERY_STATUS, DISTANCE_SENSOR, ...)
bool mavlink_msg_coalescible(uint32_t msgid)
{
    switch (msgid) {
    case 0:   // HEARTBEAT
    case 1:   // SYS_STATUS
    case 2:   // SYSTEM_TIME
    case 24:  // GPS_RAW_INT
    case 26:  // SCALED_IMU
    case 27:  // RAW_IMU
    case 29:  // SCALED_PRESSURE
    case 30:  // ATTITUDE
    case 31:  // ATTITUDE_QUATERNION
    case 32:  // LOCAL_POSITION_NED
    case 33:  // GLOBAL_POSITION_INT
    case 35:  // RC_CHANNELS_RAW
    case 36:  // SERVO_OUTPUT_RAW
    case 42:  // MISSION_CURRENT
    case 62:  // NAV_CONTROLLER_OUTPUT
    case 65:  // RC_CHANNELS
    case 74:  // VFR_HUD
    case 116: // SCALED_IMU2
    case 124: // GPS2_RAW
    case 125: // POWER_STATUS
    case 129: // SCALED_IMU3
    case 136: // TERRAIN_REPORT
    case 137: // SCALED_PRESSURE2
    case 141: // ALTITUDE
    case 152: // MEMINFO
    case 163: // AHRS
    case 165: // HWSTATUS
    case 168: // WIND
    case 178: // AHRS2
    case 182: // AHRS3
    case 193: // EKF_STATUS_REPORT
    case 226: // RPM
    case 241: // VIBRATION
    case 245: // EXTENDED_SYS_STATE
        return true;
    }
    return (msgid >= 11030 && msgid <= 11040); // ESC_TELEMETRY_1_TO_4 ... ESC_TELEMETRY_29_TO_32
}


class tMavlinkCoalesce
{
  public:
    void Init(void)
    {
        Flush();
        dropped = 0;
    }

    void Flush(void)
    {
        n = 0;
        used = 0;
        head_done = 0;
        live = 0;
    }

    // room for a frame of len bytes; drops the space of discarded frames when needed
    bool HasSpace(uint16_t len)
    {
        if (n < MAVLINK_COALESCE_FRAMES && used + len <= MAVLINK_COALESCE_BUF_SIZE) return true;
        compact();
        return (n < MAVLINK_COALESCE_FRAMES && used + len <= MAVLINK_COALESCE_BUF_SIZE);
    }

    // frame: as it goes on the link (MAVLink v2 or mavlinkX); call only after HasSpace(len)
    void Put(const uint8_t* const frame, uint16_t len, uint32_t msgid, uint8_t sysid, uint8_t compid)
    {
        uint16_t sender = ((uint16_t)sysid << 8) | compid;
        bool coalesce = mavlink_msg_coalescible(msgid);
        if (coalesce) {
            for (uint8_t i = 0; i < n; i++) {
                if (f[i].dead || f[i].started || !f[i].coalesce) continue;
                if (f[i].msgid != msgid || f[i].sender != sender) continue;
                f[i].dead = 1;
                live -= f[i].len;
                dropped++;
            }
        }
        memcpy(buf + used, frame, len);
        f[n] = { .pos = used, .len = len, .msgid = msgid, .sender = sender, .coalesce = coalesce, .started = 0, .dead = 0 };
        n++;
        used += len;
        live += len;
    }

    uint16_t Available(void) { return live; }

    // call only if Available()
    uint8_t Get(void)
    {
        while (n && f[0].dead) pop();
        f[0].started = 1;
        uint8_t c = buf[f[0].pos + head_done];
        head_done++;
        live--;
        if (head_done >= f[0].len) {
            pop();
        }
        return c;
    }

    uint32_t dropped; // copies discarded since start, for statistics

  private:
    void pop(void)
    {
        for (uint8_t i = 1; i < n; i++) f[i - 1] = f[i];
        n--;
        head_done = 0;
        if (!n) used = 0;
    }

    // moves the frames still to be sent to the start of the buffer, dropping discarded ones
    void compact(void)
    {
        uint16_t to = 0;
        uint8_t k = 0;
        for (uint8_t i = 0; i < n; i++) {
            if (f[i].dead) continue;
            uint16_t from = f[i].pos;
            uint16_t len = f[i].len;
            if (i == 0 && head_done) { // the frame on its way keeps only what is still to go
                from += head_done;
                len -= head_done;
            }
            memmove(buf + to, buf + from, len);
            f[k] = f[i];
            f[k].pos = to;
            f[k].len = len;
            to += len;
            k++;
        }
        head_done = 0; // the part of the head already sent was cut off above
        n = k;
        used = to;
    }

    typedef struct {
        uint16_t pos;
        uint16_t len;
        uint32_t msgid;
        uint16_t sender;
        uint8_t coalesce;
        uint8_t started;
        uint8_t dead;
    } tFrame;

    uint8_t buf[MAVLINK_COALESCE_BUF_SIZE];
    tFrame f[MAVLINK_COALESCE_FRAMES]; // f[0] is the next to go
    uint8_t n;
    uint16_t used;
    uint16_t head_done; // bytes of f[0] already given to the link
    uint16_t live; // bytes still to go, discarded frames not counted
};


#endif // MAVLINK_COALESCE_H
