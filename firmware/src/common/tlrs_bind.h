//*******************************************************
// Copyright (c) TetherLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.html
//*******************************************************
// The bind phrase and the link key the tool writes into the image before flashing
//*******************************************************
#ifndef TLRS_BIND_H
#define TLRS_BIND_H
#pragma once


// the tool finds the magic, writes the user's phrase and a fresh stamp, and corrects the image's checksum and hash,
// so no image on the web carries a phrase anyone could link with
typedef struct {
    char magic[8];
    char phrase[6+1];
    uint8_t slot; // the receiver's reply slot, 0xFF = leave it; 1.0 had a spare at 0 here, which tells the tool its layout
    uint32_t stamp; // 0xFFFFFFFF: the tool wrote nothing, the stored phrase and key stay
    uint8_t key[32]; // the link key, all zero = none
    uint8_t rx_count; // the receivers on the link, 1 to 4, 0xFF = leave it
} tTlrsBind;

__attribute__((used)) const volatile tTlrsBind tlrs_bind = { "TLRSBND", BIND_PHRASE, 0xFF, 0xFFFFFFFF, {}, 0xFF };


// a stamp the stored setup has not taken yet: the first start after that flash, every config gets the phrase and
// the radio the key
bool tlrs_bind_apply(void)
{
    uint32_t stamp = tlrs_bind.stamp;
    uint32_t taken;
    memcpy(&taken, Setup.BindStamp, 4);
    if (stamp == 0xFFFFFFFF || stamp == taken) return false;

    char phrase[6+1];
    for (uint8_t i = 0; i < 6; i++) phrase[i] = tlrs_bind.phrase[i];
    phrase[6] = '\0';
    for (uint8_t id = 0; id < SETUP_CONFIG_NUM; id++) {
        setup_default_bindphrase(Setup.Common[id].BindPhrase, id, phrase);
    }
    for (uint8_t i = 0; i < 32; i++) Setup.LinkKey[i] = tlrs_bind.key[i];
#ifdef DEVICE_IS_RECEIVER
    if (tlrs_bind.slot < RX_SLOT_NUM) Setup.Rx.Slot = tlrs_bind.slot;
    if (tlrs_bind.rx_count >= 1 && tlrs_bind.rx_count <= RX_SLOT_NUM) {
        for (uint8_t id = 0; id < SETUP_CONFIG_NUM; id++) Setup.Common[id].RxCount = tlrs_bind.rx_count - 1;
    }
#endif
    memcpy(Setup.BindStamp, &stamp, 4);
    return true;
}


#endif // TLRS_BIND_H
