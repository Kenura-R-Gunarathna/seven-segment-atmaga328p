# UART and HC-06 Bluetooth — complete teaching notes

How the ATmega328P USART works from first principles, how the HC-06 Bluetooth
module uses it, and how every register setting maps to a clickable line in the
real code.

**Base repo:** `https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p`
**Branch:** `capacitance_meter`

Shorthand used below:
- [`uart.h`] = [`src/drivers/uart.h`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h)
- [`hc06.h`] = [`src/drivers/hc06.h`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/hc06.h)
- [`main.c`] = [`src/main.c`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/main.c)
- [`capmeas.h`] = [`src/drivers/capmeas.h`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/capmeas.h)

---

## 1. What UART is

**UART (Universal Asynchronous Receiver-Transmitter)** is the simplest serial
communication protocol: two wires (TX and RX), no shared clock, both ends agree
on a speed (baud rate) in advance and count their own timing from the first edge
they see.

One byte on the wire (8N1 — 8 data bits, No parity, 1 stop bit):

```
idle  START  D0  D1  D2  D3  D4  D5  D6  D7  STOP  idle
HIGH    0     ?   ?   ?   ?   ?   ?   ?   ?    1    HIGH
           ←─────────────── 10 bit-times ───────────────→
```

- **Idle = HIGH** — the line sits at logic 1 when quiet.
- **Start bit = LOW** — the receiver detects this falling edge and starts counting.
- **8 data bits** LSB first — the byte, one bit at a time.
- **Stop bit = HIGH** — gives the receiver time to process.

At 9600 baud each bit lasts `1/9600 = 104 µs`; one full byte = 1.04 ms.
There is **no clock wire** — if the baud rates differ by more than ~2 %, the
receiver samples at the wrong moment and reads garbage.

---

## 2. The ATmega328P USART registers

All four registers are written in **one function** called once at startup:

→ [`uart_init()`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L14)
called from
[`main.c` line 18](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/main.c#L18)

---

### 2.1 UBRR0 — baud rate register

The USART divides the CPU clock to generate the baud clock:

```
Normal speed:  baud = F_CPU / (16 × (UBRR + 1))
Double speed:  baud = F_CPU / ( 8 × (UBRR + 1))   ← U2X0 = 1
```

We use **double-speed** — at 16 MHz the normal-speed UBRR for 9600 is not an
integer. Double-speed gives:

```
UBRR = 16 000 000 / (8 × 9600) − 1 = 207
actual baud = 16 000 000 / (8 × 208) = 9615   (0.16 % error ✓)
```

The macro is computed at compile time from `F_CPU` (set in `Makefile`), so
changing the clock auto-recalculates the baud divider:

| What | Where in code |
|------|--------------|
| `#define UART_BAUD` | [`uart.h` line 11](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L11) |
| `#define UART_UBRR` (formula) | [`uart.h` line 12](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L12) |
| `UBRR0H = ...` (write high byte) | [`uart.h` line 15](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L15) |
| `UBRR0L = ...` (write low byte) | [`uart.h` line 16](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L16) |

---

### 2.2 UCSR0A — status register

| Bit | Name | Meaning |
|-----|------|---------|
| 1 | U2X0 | Double-speed mode — halves the baud divider |
| 5 | UDRE0 | TX buffer empty — safe to load the next byte |
| 7 | RXC0 | RX complete — a received byte is waiting in UDR0 |

| Action | Where in code |
|--------|--------------|
| **SET** U2X0 (double-speed on) | [`uart.h` line 17](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L17) |
| **READ** UDRE0 (wait TX free) | [`uart.h` line 22](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L22) in `uart_putc` |
| **READ** RXC0 (byte arrived?) | [`uart.h` line 36](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L36) in `uart_try_getc` |

---

### 2.3 UCSR0B — control register

| Bit | Name | Meaning |
|-----|------|---------|
| 3 | TXEN0 | Enable transmitter — hardware takes over PD1 |
| 4 | RXEN0 | Enable receiver — hardware takes over PD0 |

Setting TXEN0 makes the USART hardware control PD1 automatically — you do **not**
need to write the DDR register for PD0/PD1.

| Action | Where in code |
|--------|--------------|
| **SET** TXEN0 + RXEN0 | [`uart.h` line 18](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L18) |

---

### 2.4 UCSR0C — frame format register (8N1)

| Bits | Name | Our setting |
|------|------|-------------|
| UCSZ01:UCSZ00 | Character size | `11` = 8 bits |
| UPM01:UPM00 | Parity | `00` = none (default) |
| USBS0 | Stop bits | `0` = 1 stop bit (default) |

| Action | Where in code |
|--------|--------------|
| **SET** 8N1 frame | [`uart.h` line 19](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L19) |

---

### 2.5 UDR0 — data register

- **Write** → byte shifted out on PD1 (TX).
- **Read** → byte that arrived on PD0 (RX). Reading also clears RXC0.

Writing while UDRE0 = 0 silently loses the previous byte — hence the wait that
always precedes a write.

| Action | Where in code |
|--------|--------------|
| **WRITE** UDR0 (send a byte) | [`uart.h` line 23](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L23) in `uart_putc` |
| **READ** UDR0 (receive a byte) | [`uart.h` line 36](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L36) in `uart_try_getc` |

---

## 3. Sending a byte — blocking transmit

→ [`uart_putc()`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L21)

```c
void uart_putc(char c) {
    while (!(UCSR0A & (1 << UDRE0))) { }   // ← uart.h line 22: wait TX free
    UDR0 = c;                              // ← uart.h line 23: hand to hardware
}
```

1. [Line 22](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L22) — spin until UDRE0 = 1 (previous byte finished shifting, ~1 ms at 9600 baud).
2. [Line 23](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L23) — write UDR0. Hardware shifts bits out on PD1 independently; our code returns immediately.

This is **polling / blocking**: simple, no interrupts, CPU waits between bytes.

Sending strings and numbers:

| Function | Location | What it does |
|----------|----------|--------------|
| `uart_puts()` | [`uart.h` lines 26–28](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L26) | loop `uart_putc` over a string |
| `uart_put_2d()` | [`uart.h` lines 30–33](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L30) | send a value as exactly 2 ASCII digits |
| `uart_put_u32()` | [`uart.h` lines 35–43](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L35) | send any uint32_t as ASCII decimal |

The value `21` is sent as characters `'2'` then `'1'` (bytes 0x32, 0x31) — not
raw byte 0x15. `uart_put_u32` extracts digits via `% 10` / `/ 10`, reverses
them, then sends each via `uart_putc`.

---

## 4. Receiving a byte — non-blocking poll

→ [`uart_try_getc()`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L45)

```c
int16_t uart_try_getc(void) {
    if (UCSR0A & (1 << RXC0)) { return UDR0; }   // ← uart.h line 36
    return -1;
}
```

- [Line 36](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L36) — check RXC0. If a byte is waiting, read UDR0 (also clears RXC0) and return it as `int16_t` (range 0–255).
- If nothing arrived — return −1 immediately, no blocking.

`int16_t` (not `uint8_t`) lets −1 be a sentinel without colliding with real byte
values 0–255.

**Why non-blocking?** The main loop calls
[`display_refresh()`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/main.c#L31)
every iteration to multiplex the 7-seg. A blocking `while(!RXC0){}` would freeze
the display while waiting for a command. Non-blocking lets both coexist:

```c
// main.c lines 26–29
int16_t  rx  = hc06_getc();           // peek — returns -1 if nothing
uint8_t  go  = ... || hc06_is_trigger(rx);
```

→ [`main.c` lines 26–29](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/main.c#L26)

---

## 5. Physical pins and voltage divider

USART0 pins are fixed by the silicon:

```
PD1 (Arduino D1) = TXD   AVR transmits  → HC-06 receives
PD0 (Arduino D0) = RXD   HC-06 transmits → AVR receives
```

Setting TXEN0/RXEN0 in
[`uart.h` line 18](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L18)
hands control of PD0/PD1 to the USART — no DDR writes needed.

### Voltage divider (AVR TX → HC-06 RX only)

AVR PD1 idles at **5 V**; HC-06 RX is rated **3.3 V max**:

```
AVR PD1 (5 V) ──[1 kΩ]──┬──── HC-06 RXD
                       [2.29 kΩ]     node = 5 × 2.29/(1+2.29) = 3.48 V ✓
                          │
                         GND
```

HC-06 TXD (3.3 V) → AVR PD0: **direct, no divider** (AVR logic-HIGH ≈ 1.5 V;
3.3 V reads as HIGH). **Common ground is mandatory.**

---

## 6. The HC-06 module

A transparent **SPP (Serial Port Profile) Bluetooth 2.0** bridge. Inside: a
radio chip + microcontroller that forwards bytes between the radio and its UART
pins. Our firmware sees it as just another UART peer:

```
ATmega USART0  ──bytes──►  HC-06 UART pins  ──wireless──►  phone / PC
phone / PC     ──bytes──►  HC-06 UART pins  ──bytes────►   ATmega USART0
```

Bluetooth pairing, packet framing, retransmission — all inside the module,
invisible to our code.

| LED | Meaning |
|-----|---------|
| Blinking | Waiting for connection |
| Steady | Connected — data flows |

| Parameter | Default |
|-----------|---------|
| Baud | 9600 |
| PIN | 1234 (or 0000) |
| Mode | Slave only |
| Frame | 8N1 |

**Connect from Linux** — no `rfcomm` binary needed, use a raw socket
([`tools/bt_cap.py`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/tools/bt_cap.py)):

```python
import socket
s = socket.socket(socket.AF_BLUETOOTH, socket.SOCK_STREAM, socket.BTPROTO_RFCOMM)
s.connect(('98:DA:60:0F:A8:4E', 1))   # MAC, channel 1 = SPP
while True: print(s.recv(64).decode(errors='replace'), end='', flush=True)
```

---

## 7. hc06.h — protocol layer on top of uart.h

→ [`src/drivers/hc06.h`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/hc06.h)

| Function / alias | Location | What it does |
|-----------------|----------|--------------|
| `hc06_ready()` | [`hc06.h` line 12](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/hc06.h#L12) | sends `"READY\n"` on boot |
| `hc06_getc()` | [`hc06.h` line 17](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/hc06.h#L17) | delegates to `uart_try_getc()` |
| `hc06_is_trigger()` | [`hc06.h` line 22](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/hc06.h#L22) | returns 1 for `'m'`,`'M'`,CR,LF |
| `hc06_putc` | [`hc06.h` line 28](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/hc06.h#L28) | `#define` alias → `uart_putc` |
| `hc06_puts` | [`hc06.h` line 29](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/hc06.h#L29) | `#define` alias → `uart_puts` |
| `hc06_put_u32` | [`hc06.h` line 30](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/hc06.h#L30) | `#define` alias → `uart_put_u32` |

The `#define` aliases decouple callers from UART details. If the module changes
(different baud, HC-05, etc.), only `uart.h` + `hc06.h` need updating.

---

## 8. Full data-flow with clickable references

### Transmit path (AVR → phone)

```
capmeas_run()   capmeas.h → hc06_puts() → uart_puts() → uart_putc()
                                                              │
                                                         wait UDRE0 (uart.h:22)
                                                         write UDR0 (uart.h:23)
                                                              │
                                              PD1 → [1k/2.29k divider] → HC-06 RXD
                                                              │
                                                     wireless SPP Bluetooth
                                                              │
                                                        phone / bt_cap.py
```

Key links in the chain:

| Step | Link |
|------|------|
| `capmeas_run()` calls `hc06_puts()` | [`capmeas.h`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/capmeas.h) |
| `hc06_puts` → `uart_puts` alias | [`hc06.h` line 29](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/hc06.h#L29) |
| `uart_puts` loops `uart_putc` | [`uart.h` lines 26–28](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L26) |
| `uart_putc` waits UDRE0 | [`uart.h` line 22](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L22) |
| `uart_putc` writes UDR0 | [`uart.h` line 23](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L23) |

### Receive path (phone → AVR)

| Step | Link |
|------|------|
| Main loop polls `hc06_getc()` | [`main.c` line 26](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/main.c#L26) |
| `hc06_getc()` calls `uart_try_getc()` | [`hc06.h` line 17](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/hc06.h#L17) |
| `uart_try_getc()` checks RXC0, reads UDR0 | [`uart.h` line 36](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L36) |
| `hc06_is_trigger('m')` fires measurement | [`main.c` line 27–29](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/main.c#L27) |

---

## 9. Register map — every set and read site

| Register | SET at | READ at | Purpose |
|----------|--------|---------|---------|
| `UBRR0H` | [`uart.h:15`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L15) | — | Baud rate high byte |
| `UBRR0L` | [`uart.h:16`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L16) | — | Baud rate low byte |
| `UCSR0A` (U2X0) | [`uart.h:17`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L17) | — | Double-speed mode |
| `UCSR0A` (UDRE0) | — | [`uart.h:22`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L22) | TX buffer empty flag |
| `UCSR0A` (RXC0) | — | [`uart.h:36`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L36) | RX byte ready flag |
| `UCSR0B` | [`uart.h:18`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L18) | — | Enable TX + RX |
| `UCSR0C` | [`uart.h:19`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L19) | — | 8N1 frame format |
| `UDR0` (TX) | [`uart.h:23`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L23) | — | Write byte to send |
| `UDR0` (RX) | — | [`uart.h:36`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L36) | Read received byte |

---

## 10. Common mistakes

| Mistake | Symptom | Fix |
|---------|---------|-----|
| No common GND | Garbage / no data | Wire GNDs together |
| No voltage divider AVR TX → HC-06 RX | HC-06 damaged / won't pair | 1 kΩ + 2.29 kΩ on PD1 |
| UBRR wrong for F\_CPU | Garbled characters | Use [`#define UART_UBRR`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L12) (scales with F\_CPU automatically) |
| Read UDR0 without checking RXC0 | Stale / wrong byte | Check first — [`uart.h:36`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L36) does this |
| Write UDR0 without checking UDRE0 | Byte lost | Wait first — [`uart.h:22`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L22) does this |
| Manually set DDR for PD0/PD1 | Conflict with USART | Don't — TXEN0/RXEN0 (set at [`uart.h:18`](https://github.com/Kenura-R-Gunarathna/seven-segment-atmaga328p/blob/capacitance_meter/src/drivers/uart.h#L18)) own those pins |
