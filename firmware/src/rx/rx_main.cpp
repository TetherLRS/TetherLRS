//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
// OlliW @ www.olliw.eu
// TetherLRS changes: Copyright (c) TetherLRS project
//*******************************************************
// mLRS RX
//*******************************************************


#define DBG_MAIN(x)
#define DBG_MAIN_SLIM(x)
#define DEBUG_ENABLED
#define FAIL_ENABLED


// we set the priorities here to have an overview, SysTick is at 15
#define CLOCK_IRQ_PRIORITY          10
#define UARTB_IRQ_PRIORITY          11 // serial
#define UART_IRQ_PRIORITY           12 // out pin
#define UARTF_IRQ_PRIORITY          11 // debug
#define SX_DIO_EXTI_IRQ_PRIORITY    13
#define SX2_DIO_EXTI_IRQ_PRIORITY   13 // on single spi diversity systems must be equal to DIO priority
#define SWUART_TIM_IRQ_PRIORITY      9 // debug on swuart
#define FDCAN_IRQ_PRIORITY          14

#include "../common/common_conf.h"
#include "../common/common_types.h" // includes setup_types.h
#include "../common/sx-drivers/sx12xx.h"

#if defined ESP8266 || defined ESP32

#include "../common/hal/esp-glue.h"
#include "../modules/stm32ll-lib/src/stdstm32.h"
#include "../modules/esp-lib/esp-peripherals.h"
#include "../modules/esp-lib/esp-mcu.h"
#include "../modules/esp-lib/esp-stack.h"
#include "../common/hal/hal.h"
#include "../modules/esp-lib/esp-delay.h" // these are dependent on hal
#include "../modules/esp-lib/esp-eeprom.h"
#include "../modules/esp-lib/esp-spi.h"
#ifdef USE_SERIAL
#include "../modules/esp-lib/esp-uartb.h"
#endif
#ifdef USE_USB
#include "../modules/esp-lib/esp-usb.h"
#endif
#ifdef USE_DEBUG
#ifdef DEVICE_HAS_DEBUG_SWUART
#include "../modules/esp-lib/esp-uart-sw.h"
#else
#include "../modules/esp-lib/esp-uartf.h"
#endif
#endif
#include "../common/hal/esp-timer.h"
#include "../common/hal/esp-powerup.h"
#include "../common/hal/esp-rxclock.h"

#else

#include "../common/hal/glue.h"
#include "../modules/stm32ll-lib/src/stdstm32.h"
#include "../modules/stm32ll-lib/src/stdstm32-peripherals.h"
#include "../modules/stm32ll-lib/src/stdstm32-mcu.h"
#include "../modules/stm32ll-lib/src/stdstm32-dac.h"
#include "../modules/stm32ll-lib/src/stdstm32-stack.h"
#include "../common/thirdparty/stdstm32-exti.h"
#ifdef STM32WL
#include "../modules/stm32ll-lib/src/stdstm32-subghz.h"
#endif
#include "../common/hal/hal.h"
#include "../modules/stm32ll-lib/src/stdstm32-delay.h" // these are dependent on hal
#include "../modules/stm32ll-lib/src/stdstm32-eeprom.h"
#include "../modules/stm32ll-lib/src/stdstm32-spi.h"
#ifdef USE_SX2
#include "../modules/stm32ll-lib/src/stdstm32-spib.h"
#endif
#ifdef USE_SERIAL
#include "../modules/stm32ll-lib/src/stdstm32-uartb.h"
#endif
#ifdef USE_SERIAL_OUT2
#include "../modules/stm32ll-lib/src/stdstm32-uart.h"
#endif
#ifdef USE_DEBUG
#ifdef DEVICE_HAS_DEBUG_SWUART
#include "../modules/stm32ll-lib/src/stdstm32-uart-sw.h"
#else
#include "../modules/stm32ll-lib/src/stdstm32-uartf.h"
#endif
#endif
#ifdef USE_I2C
#include "../modules/stm32ll-lib/src/stdstm32-i2c.h"
#endif
#include "../common/hal/timer.h"
#include "powerup.h"
#include "rxclock.h"

#endif //#if defined ESP8266 || defined ESP32

#include "../common/mavlink/fmav.h"
#include "../common/setup.h"
#include "../common/common.h"

// TetherLRS: what this image is, for the tool to read from a flash it reached through the ROM bootloader
#include "../common/tlrs_marker.h"
const char tlrs_marker[] = TLRS_MARKER("Rx");
const char* volatile tlrs_marker_ref = tlrs_marker; // read at startup: the linker must keep the text
#include "../common/diversity.h"
#ifdef DEVICE_HAS_ANTENNA_SWITCH
#include "../common/antenna_switch.h"
#endif
#include "../common/arq.h"
#include "../common/bond.h"
#include "../common/crypto.h"
//#include "../common/time_stats.h" // un-comment if you want to use
//#include "../common/test.h" // un-comment if you want to compile for board test



#include "../common/libs/fifo.h"

tFifo<uint8_t, 512> loop_fifo; // Rx Test loopback: what came down waits here to go back up

tRxClock rxclock;
tPowerupCounter powerup;
tRDiversity rdiversity;
tTDiversity tdiversity;
#ifdef DEVICE_HAS_ANTENNA_SWITCH
tAntennaSwitch antsw;
#endif
tTransmitArq tarq[2]; // lane A, and lane B of bonded radios
tReceiveArq rarq[2];
tCrypto crypto;


// is required in bind.h
void clock_reset(void) { rxclock.Reset(); }


//-------------------------------------------------------
// MAVLink & MSP & DroneCAN
//-------------------------------------------------------

#include "mavlink_interface_rx.h"

tRxMavlink mavlink;

#include "msp_interface_rx.h"

tRxMsp msp;

#include "serial_out2.h"
tRxSerialOut2 serial_out2;
#include "sx_serial_interface_rx.h"

tRxSxSerial sx_serial;

#include "rx_cli.h"

tRxCli rxcli;

void rxcli_handle_cmd_long(fmav_message_t* const msg) { rxcli.HandleCmdLong(msg); }

#include "dronecan_interface_rx.h"

tRxDroneCan dronecan;


//-------------------------------------------------------
// Init
//-------------------------------------------------------

// the receiver answers 1 ms after the frame, and later by its slot when there are several
uint16_t rxclock_shift_us(void)
{
    uint16_t shift_us = 1000;
    if (Config.RxSlot < Config.RxCount) shift_us += Config.RxSlot * Config.SlotPeriod_ms * 1000;
    return shift_us;
}


void init_hw(void)
{
    delay_init();
    systembootloader_init(); // after delay_init() since it may need delay
    timer_init();

    leds_init();
    button_init();


    fan.Init();
    dbg.Init();

    sx.Init();
    sx2.Init();

    setup_init();
    powerup.Init();

    rxclock.Init(Config.frame_rate_ms, rxclock_shift_us()); // rxclock needs Config, so call after setup_init()

    uartb_port.Init();
    dronecan_port.Init();
    Serials.Init(Setup.Rx.SerialPort, Config.SerialBaudrate);
    serial_out2.Init(Setup.Rx.SerialOut2, Config.SerialBaudrate2);
    dronecan.Init(Setup.Rx.SerialPort); // after delay_init() since it needs delay
}


//-------------------------------------------------------
// SX12xx
//-------------------------------------------------------

volatile uint32_t irq_status;
volatile uint32_t irq2_status;

IRQHANDLER(
void SX_DIO_EXTI_IRQHandler(void)
{
    sx_dio_exti_isr_clearflag();
    irq_status = sx.GetAndClearIrqStatus(SX_IRQ_ALL);
    if (irq_status & SX_IRQ_RX_DONE) {
        if (bind.IsInBind()) {
            uint64_t bind_signature;
            sx.ReadBuffer(0, (uint8_t*)&bind_signature, 8);
            if (bind_signature != bind.TxSignature) irq_status = 0; // not binding frame, so ignore it
        } else {
            uint16_t sync_word;
            sx.ReadBuffer(0, (uint8_t*)&sync_word, 2); // rxStartBufferPointer is always 0, so no need for sx.GetRxBufferStatus()
            if (sync_word != Config.FrameSyncWord) irq_status = 0; // not for us, so ignore it
        }
    }
})
#ifdef USE_SX2
IRQHANDLER(
void SX2_DIO_EXTI_IRQHandler(void)
{
    sx2_dio_exti_isr_clearflag();
    irq2_status = sx2.GetAndClearIrqStatus(SX2_IRQ_ALL);
    if (irq2_status & SX2_IRQ_RX_DONE) {
        if (bind.IsInBind()) {
            uint64_t bind_signature;
            sx2.ReadBuffer(0, (uint8_t*)&bind_signature, 8);
            if (bind_signature != bind.TxSignature) irq2_status = 0;
        } else {
            uint16_t sync_word;
            sx2.ReadBuffer(0, (uint8_t*)&sync_word, 2);
            if (sync_word != Config.FrameSyncWord) irq2_status = 0;
        }
    }
})
#endif


uint8_t link_rx1_status;
uint8_t link_rx2_status;
bool rx_silent; // the Tx's last frame said Downlink off; frames keep saying it, so nothing is stored
bool bond_single_rx; // bonded radios: the Tx's last frame said one frame goes out on both; mirrored in the replies
bool rxFrameB_valid; // lane B's frame holds a payload it may send again
tBondReorder bond; // the Tx's lanes, put out in order


// the data a cycle carries up and down, for the autopilot's flow control: a frame's payload less the link header and
// the crypto, twice with bonded lanes
uint16_t rx_link_data_up(void)
{
    return (FRAME_PAYLOAD_LEN(Config.FrameLenRx) - FRAME_HDR_LEN - crypto.NonceLen()) * ((Config.Bonded && !bond_single_rx) ? 2 : 1);
}

uint16_t rx_link_data_down(void)
{
    return (FRAME_PAYLOAD_LEN(Config.FrameLenTx) - FRAME_HDR_LEN - crypto.NonceLen()) * ((Config.Bonded && !bond_single_rx) ? 2 : 1);
}


// what came down goes out of the serial port, or back up with the loopback test
void rx_output(uint8_t* const data, uint8_t len)
{
    if (Setup.Rx.Test == RX_TEST_LOOPBACK) {
        loop_fifo.PutBuf(data, len);
    } else {
        sx_serial.putbuf(data, len);
    }
}


//-- Tx/Rx cmd frame handling

uint8_t link_task;
uint8_t transmit_frame_type;
bool doParamsStore;
bool doLinkRestart; // Mode = auto: the link restarts with mode_auto_current
bool link_restart_after_ack; // the answer naming the new mode went out: restart once the Tx has it
uint8_t mode_auto_lost_s;


void link_task_init(void)
{
    link_task = LINK_TASK_NONE;
    transmit_frame_type = TRANSMIT_FRAME_TYPE_NORMAL;

    doParamsStore = false;
    doLinkRestart = false;
    link_restart_after_ack = false;
    mode_auto_lost_s = 0;
    mode_auto_pending = MODE_AUTO;
}


void link_task_set(uint8_t task)
{
    link_task = task;
    transmit_frame_type = TRANSMIT_FRAME_TYPE_CMD;
}


// we clear then we receive a non-cmd frame, as this indicates that the tx has gotten the response
// we also should clear then disconnected
void link_task_reset(void)
{
    link_task = LINK_TASK_NONE;
    transmit_frame_type = TRANSMIT_FRAME_TYPE_NORMAL;
}


void process_received_txcmdframe(tTxFrame* const frame)
{
tCmdFrameHeader* head = (tCmdFrameHeader*)FRAME_DATA(frame);

    // with several receivers a command is for one of them, or for all with the settings of the one it names
    uint8_t hdr = FRAME_LINK_HDR(frame);
    bool for_me = !Config.LinkHeader || ((hdr >> LINK_HDR_TX_SLOT_SHIFT) & LINK_HDR_SLOT_MASK) == Config.RxSlot;
    bool for_all = Config.LinkHeader && (hdr & LINK_HDR_TX_ALL);
    if (!for_me && !for_all) return;

    switch (head->cmd) {
    case FRAME_CMD_GET_RX_SETUPDATA:
        if (!for_me) break;
        // request to send setup data, trigger sending RX_SETUPDATA in next transmission
        link_task_set(LINK_TASK_RX_SEND_RX_SETUPDATA);
        break;
    case FRAME_CMD_SET_RX_PARAMS:
        // received rx params, trigger sending RX_SETUPDATA in next transmission; the others take the common ones
        unpack_txcmdframe_setrxparams(frame, for_me);
        link_task_set(LINK_TASK_RX_SEND_RX_SETUPDATA);
        break;
    case FRAME_CMD_STORE_RX_PARAMS:
        // got request to store rx params
        doParamsStore = true;
        break;
    case FRAME_CMD_GET_RX_SETUPDATA_WRELOAD:
        if (!for_me) break;
        // request to send setup data, trigger sending RX_SETUPDATA in next transmission
        setup_reload();
        link_task_set(LINK_TASK_RX_SEND_RX_SETUPDATA);
        break;
    case FRAME_CMD_GET_RX_SETUPDATA_STARTUP:
        if (!for_me) break; // the randoms in it are this slot's
        // request to send setup data, trigger sending RX_SETUPDATA in next transmission
        crypto.SetSessionKeysFromEncryptedBuf(FRAME_DATA(frame) + 1);
        Config.SessionRandom = crypto.SessionRandom(); // only for reporting
        link_task_set(LINK_TASK_RX_SEND_RX_SETUPDATA);
        break;
    case FRAME_CMD_SET_LINK: {
        if (!for_me) break;
        // Mode = auto: the mode and band to restart with; the answer names them, the restart waits for the Tx to have
        // the answer; a band or mode this receiver cannot run is not taken, and the answer says none
        uint8_t mode = FRAME_DATA(frame)[1];
        uint8_t band = FRAME_DATA(frame)[2];
        bool band_ok = (band == Config.Band1) || (band == Config.Band2);
        if (Config.ModeAuto && band_ok && mode < MODE_AUTO && (mode_mask_for_band(band, Config.Region) & (1 << mode))) {
            mode_auto_pending = mode;
            band_auto_pending = band;
        } else {
            mode_auto_pending = MODE_AUTO;
        }
        link_task_set(LINK_TASK_RX_SEND_RX_SETUPDATA);
        break; }
    }
}


void pack_rxcmdframe(tRxFrame* const frame, tFrameStats* const frame_stats)
{
    switch (link_task) {
    case LINK_TASK_RX_SEND_RX_SETUPDATA:
        // send rx setup data
        pack_rxcmdframe_rxsetupdata(frame, frame_stats);
        if (mode_auto_pending != MODE_AUTO) link_restart_after_ack = true;
        break;
    }
}


//-- normal Tx, Rx frames handling
// transmit
//   -> do_transmit(antenna)
//       -> prepare_transmit_frame(antenna)
//           -> pack_rx_frame(...) or pack_rxcmdframe(...)
// receive
//   isr:        -> irq_status
//   isr loop:   -> link_rx1/2_status = do_receive(antenna, do_clock_reset)
//   post loop:  -> handle_receive(antenna) or handle_receive_none()
//                  if valid -> process_received_frame(do_payload, frame)
//                               -> decrypt_txframe(frame)
//                                  rcdata_rc1_from_txframe(...) or rcdata_from_txframe(...)

// the data to send up: from the serial port, or what came down with the loopback test
uint8_t rx_read_payload(uint8_t* const payload, uint8_t len_max)
{
uint8_t len = 0;

    if (Setup.Rx.Test == RX_TEST_LOOPBACK) { // the tool's benchmark: what came down goes back up
        while (len < len_max && loop_fifo.Available()) payload[len++] = loop_fifo.Get();
    } else {
        while (len < len_max && sx_serial.available()) payload[len++] = sx_serial.getc();
    }
    return len;
}


void prepare_transmit_frame(uint8_t antenna)
{
uint8_t payload[FRAME_PAYLOAD_MAX];
uint8_t payload_len = 0;
uint8_t payload_b[FRAME_PAYLOAD_MAX];
uint8_t payload_b_len = 0;

    // bonded radios: a frame per lane, as the Tx runs them
    bool lanes = (Config.Bonded && !bond_single_rx);

    // new data only when every lane is free, so the Tx can keep the lanes in order
    bool get_fresh_payload;
    if (lanes) {
        get_fresh_payload = tarq[0].WouldAdvance() && tarq[1].WouldAdvance();
        tarq[0].Settle(get_fresh_payload);
        tarq[1].Settle(get_fresh_payload);
    } else {
        get_fresh_payload = tarq[0].GetFreshPayload();
    }

    if (get_fresh_payload && link_restart_after_ack) { // the Tx has the answer naming the new mode: restart with it
        link_restart_after_ack = false;
        doLinkRestart = true;
    }

    if (get_fresh_payload) {
        if (transmit_frame_type == TRANSMIT_FRAME_TYPE_NORMAL) {
            // read data from serial; not into the last frame before a link restart, it would be lost
            if (doLinkRestart) {
            } else if (connected()) {
                uint8_t len_max = FRAME_PAYLOAD_LEN(Config.FrameLenRx) - FRAME_HDR_LEN - crypto.NonceLen();
                payload_len = rx_read_payload(payload, len_max);
                if (lanes) payload_b_len = rx_read_payload(payload_b, len_max);

                stats.bytes_transmitted.Add(payload_len + payload_b_len);
                stats.serial_data_transmitted.Inc();
            } else {
                sx_serial.flush();
            }
        }
    }

    stats.last_transmit_antenna = antenna;

    tFrameStats frame_stats;
    frame_stats.seq_no = tarq[0].SeqNo();
    frame_stats.ack = rarq[0].AckSeqNo();
    frame_stats.link_header = Config.RxSlot & LINK_HDR_RX_SLOT_MASK;
    if (Config.Bonded && bond_single_rx) frame_stats.link_header |= LINK_HDR_BOND_SINGLE;
    frame_stats.antenna = stats.last_antenna;
    frame_stats.transmit_antenna = antenna;
    frame_stats.rssi = stats.GetLastRssi();
    frame_stats.LQ_link = stats.GetLQ_link();
    frame_stats.LQ_serial = stats.GetLQ_serial();
    frame_stats.power_code = rfpower.Code();

    static bool rxFrame_valid = false; // just for now
    if (get_fresh_payload) {
        if (transmit_frame_type == TRANSMIT_FRAME_TYPE_NORMAL) {
            pack_rxframe(&rxFrame, &frame_stats, payload, payload_len);
        } else {
            pack_rxcmdframe(&rxFrame, &frame_stats);
        }
        rxFrame_valid = true;

        stats.cntFrameTransmitted();
    } else {
        // rxFrame should still hold the previous data
        if (!rxFrame_valid) while(1){} // should not happen

        update_rxframe_stats(&rxFrame, &frame_stats);
        rxFrame_valid = true;

        stats.cntFrameSkipped();
    }

    if (lanes) { // lane B: the same stats, its own seq no and ack; empty beside a command frame
        tFrameStats frame_stats_b = frame_stats;
        frame_stats_b.seq_no = tarq[1].SeqNo();
        frame_stats_b.ack = rarq[1].AckSeqNo();
        frame_stats_b.transmit_antenna = ANTENNA_2;
        if (get_fresh_payload || !rxFrameB_valid) {
            pack_rxframe(&rxFrameB, &frame_stats_b, payload_b, (transmit_frame_type == TRANSMIT_FRAME_TYPE_NORMAL) ? payload_b_len : 0);
            rxFrameB_valid = true;
        } else {
            update_rxframe_stats(&rxFrameB, &frame_stats_b);
        }
    }

    tarq[0].SetRetryCntAuto(stats.GetFrameCnt(), Config.Mode);
    tarq[1].SetRetryCntAuto(stats.GetFrameCnt(), Config.Mode);
}


void process_received_frame(tTxFrame* const frame, uint8_t lane)
{
    // the status fields are of one receiver at a time: with several, the link header says which
    if (!Config.LinkHeader || ((FRAME_LINK_HDR(frame) >> LINK_HDR_TX_SLOT_SHIFT) & LINK_HDR_SLOT_MASK) == Config.RxSlot) {
        stats.received_antenna = frame->status.antenna;
        stats.received_transmit_antenna = frame->status.transmit_antenna;
        stats.received_rssi = rssi_i8_from_u7(frame->status.rssi_u7);
    }

    stats.received_fhss_index_band = frame->status.fhss_index_band;
    stats.received_fhss_index = frame->status.fhss_index;

    stats.received_LQ_serial = frame->status.LQ_serial;
    rx_silent = frame->status.rx_silent;

    if (Config.Bonded) { // the Tx says how its frames go: a lane each, or one on both; lane B starts afresh on a change
        bool single = (FRAME_LINK_HDR(frame) & LINK_HDR_BOND_SINGLE) != 0;
        if (single != bond_single_rx) {
            bond_single_rx = single;
            tarq[1].Disconnected();
            rarq[1].Disconnected();
            tarq[1].payload_seq_no = tarq[0].payload_seq_no; // the lanes' payloads pair by seq no
            rxFrameB_valid = false; // and lane B has no old payload to send again
        }
    }

    bool accept_payload = rarq[lane].AcceptPayload(); // a payload sent again is one we already have

    // handle cmd frame
    if (frame->status.frame_type == FRAME_TYPE_CMD || frame->status.frame_type == FRAME_TYPE_CMD_ENCRYPTED) {
        if (accept_payload) process_received_txcmdframe(frame);
        return;
    }

    // a data frame tells the Tx has its answer, on lane A: lane B carries data beside lane A's commands; and a link
    // change still waiting was given up, since after a change the Tx sends one more SET_LINK and restarts
    if (lane == BOND_LANE_A) {
        link_task_reset();
        mode_auto_pending = MODE_AUTO;
        link_restart_after_ack = false;
    }

    // output data on serial, but only if connected; bonded lanes go out in order
    if (!connected() || !accept_payload) return;
    if (Config.Bonded) {
        bond.Accepted(lane, frame->status.seq_no, FRAME_DATA(frame), FRAME_DATA_LEN(frame));
    } else {
        rx_output(FRAME_DATA(frame), FRAME_DATA_LEN(frame));
    }

    stats.bytes_received.Add(FRAME_DATA_LEN(frame));
    stats.serial_data_received.Inc();
}


//-- transmit/receive handling api

void do_transmit(uint8_t antenna) // we send a frame to transmitter
{
    if (bind.IsInBind()) {
        bind.do_transmit(antenna);
        return;
    }

    prepare_transmit_frame(antenna);

    // to test asymmetric connection, fake rxFrame, to no send doesn't work as it blocks the sx
    if (Config.Bonded && !bond_single_rx) {
        sxSendFrames(&rxFrame, &rxFrameB, Config.FrameLenRx, SEND_FRAME_TMO_MS); // a lane out of each radio
    } else {
        sxSendFrame(antenna, &rxFrame, Config.FrameLenRx, SEND_FRAME_TMO_MS); // 10ms tmo
    }
}


// called in isr loop
uint8_t do_receive(uint8_t antenna, bool do_clock_reset) // we receive a frame from receiver
{
uint8_t res;
uint8_t rx_status = RX_STATUS_INVALID; // this also signals that a frame was received

    if (bind.IsInBind()) {
        return bind.do_receive(antenna, do_clock_reset);
    }

    // we don't need to read sx.GetRxBufferStatus(), but hey
    // we could save 2 byte's time by not reading sync_word again, but hey
    sxReadFrame(antenna, &txFrame, &txFrame2, Config.FrameLenTx);
    res = (antenna == ANTENNA_1) ? check_txframe(&txFrame) : check_txframe(&txFrame2);

    if (res != CHECK_OK) {
        DBG_MAIN(dbg.puts("fail ");dbg.putc('\n');)
dbg.puts("fail a");dbg.putc(antenna+'0');dbg.puts(" ");dbg.puts(u8toHEX_s(res));dbg.putc('\n');
    }

    // must not happen !
    // it can happen though, I've observed it on R9, maybe if in the ca 1 ms after receive the sx starts receiving something?
    if (res == CHECK_ERROR_SYNCWORD) { FAIL_WMSG("do_receive() CHECK_ERROR_SYNCWORD"); return RX_STATUS_INVALID; }

    // can be CHECK_ERROR_HEADER, CHECK_ERROR_CRC, CHECK_OK

    if (res == CHECK_OK) {
        if (do_clock_reset) rxclock.Reset();
        rx_status = RX_STATUS_VALID;
    }

    // we want to have the rssi,snr stats even if it's a bad packet
    sxGetPacketStatus(antenna, &stats);

    return rx_status; // INVALID, VALID
}


// called in doPostReceive loop; lane: whose ARQ the frame feeds, A unless the radios run bonded lanes
bool handle_receive(uint8_t antenna, uint8_t lane) // called if INVALID, VALID
{
uint8_t rx_status;
tTxFrame* frame;

    if (antenna == ANTENNA_1) {
        rx_status = link_rx1_status;
        frame = &txFrame;
    } else {
        rx_status = link_rx2_status;
        frame = &txFrame2;
    }

    if (bind.IsInBind()) {
        bind.handle_receive(antenna, rx_status);
        return true;
    }

    // can be INVALID, VALID (NONE is handled elsewhere)

    if (rx_status < RX_STATUS_INVALID) { // = RX_STATUS_NONE, must not happen
        FAIL_WSTATE(BLINK_4, "rx_status failure", 0,0, link_rx1_status, link_rx2_status);
    }

    // check decrypt_txframe()
    bool decrypt_ok = false;
    if (rx_status > RX_STATUS_INVALID) {
        decrypt_ok = decrypt_txframe(frame); // don't do for INVALID, payload is nonsense
    }

    // handle transmit ARQ
    if (rx_status > RX_STATUS_INVALID) { // = RX_STATUS_VALID: we have valid information on ack
        if (decrypt_ok) {
            // with several receivers each has its ack bit in the link header; bonded lanes have a frame each
            uint8_t ack = (Config.LinkHeader && !Config.Bonded) ? (FRAME_LINK_HDR(frame) >> Config.RxSlot) & 0x01 : frame->status.ack;
            tarq[lane].AckReceived(ack);
        } else {
            tarq[lane].Disconnected(); // force sending a new payload
        }
    } else {
        tarq[lane].FrameMissed();
    }

    // handle receive ARQ, must come before process_received_frame()
    if (rx_status == RX_STATUS_VALID && decrypt_ok) { // we have valid information on seq_no
        rarq[lane].Received(frame->status.seq_no);
    } else {
        rarq[lane].FrameMissed();
    }
    // check this before received data may be passed to parsers
    if (rarq[lane].FrameLost()) {
        mavlink.FrameLost();
        msp.FrameLost();
    }

    if (rx_status > RX_STATUS_INVALID && decrypt_ok) { // = RX_STATUS_VALID

        process_received_frame(frame, lane);

        stats.doValidFrameReceived(); // counts both rx and cmd frames, but cmd frames are rare, so no worry

    } else { // RX_STATUS_INVALID or !decrypt_ok
    }

    // we set it for all received frames
    stats.last_antenna = antenna;

    // we count all received frames
    stats.doFrameReceived();

    return decrypt_ok;
}


// called in doPostReceive loop
void handle_receive_none(uint8_t lane) // called if RX_STATUS_NONE
{
    tarq[lane].FrameMissed();
    rarq[lane].FrameMissed();
}


//##############################################################################################################
//*******************************************************
// MAIN routine
//*******************************************************

uint16_t tick_1hz;
uint16_t tick_1hz_commensurate;

uint8_t link_state;
uint8_t link_tx_status;
uint8_t connect_state;
uint16_t connect_tmo_cnt;
uint8_t connect_sync_cnt;
uint8_t connect_fhss_index_band_seen;
uint8_t connect_listen_cnt;
bool connect_occured_once;

uint8_t doPostReceive2_cnt;
bool doPostReceive2;


bool connected(void)
{
    return (connect_state == CONNECT_STATE_CONNECTED);
}


void main_loop(void)
{
#ifdef BOARD_TEST_H
    main_test();
#endif
INITCONTROLLER_ONCE
    usb_port.InitOnce();
    stack_check_init();
RESTARTCONTROLLER
    init_hw();
    DBG_MAIN(dbg.puts("\n\n\nHello\n\n");)

    // startup sign of life
    leds.Init();
    (void)tlrs_marker_ref;

    // start up sx
    if (!sx.isOk()) { FAILALWAYS(BLINK_RD_GR_OFF, "Sx not ok"); } // fail!
    if (!sx2.isOk()) { FAILALWAYS(BLINK_GR_RD_OFF, "Sx2 not ok"); } // fail!
    irq_status = irq2_status = 0;
    IF_SX(sx.StartUp(&Config.Sx));
    IF_SX2(sx2.StartUp(&Config.Sx2));
    bind.Init();
    fhss.Init(&Config.Fhss, &Config.Fhss2);
    fhss.Start();
    rfpower.Init();
    regulatory.Init();

    sx.SetRfFrequency(fhss.GetCurrFreq());
    sx2.SetRfFrequency(fhss.GetCurrFreq2());

    doPostReceive2_cnt = 0;
    doPostReceive2 = false;
    link_state = LINK_STATE_RECEIVE;
    connect_state = CONNECT_STATE_LISTEN;
    connect_tmo_cnt = 0;
    connect_sync_cnt = 0;
    connect_fhss_index_band_seen = 0;
    connect_listen_cnt = 0;
    connect_occured_once = false;
    link_rx1_status = link_rx2_status = RX_STATUS_NONE;
    link_tx_status = TX_STATUS_NONE;
    link_task_init();

    stats.Init(Config.LQAveragingPeriod, Config.frame_rate_hz, Config.frame_rate_ms);
    rdiversity.Init();
    tdiversity.Init(Config.frame_rate_ms);
#ifdef DEVICE_HAS_ANTENNA_SWITCH
    antsw.Init();
#endif
    tarq[0].Init();
    tarq[1].Init();
    rarq[0].Init();
    rarq[1].Init();
    bond_single_rx = false;
    rxFrameB_valid = false;
    bond.Init(rx_output, FRAME_PAYLOAD_MAX);
    crypto.Init(tCrypto::RX,
        Setup.Common[0].BindPhrase, Setup.peer_uid[0], Setup.tx_random[0],
        Setup.Common[0].Privacy, Setup.LinkKey);

    loop_fifo.Init();
    mavlink.Init();
    rxcli.Init();
    msp.Init();
    sx_serial.Init();
    fan.SetPower(SX_OR_SX2(sx.RfPower_dbm(),sx2.RfPower_dbm()));
    dronecan.Start();

    tick_1hz = 0;
    tick_1hz_commensurate = 0;
    resetSysTask(); // helps in avoiding too short first loop
INITCONTROLLER_END

    //-- SysTask handling

    if (doSysTask()) {

        if (connect_tmo_cnt) {
            connect_tmo_cnt--;
        }

        leds.Tick_ms(connected());

        DECc(tick_1hz, SYSTICK_DELAY_MS(1000));

        if (!connect_occured_once) bind.AutoBind();
        bind.Tick_ms();
        fan.SetPower(SX_OR_SX2(sx.RfPower_dbm(),sx2.RfPower_dbm()));
        fan.Tick_ms();
        dronecan.Tick_ms();

        if (!tick_1hz) {
            dbg.puts(".");
/*            dbg.puts("\nRX: ");
            dbg.puts(u8toBCD_s(stats.GetLQ_link())); dbg.putc(',');
            dbg.puts(u8toBCD_s(stats.GetLQ_serial()));
            dbg.puts(" (");
            dbg.puts(u8toBCD_s(stats.frames_received.GetLQ())); dbg.putc(',');
            dbg.puts(u8toBCD_s(stats.valid_frames_received.GetLQ()));
            dbg.puts("),");
            dbg.puts(u8toBCD_s(stats.received_LQ_serial)); dbg.puts(", ");

            dbg.puts(s8toBCD_s(stats.last_rssi1)); dbg.putc(',');
            dbg.puts(s8toBCD_s(stats.received_rssi)); dbg.puts(", ");
            dbg.puts(s8toBCD_s(stats.last_snr1)); dbg.puts("; ");

            dbg.puts(u16toBCD_s(stats.bytes_transmitted.GetBytesPerSec())); dbg.puts(", ");
            dbg.puts(u16toBCD_s(stats.bytes_received.GetBytesPerSec())); dbg.puts("; "); */
        }
    }

    //-- SX handling

    switch (link_state) {
    case LINK_STATE_RECEIVE:
        if (connect_state >= CONNECT_STATE_SYNC) { // we hop only if not in listen
            fhss.HopToNext();
        }
        sx.SetRfFrequency(fhss.GetCurrFreq());
        sx2.SetRfFrequency(fhss.GetCurrFreq2());
        sx.SetFrameLength(Config.FrameLenTx); // the Link Split profiles differ per direction
        sx2.SetFrameLength(Config.FrameLenTx);
        IF_ANTENNA1(sx.SetToRx());
        IF_ANTENNA2(sx2.SetToRx());
        link_state = LINK_STATE_RECEIVE_WAIT;
        link_rx1_status = link_rx2_status = RX_STATUS_NONE;
        irq_status = irq2_status = 0;
        DBG_MAIN_SLIM(dbg.puts("\n>");)
        break;

    case LINK_STATE_TRANSMIT:
        // Rx Downlink off: beside a receiver that answers; a slot beyond Rx Count has no reply window either
        if ((rx_silent || Setup.Rx.Downlink == RX_DOWNLINK_OFF || Config.RxSlot >= Config.RxCount) && !bind.IsInBind()) {
            sx_serial.flush(); // or the autopilot's old data would go out once the downlink is back
            link_state = LINK_STATE_RECEIVE;
            break;
        }
        sx.SetFrameLength(Config.FrameLenRx);
        sx2.SetFrameLength(Config.FrameLenRx);
        rfpower.Update();
        bool go = regulatory.TxGo(tdiversity.Antenna(), fhss.GetCurrFreq());
        if (go && Config.TransmitBoth && !Config.IsDualBand) go = regulatory.TxGo(ANTENNA_2, fhss.GetCurrFreq2()); // the 2nd channel books its time too
        if (!go) { // not this frame, the ARQ resends its data
            link_state = LINK_STATE_RECEIVE;
            break;
        }
        do_transmit(tdiversity.Antenna());
        link_state = LINK_STATE_TRANSMIT_WAIT;
        link_tx_status = TX_STATUS_NONE;
        irq_status = irq2_status = 0; // important, in low connection condition, RxDone isr could trigger
        DBG_MAIN_SLIM(dbg.puts("t");)
        break;
    }//end of switch(link_state)

IF_SX(
    if (irq_status) {
        if (link_state == LINK_STATE_TRANSMIT_WAIT) {
            if (irq_status & SX_IRQ_TX_DONE) {
                irq_status = 0;
                link_tx_status |= TX_STATUS_TX1_DONE;
                if (!Config.TransmitBoth || (link_tx_status & TX_STATUS_TX2_DONE)) { link_state = LINK_STATE_RECEIVE; }
                DBG_MAIN_SLIM(dbg.puts("1<");)
            }
        } else
        if (link_state == LINK_STATE_RECEIVE_WAIT) {
            if (irq_status & SX_IRQ_RX_DONE) {
                irq_status = 0;
                if (link_rx1_status != RX_STATUS_VALID) { // the cycle's frame stays; what follows are other receivers' replies
                    bool do_clock_reset = (link_rx2_status == RX_STATUS_NONE);
                    link_rx1_status = do_receive(ANTENNA_1, do_clock_reset);
                    if (link_rx1_status == RX_STATUS_VALID) sx.HandleAFC();
                    if (link_rx1_status == RX_STATUS_VALID && Config.RxCount > 1) sx.SetToIdle(); // nothing more for us this cycle
                }
                DBG_MAIN_SLIM(dbg.puts("1!");)
            }
        }

        if (irq_status) { // these should not happen
            if (irq_status & SX_IRQ_TIMEOUT) {
                FAIL_WSTATE(BLINK_COMMON, "IRQ TMO FAIL", irq_status, link_state, link_rx1_status, link_rx2_status);
            }
            if (irq_status & SX_IRQ_RX_DONE) { // R, T, TW
                FAIL_WSTATE(BLINK_RD_GR_OFF, "IRQ RX DONE FAIL", irq_status, link_state, link_rx1_status, link_rx2_status);
            }
            if (irq_status & SX_IRQ_TX_DONE) {
                FAIL_WSTATE(BLINK_GR_RD_OFF, "IRQ TX DONE FAIL", irq_status, link_state, link_rx1_status, link_rx2_status);
            }
            irq_status = 0;
            link_state = LINK_STATE_RECEIVE;
            link_rx1_status = link_rx2_status = RX_STATUS_NONE;
            DBG_MAIN_SLIM(dbg.puts("1?");)
        }
    }//end of if(irq_status)
);
IF_SX2(
    if (irq2_status) {
        if (link_state == LINK_STATE_TRANSMIT_WAIT) {
            if (irq2_status & SX2_IRQ_TX_DONE) {
                irq2_status = 0;
                link_tx_status |= TX_STATUS_TX2_DONE;
                if (!Config.TransmitBoth || (link_tx_status & TX_STATUS_TX1_DONE)) { link_state = LINK_STATE_RECEIVE; }
                DBG_MAIN_SLIM(dbg.puts("2<");)
            }
        } else
        if (link_state == LINK_STATE_RECEIVE_WAIT) {
            if (irq2_status & SX2_IRQ_RX_DONE) {
                irq2_status = 0;
                if (link_rx2_status != RX_STATUS_VALID) {
                    bool do_clock_reset = (link_rx1_status == RX_STATUS_NONE);
                    link_rx2_status = do_receive(ANTENNA_2, do_clock_reset);
                    if (link_rx2_status == RX_STATUS_VALID) sx2.HandleAFC();
                    if (link_rx2_status == RX_STATUS_VALID && Config.RxCount > 1) sx2.SetToIdle();
                }
                DBG_MAIN_SLIM(dbg.puts("2!");)
            }
        }

        if (irq2_status) { // this should not happen
            if (irq2_status & SX2_IRQ_TIMEOUT) {
                FAIL_WSTATE(BLINK_ALTERNATE, "IRQ2 TMO FAIL", irq2_status, link_state, link_rx1_status, link_rx2_status);
            }
            if (irq2_status & SX2_IRQ_RX_DONE) { // R, T, TW
                FAIL_WSTATE(BLINK_RD_GR_ON, "IRQ2 RX DONE FAIL", irq2_status, link_state, link_rx1_status, link_rx2_status);
            }
            if (irq2_status & SX2_IRQ_TX_DONE) {
                FAIL_WSTATE(BLINK_GR_RD_ON, "IRQ2 TX DONE FAIL", irq2_status, link_state, link_rx1_status, link_rx2_status);
            }
            irq2_status = 0;
            link_state = LINK_STATE_RECEIVE;
            link_rx1_status = link_rx2_status = RX_STATUS_NONE;
            DBG_MAIN_SLIM(dbg.puts("2?");)
        }
    }//end of if(irq2_status)
);

    // this happens ca 1 ms after a frame was or should have been received
    if (doPostReceive) {
        doPostReceive = false;

        bool frame_received, valid_frame_received, invalid_frame_received;
        if (USE_ANTENNA1 && USE_ANTENNA2) {
            frame_received = (link_rx1_status > RX_STATUS_NONE) || (link_rx2_status > RX_STATUS_NONE);
            valid_frame_received = (link_rx1_status > RX_STATUS_INVALID) || (link_rx2_status > RX_STATUS_INVALID);
            invalid_frame_received = frame_received && !valid_frame_received;
        } else if (USE_ANTENNA2) {
            frame_received = (link_rx2_status > RX_STATUS_NONE);
            valid_frame_received = (link_rx2_status > RX_STATUS_INVALID);
            invalid_frame_received = (link_rx2_status == RX_STATUS_INVALID);
        } else { // use antenna1
            frame_received = (link_rx1_status > RX_STATUS_NONE);             // INVALID, VALID
            valid_frame_received = (link_rx1_status > RX_STATUS_INVALID);    // VALID
            invalid_frame_received = (link_rx1_status == RX_STATUS_INVALID); // INVALID, = frame_received && !valid_frame_received
        }

/*dbg.puts("\n> 1: ");
dbg.puts(s8toBCD_s(stats.last_rssi1));
dbg.puts(" 2: ");
dbg.puts(s8toBCD_s(stats.last_rssi2));*/

        // bonded radios: each antenna is a lane, unless the Tx's frames say one went out on both: the better copy then
        bool lanes = false;
        if (Config.Bonded) {
            bool single = bond_single_rx;
            if (link_rx1_status == RX_STATUS_VALID) single = (txFrame.payload[0] & LINK_HDR_BOND_SINGLE) != 0;
            else if (link_rx2_status == RX_STATUS_VALID) single = (txFrame2.payload[0] & LINK_HDR_BOND_SINGLE) != 0;
            lanes = !single;
        }

        if (lanes) {
            bool ok_a = false, ok_b = false;
            if (link_rx1_status > RX_STATUS_NONE) ok_a = handle_receive(ANTENNA_1, BOND_LANE_A); else handle_receive_none(BOND_LANE_A);
            if (link_rx2_status > RX_STATUS_NONE) ok_b = handle_receive(ANTENNA_2, BOND_LANE_B); else handle_receive_none(BOND_LANE_B);
            valid_frame_received = ok_a || ok_b;
            invalid_frame_received = frame_received && !valid_frame_received;
        } else
        if (frame_received) { // frame received
            uint8_t antenna = ANTENNA_1;
            if (USE_ANTENNA1 && USE_ANTENNA2) {
                antenna = rdiversity.Antenna(link_rx1_status, link_rx2_status, stats.last_rssi1, stats.last_rssi2);
            } else if (USE_ANTENNA2) {
                antenna = ANTENNA_2;
            }
            bool decrypt_ok = handle_receive(antenna, BOND_LANE_A);
            if (!decrypt_ok) { // if decrypt fails, count it as invalid
                valid_frame_received = false;
                invalid_frame_received = true;
            }
        } else {
            handle_receive_none(BOND_LANE_A);
        }

#ifdef DEVICE_HAS_ANTENNA_SWITCH
        antsw.Update(valid_frame_received, stats.last_rssi1);
#endif

        if (TRANSMIT_USE_ANTENNA1 && TRANSMIT_USE_ANTENNA2) {
            tdiversity.DoEstimate(link_rx1_status, link_rx2_status, stats.last_rssi1, stats.last_rssi2);
        } else if (TRANSMIT_USE_ANTENNA2) {
            tdiversity.SetAntenna(ANTENNA_2);
        } else {
            tdiversity.SetAntenna(ANTENNA_1);
        }

        // serial data is received if !IsInBind() && RX_STATUS_VALID && !FRAME_TYPE_CMD && connected()
        // a missed frame is not lost yet: the ARQ tells the parsers when a payload really went missing

        // check fhss index
        // do it only in LISTEN and SYNC
        // for older versions not supporting the mechanism holds received_fhss_index >= 63
        if ((connect_state <= CONNECT_STATE_SYNC) && valid_frame_received &&
            (stats.received_fhss_index < 63)) { // only when supported by tx, older version don't offer this
            if (stats.received_fhss_index_band == 0) {
                if (fhss.GetCurrI() != stats.received_fhss_index) { // somehow wrong frequency, discard
                    valid_frame_received = false;
                    invalid_frame_received = true;
                    fhss.SetCurrI(stats.received_fhss_index); // set 1st band, should only happen for dual band
                } else { // ok
                    connect_fhss_index_band_seen |= 0x01; // 0 seen
                }
            } else {
                if (fhss.GetCurrI2() != stats.received_fhss_index) { // somehow wrong frequency, so discard
                    valid_frame_received = false;
                    invalid_frame_received = true;
                    fhss.SetCurrI2(stats.received_fhss_index); // set 2nd band, should only happen for dual band
                } else { // ok
                    connect_fhss_index_band_seen |= 0x02; // 1 seen
                }
            }
        }

        if (valid_frame_received) { // valid frame received
            switch (connect_state) {
            case CONNECT_STATE_LISTEN:
                connect_state = CONNECT_STATE_SYNC;
                connect_sync_cnt = 0;
                connect_fhss_index_band_seen = (stats.received_fhss_index < 63) ? 0 : 0x03; // set as seen when mechanism not supported
                break;
            case CONNECT_STATE_SYNC:
                connect_sync_cnt++;
                uint8_t connect_sync_cnt_max = CONNECT_SYNC_CNT;
                if (!connect_occured_once) {
                    connect_sync_cnt_max = Config.connect_sync_cnt_max;
                }
                uint8_t band_seen_needed = (Config.IsDualBand) ? 0x03 : 0x01; // one band: the Tx always reports band 0
                if ((connect_sync_cnt >= connect_sync_cnt_max) && ((connect_fhss_index_band_seen & band_seen_needed) != band_seen_needed)) {
                    connect_sync_cnt = connect_sync_cnt_max - 1; // not yet
                }
                if (connect_sync_cnt >= connect_sync_cnt_max && crypto.ValidKeys()) { // can't connect if crypto doesn't allow
                    connect_state = CONNECT_STATE_CONNECTED;
                    connect_occured_once = true;
                }
                break;
            }
            connect_tmo_cnt = CONNECT_TMO_SYSTICKS;

            link_state = LINK_STATE_TRANSMIT; // switch to TX
        }

        // when in listen: we received something, but something wrong, so we need go back to RX
        if ((connect_state == CONNECT_STATE_LISTEN) && invalid_frame_received) {
            link_state = LINK_STATE_RECEIVE;
        }

        // when in listen, slowly loop through frequencies
        if (connect_state == CONNECT_STATE_LISTEN) {
            connect_listen_cnt++;
            if (connect_listen_cnt >= CONNECT_LISTEN_HOP_CNT) {
                fhss.HopToNext();
                connect_listen_cnt = 0;
                link_state = LINK_STATE_RECEIVE; // switch back to RX
            }
            if (fhss.HopToNextBind()) {
                bind.HopToNextBind(fhss.GetCurrBindSetupFrequencyBand());
                link_state = LINK_STATE_RECEIVE; // switch back to RX
            }
        }

        // we just disconnected, or are in sync but don't receive anything
        if ((connect_state >= CONNECT_STATE_SYNC) && !connect_tmo_cnt) {
            // switch to listen state
            // only do it if not in listen, since otherwise it never could reach receive wait and hence never could connect
            connect_state = CONNECT_STATE_LISTEN;
            connect_listen_cnt = 0;
            link_state = LINK_STATE_RECEIVE; // switch back to RX
        }

        // we didn't receive a valid frame
        if ((connect_state >= CONNECT_STATE_SYNC) && !valid_frame_received) {
            // reset sync counter, relevant if in sync
            // connect_sync_cnt = 0; // NO!! when in sync this means that we need to get five in a row, right!?!
            // switch to transmit state
            // only do it if receiving, else keep it in RX mode, otherwise chances to connect are dim
            // we are on the correct frequency, so no need to hop
            link_state = LINK_STATE_TRANSMIT; // TODO: is superfluous, isn't it? no path for which it would not be TX
        }

        if ((connect_state >= CONNECT_STATE_SYNC) ||
            (link_state == LINK_STATE_RECEIVE) || (link_state == LINK_STATE_TRANSMIT)) {
            sx.SetToIdle();
            sx2.SetToIdle();
        }

        if (connect_state == CONNECT_STATE_LISTEN) {
            link_task_reset();
            link_task_set(LINK_TASK_RX_SEND_RX_SETUPDATA);
        }

        if (!connected()) { tarq[0].Disconnected(); tarq[1].Disconnected(); rarq[0].Disconnected(); rarq[1].Disconnected(); }

        DECc(tick_1hz_commensurate, Config.frame_rate_hz);
        if (!tick_1hz_commensurate) {
            stats.Update1Hz();
            // Mode = auto: without a link both ends meet again at the base mode of the lower band
            if (Config.ModeAuto && !bind.IsInBind()) {
                if (connected()) {
                    mode_auto_lost_s = 0;
                } else if (!mode_auto_at_meeting() && ++mode_auto_lost_s >= MODE_AUTO_REVERT_S) {
                    mode_auto_pending = MODE_AUTO;
                    mode_auto_to_meeting();
                    doLinkRestart = true;
                }
            }
        }
        stats.Next();
        if (!connected()) stats.Clear();

        powerup.Do();
        if (powerup.Task() == POWERUPCNT_TASK_BIND) bind.StartBind();

        bind.Do();
        switch (bind.Task()) {
        case BIND_TASK_CHANGED_TO_BIND:
            mode_auto_current = MODE_AUTO; // after the bind both ends start at the meeting point
            band_auto_current = SETUP_FREQUENCY_BAND_NUM;
            bind.ConfigForBind(); // may change Config
            rxclock.SetPeriod(Config.frame_rate_ms);
            rxclock.SetShift(rxclock_shift_us()); // 1 ms: the bind runs with one receiver
            rxclock.Reset();
            fhss.SetToBind(Config.frame_rate_ms); // needs to come after fhss
            leds.SetToBind();
            connect_state = CONNECT_STATE_LISTEN;
            link_state = LINK_STATE_RECEIVE;
            break;
        case BIND_TASK_RX_STORE_PARAMS:
            // is already set in tBindBase::handle_receive()
            //Setup.Common[0].FrequencyBand = fhss.GetCurrBindSetupFrequencyBand();
            doParamsStore = true;
            break;
        }

        doPostReceive2_cnt = 5; // postpone this few loops, to allow link_state changes to be handled

        return; // link state may have changed, process immediately
    }//end of if(doPostReceive)

    //-- Update channels, Out handling, etc

    if (doPostReceive2_cnt) {
        doPostReceive2_cnt--;
        if (!doPostReceive2_cnt) doPostReceive2 = true;
    }
    if (doPostReceive2) {
        doPostReceive2 = false;

        rfpower.Set(Setup.Rx.Power); // the dynamic power below works from it
        if (!bind.IsInBind()) {
            rfpower.Dynamic(Setup.Common[0].DynPower, connected(), stats.received_rssi, stats.received_LQ_serial,
                            sx.ReceiverSensitivity_dbm());
        }
    }//end of if(doPostReceive2)

    //-- Do MAVLink & MSP & DroneCAN, or the settings session, which has the serial to itself

    rxcli.Do(connect_state == CONNECT_STATE_LISTEN && !bind.IsInBind());
    if (!rxcli.Active()) {
        mavlink.Do();
        msp.Do();
    }
    dronecan.Do();

    //-- Store parameters

    if (rxcli.StoreRequested()) { // the session's copy becomes the setup, and the receiver restarts with it
        delay_ms(50); // its "parameters stored" leaves first
        memcpy(&Setup, rxcli.Copy(), sizeof(tSetup));
        doParamsStore = true;
    }
    if (doParamsStore) {
        sx.SetToIdle();
        sx2.SetToIdle();
        leds.SetToParamStore();
        setup_store_to_EEPROM();
        GOTO_RESTARTCONTROLLER;
    }

    // Mode = auto: the link restarts with the mode the Tx chose, nothing stored
    if (doLinkRestart) {
        sx.SetToIdle();
        sx2.SetToIdle();
        if (mode_auto_pending != MODE_AUTO) {
            mode_auto_current = mode_auto_pending;
            band_auto_current = band_auto_pending;
        }
        GOTO_RESTARTCONTROLLER;
    }

}//end of main_loop
