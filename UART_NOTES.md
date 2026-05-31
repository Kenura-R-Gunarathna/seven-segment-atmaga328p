# UART / Bluetooth — how it works (temporary notes)

How the ATmega328P talks to the HC-06 Bluetooth module. Delete this file
once it's in your head.

## 1. The big idea

The HC-06 is **not** the clever part. It's a dumb, transparent bridge:

```
AVR USART  ──serial bytes──▶  HC-06  ──Bluetooth SPP──▶  phone / PC
```

Whatever bytes the AVR writes to its USART come out the other side on your
laptop. "Send over Bluetooth" literally just means "write bytes to the USART."
You never configure the HC-06 in code — it sits at 9600 baud and forwards
everything both ways.

## 2. UART basics

UART = one wire each direction, no shared clock. Both ends must agree on the
**baud rate** (bits per second) ahead of time:

```
TX ──────▶ RX     (AVR PD1 -> HC-06 RXD)
RX ◀────── TX     (AVR PD0 <- HC-06 TXD)
GND ────── GND    (common reference, mandatory)
```

A byte goes out as: 1 start bit, 8 data bits, 1 stop bit ("8N1"). No clock
line — the receiver samples at the agreed baud, so if the rates don't match
within ~2%, you get garbage.

## 3. Baud rate — the one tricky number

We run at **F_CPU = 1 MHz**. The USART derives baud from a divider register
`UBRR`:

```
baud = F_CPU / (16 * (UBRR + 1))           // normal mode
baud = F_CPU / (8  * (UBRR + 1))           // double-speed (U2X = 1)
```

For 9600 at 1 MHz, normal mode gives UBRR ≈ 5.5 → rounding to 6 is **~7% off**
= unreliable. So we turn on **double-speed (U2X0)**:

```
UBRR = 1000000 / (8 * 9600) - 1 = 12.02 → 12
actual baud = 1000000 / (8 * 13) = 9615   → only 0.16% error  ✓
```

That's why the driver sets `UBRR0=12` **and** `U2X0=1`.

## 4. The registers (ATmega328P USART0)

| Register | Role |
|---|---|
| `UBRR0H/L` | baud divider (we use `UBRR0L = 12`) |
| `UCSR0A`   | status flags: `U2X0` (double speed), `UDRE0` (TX buffer empty), `RXC0` (byte received) |
| `UCSR0B`   | enables: `TXEN0` (transmitter), `RXEN0` (receiver) |
| `UCSR0C`   | frame format: `UCSZ01 | UCSZ00` = 8 data bits |
| `UDR0`     | the data register — **write** to send a byte, **read** to get a received byte |

## 5. The driver (`src/drivers/uart.h`)

**Init** — set baud, double-speed, enable TX+RX, pick 8N1:
```c
UBRR0L = 12;
UCSR0A = (1 << U2X0);
UCSR0B = (1 << TXEN0) | (1 << RXEN0);
UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
```

**Send a byte** — wait until the hardware can take it, then drop it in:
```c
while (!(UCSR0A & (1 << UDRE0))) { }   // UDRE0 = "data register empty?"
UDR0 = c;                              // hardware shifts it out bit by bit
```
`UDRE0` is the key: it goes high when `UDR0` is free for the next byte. We
spin on it so we never overwrite a byte mid-transmission. (TX works whether
or not anyone is connected — the bytes just go nowhere if unpaired.)

**Receive a byte (non-blocking)** — only read if one actually arrived:
```c
if (UCSR0A & (1 << RXC0)) { return UDR0; }   // RXC0 = "byte waiting?"
return -1;                                   // nothing yet
```
Non-blocking matters: the main loop must keep calling `display_refresh()`, so
we can't sit and wait for input. We peek `RXC0` each loop and move on if empty.

**Helpers** — `uart_puts()` loops `uart_putc` over a string; `uart_put_2d()`
prints a number as two ASCII digits (`7 -> '0','7'`). Numbers must be turned
into characters — `21` is sent as the bytes `'2'` `'1'`, not the value 21.

## 6. How the clock uses it (`src/main.c`)

**Transmit** — once a second, format the time and push it out:
```c
uart_put_2d(clock_get_hh()); uart_putc(':');
uart_put_2d(clock_get_mm()); uart_putc(':');
uart_put_2d(clock_get_ss()); uart_putc('\n');
```
Result on your terminal: `21:37:01`, `21:37:02`, …

**Receive** — we build up a line until a newline, then parse it:
```c
int16_t ch = uart_try_getc();
if (ch >= 0) {
    if (ch == '\n' || ch == '\r') { bt_handle_line(cmd, cmdlen); cmdlen = 0; }
    else cmd[cmdlen++] = ch;
}
```
`bt_handle_line` pulls the digits out of whatever you typed (`21:45`, `2145`,
`21:45:30` all work), validates the range, and calls `clock_set()`.

So the command protocol is just: **send a line of digits, get the clock set,
get `OK` back.**

## 7. The HC-06 wiring gotcha

The AVR TX pin idles/drives at **5 V**, but the HC-06 RX input is **3.3 V**.
Feeding 5 V straight in can damage it, so put a divider on that one line:

```
AVR PD1 (TX, 5V) ──[1k]──┬── HC-06 RXD     node = 5 * 2k/(1k+2k) = 3.3V
                        [2k]
                         │
                        GND
```
The other direction (HC-06 TX at 3.3 V → AVR RX) is fine direct — 3.3 V reads
as logic-high on a 5 V AVR. And **common ground** between the two, always.

## 8. Reading it on Linux (no rfcomm binary needed)

Pair in `bluetoothctl` (PIN 1234/0000), then use the raw socket TUI:
```bash
python3 tools/bt_clock.py            # default MAC baked in
```
Type a time + Enter to set the clock; the stream scrolls above. The socket
replaces the missing `rfcomm` command entirely.
```python
s = socket.socket(socket.AF_BLUETOOTH, socket.SOCK_STREAM, socket.BTPROTO_RFCOMM)
s.connect((MAC, 1))      # channel 1 = SPP
```
