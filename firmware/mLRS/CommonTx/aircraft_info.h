//*******************************************************
// TetherLRS
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
//*******************************************************
// What the display shows of the aircraft: read from the MAVLink its autopilot sends down the link, any autopilot
//********************************************************
#ifndef AIRCRAFT_INFO_H
#define AIRCRAFT_INFO_H
#pragma once


class tTxAircraft
{
  public:
    void Init(void)
    {
        sysid = 0;
        autopilot = MAV_AUTOPILOT_INVALID;
        type = 0;
        armed = false;
        heartbeat_ms = 0;
        voltage_mv = UINT16_MAX;
        current_ca = -1;
        remaining = -1;
        fix_type = UINT8_MAX;
        sats = UINT8_MAX;
        has_hud = false;
    }

    void handle_msg(fmav_message_t* const msg)
    {
        if (msg->compid != MAV_COMP_ID_AUTOPILOT1) return;

        if (msg->msgid == FASTMAVLINK_MSG_ID_HEARTBEAT) {
            fmav_heartbeat_t hb;
            fmav_msg_heartbeat_decode(&hb, msg);
            if (hb.autopilot == MAV_AUTOPILOT_INVALID) return;
            if (!sysid) sysid = msg->sysid; // the first autopilot heard
            if (msg->sysid != sysid) return;
            autopilot = hb.autopilot;
            type = hb.type;
            armed = (hb.base_mode & MAV_MODE_FLAG_SAFETY_ARMED);
            heartbeat_ms = millis32();
            return;
        }
        if (!sysid || msg->sysid != sysid) return;

        switch (msg->msgid) {
        case FASTMAVLINK_MSG_ID_SYS_STATUS: {
            fmav_sys_status_t s;
            fmav_msg_sys_status_decode(&s, msg);
            voltage_mv = s.voltage_battery;
            current_ca = s.current_battery;
            remaining = s.battery_remaining;
            }break;
        case FASTMAVLINK_MSG_ID_GPS_RAW_INT: {
            fmav_gps_raw_int_t g;
            fmav_msg_gps_raw_int_decode(&g, msg);
            fix_type = g.fix_type;
            sats = g.satellites_visible;
            }break;
        case FASTMAVLINK_MSG_ID_VFR_HUD: {
            fmav_vfr_hud_t h;
            fmav_msg_vfr_hud_decode(&h, msg);
            alt_m = h.alt;
            groundspeed_ms = h.groundspeed;
            has_hud = true;
            }break;
        }
    }

    bool Seen(void) { return sysid != 0; }
    uint32_t HeartbeatAge_ms(void) { return millis32() - heartbeat_ms; }

    uint8_t sysid; // 0: no autopilot heard yet
    uint8_t autopilot; // MAV_AUTOPILOT, INAV reports GENERIC unless set to ArduPilot
    uint8_t type; // MAV_TYPE
    bool armed;
    uint32_t heartbeat_ms;

    uint16_t voltage_mv; // UINT16_MAX: not sent
    int16_t current_ca; // -1: not sent
    int8_t remaining; // -1: not sent
    uint8_t fix_type; // GPS_FIX_TYPE, UINT8_MAX: no GPS_RAW_INT
    uint8_t sats;
    bool has_hud;
    float alt_m;
    float groundspeed_ms;
};

tTxAircraft aircraft;


#endif // AIRCRAFT_INFO_H
