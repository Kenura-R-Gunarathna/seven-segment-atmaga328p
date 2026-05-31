#include "drivers/millis.h"
#include "drivers/gpio.h"
#include "drivers/display.h"
#include "drivers/uart.h"
#include "utils.h"

// send current clock time over Bluetooth as "HH:MM:SS\n"
static void bt_send_time(void) {
    uart_put_2d(clock_get_hh());
    uart_putc(':');
    uart_put_2d(clock_get_mm());
    uart_putc(':');
    uart_put_2d(clock_get_ss());
    uart_putc('\n');
}

// Parse a received command line. Accepts a line with 4 digits (HH:MM) or
// 6 digits (HH:MM:SS); separators are ignored ("21:45", "2145", "214530").
static void bt_handle_line(const char* line, uint8_t len) {
    uint8_t d[6], n = 0;
    for (uint8_t i = 0; i < len && n < 6; i++) {
        if (line[i] >= '0' && line[i] <= '9') { d[n++] = line[i] - '0'; }
    }
    if (n != 4 && n != 6) { uart_puts("ERR fmt (HH:MM or HH:MM:SS)\n"); return; }

    uint8_t hh = d[0] * 10 + d[1];
    uint8_t mm = d[2] * 10 + d[3];
    uint8_t ss = (n == 6) ? d[4] * 10 + d[5] : 0;
    if (hh >= 24 || mm >= 60 || ss >= 60) { uart_puts("ERR range\n"); return; }

    clock_set(hh, mm, ss);
    uart_puts("OK ");
    bt_send_time();
}

int main(void) {
    gpio_set_all_output();
    millis_init();
    uart_init();             // HC-06 on USART, 9600 8N1
    sei();

    clock_init(21, 13);      // start at 21.13.00

    uint32_t last_sec = 0;   // 1Hz — tick clock, blink dot, send over BT

    char     cmd[12];
    uint8_t  cmdlen = 0;

    for (;;) {
        display_refresh();

        // ── receive: accumulate a line, parse on newline ──
        int16_t ch = uart_try_getc();
        if (ch >= 0) {
            char c = (char)ch;
            if (c == '\n' || c == '\r') {
                if (cmdlen > 0) { bt_handle_line(cmd, cmdlen); cmdlen = 0; }
            } else if (cmdlen < sizeof(cmd) - 1) {
                cmd[cmdlen++] = c;
            }
        }

        // ── once per second ──
        uint32_t now = millis();
        if (now - last_sec >= 1000) {
            last_sec += 1000;
            clock_tick_sec();      // advance time (cascades to min/hour)
            clock_dot_toggle();    // blink the colon dot
            bt_send_time();        // stream HH:MM:SS over Bluetooth
        }
    }
    return 0;
}
