//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
// OlliW @ www.olliw.eu
// TetherLRS changes: Copyright (c) TetherLRS project
//*******************************************************
// FRAME TYPES & DEFS
//*******************************************************
#ifndef FRAME_TYPES_H
#define FRAME_TYPES_H
#pragma once


#include <inttypes.h>


#ifndef PACKED
  #define PACKED(__Declaration__)  __Declaration__ __attribute__((packed)) // that's for __GNUC__
#endif


//-------------------------------------------------------
// frames types only
//-------------------------------------------------------

typedef enum {
    FRAME_TYPE_TX = 0x00,
    FRAME_TYPE_RX = 0x01,
    FRAME_TYPE_CMD = 0x02, // these commands use the normal Tx/Rx frames, with repurposed payload however
    FRAME_TYPE_CMD_ENCRYPTED = 0x03,
} FRAME_TYPE_ENUM;


//-------------------------------------------------------
// normal Tx,Rx frames
//-------------------------------------------------------

typedef struct
{
    uint8_t seq_no;
    uint8_t ack;
    int8_t rssi;
    uint8_t LQ_link; // the Rx's link LQ, what the world gets to see
    uint8_t LQ_serial;
    uint8_t antenna;
    uint8_t transmit_antenna;
    uint8_t power_code; // Rx: its power index + 1, for the Tx to show
    uint8_t rx_silent; // Tx: tells the receiver not to transmit
    // not stats but needed
    uint8_t tx_fhss_index_band;
    uint8_t tx_fhss_index;
} tFrameStats;


#define FRAME_TX_RX_HEADER_LEN  7
// no RC channels: a frame is header, data and CRC, its length that of its direction's Link Split profile
#define FRAME_PAYLOAD_LEN(frame_len)  ((frame_len) - FRAME_TX_RX_HEADER_LEN - 2) // the data a frame of that length carries
#define FRAME_PAYLOAD_MAX       FRAME_PAYLOAD_LEN(FRAME_LEN_MAX) // 100
#define FRAME_CMD_PAYLOAD_LEN   64 // command frames fit the shortest frame of every Link Split


PACKED(
typedef struct
{
    uint32_t seq_no : 3;
    uint32_t ack : 1;
    uint32_t frame_type : 3;
    uint32_t spare1 : 1; // was is_32channels
    uint32_t antenna : 1;
    uint32_t rssi_u7 : 7;
    uint32_t fhss_index_band : 1; // fhss index is for band 0 or 1
    uint32_t fhss_index : 6; // older versions have set that field to 63
    uint32_t LQ_serial : 7;
    uint32_t transmit_antenna : 1;
    uint32_t rx_silent : 1; // TetherLRS: the receiver does not transmit (Tx Downlink off), 0 from an mLRS transmitter
    uint32_t spare : 1;
    uint32_t payload_len : 7;
}) tTxFrameStatus; // 5 bytes


PACKED(
typedef struct
{
    uint32_t seq_no : 3;
    uint32_t ack : 1;
    uint32_t frame_type : 3;
    uint32_t power_hi : 1; // TetherLRS: the Rx's power index + 1, 0 = not known (an mLRS receiver); with power_lo
    uint32_t antenna : 1;
    uint32_t rssi_u7 : 7;
    uint32_t LQ_link : 7; // available only for Rx->Tx frame, not for Tx->Rx
    uint32_t LQ_serial : 7;
    uint32_t transmit_antenna : 1;
    uint32_t power_lo : 2;
    uint32_t payload_len : 7;
}) tRxFrameStatus; // 5 bytes


//-- Tx Frame ----------

PACKED(
typedef struct
{
    uint16_t sync_word; // 2 bytes
    tTxFrameStatus status; // 5 bytes
    uint8_t payload[FRAME_PAYLOAD_MAX + 2]; // the data, the CRC at frame length - 2
}) tTxFrame; // FRAME_LEN_MAX bytes


//-- Rx Frame ----------

PACKED(
typedef struct
{
    uint16_t sync_word; // 2 bytes
    tRxFrameStatus status; // 5 bytes
    uint8_t payload[FRAME_PAYLOAD_MAX + 2]; // the data, the CRC at frame length - 2
}) tRxFrame; // FRAME_LEN_MAX bytes


//-------------------------------------------------------
// Bind frames
// are send on bind frequency in 19 Hz mode
//-------------------------------------------------------

PACKED(
typedef struct
{
    uint64_t bind_signature; // 8 bytes // different for Tx and Rx
    uint8_t seq_no : 3; // 1 byte
    uint8_t ack : 1;
    uint8_t frame_type : 3;
    uint8_t spare : 1;

    uint8_t connected : 1;
    uint8_t spare2 : 7;

    char BindPhrase_6[6];
    uint8_t FrequencyBand: 4; // required for bind to know
    uint8_t Mode : 4;
    uint8_t Ortho : 4;
    uint8_t Privacy : 4;
    uint8_t Region;
    uint8_t DynPower;
    uint8_t LinkSplit;
    uint8_t RadioScheme;
    uint32_t CustomFreqKHz;
    uint16_t CustomSpanKHz;
    uint8_t CustomLbt;
    uint8_t CustomPowerMaxDbm;

    uint8_t spare3[39];

    uint8_t tx_uid[12]; // 12 bytes = 96 bits
    uint64_t tx_random; //  8 bytes = 64 bits

    uint16_t crc; // 2 bytes
}) tTxBindFrame; // 91 bytes


PACKED(
typedef struct
{
    uint64_t bind_signature; // 8 bytes // different for Tx and Rx
    uint8_t seq_no : 3; // 1 byte
    uint8_t ack : 1;
    uint8_t frame_type : 3;
    uint8_t spare : 1;

    uint8_t connected : 1;
    uint8_t spare2 : 7;

    uint32_t firmware_version;
    char device_name_20[20];

    uint8_t radio_count; // radios this receiver has, 1 or 2
    uint8_t radio_bands; // bit per SX_FHSS_FREQUENCY_BAND_ENUM it can run
    uint8_t spare3[41];

    uint8_t rx_uid[12]; // 12 bytes = 96 bits

    uint16_t crc; // 2 bytes
}) tRxBindFrame; // 91 bytes


//-------------------------------------------------------
// Cmd & SetupData frames
// these are just normal Tx and Rx frames, with special payloads though
//-------------------------------------------------------
// Tx -> Rx
// CMD_GET_RX_SETUPDATA           only 1 cmd byte                   -> response from Rx with RX_SETUPDATA
// CMD_GET_RX_SETUPDATA_WRELOAD   only 1 cmd byte                   -> response from Rx with RX_SETUPDATA
// CMD_GET_RX_SETUPDATA_STARTUP   1 cmd byte + 18 bytes nonce&MAC   -> response from Rx with RX_SETUPDATA
// CMD_SET_RX_PARAMS              tTxCmdFrameRxParams = 64-12 bytes -> response from Rx with RX_SETUPDATA
// CMD_STORE_RX_PARAMS            only 1 cmd byte                   -> no response, rx reboots
// Rx -> Tx
// CMD_RX_SETUPDATA               tRxCmdFrameRxSetupData = 82 bytes

typedef enum {
    FRAME_CMD_NONE = 0,
//not used    FRAME_CMD_RX_REBOOT,  // tx -> rx, rx reboots
//not used    FRAME_CMD_RX_BIND,    // tx -> rx, rx goes into bind mode
    // some of these commands have additional data
    FRAME_CMD_GET_RX_SETUPDATA = 32,    // tx -> rx, ask for parameters & metadata
    FRAME_CMD_RX_SETUPDATA,             // rx -> tx, return parameters & metadata
    FRAME_CMD_SET_RX_PARAMS,            // tx -> rx, set parameters
    FRAME_CMD_STORE_RX_PARAMS,          // tx -> rx, store parameters, reboots (no extra data)
    FRAME_CMD_GET_RX_SETUPDATA_WRELOAD, // tx -> rx, reload parameters
    FRAME_CMD_GET_RX_SETUPDATA_STARTUP, // tx -> rx, ask for param & metadata, send session random
} FRAME_CMD_ENUM;


// Rx Parameter structure
// limit to 4 bits, so max 16 options per parameter; is also the limit of allowed_mask_ptr in tSetupParameterItem
PACKED(
typedef struct
{
    uint8_t Power : 4;
    uint8_t Diversity : 4;
    uint8_t SerialBaudrate : 4;
    uint8_t SerialLinkMode : 4;
    uint8_t SendRadioStatus : 4;
    uint8_t SerialPort : 4;
    uint8_t MavlinkSystemID : 4;
    uint8_t MavCoalesce : 4;
    uint8_t Downlink : 4;
    uint8_t SerialOut2 : 4;
    uint8_t SerialBaudrate2 : 4;
    uint8_t Test : 4;
    uint8_t spare[2];
}) tCmdFrameRxParameters; // 8 bytes


// send from Rx as response to commands GET_RX_SETUPDATA, SET_RX_PARAMS, GET_RX_SETUPDATA_WRELOAD, GET_RX_SETUPDATA_STARTUP
// Note: not encrypted, it carries no secret; 64 bytes so that it fits every link split
PACKED(
typedef struct
{
    uint8_t cmd;
    uint8_t spare;
    // rx setup meta data 1
    uint16_t firmware_version_u16; // 16.64.64
    uint16_t setup_layout_u16; // 16.64.64
    char device_name_20[20];
    int8_t actual_power_dbm;
    uint8_t actual_diversity;
    // rx parameter values
    // BindPhrase, FrequencyBand, Mode must be equal to Tx, otherwise Rx wouldn't connect, so don't have to be send
    tCmdFrameRxParameters RxParams; // 8 bytes
    // rx setup meta data 2, parameter metadata
    uint16_t FrequencyBand_allowed_mask;
    uint8_t Mode_allowed_mask;
    uint8_t spare2;
    int16_t Power_list[8];
    uint8_t Diversity_allowed_mask;
    uint8_t SerialPort_allowed_mask;
    uint8_t SerialOut2_allowed_mask;
    uint8_t radio_count; // radios the receiver has, 1 or 2
    uint8_t radio_bands; // bit per SX_FHSS_FREQUENCY_BAND_ENUM it can run
    uint8_t spare3[3];
}) tRxCmdFrameRxSetupData; // 64 bytes


// send from Tx to do SET_RX_PARAMS
// Note: encryptable, the last 12 bytes are the room for nonce and MAC
PACKED(
typedef struct
{
    uint8_t cmd;
    uint8_t spare;
    // rx parameter values
    char BindPhrase_6[6];
    uint8_t FrequencyBand : 4;
    uint8_t Mode : 4;
    uint8_t Ortho : 4;
    uint8_t Privacy : 4;
    uint8_t Region;
    uint8_t DynPower;
    uint8_t LinkSplit;
    uint8_t RadioScheme;
    tCmdFrameRxParameters RxParams; // 8 bytes
    uint32_t CustomFreqKHz;
    uint16_t CustomSpanKHz;
    uint8_t CustomLbt;
    uint8_t CustomPowerMaxDbm;
    uint16_t tx_firmware_version_u16; // 16.64.64
    uint16_t tx_setup_layout_u16; // 16.64.64
    uint8_t spare3[18];
    //--
    uint8_t reserved_for_crypto[12]; // do not use
}) tTxCmdFrameRxParams; // 64 bytes


// for type casting to get the header
PACKED(
typedef struct
{
    uint8_t cmd;
}) tCmdFrameHeader; // 1 byte



#endif // FRAME_TYPES_H
