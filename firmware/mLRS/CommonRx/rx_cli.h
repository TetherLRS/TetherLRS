//*******************************************************
// TetherLRS
//*******************************************************
// The receiver's settings from the tool, on its own serial, only while no Tx is heard
//*******************************************************
// One MAVLink COMMAND_LONG to the radio component opens the session: TLRS_RXCLI_CMD with the magic values below,
// only in connect state LISTEN and not binding. Then nothing from a flight controller would go anywhere, so the
// serial is taken only when its bytes are thrown away anyway; with a Tx heard the command is not even looked at.
// In the session the serial is a text CLI like the Tx's (v, pl, p name = ?, p name = value, pstore, exit) on a
// copy of the setup: the running one never changes. It ends on exit, after 10 s without a byte, or the moment a
// Tx is heard; pstore writes the copy and the receiver restarts with it.
//*******************************************************
#ifndef RX_CLI_H
#define RX_CLI_H
#pragma once


#include <ctype.h>
#include <string.h>
#include "../Common/setup_list.h"


#define TLRS_RXCLI_CMD            31010 // MAV_CMD_USER_1, and the values below on top
#define TLRS_RXCLI_MAGIC          1234321
#define TLRS_RXCLI_TMO_MS         10000
#define TLRS_RXCLI_LINES_PER_DO   4 // keeps a parameter list well inside the serial's transmit buffer


typedef enum {
    RXCLI_TYPE_INT8 = 0,
    RXCLI_TYPE_LIST,
    RXCLI_TYPE_STR6,
} RXCLI_TYPE_ENUM;


typedef struct {
    void* ptr; // the field in Setup; the session works on the same field of its copy
    uint8_t type;
    const char* name;
    const char* unit;
    int8_t min;
    int8_t max;
    const char* optstr;
    uint16_t* allowed_mask_ptr;
} tRxCliParam;


const tRxCliParam RxCliParam[] = {
    #define X(p,t, n,mn, d,mi,ma,u, s, amp) { \
                .ptr = (void*)&(p), \
                .type = RXCLI_TYPE_##t, \
                .name = n, \
                .unit = u, \
                .min = mi, \
                .max = ma, \
                .optstr = s, \
                .allowed_mask_ptr = amp },
    SETUP_PARAMETER_LIST_COMMON_BINDPHRASE \
    SETUP_PARAMETER_LIST_COMMON_FURTHER \
    SETUP_PARAMETER_LIST_RX
    #undef X
};

#define RXCLI_PARAM_NUM  (sizeof(RxCliParam)/sizeof(tRxCliParam))


class tRxCli
{
  public:
    void Init(void);
    bool Active(void) { return active; }
    bool StoreRequested(void) { return store_requested; }
    tSetup* Copy(void) { return &copy; }

    // each loop, before MAVLink and MSP; allowed: no Tx heard and not binding
    void Do(bool allowed);

    // from the MAVLink parser in the MAVLink modes; in the others Do() parses the serial itself
    void HandleCmdLong(fmav_message_t* const msg);

  private:
    bool is_unlock(fmav_command_long_t* const cmd);
    void open(void);
    void close(void);
    void listen(void);
    void session(void);
    void send_ack(void);
    void command(void);

    uint8_t* copy_ptr(uint8_t idx) { return (uint8_t*)&copy + ((uint8_t*)RxCliParam[idx].ptr - (uint8_t*)&Setup); }
    uint16_t allowed_mask(uint8_t idx);
    uint8_t opt_num(uint8_t idx);
    bool opt_str(char* const s, uint8_t idx, uint8_t value);
    bool find(uint8_t* const idx, const char* const name);
    void print_param(uint8_t idx);
    void print_options(uint8_t idx);
    bool set_param(uint8_t idx, const char* const svalue);

    void puts(const char* s) { serial->puts(s); }
    void putsn(const char* s) { serial->puts(s); serial->puts("\r\n"); }

    bool active;
    bool allowed;
    bool unlock_pending;
    bool store_requested;
    uint8_t requester_sysid;
    uint8_t requester_compid;
    uint32_t tlast_ms;

    tSetup copy;

    char buf[48];
    uint8_t pos;
    int8_t list_idx; // next parameter of a "pl" being printed, -1 when none

    fmav_status_t status; // the parser for the unlock in the transparent and MSP modes
    uint8_t parse_buf[296];
    fmav_message_t msg;
};


void tRxCli::Init(void)
{
    active = false;
    allowed = false;
    unlock_pending = false;
    store_requested = false;
    pos = 0;
    list_idx = -1;
    memset(&status, 0, sizeof(status));
}


bool tRxCli::is_unlock(fmav_command_long_t* const cmd)
{
    return (cmd->command == TLRS_RXCLI_CMD &&
            cmd->target_component == MAV_COMP_ID_TELEMETRY_RADIO &&
            cmd->param1 == 84.0f && cmd->param2 == 76.0f && cmd->param3 == 82.0f && cmd->param4 == 83.0f && // TLRS
            cmd->param7 == (float)TLRS_RXCLI_MAGIC);
}


void tRxCli::HandleCmdLong(fmav_message_t* const msg)
{
    if (!allowed || active) return;

    fmav_command_long_t cmd;
    fmav_msg_command_long_decode(&cmd, msg);
    if (!is_unlock(&cmd) || cmd.target_system != RADIO_LINK_SYSTEM_ID) return;

    requester_sysid = msg->sysid;
    requester_compid = msg->compid;
    unlock_pending = true; // opened in Do(), outside the MAVLink parser
}


void tRxCli::Do(bool _allowed)
{
    allowed = _allowed;

    if (active) {
        if (!allowed) { close(); return; } // a Tx is heard: the data path gets the serial back at once
        session();
        return;
    }

    if (!allowed) { unlock_pending = false; return; }

    if (!SERIAL_LINK_MODE_IS_MAVLINK(Setup.Rx.SerialLinkMode)) listen();

    if (unlock_pending) open();
}


// transparent and MSP modes, no Tx heard: their bytes would be lost anyway, here they are looked at for the unlock
void tRxCli::listen(void)
{
    fmav_result_t result;

    while (serial->available()) {
        char c = serial->getc();
        fmav_parse_and_check_to_frame_buf(&result, parse_buf, &status, c);
        if (result.res != FASTMAVLINK_PARSE_RESULT_OK) continue;
        if (result.msgid != FASTMAVLINK_MSG_ID_COMMAND_LONG) continue;
        fmav_frame_buf_to_msg(&msg, &result, parse_buf);
        HandleCmdLong(&msg);
        if (unlock_pending) return;
    }
}


void tRxCli::open(void)
{
    unlock_pending = false;
    active = true;
    store_requested = false;
    memcpy(&copy, &Setup, sizeof(tSetup));
    pos = 0;
    list_idx = -1;
    tlast_ms = millis32();
    sx_serial.flush(); // repeats of the unlock and half parsed frames of the other parsers go
    send_ack();
}


void tRxCli::close(void)
{
    active = false;
    store_requested = false;
    list_idx = -1;
    pos = 0;
    sx_serial.flush(); // CLI leftovers must not reach the MAVLink or MSP parsers
    memset(&status, 0, sizeof(status));
}


void tRxCli::send_ack(void)
{
    uint8_t frame[FASTMAVLINK_FRAME_LEN_MAX];
    fmav_status_t st = {};

    uint16_t len = fmav_msg_command_ack_pack_to_frame_buf(
        frame,
        RADIO_LINK_SYSTEM_ID, MAV_COMP_ID_TELEMETRY_RADIO,
        TLRS_RXCLI_CMD, MAV_RESULT_ACCEPTED, 0, TLRS_RXCLI_MAGIC + 1, requester_sysid, requester_compid,
        //uint16_t command, uint8_t result, uint8_t progress, int32_t result_param2, uint8_t target_system, uint8_t target_component,
        &st);
    serial->putbuf(frame, len);
}


void tRxCli::session(void)
{
    uint32_t tnow_ms = millis32();

    if (store_requested) return; // main is writing the copy and restarts

    if (list_idx >= 0) { // "pl" in chunks
        for (uint8_t n = 0; n < TLRS_RXCLI_LINES_PER_DO && list_idx < (int8_t)RXCLI_PARAM_NUM; n++) {
            print_param(list_idx++);
        }
        if (list_idx >= (int8_t)RXCLI_PARAM_NUM) list_idx = -1;
        tlast_ms = tnow_ms;
        return;
    }

    if (tnow_ms - tlast_ms > TLRS_RXCLI_TMO_MS) { close(); return; }

    while (serial->available()) {
        char c = serial->getc();
        tlast_ms = tnow_ms;

        if (c == '\r' || c == '\n') {
            if (!pos) continue;
            buf[pos] = '\0';
            putsn(">");
            command();
            pos = 0;
            if (!active || list_idx >= 0 || store_requested) return;
            continue;
        }
        if (c < 0x20 || c > 0x7E) continue; // a repeat of the binary unlock, never part of a command
        if (pos >= sizeof(buf) - 1) continue;
        buf[pos++] = c;
        serial->putc(c);
    }
}


void tRxCli::command(void)
{
    if (!strcmp(buf, "v")) {
        putsn("  Rx: " DEVICE_NAME ", " VERSIONONLYSTR);
        return;
    }
    if (!strcmp(buf, "pl")) {
        list_idx = 0;
        return;
    }
    if (!strcmp(buf, "pstore")) {
        putsn("  parameters stored");
        store_requested = true;
        return;
    }
    if (!strcmp(buf, "exit")) {
        putsn("  back to data");
        close();
        return;
    }

    // p name = value, p name = ?
    if (buf[0] != 'p' || buf[1] != ' ') { putsn("err: invalid command"); return; }

    char name[40], svalue[24];
    uint8_t n = 0, v = 0;
    bool value_part = false;
    for (uint8_t i = 2; buf[i] != '\0'; i++) {
        if (buf[i] == ' ') continue;
        if (buf[i] == '=' && !value_part) { value_part = true; continue; }
        if (value_part) {
            if (v < sizeof(svalue) - 1) svalue[v++] = buf[i]; // as typed: a bind phrase is lower case
        } else {
            if (n < sizeof(name) - 1) name[n++] = toupper(buf[i]);
        }
    }
    name[n] = '\0';
    svalue[v] = '\0';
    if (!value_part) strcpy(svalue, "?");

    uint8_t idx;
    if (!find(&idx, name)) { putsn("err: invalid parameter name"); return; }
    if (svalue[0] == '?') {
        print_param(idx);
        print_options(idx);
        return;
    }
    if (svalue[0] == '\0') { putsn("err: no value specified"); return; }
    if (!set_param(idx, svalue)) { putsn("err: invalid value"); return; }
    print_param(idx);
}


uint16_t tRxCli::allowed_mask(uint8_t idx)
{
    return (RxCliParam[idx].allowed_mask_ptr) ? *RxCliParam[idx].allowed_mask_ptr : UINT16_MAX;
}


uint8_t tRxCli::opt_num(uint8_t idx)
{
    uint8_t num = 1;
    for (const char* c = RxCliParam[idx].optstr; *c; c++) if (*c == ',') num++;
    return num;
}


// the value's text in the option string, the CLI wording for band and diversity as the Tx prints them
bool tRxCli::opt_str(char* const s, uint8_t idx, uint8_t value)
{
    const char* optstr = RxCliParam[idx].optstr;
    if (RxCliParam[idx].ptr == &(Setup.Common[0].FrequencyBand)) optstr = SETUP_OPT_RF_BAND_CLI;
    if (RxCliParam[idx].ptr == &(Setup.Rx.Diversity)) optstr = SETUP_OPT_DIVERSITY_CLI;

    uint8_t i = 0, n = 0;
    for (const char* c = optstr; ; c++) {
        if (*c == ',' || *c == '\0') {
            if (i == value) { s[n] = '\0'; return true; }
            if (*c == '\0') break;
            i++;
            n = 0;
            continue;
        }
        if (i == value && n < 30) s[n++] = *c;
    }
    s[0] = '\0';
    return false;
}


bool tRxCli::find(uint8_t* const idx, const char* const name)
{
    char s[40];

    for (uint8_t i = 0; i < RXCLI_PARAM_NUM; i++) {
        uint8_t n = 0;
        for (const char* c = RxCliParam[i].name; *c && n < sizeof(s) - 1; c++) s[n++] = (*c == ' ') ? '_' : toupper(*c);
        s[n] = '\0';
        if (!strcmp(s, name)) { *idx = i; return true; }
    }
    return false;
}


void tRxCli::print_param(uint8_t idx)
{
    char s[32];
    uint8_t* p = copy_ptr(idx);

    puts("  ");
    puts(RxCliParam[idx].name);
    puts(" = ");
    switch (RxCliParam[idx].type) {
    case RXCLI_TYPE_INT8:
        stoBCDstr(*(int8_t*)p, s);
        puts(s);
        if (RxCliParam[idx].unit[0] != '\0') { puts(" "); puts(RxCliParam[idx].unit); }
        break;
    case RXCLI_TYPE_LIST: {
        uint16_t mask = allowed_mask(idx);
        uint8_t allowed_num = 0;
        for (uint8_t i = 0; i < opt_num(idx); i++) if (mask & (1 << i)) allowed_num++;
        if (allowed_num == 0) { putsn("- (unavailable)"); return; }
        opt_str(s, idx, *p);
        puts(s);
        puts(" [");
        serial->putc('0' + *p);
        puts("]");
        if (allowed_num == 1) puts("(unchangeable)");
        }break;
    case RXCLI_TYPE_STR6:
        strstrbufcpy(s, (char*)p, 6);
        puts(s);
        break;
    }
    putsn("");
}


void tRxCli::print_options(uint8_t idx)
{
    char s[32];

    switch (RxCliParam[idx].type) {
    case RXCLI_TYPE_INT8:
        puts("  min: "); stoBCDstr(RxCliParam[idx].min, s); putsn(s);
        puts("  max: "); stoBCDstr(RxCliParam[idx].max, s); putsn(s);
        break;
    case RXCLI_TYPE_LIST: {
        uint16_t mask = allowed_mask(idx);
        for (uint8_t i = 0; opt_str(s, idx, i); i++) {
            if (!(mask & (1 << i))) continue;
            puts("  "); serial->putc('0' + i); puts(" = "); putsn(s);
        }
        }break;
    case RXCLI_TYPE_STR6:
        putsn("  [a-z0-9#-._]");
        break;
    }
}


bool tRxCli::set_param(uint8_t idx, const char* const svalue)
{
    uint8_t* p = copy_ptr(idx);

    switch (RxCliParam[idx].type) {
    case RXCLI_TYPE_INT8: {
        int32_t v = atoi(svalue);
        if (v < RxCliParam[idx].min || v > RxCliParam[idx].max) return false;
        *(int8_t*)p = v;
        return true; }
    case RXCLI_TYPE_LIST: {
        for (const char* c = svalue; *c; c++) if (!isdigit(*c)) return false;
        int32_t v = atoi(svalue);
        if (v < 0 || v >= opt_num(idx) || v > 15 || !(allowed_mask(idx) & (1 << v))) return false;
        *p = v;
        return true; }
    case RXCLI_TYPE_STR6:
        if (strlen(svalue) != 6) return false;
        for (uint8_t i = 0; i < 6; i++) if (!is_valid_bindphrase_char(svalue[i])) return false;
        memcpy(p, svalue, 6);
        return true;
    }
    return false;
}


#endif // RX_CLI_H
