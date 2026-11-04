//*******************************************************
// Copyright (c) MLRS project
// GPL3
// https://www.gnu.org/licenses/gpl-3.0.de.html
// OlliW @ www.olliw.eu
// TetherLRS changes: Copyright (c) TetherLRS project
//*******************************************************
// mLRS TX
//*******************************************************


#define DBG_MAIN(x)
#define DBG_MAIN_SLIM(x)
#define DEBUG_ENABLED
#define FAIL_ENABLED


// we set the priorities here to have an overview, SysTick is at 15, I2C is at 15, USB is at 0
#define UART_IRQ_PRIORITY           10 // jrpin5 bridge, this needs to be high, when lower than DIO1, the module could stop sending via the bridge
#define UARTB_IRQ_PRIORITY          11 // serial
#define UARTC_IRQ_PRIORITY          11 // com
#define UARTD_IRQ_PRIORITY          15 // serial2/ESP/BT
#define UARTF_IRQ_PRIORITY          15 // debug
#define SX_DIO_EXTI_IRQ_PRIORITY    13
#define SX2_DIO_EXTI_IRQ_PRIORITY   13 // on single spi diversity systems must be equal to DIO priority
#define SWUART_TIM_IRQ_PRIORITY      9 // debug on swuart
#define BUZZER_TIM_IRQ_PRIORITY     14

#include "../common/common_conf.h"
#include "../common/common_types.h" // includes setup_types.h
#include "../common/sx-drivers/sx12xx.h"

#if defined ESP8266 || defined ESP32

#include "../common/hal/esp-glue.h"
#include "../modules/stm32ll-lib/src/stdstm32.h"
#include "../modules/esp-lib/esp-peripherals.h"
#include "../modules/esp-lib/esp-mcu.h"
//xx #include "../modules/esp-lib/esp-adc.h"
#include "../modules/esp-lib/esp-stack.h"
#include "../modules/esp-lib/esp-trng.h"
#include "../common/hal/hal.h"
#include "../modules/esp-lib/esp-delay.h" // these are dependent on hal
#include "../modules/esp-lib/esp-eeprom.h"
#include "../modules/esp-lib/esp-spi.h"
#ifdef USE_SERIAL
#include "../modules/esp-lib/esp-uartb.h"
#endif
#if defined USE_COM && !defined DEVICE_HAS_COM_ON_USB
#include "../modules/esp-lib/esp-uartc.h"
#endif
#ifdef USE_USB
#include "../modules/esp-lib/esp-usb.h"
#endif
#ifdef USE_SERIAL2 // is set when either HAS_SERIAL2 or USE_WIRELESS_BRIDGE
#include "../modules/esp-lib/esp-uartd.h"
#endif
#ifdef USE_DEBUG
#ifdef DEVICE_HAS_DEBUG_SWUART
#include "../modules/esp-lib/esp-uart-sw.h"
#else
#include "../modules/esp-lib/esp-uartf.h"
#endif
#endif
#ifdef USE_I2C
#include "../modules/esp-lib/esp-i2c.h"
#elif defined DEVICE_HAS_SPI_DISPLAY || defined DEVICE_HAS_TFT_DISPLAY
#include "../modules/esp-lib/esp-display-spi.h"
#endif
#include "../common/hal/esp-timer.h"

#else

#include "../common/hal/glue.h"
#include "../modules/stm32ll-lib/src/stdstm32.h"
#include "../modules/stm32ll-lib/src/stdstm32-peripherals.h"
#include "../modules/stm32ll-lib/src/stdstm32-mcu.h"
#include "../modules/stm32ll-lib/src/stdstm32-adc.h"
#include "../modules/stm32ll-lib/src/stdstm32-stack.h"
#include "../common/thirdparty/stdstm32-exti.h"
#include "../common/thirdparty/stdstm32-trng.h"
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
#if defined USE_COM && !defined DEVICE_HAS_COM_ON_USB
#include "../modules/stm32ll-lib/src/stdstm32-uartc.h"
#endif
#ifdef USE_SERIAL2 // is set when either HAS_SERIAL2 or USE_WIRELESS_BRIDGE
#include "../modules/stm32ll-lib/src/stdstm32-uartd.h"
#endif
#ifdef USE_USB
#include "../modules/stm32-usb-device/stdstm32-usb-vcp.h"
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
#include "clock_tx.h"

#endif //#if defined ESP8266 || defined ESP32

#include "../common/mavlink/fmav.h"
#include "../common/setup.h"
#include "../common/common.h"
#include "../common/diversity.h"
#ifdef DEVICE_HAS_ANTENNA_SWITCH
#include "../common/antenna_switch.h"
#endif
#include "../common/arq.h"
#include "../common/bond.h"
#include "../common/crypto.h"
//#include "../common/time_stats.h" // un-comment if you want to use
//#include "../common/test.h" // un-comment if you want to compile for board test

#include "tasks.h"
#include "config_id.h"
#include "info.h"
#include "cli.h"


tRDiversity rdiversity;
tTDiversity tdiversity;
#ifdef DEVICE_HAS_ANTENNA_SWITCH
tAntennaSwitch antsw;
#endif
tReceiveArq rarq[RX_SLOT_NUM]; // one per receiver, or per lane of bonded radios
tTransmitArq tarq[2]; // one for all receivers: a new payload once every connected one acked the last; two lanes when bonded
tCrypto crypto;
tConfigId config_id;
tTxInfo txinfo;

// TetherLRS: what this image is, for the tool to read from a flash it reached through the ROM bootloader
#include "../common/tlrs_marker.h"
const char tlrs_marker[] = TLRS_MARKER("Tx");
const char* volatile tlrs_marker_ref = tlrs_marker; // read at startup: the linker must keep the text
tTxCli cli;
tTxTasks tasks;


//-------------------------------------------------------
// MAVLink & MSP
//-------------------------------------------------------

#include "mavlink_interface_tx.h"

tTxMavlink mavlink;


uint8_t mavlink_vehicle_state(void)
{
    return mavlink.VehicleState();
}


#include "msp_interface_tx.h"

tTxMsp msp;


#include "sx_serial_interface_tx.h"

tTxSxSerial sx_serial;


//-------------------------------------------------------
// Display
//-------------------------------------------------------
// doing one, draw or update, every cycle in 50 Hz mode works, but
// doing both every cycle does not work! why ???

#include "disp.h"

tTxDisp disp;


//-------------------------------------------------------
// Wifi Bridge
//-------------------------------------------------------

#include "esp-wifi-bridge.h"

tTxEspWifiBridge espbridge;

#include "hc04-bridge.h"

tTxHc04Bridge hc04bridge;


//-------------------------------------------------------
// While transmit/receive tasks
//-------------------------------------------------------

#include "../common/while.h"


class tWhileTransmit : public tWhileBase
{
  public:
    uint32_t dtmax_us(void) override { return sx.TimeOverAir_us() - 1000; }
    void handle_once(void) override;
};

tWhileTransmit whileTransmit;


void tWhileTransmit::handle_once(void)
{
    cli.Do();

#ifdef USE_DISPLAY
    uint32_t tnow_ms = millis32();

    static uint32_t main_tlast_ms = 0;
    if (tnow_ms - main_tlast_ms >= 250) { // Update Main page at 4 Hz
        main_tlast_ms = tnow_ms;
        disp.UpdateMain();
    }

    if (bind.IsInBind()) disp.SetBind();

    // postpone to next cycle if just updated and it's a short time slot
    bool allow_draw = (dtmax_us() > 2000) || (main_tlast_ms != tnow_ms);

    static uint32_t draw_tlast_ms = 0;
    if (allow_draw && (tnow_ms - draw_tlast_ms >= 50)) { // effectively slows down (drawing takes time, ca 30 ms on G4, slower on other mcu)
        draw_tlast_ms = tnow_ms;
        disp.Draw();
    }
#endif
}


//-------------------------------------------------------
// Some helper
//-------------------------------------------------------

void enter_system_bootloader(void)
{
    disp.DrawBoot();
    BootLoaderInit();
}


//-------------------------------------------------------
// Init
//-------------------------------------------------------

void init_once(void)
{
    usb_port.InitOnce();
}


void init_hw(void)
{
    // disable all interrupts, they may be enabled with restart
    __disable_irq();

    delay_init();
    systembootloader_init(); // after delay_init() since it may need delay
    timer_init();
    trng_init();

    leds_init();
    button_init();
    esp_init();
    fiveway_init();

    uartb_port.Init();
    uartc_port.Init();
    uartd_port.Init();

    buzzer.Init();
    fan.Init();
    dbg.Init();

    setup_init();

    esp_enable(Setup.Tx[Config.ConfigId].SerialPort, Setup.Tx[Config.ConfigId].SerialPort2);
    Serials.Init(Setup.Tx[Config.ConfigId].SerialPort, Config.SerialBaudrate, Setup.Tx[Config.ConfigId].SerialPort2, Config.SerialBaudrate2);

    sx.Init(); // these take time
    sx2.Init();

    __enable_irq();
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
            if (bind_signature != bind.RxSignature) irq_status = 0; // not binding frame, so ignore it
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
            if (bind_signature != bind.RxSignature) irq2_status = 0;
        } else {
            uint16_t sync_word;
            sx2.ReadBuffer(0, (uint8_t*)&sync_word, 2);
            if (sync_word != Config.FrameSyncWord) irq2_status = 0;
        }
    }
})
#endif


uint8_t link_rx1_status; // the best this cycle on each antenna, for the transmit diversity
uint8_t link_rx2_status;

uint16_t tx_tick; // counts the cycle down, in sys ticks


//-- the receivers: one reply slot each, the frame of each lands in its own buffer

typedef struct {
    uint8_t rx_status; // RX_STATUS_NONE/INVALID/VALID this cycle
    bool decrypt_ok; // and it could be read
    uint8_t antenna; // the antenna its frame was taken from
    int8_t rssi;
    int8_t snr;
    uint8_t ack; // the ack bit of its last valid frame
    uint8_t connect_state;
    uint16_t connect_tmo_cnt;
    uint8_t connect_sync_cnt;
    bool connect_occured_once;
    bool key_fresh; // its receive key was drawn since it was last seen dropping
    bool setup_known; // it answered a setup request since it connected
    char device_name[20+1];
    uint32_t firmware_version;
    tStatsLQ lq; // its valid frames
    tStatsLQ lq_serial; // its frames with fresh data: the downlink LQ it is told
    int8_t received_rssi; // what it reports of the Tx's frames: Dyn Power serves the weakest
    uint8_t received_LQ_serial;
} tRxSlot;

tRxSlot rxslot[RX_SLOT_NUM];
tRxFrame rxFrames[RX_SLOT_NUM];
uint8_t rx_selected; // the slot the parameters, the setup data and the stats are of
uint8_t link_rx_last_slot; // the slot of the last frame read, for re-arming the receiver


bool slot_connected(uint8_t slot)
{
    if (Config.Bonded) slot = 0; // the lanes share the one receiver's connection
    return (slot < Config.RxCount && rxslot[slot].connect_state == CONNECT_STATE_CONNECTED);
}


// reply frames a cycle brings: one per receiver, or one per lane
uint8_t link_frames(void)
{
    return (Config.Bonded) ? 2 : Config.RxCount;
}


//-- bonded radios: two lanes, or one frame on both while a channel is poor

bool bond_single; // the Tx's choice, in every frame's link header; the receiver follows it
uint8_t bond_cnt; // seconds the condition for a switch has held
uint8_t bond_connected_s; // seconds since the connect, the lanes are judged after a few
tStatsLQ ant_lq[2]; // valid frames on each antenna: a lane's health, also when one frame goes out on both
tBondReorder bond; // the receiver's lanes, put out in order

bool txFrameB_valid; // lane B's frame holds a payload it may send again

void bond_emit(uint8_t* const data, uint8_t len) { sx_serial.putbuf(data, len); }

// two lanes again: lane B starts afresh with lane A's seq no, so the receiving side pairs the lanes' payloads, and has
// no old payload to send again
void bond_lanes_resume(void)
{
    tarq[1].Disconnected();
    rarq[1].Disconnected();
    tarq[1].payload_seq_no = tarq[0].payload_seq_no;
    txFrameB_valid = false;
}

void bond_init(void)
{
    txFrameB_valid = false;
    bond_single = false;
    bond_cnt = 0;
    bond_connected_s = 0;
    ant_lq[0].Init(Config.frame_rate_hz);
    ant_lq[1].Init(Config.frame_rate_hz);
    bond.Init(bond_emit, FRAME_PAYLOAD_MAX);
}

// called at 1 Hz
void bond_update_1hz(void)
{
    ant_lq[0].Update1Hz();
    ant_lq[1].Update1Hz();
    if (!Config.Bonded) return;

    if (!slot_connected(0)) { // a connect starts with the lanes
        if (bond_single) bond_lanes_resume();
        bond_single = false;
        bond_cnt = 0;
        bond_connected_s = 0;
        return;
    }
    if (bond_connected_s < BOND_GRACE_S) { bond_connected_s++; return; }

    uint8_t lq = (ant_lq[0].GetLQ() < ant_lq[1].GetLQ()) ? ant_lq[0].GetLQ() : ant_lq[1].GetLQ();
    if (!bond_single) {
        if (lq < BOND_LANE_LQ_MIN) { if (++bond_cnt >= BOND_SINGLE_AFTER_S) { bond_single = true; bond_cnt = 0; } } else bond_cnt = 0;
    } else {
        if (lq > BOND_LANES_LQ_MIN) {
            if (++bond_cnt >= BOND_LANES_AFTER_S) {
                bond_single = false;
                bond_cnt = 0;
                bond_lanes_resume();
            }
        } else bond_cnt = 0;
    }
}

// for the CLI: 0 not bonded, 1 one frame on both radios, 2 lanes; and an antenna's LQ
uint8_t tx_bond_lanes(void) { return (!Config.Bonded) ? 0 : (bond_single) ? 1 : 2; }
uint8_t tx_antenna_lq(uint8_t antenna) { return ant_lq[(antenna == ANTENNA_1) ? 0 : 1].GetLQ(); }


void slot_init(uint8_t slot)
{
    tRxSlot* rs = &rxslot[slot];
    rs->rx_status = RX_STATUS_NONE;
    rs->decrypt_ok = false;
    rs->antenna = ANTENNA_1;
    rs->rssi = RSSI_INVALID;
    rs->snr = SNR_INVALID;
    rs->ack = 0;
    rs->connect_state = CONNECT_STATE_LISTEN;
    rs->connect_tmo_cnt = 0;
    rs->connect_sync_cnt = 0;
    rs->connect_occured_once = false;
    rs->key_fresh = false;
    rs->setup_known = false;
    rs->device_name[0] = '\0';
    rs->firmware_version = 0;
    rs->lq.Init(Config.frame_rate_hz);
    rs->lq_serial.Init(Config.frame_rate_hz);
    rs->received_rssi = RSSI_INVALID;
    rs->received_LQ_serial = 100; // nothing reported yet
    rarq[slot].Init();
}


// the slot a frame that could not be read belongs to: the reply window the cycle is in
uint8_t slot_from_time(void)
{
    if (Config.RxCount <= 1) return 0;
    uint16_t elapsed_ms = SYSTICK_DELAY_MS(Config.frame_rate_ms) - tx_tick; // 1 ms ticks
    if (elapsed_ms + Config.SlotPeriod_ms / 2 < Config.FirstReplyEnd_ms) return 0;
    uint16_t slot = (elapsed_ms + Config.SlotPeriod_ms / 2 - Config.FirstReplyEnd_ms) / Config.SlotPeriod_ms;
    return (slot < Config.RxCount) ? slot : Config.RxCount - 1;
}


// for the CLI: what is known of a receiver
uint8_t tx_rx_selected(void) { return rx_selected; }

bool tx_rx_slot_info(uint8_t slot, char* const name_21, uint32_t* const version, uint8_t* const lq, int8_t* const rssi)
{
    if (slot >= Config.RxCount) return false;
    tRxSlot* rs = &rxslot[slot];
    strbufstrcpy(name_21, rs->device_name, 21);
    *version = rs->firmware_version;
    *lq = rs->lq.GetLQ();
    *rssi = rs->rssi;
    return slot_connected(slot);
}


//-- Tx/Rx cmd frame handling

uint8_t link_task;
uint8_t link_task_slot; // the receiver the command is for
uint8_t transmit_frame_type;
uint16_t link_task_delay_ms;
bool link_task_sent; // the command went out at least once
bool doParamsStore;
uint8_t link_startup_cnt; // cycles since the last setup request to a receiver that is not connected
bool doLinkRestart; // Mode = auto: the link restarts with mode_auto_current
uint8_t link_restart_cnt; // the restart waits for this many cycles: the next frame acks the receiver's answer
uint8_t mode_auto_up_s, mode_auto_down_s, mode_auto_lost_s, mode_auto_mismatch, mode_auto_task_s;
uint8_t mode_auto_hold_s = 0; // seconds before the next change; not reset by the restart a change makes


void link_task_init(void)
{
    link_task = LINK_TASK_NONE;
    link_task_slot = 0;
    link_task_delay_ms = 0;
    link_task_sent = false;
    transmit_frame_type = TRANSMIT_FRAME_TYPE_NORMAL;
    link_startup_cnt = 0;

    doParamsStore = false;
    doLinkRestart = false;
    link_restart_cnt = 0;
    mode_auto_up_s = mode_auto_down_s = mode_auto_lost_s = mode_auto_mismatch = mode_auto_task_s = 0;
    mode_auto_pending = MODE_AUTO;
}


bool link_task_free(void)
{
    if (link_task != LINK_TASK_NONE) return false; // a task is running
    return true;
}


bool link_task_set(uint8_t task, uint8_t slot = 0xFF) // 0xFF: the selected receiver
{
    if (link_task != LINK_TASK_NONE) return false; // a task is running
    if (slot >= RX_SLOT_NUM) slot = rx_selected;

    link_task = task;
    link_task_slot = slot;
    link_task_sent = false;
    transmit_frame_type = TRANSMIT_FRAME_TYPE_CMD;

    link_task_delay_ms = 0;

    // set a timeout if relevant, or do other things as needed
    switch (link_task) {
    case LINK_TASK_TX_GET_RX_SETUPDATA_STARTUP:
        if (!rxslot[slot].key_fresh) { // a fresh key once per connect, the repeats of the command carry the same one
            crypto.SetReceiveKey(slot, setup_new_random());
            rxslot[slot].key_fresh = true;
        }
        // fall through
    case LINK_TASK_TX_GET_RX_SETUPDATA:
    case LINK_TASK_TX_GET_RX_SETUPDATA_WRELOAD:
        if (slot == rx_selected) SetupMetaData.rx_available = false;
        break;
    case LINK_TASK_TX_STORE_RX_PARAMS: // store rx parameters
        link_task_delay_ms = 500; // we set a delay, the actual store is triggered when it expires
        break;
    }

    return true;
}


void link_task_reset(void)
{
    link_task = LINK_TASK_NONE;
    link_task_delay_ms = 0;
    link_task_sent = false;
    transmit_frame_type = TRANSMIT_FRAME_TYPE_NORMAL;
}


void link_task_tick_ms(void)
{
    // if a delay has been set, count it down
    if (link_task_delay_ms) {
        link_task_delay_ms--;
        if (!link_task_delay_ms) {
            switch (link_task) {
            case LINK_TASK_TX_STORE_RX_PARAMS: doParamsStore = true; break;
            }
            link_task_reset();
        }
    }
}


// called each cycle with no task running: which receiver gets a setup request. A connected one that has not answered
// since it connected comes first, and the request stays until it answers. One that is not connected gets a STARTUP
// with its key: every cycle while no receiver at all is connected, else one cycle in eight so the data keeps flowing.
// the CLI's and the tool's tasks that found another one running wait here, in order, until it is done
uint8_t link_task_queue[3];
uint8_t link_task_queue_len;

void link_task_queue_add(uint8_t task)
{
    for (uint8_t i = 0; i < link_task_queue_len; i++) if (link_task_queue[i] == task) return; // already waiting
    if (link_task_queue_len < sizeof(link_task_queue)) link_task_queue[link_task_queue_len++] = task;
}

void link_task_from_queue(void)
{
    if (!link_task_queue_len || !link_task_free()) return;
    uint8_t task = link_task_queue[0];
    link_task_queue_len--;
    for (uint8_t i = 0; i < link_task_queue_len; i++) link_task_queue[i] = link_task_queue[i + 1];
    if (connected()) {
        link_task_set(task);
    } else if (task == LINK_TASK_TX_STORE_RX_PARAMS) {
        doParamsStore = true;
    }
}


void link_task_pick(void)
{
    if (link_task != LINK_TASK_NONE || bind.IsInBind()) return;
    if (Setup.Tx[Config.ConfigId].Downlink == TX_DOWNLINK_OFF) return; // a silent receiver never answers

    for (uint8_t s = 0; s < Config.RxCount; s++) {
        if (slot_connected(s) && !rxslot[s].setup_known) { link_task_set(LINK_TASK_TX_GET_RX_SETUPDATA_STARTUP, s); return; }
    }

    bool any_connected = connected();
    link_startup_cnt++;
    if (any_connected && link_startup_cnt < 8) return;
    for (uint8_t n = 0; n < Config.RxCount; n++) {
        uint8_t s = (link_task_slot + 1 + n) % Config.RxCount; // in turn, from the one after the last asked
        if (!slot_connected(s)) {
            link_startup_cnt = 0;
            link_task_set(LINK_TASK_TX_GET_RX_SETUPDATA_STARTUP, s);
            return;
        }
    }
}


void process_received_rxcmdframe(tRxFrame* const frame, uint8_t slot)
{
tCmdFrameHeader* head = (tCmdFrameHeader*)FRAME_DATA(frame);

    switch (head->cmd) {
    case FRAME_CMD_RX_SETUPDATA:
        // received rx setup data
        rxslot[slot].setup_known = true;
        if (slot == rx_selected) {
            unpack_rxcmdframe_rxsetupdata(frame);
            strbufstrcpy(rxslot[slot].device_name, SetupMetaData.rx_device_name, 21);
            rxslot[slot].firmware_version = SetupMetaData.rx_firmware_version;
        } else {
            unpack_rxcmdframe_rxsetupdata_brief(frame, rxslot[slot].device_name, &rxslot[slot].firmware_version);
        }
        if (link_task == LINK_TASK_TX_SET_LINK && slot == rx_selected) {
            // the receiver restarts with the mode it names; an answer to an older request names none
            if (SetupMetaData.rx_link_pending == (uint8_t)(mode_auto_pending | (band_auto_pending << 4))) {
                // one more frame, SET_LINK again, which acks this answer: the receiver restarts on that ack, while a
                // data frame would tell it the change was given up
                if (!link_restart_cnt) link_restart_cnt = 2;
            } else if (++mode_auto_mismatch >= MODE_AUTO_ANSWER_MISMATCH_MAX) { // the receiver will not take it
                mode_auto_pending = MODE_AUTO;
                link_task_reset();
            }
            break;
        }
        if (link_task_slot == slot) link_task_reset();
        break;
    }
}


void pack_txcmdframe(tTxFrame* const frame, tFrameStats* const frame_stats)
{
    link_task_sent = true;

    switch (link_task) {
    case LINK_TASK_TX_GET_RX_SETUPDATA:
        pack_txcmdframe_cmd(frame, frame_stats, FRAME_CMD_GET_RX_SETUPDATA, link_task_slot);
        break;
    case LINK_TASK_TX_GET_RX_SETUPDATA_WRELOAD:
        pack_txcmdframe_cmd(frame, frame_stats, FRAME_CMD_GET_RX_SETUPDATA_WRELOAD, link_task_slot);
        break;
    case LINK_TASK_TX_GET_RX_SETUPDATA_STARTUP:
        pack_txcmdframe_cmd(frame, frame_stats, FRAME_CMD_GET_RX_SETUPDATA_STARTUP, link_task_slot);
        break;
    case LINK_TASK_TX_SET_RX_PARAMS:
        pack_txcmdframe_setrxparams(frame, frame_stats);
        break;
    case LINK_TASK_TX_STORE_RX_PARAMS:
        pack_txcmdframe_cmd(frame, frame_stats, FRAME_CMD_STORE_RX_PARAMS, link_task_slot);
        transmit_frame_type = TRANSMIT_FRAME_TYPE_NORMAL;
        break;
    case LINK_TASK_TX_SET_LINK:
        pack_txcmdframe_cmd(frame, frame_stats, FRAME_CMD_SET_LINK, link_task_slot, mode_auto_pending, band_auto_pending);
        break;
    }
}


//-- Mode = auto: the Tx steps the mode up while the link has margin to spare, down when it gets poor

// a power in mW as whole dBm, to the nearest; INT8_MIN when there is none (0 mW, or the end of a list)
int8_t mw_to_dbm(int16_t mw)
{
    if (mw <= 0 || mw == INT16_MAX) return INT8_MIN;
    return db_from_x100((uint32_t)mw * 100);
}


void mode_auto_set(uint8_t band, uint8_t mode)
{
    band_auto_pending = band;
    mode_auto_pending = mode;
    mode_auto_mismatch = 0;
    mode_auto_task_s = 0;
    mode_auto_hold_s = MODE_AUTO_HOLD_S;
    mode_auto_up_s = mode_auto_down_s = 0;
    link_task_set(LINK_TASK_TX_SET_LINK, 0);
}


// a step's link budget: the power the region allows there, the mode's sensitivity, the band's extra path loss
int16_t mode_auto_budget(uint8_t band, uint8_t mode)
{
    uint8_t idx = region_power_idx_max(Config.Region, band, mode, Setup.Common[Config.ConfigId].CustomPowerMaxDbm);
    if (Setup.Tx[Config.ConfigId].Power < idx) idx = Setup.Tx[Config.ConfigId].Power;
    return rfpower_list[idx].dbm - mode_sensitivity_dbm(mode) - band_offset_db(band, band_auto_meeting());
}

// the steps of Mode = auto, fastest first, each next one slower and with a better budget: the modes both ends can
// run on RF Band and on RF Band 2; at equal rate and budget RF Band comes first
uint8_t mode_auto_ladder(tAutoStep* const steps)
{
tAutoStep all[2 * MODE_AUTO];
uint8_t n = 0;

    for (uint8_t k = 0; k < 2; k++) {
        uint8_t band = (k == 0) ? Config.Band1 : Config.Band2;
        if (band >= SETUP_FREQUENCY_BAND_NUM) continue;
        uint16_t modes = mode_mask_for_band(band, Config.Region) & ~(1 << MODE_AUTO);
        if (k == 0 && SetupMetaData.rx_Mode_allowed_mask) modes &= SetupMetaData.rx_Mode_allowed_mask;
        if (k == 1) modes &= SetupMetaData.rx_Mode_allowed_mask_band2; // none, while the receiver has not told
        for (uint8_t m = 0; m < MODE_AUTO; m++) {
            if (!(modes & (1 << m))) continue;
            tAutoStep st = { band, m, mode_rate_hz(m), mode_auto_budget(band, m) };
            auto_ladder_add(all, &n, st);
        }
    }
    return auto_ladder_front(all, n, steps);
}

// called at 1 Hz
void mode_auto_update_1hz(void)
{
    if (!Config.ModeAuto || bind.IsInBind()) return;

    if (!connected()) { // without a link both ends meet again at the base mode of the lower band
        mode_auto_up_s = mode_auto_down_s = 0;
        if (!mode_auto_at_meeting() && ++mode_auto_lost_s >= MODE_AUTO_REVERT_S) {
            mode_auto_pending = MODE_AUTO;
            mode_auto_to_meeting();
            doLinkRestart = true;
        }
        return;
    }
    mode_auto_lost_s = 0;

    if (link_task == LINK_TASK_TX_SET_LINK && !link_restart_cnt && ++mode_auto_task_s >= MODE_AUTO_REVERT_S) { // never answered: the data must flow
        mode_auto_pending = MODE_AUTO;
        link_task_reset();
    }
    if (mode_auto_hold_s) { mode_auto_hold_s--; return; }
    if (link_task != LINK_TASK_NONE || !SetupMetaData.rx_available) return;

    // the next step up: the closest faster one; down: the closest slower one with a better budget, or the same rate
    tAutoStep steps[2 * MODE_AUTO];
    uint8_t num = mode_auto_ladder(steps);
    uint8_t cur_rate = mode_rate_hz(Config.Mode);
    int16_t cur_budget = mode_auto_budget(Config.FrequencyBand, Config.Mode);
    int8_t up, down;
    auto_ladder_neighbours(steps, num, cur_rate, cur_budget, &up, &down);

    // what each end hears, with the power Dyn Power is holding back added: it would come back at once
    uint8_t tx_power_idx = Setup.Tx[Config.ConfigId].Power;
    if (tx_power_idx > Config.PowerIdxMax) tx_power_idx = Config.PowerIdxMax;
    int16_t headroom_tx = rfpower_list[tx_power_idx].dbm - rfpower.Current_dbm();
    // the receiver's levels are those of its own list, from its setup data; the region caps it as it caps the Tx
    int16_t headroom_rx = 0;
    uint8_t code = stats.received_power_code;
    if (code >= 1 && code <= 8 && Setup.Rx.Power < 8) {
        int8_t cur = mw_to_dbm(SetupMetaData.rx_power_mw[code - 1]);
        int8_t ceiling = mw_to_dbm(SetupMetaData.rx_power_mw[Setup.Rx.Power]);
        if (ceiling > rfpower_list[Config.PowerIdxMax].dbm) ceiling = rfpower_list[Config.PowerIdxMax].dbm;
        if (cur != INT8_MIN && ceiling != INT8_MIN && ceiling > cur) headroom_rx = ceiling - cur;
    }
    if (stats.received_rssi == RSSI_INVALID || stats.GetLastRssi() == RSSI_INVALID) { // nothing to judge by
        mode_auto_up_s = mode_auto_down_s = 0;
        return;
    }
    int16_t rssi_now = (stats.received_rssi < stats.GetLastRssi()) ? stats.received_rssi : stats.GetLastRssi();
    int16_t rssi_at_rx = stats.received_rssi + headroom_tx;
    int16_t rssi_at_tx = stats.GetLastRssi() + headroom_rx;
    int16_t rssi_min = (rssi_at_rx < rssi_at_tx) ? rssi_at_rx : rssi_at_tx;
    uint8_t lq_tx = stats.valid_frames_received.GetLQ();
    uint8_t lq = (lq_tx < stats.received_LQ_link) ? lq_tx : stats.received_LQ_link;
    int16_t sens = mode_sensitivity_dbm(Config.Mode);

    // the margin a step would have: today's, less what its budget gives up
    if (up >= 0 && rssi_min - sens + steps[up].budget - cur_budget >= MODE_AUTO_UP_MARGIN_DB && lq >= MODE_AUTO_UP_LQ) {
        if (++mode_auto_up_s >= MODE_AUTO_UP_S) { mode_auto_set(steps[up].band, steps[up].mode); return; }
    } else {
        mode_auto_up_s = 0;
    }
    if (down >= 0 && (lq < MODE_AUTO_DOWN_LQ || rssi_now - sens < MODE_AUTO_DOWN_MARGIN_DB)) {
        if (++mode_auto_down_s >= MODE_AUTO_DOWN_S) { mode_auto_set(steps[down].band, steps[down].mode); return; }
    } else {
        mode_auto_down_s = 0;
    }
}


// for the CLI: the mode in use
uint8_t tx_mode_current(void) { return Config.Mode; }



// the receiver whose parameters, setup data and stats the Tx shows and sets
void tx_select_rx(uint8_t slot)
{
    if (slot >= Config.RxCount || slot == rx_selected) return;
    rx_selected = slot;
    SetupMetaData.rx_available = false;
    rxslot[slot].setup_known = false; // its setup is fetched for the parameters
    link_task_reset();
    stats.Clear();
}


//-- normal Tx, Rx frames handling
// transmit
//   -> do_transmit_prepare(antenna, fhss1_curr_i, fhss2_curr_i)
//       -> prepare_transmit_frame(antenna, fhss1_curr_i, fhss2_curr_i)
//           -> pack_txframe(...) or pack_txcmdframe(...)
//   -> do_transmit_send(antenna)
// receive
//   isr:        -> irq_status
//   isr loop:   -> do_receive(antenna)
//               -> link_rx1_status
//   post loop:  -> handle_receive(antenna) or handle_receive_none()
//                  if valid -> process_received_frame(do_payload, frame)
//                               -> decrypt_rxframe(frame)
//                                  process_received_rxcmdframe(frame)

void prepare_transmit_frame(uint8_t antenna, uint8_t fhss1_curr_i, uint8_t fhss2_curr_i)
{
uint8_t payload[FRAME_PAYLOAD_MAX];
uint8_t payload_len = 0;
uint8_t payload_b[FRAME_PAYLOAD_MAX];
uint8_t payload_b_len = 0;

    // bonded radios: a frame per lane, unless a channel is poor and one frame goes out on both
    bool lanes = (Config.Bonded && !bond_single);

    // a silent receiver never acks: its data goes one way, every frame a new payload
    bool one_way = (Setup.Tx[Config.ConfigId].Downlink == TX_DOWNLINK_OFF);
    if (one_way) { tarq[0].SetRetryCnt(0); tarq[1].SetRetryCnt(0); }

    // new data only when every lane is free, so the receiver can keep the lanes in order
    bool get_fresh_payload;
    if (lanes) {
        get_fresh_payload = tarq[0].WouldAdvance() && tarq[1].WouldAdvance();
        tarq[0].Settle(get_fresh_payload);
        tarq[1].Settle(get_fresh_payload);
    } else {
        get_fresh_payload = tarq[0].GetFreshPayload();
    }

    if (get_fresh_payload) {
        if (transmit_frame_type == TRANSMIT_FRAME_TYPE_NORMAL) {
            // read data from serial port; not into the last frames before a link restart, they would be lost
            if (link_restart_cnt) {
            } else if (connected() || one_way) {
                uint8_t len_max = FRAME_PAYLOAD_LEN(Config.FrameLenTx) - FRAME_HDR_LEN - crypto.NonceLen();
                while (payload_len < len_max && sx_serial.available()) payload[payload_len++] = sx_serial.getc();
                if (lanes) while (payload_b_len < len_max && sx_serial.available()) payload_b[payload_b_len++] = sx_serial.getc();

                stats.bytes_transmitted.Add(payload_len + payload_b_len);
                stats.serial_data_transmitted.Inc();
            } else {
                sx_serial.flush();
            }
        }
    }

    stats.last_transmit_antenna = antenna;

    // whose stats the status fields carry and whom a command is for: the command's receiver, else the connected ones in
    // turn; a payload sent again keeps its own, and the changes of a multipoint link go to every receiver
    static uint8_t stats_slot = 0;
    static bool txFrame_valid = false;
    uint8_t hdr_all = 0;
    if (!get_fresh_payload && Config.LinkHeader && txFrame_valid) {
        stats_slot = (txFrame.payload[0] >> LINK_HDR_TX_SLOT_SHIFT) & LINK_HDR_SLOT_MASK;
        hdr_all = txFrame.payload[0] & LINK_HDR_TX_ALL;
    } else if (transmit_frame_type != TRANSMIT_FRAME_TYPE_NORMAL) {
        stats_slot = link_task_slot;
        if (Config.RxCount > 1 && (link_task == LINK_TASK_TX_SET_RX_PARAMS || link_task == LINK_TASK_TX_STORE_RX_PARAMS)) {
            hdr_all = LINK_HDR_TX_ALL;
        }
    } else if (Config.RxCount > 1) {
        for (uint8_t n = 0; n < Config.RxCount; n++) {
            uint8_t s = (stats_slot + 1 + n) % Config.RxCount;
            if (slot_connected(s)) { stats_slot = s; break; }
        }
    } else {
        stats_slot = 0;
    }

    tFrameStats frame_stats;
    frame_stats.seq_no = tarq[0].SeqNo();
    frame_stats.ack = rarq[0].AckSeqNo();
    frame_stats.antenna = rxslot[stats_slot].antenna;
    frame_stats.transmit_antenna = antenna;
    frame_stats.rssi = rxslot[stats_slot].rssi;

    frame_stats.link_header = 0;
    if (Config.LinkHeader) {
        for (uint8_t s = 0; s < Config.RxCount; s++) {
            if (rarq[s].AckSeqNo() & 0x01) frame_stats.link_header |= (1 << s);
        }
        frame_stats.link_header |= (stats_slot & LINK_HDR_SLOT_MASK) << LINK_HDR_TX_SLOT_SHIFT;
        frame_stats.link_header |= hdr_all;
        if (Config.Bonded && bond_single) frame_stats.link_header |= LINK_HDR_BOND_SINGLE;
    }

    // Note: the receiver wants to see both bands, also single band receivers.
    // It is then important however that fhss1_curr_i and fhss2_curr_i are identical, as otherwise
    // the receiver would jump to wrong frequencies
    // this must be ensured by setup
    uint8_t fhss_band = (Config.IsDualBand) ? fhss_band_next() : 0; // dual band: randomly toggles between 0 and 1; one band: the 2nd sequencer follows the 1st
    frame_stats.tx_fhss_index_band = fhss_band;
    frame_stats.tx_fhss_index = ((fhss_band & 0x01) == 0) ? fhss1_curr_i : fhss2_curr_i;

if (!Config.TransmitBoth && (fhss1_curr_i != fhss2_curr_i)) while(1){} // must not happen, catch it

    frame_stats.LQ_serial = stats.GetLQ_serial();
    if (Config.RxCount > 1) { // each receiver its own downlink LQ, as with the other fields of its slot
        uint8_t lq = rxslot[stats_slot].lq_serial.GetLQ();
        frame_stats.LQ_serial = (!slot_connected(stats_slot)) ? 0 : (lq) ? lq : 1;
    }
    frame_stats.rx_silent = (one_way) ? 1 : 0;

    if (get_fresh_payload) {
        if (transmit_frame_type == TRANSMIT_FRAME_TYPE_NORMAL) {
            pack_txframe(&txFrame, &frame_stats, payload, payload_len);
        } else {
            pack_txcmdframe(&txFrame, &frame_stats);
        }
        txFrame_valid = true;

        stats.cntFrameTransmitted();
    } else {
        // txFrame still holds the previous payload, a receiver asked for it again
        if (!txFrame_valid) while(1){} // should not happen

        update_txframe_stats(&txFrame, &frame_stats);

        stats.cntFrameSkipped();
    }

    if (lanes) { // lane B: the same stats and hop, its own seq no and ack; empty beside a command frame
        tFrameStats frame_stats_b = frame_stats;
        frame_stats_b.seq_no = tarq[1].SeqNo();
        frame_stats_b.ack = rarq[1].AckSeqNo();
        frame_stats_b.transmit_antenna = ANTENNA_2;
        if (get_fresh_payload || !txFrameB_valid) { // the lanes may have just come back: nothing to send again yet
            pack_txframe(&txFrameB, &frame_stats_b, payload_b, (transmit_frame_type == TRANSMIT_FRAME_TYPE_NORMAL) ? payload_b_len : 0);
            txFrameB_valid = true;
        } else {
            update_txframe_stats(&txFrameB, &frame_stats_b);
        }
    }

    if (!one_way) {
        tarq[0].SetRetryCntAuto(stats.GetFrameCnt(), Config.Mode);
        tarq[1].SetRetryCntAuto(stats.GetFrameCnt(), Config.Mode);
    }
}


// what the cycle counted: bonded lanes are one receiver's, and the stats are the selected one's
bool lq_frame_counted, lq_valid_counted;
uint8_t lq_serial_counted; // a bit per receiver


void process_received_frame(uint8_t slot, bool do_payload, tRxFrame* const frame)
{
    bool accept_payload = rarq[slot].AcceptPayload();

    rxslot[slot].ack = frame->status.ack;
    tRxSlot* rs = &rxslot[(Config.Bonded) ? 0 : slot];
    rs->received_rssi = rssi_i8_from_u7(frame->status.rssi_u7);
    rs->received_LQ_serial = frame->status.LQ_serial;
    if (slot == rx_selected) {
        stats.received_antenna = frame->status.antenna;
        stats.received_transmit_antenna = frame->status.transmit_antenna;
        stats.received_rssi = rssi_i8_from_u7(frame->status.rssi_u7);
        stats.received_LQ_link = frame->status.LQ_link;
        stats.received_power_code = (frame->status.power_hi << 2) | frame->status.power_lo;
        stats.received_LQ_serial = frame->status.LQ_serial;
    }

    if (!do_payload) {
        return;
    }

    if (!accept_payload) return; // frame has no fresh payload

    // handle cmd frame
    if (frame->status.frame_type == FRAME_TYPE_CMD) {
        process_received_rxcmdframe(frame, slot);
        return;
    }

    // output data on serial, only of a connected receiver; bonded lanes go out in order
    if (!slot_connected(slot)) return;
    if (Config.Bonded) {
        bond.Accepted(slot, frame->status.seq_no, FRAME_DATA(frame), FRAME_DATA_LEN(frame));
    } else {
        sx_serial.putbuf_from(slot, FRAME_DATA(frame), FRAME_DATA_LEN(frame));
    }

    stats.bytes_received.Add(FRAME_DATA_LEN(frame));
    uint8_t rx = (Config.Bonded) ? 0 : slot;
    if (!(lq_serial_counted & (1 << rx))) {
        lq_serial_counted |= (1 << rx);
        rxslot[rx].lq_serial.Inc();
        if (rx == rx_selected) stats.serial_data_received.Inc();
    }
}


//-- transmit/receive handling api

void do_transmit_prepare(uint8_t antenna, uint8_t fhss1_curr_i, uint8_t fhss2_curr_i) // we prepare a TX frame to be send to receiver
{
    if (bind.IsInBind()) {
        bind.do_transmit(antenna);
        return;
    }

    prepare_transmit_frame(antenna, fhss1_curr_i, fhss2_curr_i);
}


void do_transmit_send(uint8_t antenna) // we send a TX frame to receiver
{
    if (bind.IsInBind()) {
       sxSendFrame(antenna, &txBindFrame, FRAME_TX_RX_LEN, SEND_FRAME_TMO_MS);
       return;
    }

    if (Config.Bonded && !bond_single) {
        sxSendFrames(&txFrame, &txFrameB, Config.FrameLenTx, SEND_FRAME_TMO_MS); // a lane out of each radio
    } else {
        sxSendFrame(antenna, &txFrame, Config.FrameLenTx, SEND_FRAME_TMO_MS); // 10 ms tmo
    }
}


// called in isr loop
uint8_t do_receive(uint8_t antenna) // we receive a RX frame from a receiver
{
uint8_t res;
uint8_t rx_status = RX_STATUS_INVALID; // this also signals that a frame was received

    if (bind.IsInBind()) { // one receiver binds, in slot 0; both radios read into the one bind frame: a valid one stays
        tRxSlot* rs = &rxslot[0];
        link_rx_last_slot = 0;
        if (rs->rx_status == RX_STATUS_VALID) return RX_STATUS_NONE;
        uint8_t st = bind.do_receive(antenna, false);
        if (st > rs->rx_status) {
            rs->rx_status = st;
            rs->antenna = antenna;
            rs->rssi = (antenna == ANTENNA_1) ? stats.last_rssi1 : stats.last_rssi2;
        }
        return st;
    }

    // we don't need to read sx.GetRxBufferStatus(), but hey
    // we could save 2 byte's time by not reading sync_word again, but hey
    sxReadFrame(antenna, &rxFrame, &rxFrame2, Config.FrameLenRx);
    tRxFrame* frame = (antenna == ANTENNA_1) ? &rxFrame : &rxFrame2;
    res = check_rxframe(frame);

    if (res != CHECK_OK) {
        DBG_MAIN(dbg.puts("fail ");dbg.putc('\n');)
//dbg.puts("fail a");dbg.putc(antenna+'0');dbg.puts(" ");dbg.puts(u8toHEX_s(res));dbg.putc('\n');
    }

    if (res == CHECK_ERROR_SYNCWORD) return RX_STATUS_INVALID; // must not happen !

    // can be CHECK_ERROR_HEADER, CHECK_ERROR_CRC, CHECK_OK

    if (res == CHECK_OK) {
        rx_status = RX_STATUS_VALID;
    }

    // we want to have the rssi,snr stats even if it's a bad packet
    sxGetPacketStatus(antenna, &stats);

    if (rx_status == RX_STATUS_VALID) ant_lq[(antenna == ANTENNA_1) ? 0 : 1].Inc();

    // the slot it answers in: its link header, or the reply window when the frame could not be read; bonded
    // radios: the antenna is the lane, unless the frame says one went out on both (then the better copy)
    uint8_t slot = 0;
    if (Config.Bonded) {
        bool single = (rx_status == RX_STATUS_VALID) ? (frame->payload[0] & LINK_HDR_BOND_SINGLE) : bond_single;
        slot = (single) ? 0 : ((antenna == ANTENNA_1) ? BOND_LANE_A : BOND_LANE_B);
    } else if (Config.LinkHeader) {
        slot = (rx_status == RX_STATUS_VALID) ? (frame->payload[0] & LINK_HDR_RX_SLOT_MASK) : slot_from_time();
        if (slot >= Config.RxCount) slot = Config.RxCount - 1;
    }
    link_rx_last_slot = slot;

    // the better of the antennas' copies of it: a valid one, else the stronger
    int8_t rssi = (antenna == ANTENNA_1) ? stats.last_rssi1 : stats.last_rssi2;
    tRxSlot* rs = &rxslot[slot];
    if (rx_status > rs->rx_status || (rx_status == rs->rx_status && rssi > rs->rssi)) {
        memcpy(&rxFrames[slot], frame, Config.FrameLenRx);
        rs->rx_status = rx_status;
        rs->antenna = antenna;
        rs->rssi = rssi;
        rs->snr = (antenna == ANTENNA_1) ? stats.last_snr1 : stats.last_snr2;
    }

    return rx_status; // INVALID, VALID
}


// called in doPreTransmit loop
bool handle_receive(uint8_t slot) // called if INVALID, VALID
{
uint8_t rx_status = rxslot[slot].rx_status;
uint8_t antenna = rxslot[slot].antenna;
tRxFrame* frame = &rxFrames[slot];

    if (bind.IsInBind()) {
        bind.handle_receive(antenna, rx_status);
        return (rx_status == RX_STATUS_VALID);
    }

    // can be INVALID, VALID (NONE is handled elsewhere)

    if (rx_status < RX_STATUS_INVALID) { // = RX_STATUS_NONE, must not happen
        FAIL_WSTATE(BLINK_4, "rx_status failure", 0,0, link_rx1_status, link_rx2_status);
    }

    // check decrypt_rxframe()
    bool decrypt_ok = false;
    if (rx_status > RX_STATUS_INVALID) {
        // don't do for INVALID, payload is nonsense; bonded lanes are one receiver, whose key is slot 0's
        decrypt_ok = decrypt_rxframe(frame, (Config.Bonded) ? 0 : slot);
    }

    // handle receive ARQ, must come before process_received_frame()
    if (rx_status == RX_STATUS_VALID && decrypt_ok) { // we have valid information on seq_no
        rarq[slot].Received(frame->status.seq_no);
    } else {
        rarq[slot].FrameMissed();
    }
    // check this before received data may be passed to parsers
    if (rarq[slot].FrameLost()) {
        mavlink.FrameLost((Config.Bonded) ? 0 : slot); // the lanes are one receiver's stream
        msp.FrameLost();
    }

    bool selected = (((Config.Bonded) ? 0 : slot) == rx_selected); // bonded lanes are one receiver's

    if (rx_status > RX_STATUS_INVALID && decrypt_ok) { // = RX_STATUS_VALID

        bool do_payload = true; // has no rc data, so do_payload is always

        process_received_frame(slot, do_payload, frame);

        rxslot[slot].lq.Inc();
        // counts both rx and cmd frames, but cmd frames are rare, so no worry
        if (selected && !lq_valid_counted) { stats.doValidFrameReceived(); lq_valid_counted = true; }

    } else { // RX_STATUS_INVALID or !decrypt_ok
    }

    if (slot == rx_selected) {
        // we set it for all received frames
        stats.last_antenna = antenna;
        // and its signal: the stats keep the last frame heard, which with several receivers can be another one's
        if (antenna == ANTENNA_1) { stats.last_rssi1 = rxslot[slot].rssi; stats.last_snr1 = rxslot[slot].snr; }
        else { stats.last_rssi2 = rxslot[slot].rssi; stats.last_snr2 = rxslot[slot].snr; }
    }
    if (selected && !lq_frame_counted) { // we count all received frames
        stats.doFrameReceived();
        lq_frame_counted = true;
    }

    return decrypt_ok;
}


// called in doPreTransmit loop
void handle_receive_none(uint8_t slot) // called if RX_STATUS_NONE
{
    rarq[slot].FrameMissed();
}


// called in doPreTransmit loop, after every slot: the transmit ARQ hears from all connected receivers, and moves
// to a new payload only once each of them acked the last one; bonded lanes each hear their own
void tarq_update(void)
{
    if (Config.Bonded) {
        for (uint8_t lane = 0; lane < 2; lane++) {
            if (!slot_connected(0) || (bond_single && lane == BOND_LANE_B)) { tarq[lane].Disconnected(); continue; }
            if (rxslot[lane].rx_status == RX_STATUS_VALID && rxslot[lane].decrypt_ok) {
                tarq[lane].AckReceived(rxslot[lane].ack);
            } else {
                tarq[lane].FrameMissed();
            }
        }
        return;
    }

    bool any_connected = false, any_heard = false, all_acked = true;
    uint8_t want = tarq[0].SeqNo() & 0x01;

    for (uint8_t s = 0; s < Config.RxCount; s++) {
        if (!slot_connected(s)) continue;
        any_connected = true;
        if (rxslot[s].rx_status == RX_STATUS_VALID && rxslot[s].decrypt_ok) {
            any_heard = true;
            if ((rxslot[s].ack & 0x01) != want) all_acked = false;
        } else {
            all_acked = false;
        }
    }

    if (!any_connected) { tarq[0].Disconnected(); return; }
    if (!any_heard) { tarq[0].FrameMissed(); return; }
    tarq[0].AckReceived((all_acked) ? want : (want ^ 0x01));
}


//##############################################################################################################
//*******************************************************
// MAIN routine
//*******************************************************

// the tx transmit needs to be jitter free, much less than the receiver's 1 ms gap
// the strategy is
// - block hard work one a first tx_tick, to ensure the next tx_tick comes in time
// - start a timer of 750 us, and do the preparation work for the next transmit frame
// - transmit
bool isInTimeGuard;
bool doPreTransmit;
uint16_t pretransmit_tstamp_us;

uint16_t tick_1hz;
uint16_t tick_1hz_commensurate;

uint16_t link_state;
uint8_t link_tx_status;
bool connect_occured_once; // any receiver, ever


bool connected(void) // to at least one receiver
{
    for (uint8_t s = 0; s < Config.RxCount; s++) if (slot_connected(s)) return true;
    return false;
}

bool connected_and_rx_setup_available(void)
{
    return (slot_connected(rx_selected) && SetupMetaData.rx_available);
}

// a silent receiver (Downlink off) is never heard: the data goes one way, with the receiver's serial setup the Tx
// read and stored last. With several receivers the data flows to whoever is there, the setup is for the parameters.
bool serial_link_up(void)
{
    if (Setup.Tx[Config.ConfigId].Downlink == TX_DOWNLINK_OFF) return true;
    if (Config.RxCount > 1) return connected();
    return connected_and_rx_setup_available();
}


void main_loop(void)
{
#ifdef BOARD_TEST_H
    main_test();
#endif
INITCONTROLLER_ONCE
    stack_check_init();
    init_once();
RESTARTCONTROLLER
    init_hw();
    DBG_MAIN(dbg.puts("\n\n\nHello\n\n");)

    // startup sign of life
    leds.Init();
    (void)tlrs_marker_ref;
    txinfo.Init();

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

    tx_tick = 0;
    isInTimeGuard = false;
    doPreTransmit = false;
    pretransmit_tstamp_us = 0;
    link_state = LINK_STATE_IDLE;
    connect_occured_once = false;
    link_rx1_status = link_rx2_status = RX_STATUS_NONE;
    link_rx_last_slot = 0;
    link_tx_status = TX_STATUS_NONE;
    rx_selected = 0;
    for (uint8_t s = 0; s < RX_SLOT_NUM; s++) slot_init(s);
    link_task_init();

    stats.Init(Config.LQAveragingPeriod, Config.frame_rate_hz, Config.frame_rate_ms);
    rdiversity.Init();
    tdiversity.Init(Config.frame_rate_ms);
#ifdef DEVICE_HAS_ANTENNA_SWITCH
    antsw.Init();
#endif
    tarq[0].Init();
    tarq[1].Init();
    bond_init();
    crypto.Init(tCrypto::TX,
        Setup.Common[Config.ConfigId].BindPhrase,
        Config.Uid, Setup.tx_random[Config.ConfigId],
        Setup.Common[Config.ConfigId].Privacy, Setup.LinkKey);
    crypto.SetTransmitKey(Config.SessionRandom);
    link_task_set(LINK_TASK_TX_GET_RX_SETUPDATA_STARTUP, 0); // we start with wanting to get rx setup data; after crypto.Init, it draws a key

    mavlink.Init(); // serial ports selected by SerialPort, SerialPort2
    msp.Init(); // serial port selected by SerialPort
    sx_serial.Init(); // serial port selected by SerialPort
    cli.Init();
    espbridge.Init();
    hc04bridge.Init();
    fan.SetPower(SX_OR_SX2(sx.RfPower_dbm(),sx2.RfPower_dbm()));
    whileTransmit.Init();
    disp.Init();
    tasks.Init();

    config_id.Init();


    tick_1hz = 0;
    tick_1hz_commensurate = 0;
    resetSysTask(); // helps in avoiding too short first loop
INITCONTROLLER_END

    //-- SysTask handling

    if (doSysTask()) {
        // when we do long tasks, like display transfer, we miss ticks, so we need to catch up
        // the commands below must not be sensitive to strict ms timing

        for (uint8_t s = 0; s < Config.RxCount; s++) {
            if (rxslot[s].connect_tmo_cnt) rxslot[s].connect_tmo_cnt--;
        }

        DECc(tx_tick, SYSTICK_DELAY_MS(Config.frame_rate_ms));

        if (tx_tick == 1) {
            isInTimeGuard = true; // prevent extra work
        }
        if (tx_tick == 0) {
            doPreTransmit = true; // trigger next cycle
            pretransmit_tstamp_us = micros16();
        }

        link_task_tick_ms();

        if (!doPreTransmit) {
            leds.Tick_ms(connected()); // can take long

            DECc(tick_1hz, SYSTICK_DELAY_MS(1000));

            if (!tick_1hz) {
                if (Setup.Tx[Config.ConfigId].Buzzer == BUZZER_RX_LQ && connect_occured_once) {
                    buzzer.BeepLQ(stats.GetReceivedLQ_link());
                }
            }

            bind.Tick_ms();
            disp.Tick_ms(); // can take long
            fan.SetPower(SX_OR_SX2(sx.RfPower_dbm(),sx2.RfPower_dbm()));
            fan.Tick_ms();
            espbridge.Tick_ms();

            if (!tick_1hz) {
                dbg.puts(".");
/*                dbg.puts("\nTX: ");
                dbg.puts(u8toBCD_s(stats.GetLQ_serial()));
                dbg.puts("(");
                dbg.puts(u8toBCD_s(stats.frames_received.GetLQ())); dbg.putc(',');
                dbg.puts(u8toBCD_s(stats.valid_frames_received.GetLQ()));
                dbg.puts("),");
                dbg.puts(u8toBCD_s(stats.received_LQ_link)); dbg.puts(", ");

                dbg.puts(s8toBCD_s(stats.last_rssi1)); dbg.putc(',');
                dbg.puts(s8toBCD_s(stats.received_rssi)); dbg.puts(", ");
                dbg.puts(s8toBCD_s(stats.last_snr1)); dbg.puts("; ");

                dbg.puts(u16toBCD_s(stats.bytes_transmitted.GetBytesPerSec())); dbg.puts(", ");
                dbg.puts(u16toBCD_s(stats.bytes_received.GetBytesPerSec())); dbg.puts("; "); */
            }
        } // end of if (!doPreTransmit)
    }

    //-- SX handling

    switch (link_state) {
    case LINK_STATE_IDLE:
        break;

    case LINK_STATE_TRANSMIT:
        fhss.HopToNext();
        sx.SetFrameLength(Config.FrameLenTx); // the Link Split profiles differ per direction
        sx2.SetFrameLength(Config.FrameLenTx);
        do_transmit_prepare(tdiversity.Antenna(), fhss.GetCurrI(), fhss.GetCurrI2());
        link_state = LINK_STATE_TRANSMIT_SEND;
        DBG_MAIN_SLIM(dbg.puts("\nt");)
        break;

    case LINK_STATE_TRANSMIT_SEND: {
        uint16_t dt = micros16() - pretransmit_tstamp_us;
        bool cca = regulatory.CcaNeeded(tdiversity.Antenna(), fhss.GetCurrFreq());
        if (dt + ((cca) ? REGULATORY_CCA_TIME_US : 0) < 750) break; // listening ends about when the frame would go
        isInTimeGuard = false;
        rfpower.Update();
        sx.SetRfFrequency(fhss.GetCurrFreq());
        sx2.SetRfFrequency(fhss.GetCurrFreq2());
        bool go = regulatory.TxGo(tdiversity.Antenna(), fhss.GetCurrFreq());
        if (go && Config.TransmitBoth && !Config.IsDualBand) go = regulatory.TxGo(ANTENNA_2, fhss.GetCurrFreq2()); // the 2nd channel books its time too
        if (!go) { // not this frame, the Rx sees it as lost and the ARQ sends it again
            link_state = LINK_STATE_RECEIVE;
            break;
        }
        do_transmit_send(tdiversity.Antenna());
        link_state = LINK_STATE_TRANSMIT_WAIT;
        link_tx_status = TX_STATUS_NONE;
        irq_status = irq2_status = 0;
        DBG_MAIN_SLIM(dbg.puts(">");)
        // auxiliaries
        whileTransmit.Trigger();
        break; }

    case LINK_STATE_RECEIVE:
        sx.SetFrameLength(Config.FrameLenRx);
        sx2.SetFrameLength(Config.FrameLenRx);
        IF_ANTENNA1(sx.SetToRx());
        IF_ANTENNA2(sx2.SetToRx());
        link_state = LINK_STATE_RECEIVE_WAIT;
        link_rx1_status = link_rx2_status = RX_STATUS_NONE;
        irq_status = irq2_status = 0;
        DBG_MAIN_SLIM(dbg.puts("r");)
        break;
    }//end of switch(link_state)

IF_SX(
    if (irq_status) {
        if (link_state == LINK_STATE_TRANSMIT_WAIT) {
            if (irq_status & SX_IRQ_TX_DONE) {
                irq_status = 0;
                link_tx_status |= TX_STATUS_TX1_DONE;
                if (!Config.TransmitBoth || (link_tx_status & TX_STATUS_TX2_DONE)) { link_state = LINK_STATE_RECEIVE; }
                DBG_MAIN_SLIM(dbg.puts("1!");)
            }
        } else
        if (link_state == LINK_STATE_RECEIVE_WAIT) {
            if (irq_status & SX_IRQ_RX_DONE) {
                irq_status = 0;
                uint8_t st = do_receive(ANTENNA_1);
                if (st > link_rx1_status) link_rx1_status = st;
                if (link_rx_last_slot + 1 < Config.RxCount) sx.SetToRx(); // the next slot's reply is coming
                DBG_MAIN_SLIM(dbg.puts("1<");)
            }
        }

        if (irq_status) { // these should not happen
            if (irq_status & SX_IRQ_TIMEOUT) {
            }
            if (irq_status & SX_IRQ_RX_DONE) {
                FAIL_WSTATE(BLINK_RD_GR_OFF, "IRQ RX DONE FAIL", irq_status, link_state, link_rx1_status, link_rx2_status);
            }
            if (irq_status & SX_IRQ_TX_DONE) {
                FAIL_WSTATE(BLINK_GR_RD_OFF, "IRQ TX DONE FAIL", irq_status, link_state, link_rx1_status, link_rx2_status);
            }
            irq_status = 0;
            link_state = LINK_STATE_IDLE;
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
                DBG_MAIN_SLIM(dbg.puts("2!");)
            }
        } else
        if (link_state == LINK_STATE_RECEIVE_WAIT) {
            if (irq2_status & SX2_IRQ_RX_DONE) {
                irq2_status = 0;
                uint8_t st = do_receive(ANTENNA_2);
                if (st > link_rx2_status) link_rx2_status = st;
                if (link_rx_last_slot + 1 < Config.RxCount) sx2.SetToRx(); // the next slot's reply is coming
                DBG_MAIN_SLIM(dbg.puts("2<");)
            }
        }

        if (irq2_status) { // this should not happen
            if (irq2_status & SX2_IRQ_TIMEOUT) {
            }
            if (irq2_status & SX2_IRQ_RX_DONE) {
                FAIL_WSTATE(BLINK_RD_GR_ON, "IRQ2 RX DONE FAIL", irq2_status, link_state, link_rx1_status, link_rx2_status);
            }
            if (irq2_status & SX2_IRQ_TX_DONE) {
                FAIL_WSTATE(BLINK_GR_RD_ON, "IRQ2 TX DONE FAIL", irq2_status, link_state, link_rx1_status, link_rx2_status);
            }
            irq2_status = 0;
            link_state = LINK_STATE_IDLE;
            link_rx1_status = link_rx2_status = RX_STATUS_NONE;
            DBG_MAIN_SLIM(dbg.puts("2?");)
        }
    }//end of if(irq2_status)
);

    // this happens before switching to transmit, i.e. after a frame was or should have been received
    if (doPreTransmit) {
        doPreTransmit = false;

        sx.SetToIdle();
        sx2.SetToIdle();

        bool valid_frame_received = false; // from any receiver
        lq_frame_counted = lq_valid_counted = false;
        lq_serial_counted = 0;

        for (uint8_t s = 0; s < link_frames(); s++) {
            tRxSlot* rs = &rxslot[s];
            bool valid = false;
            if (rs->rx_status > RX_STATUS_NONE) { // frame received
                valid = handle_receive(s); // false when it could not be read: counts as invalid
            } else {
                handle_receive_none(s);
            }
            rs->decrypt_ok = valid;
            if (valid) valid_frame_received = true;

            if (Config.Bonded && s == BOND_LANE_B) { // the lanes share lane A's connection, which either lane keeps
                if (valid && rxslot[0].connect_state == CONNECT_STATE_CONNECTED) rxslot[0].connect_tmo_cnt = CONNECT_TMO_SYSTICKS;
                if (!slot_connected(0) || bond_single) rarq[s].Disconnected();
                continue;
            }

            // this receiver's connection
            if (valid) {
                switch (rs->connect_state) {
                case CONNECT_STATE_LISTEN:
                    rs->connect_state = CONNECT_STATE_SYNC;
                    rs->connect_sync_cnt = 0;
                    break;
                case CONNECT_STATE_SYNC:
                    rs->connect_sync_cnt++;
                    uint8_t connect_sync_cnt_max = CONNECT_SYNC_CNT;
                    if (!rs->connect_occured_once) {
                        connect_sync_cnt_max = Config.connect_sync_cnt_max;
                    }
                    if (rs->connect_sync_cnt >= connect_sync_cnt_max && crypto.ValidKeys(s)) { // allow connect only if also crypto allows
                        if (!rs->connect_occured_once && s == rx_selected) {
                            stats.JustConnected();
                        }
                        rs->connect_state = CONNECT_STATE_CONNECTED;
                        rs->connect_occured_once = true;
                        connect_occured_once = true;
                        // its setup data, when not known, is asked for by link_task_pick()
                    }
                    break;
                }
                rs->connect_tmo_cnt = CONNECT_TMO_SYSTICKS;
            }

            // connected but the tmo ran out: so disconnect; it may come back restarted, so a fresh key then
            if (rs->connect_state == CONNECT_STATE_CONNECTED && !rs->connect_tmo_cnt) {
                rs->connect_state = CONNECT_STATE_LISTEN;
                rs->key_fresh = false;
                rs->setup_known = false;
            }

            if (rs->connect_state != CONNECT_STATE_CONNECTED) rarq[s].Disconnected();
        }

        tarq_update();

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

        // serial data is received if !IsInBind() && RX_STATUS_VALID && !FRAME_TYPE_CMD && sx_serial.IsEnabled()
        // a missed frame is not lost yet: the ARQ tells the parsers when a payload really went missing

        link_state = LINK_STATE_TRANSMIT;
        link_rx1_status = link_rx2_status = RX_STATUS_NONE;
        for (uint8_t s = 0; s < link_frames(); s++) rxslot[s].rx_status = RX_STATUS_NONE;

        // a command to a receiver that is not connected goes out once: the data frames and the other receivers come
        // back after it; a link change is given up with it (STORE ends on its own timer, a restart already counting down
        // goes on)
        if (link_task != LINK_TASK_NONE && link_task != LINK_TASK_TX_STORE_RX_PARAMS && link_task_sent &&
            !slot_connected(link_task_slot) && !link_restart_cnt) {
            if (link_task == LINK_TASK_TX_SET_LINK) mode_auto_pending = MODE_AUTO;
            link_task_reset();
        }
        link_task_from_queue();
        link_task_pick();

        // the configured power, or less with Dyn Power; binding keeps its lowest power
        if (!bind.IsInBind()) {
            rfpower.Set(Setup.Tx[Config.ConfigId].Power);
            int8_t rssi = stats.received_rssi;
            uint8_t lq = stats.received_LQ_serial;
            if (Config.RxCount > 1) { // the weakest connected receiver decides: the power has to reach every one
                rssi = RSSI_INVALID;
                lq = 100;
                for (uint8_t s = 0; s < Config.RxCount; s++) {
                    if (!slot_connected(s)) continue;
                    if (rxslot[s].received_rssi < rssi) rssi = rxslot[s].received_rssi;
                    if (rxslot[s].received_LQ_serial < lq) lq = rxslot[s].received_LQ_serial;
                }
            }
            rfpower.Dynamic(Setup.Common[Config.ConfigId].DynPower, connected(), rssi, lq, sx.ReceiverSensitivity_dbm());
        }

        DECc(tick_1hz_commensurate, Config.frame_rate_hz);
        if (!tick_1hz_commensurate) {
            stats.Update1Hz();
            for (uint8_t s = 0; s < link_frames(); s++) { rxslot[s].lq.Update1Hz(); rxslot[s].lq_serial.Update1Hz(); }
            bond_update_1hz();
            mode_auto_update_1hz();
        }
        stats.Next();
        if (!connected()) stats.Clear();

        if (Setup.Tx[Config.ConfigId].Buzzer == BUZZER_LOST_PACKETS && connect_occured_once && !bind.IsInBind()) {
            if (!valid_frame_received) buzzer.BeepLP();
        }

        bind.Do();
        switch (bind.Task()) {
        case BIND_TASK_CHANGED_TO_BIND:
            mode_auto_current = MODE_AUTO; // after the bind both ends start at the meeting point
            band_auto_current = SETUP_FREQUENCY_BAND_NUM;
            bind.ConfigForBind();
            fhss.SetToBind();
            leds.SetToBind();
            for (uint8_t s = 0; s < RX_SLOT_NUM; s++) rxslot[s].connect_state = CONNECT_STATE_LISTEN;
            // link_state was set to LINK_STATE_TRANSMIT already
            break;
        case BIND_TASK_TX_RESTART_CONTROLLER:
            doParamsStore = true;
            break;
        }

        // store parameters
        if (doParamsStore) {
            sx.SetToIdle(); // should not be needed, should not hurt either
            sx2.SetToIdle();
            leds.SetToParamStore();
            setup_store_to_EEPROM();
            GOTO_RESTARTCONTROLLER;
        }

        // Mode = auto: the link restarts with the mode chosen, nothing stored
        if (link_restart_cnt) {
            link_restart_cnt--;
            if (!link_restart_cnt) doLinkRestart = true;
        }
        if (doLinkRestart) {
            sx.SetToIdle();
            sx2.SetToIdle();
            if (mode_auto_pending != MODE_AUTO) {
                mode_auto_current = mode_auto_pending;
                band_auto_current = band_auto_pending;
            }
            mode_auto_hold_s = MODE_AUTO_HOLD_S;
            GOTO_RESTARTCONTROLLER;
        }

//dbg.puts((valid_frame_received) ? "\nvalid" : "\ninval");

        return; // link state might have changed, process immediately
    }//end of if(doPreTransmit)

    if (isInTimeGuard || link_state == LINK_STATE_TRANSMIT_SEND) return; // don't do anything else in this time slot, is important!

    //-- Do MAVLink & MSP

#ifdef USE_SERIAL_ESCAPE
    serial_escape.Do();
#endif
    mavlink.Do();
    msp.Do();

    //-- Do WhileTransmit stuff

    whileTransmit.Do();

    //-- Handle display or CLI or MAVLink task

    uint8_t tx_task = tasks.Task();
    switch (tx_task) {
    case TASK_RX_PARAM_SET:
        if (connected() && !link_task_set(LINK_TASK_TX_SET_RX_PARAMS)) link_task_queue_add(LINK_TASK_TX_SET_RX_PARAMS);
        break;
    case TASK_PARAM_STORE:
        if (!connected()) {
            doParamsStore = true;
        } else if (!link_task_set(LINK_TASK_TX_STORE_RX_PARAMS)) {
            link_task_queue_add(LINK_TASK_TX_STORE_RX_PARAMS);
        }
        break;
    case TASK_PARAM_RELOAD:
        setup_reload();
        if (connected() && !link_task_set(LINK_TASK_TX_GET_RX_SETUPDATA_WRELOAD)) link_task_queue_add(LINK_TASK_TX_GET_RX_SETUPDATA_WRELOAD);
        break;
    case TASK_BIND_START: bind.StartBind(tasks.GetBindSlot()); break;
    case TASK_SELECT_RX: tx_select_rx(tasks.GetConfigIdValue()); break;
    case TASK_BIND_STOP: bind.StopBind(); break;
    case TASK_SYSTEM_BOOT: enter_system_bootloader(); break;
    case TASK_CHANGE_CONFIG_ID: config_id.Change(tasks.GetConfigIdValue()); break;
    }
    espbridge.HandleTask(tx_task, tasks.GetEspBridgeStr());
    hc04bridge.HandleTask(tx_task, tasks.GetHc04BridgeValue());
    if (tx_task == TASK_RESTART_CONTROLLER) { GOTO_RESTARTCONTROLLER; }

    //-- Handle ESP wifi bridge

    espbridge.Do();

    //-- more

    if (config_id.Do()) {
        doParamsStore = true;
    }

}//end of main_loop

