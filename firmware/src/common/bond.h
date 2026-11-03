//*******************************************************
// Copyright (c) TetherLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.html
//*******************************************************
// Bonded radios: two lanes, A on radio 1 and B on radio 2, each with its own ARQ
//*******************************************************
// The sender takes new data for both lanes only when both are free (tTransmitArq::WouldAdvance on each), A gets the
// first bytes and B the next, and both carry the same seq no. So the receiving side can put A's payload out at once
// and hold B's until A's of the same cycle is there: the order out is always A, B, A, B. A B whose A never came
// (the retries ran out) goes out before the next A, so what did arrive keeps its order.
#ifndef BOND_H
#define BOND_H
#pragma once


#include <inttypes.h>
#include <string.h>


#define BOND_LANE_A  0
#define BOND_LANE_B  1


class tBondReorder
{
  public:
    void Init(void (*_emit)(uint8_t* const data, uint8_t len), uint8_t _hold_size);
    void Accepted(uint8_t lane, uint8_t seq_no, uint8_t* const data, uint8_t len);

  private:
    void (*emit)(uint8_t* const data, uint8_t len);
    uint8_t hold_size;
    uint8_t out_seq_a; // the seq no of the last A put out
    bool out_a_valid;
    uint8_t hold[100]; // FRAME_PAYLOAD_MAX
    uint8_t hold_len;
    uint8_t hold_seq;
    bool hold_valid;
};


void tBondReorder::Init(void (*_emit)(uint8_t* const data, uint8_t len), uint8_t _hold_size)
{
    emit = _emit;
    hold_size = (_hold_size > sizeof(hold)) ? sizeof(hold) : _hold_size;
    out_seq_a = 0;
    out_a_valid = false;
    hold_len = 0;
    hold_seq = 0;
    hold_valid = false;
}


void tBondReorder::Accepted(uint8_t lane, uint8_t seq_no, uint8_t* const data, uint8_t len)
{
    if (lane == BOND_LANE_A) {
        if (hold_valid && hold_seq != seq_no) { // a B whose A never came: out before this newer A
            emit(hold, hold_len);
            hold_valid = false;
        }
        emit(data, len);
        out_seq_a = seq_no;
        out_a_valid = true;
        if (hold_valid && hold_seq == seq_no) { // B was waiting for this A
            emit(hold, hold_len);
            hold_valid = false;
        }
        return;
    }

    // lane B
    if (out_a_valid && out_seq_a == seq_no) { // its A is out already
        emit(data, len);
        return;
    }
    if (hold_valid && hold_seq != seq_no) emit(hold, hold_len); // an older B still waiting: its A is lost, out in order
    if (len > hold_size) len = hold_size;
    memcpy(hold, data, len);
    hold_len = len;
    hold_seq = seq_no;
    hold_valid = true;
}


#endif // BOND_H
