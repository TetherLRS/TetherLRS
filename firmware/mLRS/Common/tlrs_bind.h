//*******************************************************
// TetherLRS
//*******************************************************
// The bind phrase the tool writes into the image before flashing
//*******************************************************
#ifndef TLRS_BIND_H
#define TLRS_BIND_H
#pragma once


// the tool finds the magic, writes the user's phrase and a fresh stamp, and corrects the image's checksum and hash,
// so no image on the web carries a phrase anyone could link with
typedef struct {
    char magic[8];
    char phrase[6+1];
    uint8_t spare;
    uint32_t stamp; // 0xFFFFFFFF: the tool wrote nothing, the stored phrase stays
} tTlrsBind;

__attribute__((used)) const volatile tTlrsBind tlrs_bind = { "TLRSBND", BIND_PHRASE, 0, 0xFFFFFFFF };


// a stamp the stored setup has not taken yet: the first start after that flash, every config gets the phrase
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
    memcpy(Setup.BindStamp, &stamp, 4);
    return true;
}


#endif // TLRS_BIND_H
