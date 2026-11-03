//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
// OlliW @ www.olliw.eu
// TetherLRS changes: Copyright (c) TetherLRS project
//*******************************************************
// frames
//*******************************************************
#ifndef FRAMES_H
#define FRAMES_H
#pragma once


#include "frame_types.h"
#include "crypto.h"
#include "hal/hal.h"


extern volatile uint32_t millis32(void);
extern tSetupMetaData SetupMetaData;
extern tSetup Setup;
extern tGlobalConfig Config;
extern SX_DRIVER sx;
extern SX2_DRIVER sx2;
extern tCrypto crypto;


//-------------------------------------------------------
// Tx, Rx Frames
//-------------------------------------------------------

typedef enum {
    CHECK_OK = 0,
    CHECK_ERROR_SYNCWORD, // 1
    CHECK_ERROR_HEADER,   // 2
    CHECK_ERROR_CRC,      // 3
} CHECK_ENUM;


#ifdef FAIL_ENABLED
#define CHECK_PAYLOAD_MAX_LEN(pl,ml) if (pl > ml - crypto.NonceLen()) while(1){} // must not happen
#define CHECK_PAYLOAD_MIN_LEN(pl) if (pl < crypto.NonceLen()) while(1){} // must not happen
#else
#define CHECK_PAYLOAD_MAX_LEN(pl,ml) if (pl > ml - crypto.NonceLen()) pl = ml - crypto.NonceLen(); // handle it gracefully and hope for the best
#define CHECK_PAYLOAD_MIN_LEN(pl) if (pl < crypto.NonceLen()) return false; // probably the best to do
#endif

// Note to Claude: we know that replay attack prevention is disabled/not working.
// This is momentarily by design, no need to comment on that ;)

// CMDs are currently encrypted/authenticated only for Tx -> Rx
#define FRAME_TYPE_IS_ENCRYPTABLE(ft)  (ft == FRAME_TYPE_TX || ft == FRAME_TYPE_CMD_ENCRYPTED)


// the CRC closes the frame: the last two bytes of the length its direction runs with
void frame_set_crc(void* const frame, uint8_t frame_len)
{
    uint16_t crc = fmav_crc_calculate((uint8_t*)frame, frame_len - 2);
    memcpy((uint8_t*)frame + frame_len - 2, &crc, 2);
}

bool frame_crc_ok(void* const frame, uint8_t frame_len)
{
    uint16_t crc = fmav_crc_calculate((uint8_t*)frame, frame_len - 2);
    uint16_t crc_in;
    memcpy(&crc_in, (uint8_t*)frame + frame_len - 2, 2);
    return (crc == crc_in);
}


//-------------------------------------------------------
// Tx Frames (send from Tx to Rx)
//-------------------------------------------------------

#ifdef DEVICE_IS_TRANSMITTER

// lowest level routine to construct a tTxFrame, finalizes it
// used by
//   pack_txframe()
//   pack_txcmdframe_cmd()
//   pack_txcmdframe_setrxparams()
void _pack_txframe_w_type(
    tTxFrame* const frame,
    uint8_t type,
    tFrameStats* const frame_stats,
    uint8_t* const payload,
    uint8_t payload_len)
{
uint8_t data_len = FRAME_PAYLOAD_LEN(Config.FrameLenTx);

    if (payload_len > data_len) payload_len = data_len; // should never occur, but play it safe

    memset((uint8_t*)frame, 0, sizeof(tTxFrame));

    // generate header
    frame->sync_word = Config.FrameSyncWord;
    frame->status.seq_no = frame_stats->seq_no;
    frame->status.ack = frame_stats->ack;
    frame->status.frame_type = type; // FRAME_TYPE_TX, FRAME_TYPE_CMD, FRAME_TYPE_CMD_ENCRYPTED
    frame->status.antenna = frame_stats->antenna;
    frame->status.transmit_antenna = frame_stats->transmit_antenna;
    frame->status.rssi_u7 = rssi_u7_from_i8(frame_stats->rssi);
    frame->status.fhss_index_band = frame_stats->tx_fhss_index_band;
    frame->status.fhss_index = frame_stats->tx_fhss_index;
    frame->status.LQ_serial = frame_stats->LQ_serial;
    frame->status.rx_silent = frame_stats->rx_silent;
    frame->status.payload_len = payload_len;

    // pack the payload
    for (uint8_t i = 0; i < payload_len; i++) {
        frame->payload[i] = payload[i];
    }

    // encrypt only normal TX frames and encryptable CMD frames
    if (crypto.PrivacyLevel() && FRAME_TYPE_IS_ENCRYPTABLE(frame->status.frame_type)) {
        CHECK_PAYLOAD_MAX_LEN(frame->status.payload_len,data_len);
        // encrypt data, move data to payload + N, copy nonce & mac into payload, correct len
        uint8_t payload_len = frame->status.payload_len;
        frame->status.payload_len += crypto.NonceLen(); // adjust to new payload len
        crypto.Encrypt(&(frame->status), 5, frame->payload, payload_len);
    }

    frame_set_crc(frame, Config.FrameLenTx);
}


// construct a normal tTxFrame
void pack_txframe(
    tTxFrame* const frame,
    tFrameStats* const frame_stats,
    uint8_t* const payload,
    uint8_t payload_len)
{
    _pack_txframe_w_type(frame, FRAME_TYPE_TX, frame_stats, payload, payload_len);
}

#endif
#ifdef DEVICE_IS_RECEIVER

// check credentials of a tTxFrame (sync word, frame type, payload len, CRC)
// returns 0 if OK !!
uint8_t check_txframe(tTxFrame* const frame)
{
    if (frame->sync_word != Config.FrameSyncWord) return CHECK_ERROR_SYNCWORD;

    if ((frame->status.frame_type != FRAME_TYPE_TX) &&
        (frame->status.frame_type != FRAME_TYPE_CMD) &&
        (frame->status.frame_type != FRAME_TYPE_CMD_ENCRYPTED)) {
        return CHECK_ERROR_HEADER;
    }

    if (frame->status.payload_len > FRAME_PAYLOAD_LEN(Config.FrameLenTx)) return CHECK_ERROR_HEADER;
    if (FRAME_TYPE_IS_ENCRYPTABLE(frame->status.frame_type) && frame->status.payload_len < crypto.NonceLen()) {
        return CHECK_ERROR_HEADER;
    }

    if (!frame_crc_ok(frame, Config.FrameLenTx)) return CHECK_ERROR_CRC;

    // we don't do the encryption here
    // might be logically cleaner, but performance-wise it's better to do in pre loop

    return CHECK_OK;
}


// decrypt a tTxFrame, comes before any payload processing
bool decrypt_txframe(tTxFrame* const frame)
{
    // decrypt only normal TX frames and encryptable CMD frames
    if (crypto.PrivacyLevel() && FRAME_TYPE_IS_ENCRYPTABLE(frame->status.frame_type)) {
        CHECK_PAYLOAD_MIN_LEN(frame->status.payload_len);
        if (!crypto.Decrypt(&(frame->status), 5, frame->payload, frame->status.payload_len)) return false;
        frame->status.payload_len -= crypto.NonceLen(); // adjust to new payload len
    }

    // prevent plaintext CMDs to get through, except of GET_RX_SETUPDATA_STARTUP
    if (crypto.PrivacyLevel() && frame->status.frame_type == FRAME_TYPE_CMD &&
        frame->payload[0] != FRAME_CMD_GET_RX_SETUPDATA_STARTUP) {
        return false;
    }

    return true;
}


#endif


//-------------------------------------------------------
// Rx Frames (send from Rx to Tx)
//-------------------------------------------------------

#ifdef DEVICE_IS_RECEIVER

// update header info of a tRxFrame, keep payload
void update_rxframe_stats(tRxFrame* const frame, tFrameStats* const frame_stats)
{
    frame->sync_word = Config.FrameSyncWord;
    // keep !! frame->status.seq_no = frame_stats->seq_no;
    frame->status.ack = frame_stats->ack;
    // keep !! frame->status.frame_type = type; // FRAME_TYPE_RX, FRAME_TYPE_CMD
    frame->status.antenna = frame_stats->antenna;
    frame->status.transmit_antenna = frame_stats->transmit_antenna;
    frame->status.rssi_u7 = rssi_u7_from_i8(frame_stats->rssi);
    frame->status.LQ_link = frame_stats->LQ_link;
    frame->status.LQ_serial = frame_stats->LQ_serial;
    frame->status.power_hi = frame_stats->power_code >> 2;
    frame->status.power_lo = frame_stats->power_code & 0x03;
    // keep !! frame->status.payload_len = payload_len;

    // TODO: we should also update the nonce
    // this needs changes in the higher level code however as we can't just store the last tRxFrame
    if (crypto.PrivacyLevel() && frame->status.frame_type == FRAME_TYPE_RX) {
        crypto.RecalculateMac(&(frame->status), 5, frame->payload, frame->status.payload_len);
    }

    frame_set_crc(frame, Config.FrameLenRx);
}


// lowest level routine to construct a tRxFrame, finalizes it
// used by
//   pack_rxframe()
//   pack_rxcmdframe_rxsetupdata()
void _pack_rxframe_w_type(
    tRxFrame* const frame,
    uint8_t type,
    tFrameStats* const frame_stats,
    uint8_t* const payload,
    uint8_t payload_len)
{
uint8_t data_len = FRAME_PAYLOAD_LEN(Config.FrameLenRx);

    if (payload_len > data_len) payload_len = data_len; // should never occur, but play it safe

    memset((uint8_t*)frame, 0, sizeof(tRxFrame));

    // generate header
    frame->sync_word = Config.FrameSyncWord;
    frame->status.seq_no = frame_stats->seq_no;
    frame->status.ack = frame_stats->ack;
    frame->status.frame_type = type; // FRAME_TYPE_RX, FRAME_TYPE_CMD
    frame->status.antenna = frame_stats->antenna;
    frame->status.transmit_antenna = frame_stats->transmit_antenna;
    frame->status.rssi_u7 = rssi_u7_from_i8(frame_stats->rssi);
    frame->status.LQ_link = frame_stats->LQ_link;
    frame->status.LQ_serial = frame_stats->LQ_serial;
    frame->status.power_hi = frame_stats->power_code >> 2;
    frame->status.power_lo = frame_stats->power_code & 0x03;
    frame->status.payload_len = payload_len;

    // pack the payload
    for (uint8_t i = 0; i < payload_len; i++) {
        frame->payload[i] = payload[i];
    }

    // encrypt only normal RX frames; the setup data command carries no secret and goes as it is
    if (crypto.PrivacyLevel() && frame->status.frame_type == FRAME_TYPE_RX) {
        CHECK_PAYLOAD_MAX_LEN(frame->status.payload_len,data_len);
        // encrypt data, move data to payload + N, copy nonce & mac into payload, correct len
        uint8_t payload_len = frame->status.payload_len;
        frame->status.payload_len += crypto.NonceLen(); // adjust to new payload len
        crypto.Encrypt(&(frame->status), 5, frame->payload, payload_len);
    }

    frame_set_crc(frame, Config.FrameLenRx);
}


// construct a normal tRxFrame
void pack_rxframe(
    tRxFrame* const frame,
    tFrameStats* const frame_stats,
    uint8_t* const payload,
    uint8_t payload_len)
{
    _pack_rxframe_w_type(frame, FRAME_TYPE_RX, frame_stats, payload, payload_len);
}

#endif
#ifdef DEVICE_IS_TRANSMITTER

// check credentials of a tRxFrame (sync word, frame type, payload len, CRC)
// returns 0 if OK !!
uint8_t check_rxframe(tRxFrame* const frame)
{
    if (frame->sync_word != Config.FrameSyncWord) return CHECK_ERROR_SYNCWORD;

    if ((frame->status.frame_type != FRAME_TYPE_RX) && (frame->status.frame_type != FRAME_TYPE_CMD)) {
        return CHECK_ERROR_HEADER;
    }

    if (frame->status.payload_len > FRAME_PAYLOAD_LEN(Config.FrameLenRx)) return CHECK_ERROR_HEADER;
    if (frame->status.frame_type == FRAME_TYPE_RX && frame->status.payload_len < crypto.NonceLen()) {
        return CHECK_ERROR_HEADER;
    }

    if (!frame_crc_ok(frame, Config.FrameLenRx)) return CHECK_ERROR_CRC;

    return CHECK_OK;
}


// decrypt a normal tRxFrame, comes before any payload processing
// Note: There are currently no encrypted CMDs in this direction
bool decrypt_rxframe(tRxFrame* const frame)
{
    // decrypt only normal RX frames
    if (crypto.PrivacyLevel() && frame->status.frame_type == FRAME_TYPE_RX) {
        CHECK_PAYLOAD_MIN_LEN(frame->status.payload_len);
        if (!crypto.Decrypt(&(frame->status), 5, frame->payload, frame->status.payload_len)) {
            return false;
        }
        frame->status.payload_len -= crypto.NonceLen(); // adjust to new payload len
    }

    return true;
}

#endif


//-------------------------------------------------------
// Tx/Rx Cmd Frames
//-------------------------------------------------------

// lowest level routine to copy Rx.Setup values to tCmdFrameRxParameters frame
void _copy_rxsetup_to_cmdframerxparameters(tCmdFrameRxParameters* const rx_params)
{
    rx_params->Power = Setup.Rx.Power;
    rx_params->Diversity = Setup.Rx.Diversity;
    rx_params->SerialBaudrate = Setup.Rx.SerialBaudrate;
    rx_params->SerialLinkMode = Setup.Rx.SerialLinkMode;
    rx_params->SendRadioStatus = Setup.Rx.SendRadioStatus;
    rx_params->SerialPort = Setup.Rx.SerialPort;
    rx_params->MavlinkSystemID = Setup.Rx.MavlinkSystemID;
    rx_params->MavCoalesce = Setup.Rx.MavCoalesce;
    rx_params->Downlink = Setup.Rx.Downlink;
    rx_params->SerialOut2 = Setup.Rx.SerialOut2;
    rx_params->SerialBaudrate2 = Setup.Rx.SerialBaudrate2;
    rx_params->Test = Setup.Rx.Test;
}


// lowest level routine to copy tCmdFrameRxParameters frame to Rx.Setup values
void _copy_cmdframerxparameters_to_rxsetup(tCmdFrameRxParameters* const rx_params)
{
    Setup.Rx.Power = rx_params->Power;
    Setup.Rx.Diversity = rx_params->Diversity;
    Setup.Rx.SerialBaudrate = rx_params->SerialBaudrate;
    Setup.Rx.SerialLinkMode = rx_params->SerialLinkMode;
    Setup.Rx.SendRadioStatus = rx_params->SendRadioStatus;
    Setup.Rx.SerialPort = rx_params->SerialPort;
    Setup.Rx.MavlinkSystemID = rx_params->MavlinkSystemID;
    Setup.Rx.MavCoalesce = rx_params->MavCoalesce;
    Setup.Rx.Downlink = rx_params->Downlink;
    Setup.Rx.SerialOut2 = rx_params->SerialOut2;
    Setup.Rx.SerialBaudrate2 = rx_params->SerialBaudrate2;
    Setup.Rx.Test = rx_params->Test;
}


// what this device's radios are, for the bind and setup data frames
uint8_t device_radio_count(void)
{
#ifdef USE_SX2
    return 2;
#else
    return 1;
#endif
}

uint8_t device_radio_bands(void)
{
    uint8_t bands = 0;
#ifdef FREQUENCY_BAND_2P4_GHZ
    bands |= (1 << SX_FHSS_FREQUENCY_BAND_2P4_GHZ);
#endif
#ifdef FREQUENCY_BAND_915_MHZ_FCC
    bands |= (1 << SX_FHSS_FREQUENCY_BAND_915_MHZ_FCC);
#endif
#ifdef FREQUENCY_BAND_868_MHZ
    bands |= (1 << SX_FHSS_FREQUENCY_BAND_868_MHZ);
#endif
#ifdef FREQUENCY_BAND_433_MHZ
    bands |= (1 << SX_FHSS_FREQUENCY_BAND_433_MHZ);
#endif
#ifdef FREQUENCY_BAND_70_CM_HAM
    bands |= (1 << SX_FHSS_FREQUENCY_BAND_70_CM_HAM);
#endif
#ifdef FREQUENCY_BAND_866_MHZ_IN
    bands |= (1 << SX_FHSS_FREQUENCY_BAND_866_MHZ_IN);
#endif
    return bands;
}


#ifdef DEVICE_IS_TRANSMITTER

// Tx: send CMD to Rx
void pack_txcmdframe_cmd(tTxFrame* const frame, tFrameStats* const frame_stats, uint8_t cmd)
{
uint8_t payload[32]; // 1 + CRYPTO_STARTUP_RANDOM_BUF_LEN (=28) = 29
uint8_t len;
uint8_t frame_type;

    frame_type = (crypto.PrivacyLevel()) ? FRAME_TYPE_CMD_ENCRYPTED : FRAME_TYPE_CMD;
    payload[0] = cmd;
    len = 1;

    // CMD_GET_RX_SETUPDATA_STARTUP adds random session key
    // TODO: should we only send if privacy level > 0? Doens't really matter, right
    if (cmd == FRAME_CMD_GET_RX_SETUPDATA_STARTUP) {
        crypto.EncryptSessionRandom(&(payload[1]), Config.StartupRandom, Config.BindRandom);
        len += CRYPTO_STARTUP_RANDOM_BUF_LEN;
        frame_type = FRAME_TYPE_CMD; // this one is never encrypted
    }

    _pack_txframe_w_type(frame, frame_type, frame_stats, payload, len);
}


// Tx: handle FRAME_CMD_RX_SETUPDATA from Rx
void unpack_rxcmdframe_rxsetupdata(tRxFrame* const frame)
{
tRxCmdFrameRxSetupData* rx_setupdata = (tRxCmdFrameRxSetupData*)frame->payload;

    SetupMetaData.rx_available = true;

    SetupMetaData.rx_firmware_version = version_from_u16(rx_setupdata->firmware_version_u16);
    SetupMetaData.rx_setup_layout = version_from_u16(rx_setupdata->setup_layout_u16);
    strstrbufcpy(SetupMetaData.rx_device_name, rx_setupdata->device_name_20, 20);
    SetupMetaData.rx_actual_power_dbm = rx_setupdata->actual_power_dbm;
    SetupMetaData.rx_actual_diversity = rx_setupdata->actual_diversity;

    _copy_cmdframerxparameters_to_rxsetup(&(rx_setupdata->RxParams));

    // TetherLRS: the Tx offers only the bands and modes the receiver can take too, see param_get_allowed_mask()
    SetupMetaData.rx_FrequencyBand_allowed_mask = rx_setupdata->FrequencyBand_allowed_mask;
    SetupMetaData.rx_Mode_allowed_mask = rx_setupdata->Mode_allowed_mask;

    int16_t power_list[8];
    for (uint8_t i = 0; i < 8; i++) power_list[i] = rx_setupdata->Power_list[i]; // to avoid unaligned warning
    power_optstr_from_power_list(SetupMetaData.Rx_Power_optstr, power_list, 8, 67);
    SetupMetaData.Rx_Diversity_allowed_mask = rx_setupdata->Diversity_allowed_mask;
    SetupMetaData.Rx_SerialPort_allowed_mask = rx_setupdata->SerialPort_allowed_mask;
    SetupMetaData.Rx_SerialOut2_allowed_mask = rx_setupdata->SerialOut2_allowed_mask;
    SetupMetaData.rx_radio_count = rx_setupdata->radio_count;
    SetupMetaData.rx_radio_bands = rx_setupdata->radio_bands;

    // ensure that Tx has receiver settings which are compatible with its layout, to avoid index issues
    if (SetupMetaData.rx_setup_layout != (uint32_t)SETUPLAYOUT) {
        setup_sanitize_rx_config();
    }
}


// Tx: send new receiver parameters with FRAME_CMD_SET_RX_PARAMS to Rx
// we take the values from Tx' Setup.Rx structure
// Note: Encryptable, so length must be adjusted to free the space for the nonce and MAC.
void pack_txcmdframe_setrxparams(tTxFrame* const frame, tFrameStats* const frame_stats)
{
tTxCmdFrameRxParams rx_params = {};

    rx_params.cmd = FRAME_CMD_SET_RX_PARAMS;

    rx_params.tx_firmware_version_u16 = version_to_u16(VERSION);
    rx_params.tx_setup_layout_u16 = version_to_u16(SETUPLAYOUT);

    strbufstrcpy(rx_params.BindPhrase_6, Setup.Common[Config.ConfigId].BindPhrase, 6);
    rx_params.FrequencyBand = Setup.Common[Config.ConfigId].FrequencyBand;
    rx_params.Mode = Setup.Common[Config.ConfigId].Mode;
    rx_params.Ortho = Setup.Common[Config.ConfigId].Ortho;
    rx_params.Privacy = Setup.Common[Config.ConfigId].Privacy;
    rx_params.Region = Setup.Common[Config.ConfigId].Region;
    rx_params.DynPower = Setup.Common[Config.ConfigId].DynPower;
    rx_params.LinkSplit = Setup.Common[Config.ConfigId].LinkSplit;
    rx_params.RadioScheme = Setup.Common[Config.ConfigId].RadioScheme;
    rx_params.CustomFreqKHz = Setup.Common[Config.ConfigId].CustomFreqKHz;
    rx_params.CustomSpanKHz = Setup.Common[Config.ConfigId].CustomSpanKHz;
    rx_params.CustomBw = Setup.Common[Config.ConfigId].CustomBw;
    rx_params.CustomLbt = Setup.Common[Config.ConfigId].CustomLbt;
    rx_params.CustomPowerMaxDbm = Setup.Common[Config.ConfigId].CustomPowerMaxDbm;

    _copy_rxsetup_to_cmdframerxparameters(&(rx_params.RxParams));

    uint8_t frame_type = (crypto.PrivacyLevel()) ? FRAME_TYPE_CMD_ENCRYPTED : FRAME_TYPE_CMD;
    uint8_t payload_len = sizeof(rx_params) - crypto.NonceLen();

    _pack_txframe_w_type(frame, frame_type, frame_stats, (uint8_t*)&rx_params, payload_len);
}

#endif
#ifdef DEVICE_IS_RECEIVER

// Rx: send FRAME_CMD_RX_SETUPDATA to Tx
// Note: 64 bytes, not encrypted
void pack_rxcmdframe_rxsetupdata(tRxFrame* const frame, tFrameStats* const frame_stats)
{
tRxCmdFrameRxSetupData rx_setupdata = {};

    rx_setupdata.cmd = FRAME_CMD_RX_SETUPDATA;

    rx_setupdata.firmware_version_u16 = version_to_u16(VERSION);
    rx_setupdata.setup_layout_u16 = version_to_u16(SETUPLAYOUT);
    strbufstrcpy(rx_setupdata.device_name_20, DEVICE_NAME, 20);
    rx_setupdata.actual_power_dbm = SX_OR_SX2(sx.RfPower_dbm(),sx2.RfPower_dbm());
    rx_setupdata.actual_diversity = Config.Diversity;

    _copy_rxsetup_to_cmdframerxparameters(&(rx_setupdata.RxParams));

    // TetherLRS: the modes are those of the band it is in
    rx_setupdata.FrequencyBand_allowed_mask = SetupMetaData.FrequencyBand_allowed_mask;
    rx_setupdata.Mode_allowed_mask = SetupMetaData.Mode_allowed_mask;

    for (uint8_t i = 0; i < 8; i++) {
        rx_setupdata.Power_list[i] = (i < RFPOWER_LIST_NUM) ? rfpower_list[i].mW : INT16_MAX;
    }
    rx_setupdata.Diversity_allowed_mask = SetupMetaData.Rx_Diversity_allowed_mask;
    rx_setupdata.SerialPort_allowed_mask = SetupMetaData.Rx_SerialPort_allowed_mask;
    rx_setupdata.SerialOut2_allowed_mask = SetupMetaData.Rx_SerialOut2_allowed_mask;
    rx_setupdata.radio_count = device_radio_count();
    rx_setupdata.radio_bands = device_radio_bands();

    _pack_rxframe_w_type(frame, FRAME_TYPE_CMD, frame_stats, (uint8_t*)&rx_setupdata, sizeof(rx_setupdata));
}


// Rx: handle FRAME_CMD_SET_RX_PARAMS
// new parameter values are stored in Rx' Setup.Rx fields
// Note: This frame has 64 bytes, but can be shortened by 12 bytes, so can be encrypted.
void unpack_txcmdframe_setrxparams(tTxFrame* const frame)
{
tTxCmdFrameRxParams* rx_params = (tTxCmdFrameRxParams*)frame->payload;

    strstrbufcpy(Setup.Common[0].BindPhrase, rx_params->BindPhrase_6, 6);
    Setup.Common[0].FrequencyBand = (SETUP_FREQUENCY_BAND_ENUM)rx_params->FrequencyBand;
    Setup.Common[0].Mode = rx_params->Mode;
    Setup.Common[0].Ortho = rx_params->Ortho;
    Setup.Common[0].Privacy = rx_params->Privacy;
    Setup.Common[0].Region = rx_params->Region;
    Setup.Common[0].DynPower = rx_params->DynPower;
    Setup.Common[0].LinkSplit = rx_params->LinkSplit;
    Setup.Common[0].RadioScheme = rx_params->RadioScheme;
    Setup.Common[0].CustomFreqKHz = rx_params->CustomFreqKHz;
    Setup.Common[0].CustomSpanKHz = rx_params->CustomSpanKHz;
    Setup.Common[0].CustomBw = rx_params->CustomBw;
    Setup.Common[0].CustomLbt = rx_params->CustomLbt;
    Setup.Common[0].CustomPowerMaxDbm = rx_params->CustomPowerMaxDbm;

    // don't take over Rx parameters if there is a layout version mismatch
    if (version_from_u16(rx_params->tx_setup_layout_u16) != (uint32_t)SETUPLAYOUT) return;

    _copy_cmdframerxparameters_to_rxsetup(&(rx_params->RxParams));
    // setup_sanitize_rx_config(); // should not ever be needed !
}

#endif


//-------------------------------------------------------
// Helper
//-------------------------------------------------------

// Numerical Recipe's quick generator randq1()
uint32_t nr_randq1(void)
{
    static uint32_t seed = 0;
    seed = 1664525UL * seed + 1013904223UL;
    return seed;
}


// randomly toggle between 0 and 1, but never have more than two equal symbols in a row
uint8_t fhss_band_next(void)
{
    static uint8_t fhss_band = 0;
    static uint8_t fhss_band_last = 0;

    if (fhss_band == fhss_band_last) { // we had it two times, so toggle
        fhss_band++;
    } else { // toggle with 50% probability
        fhss_band_last = fhss_band;
        if (nr_randq1() < UINT32_MAX/2) fhss_band++;
    }

    return (fhss_band & 0x01);
}


#endif // FRAMES_H
