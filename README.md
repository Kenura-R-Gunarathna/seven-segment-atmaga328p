### Strucutre 

```
main.c     → clock_tick(), counter_tick()
               ↓
utils.h    → clock, counter logic
               ↓
display.h  → buffer, multiplexing
               ↓
seg.h      → segment byte
digit.h    → digit select byte
charset.h  → number → pattern
               ↓
spi595.h   → shift 2 bytes over SPI + latch
               ↓
hardware (74xx595 chain)
```

### Pin definitions

ATmega328P hardware SPI → daisy-chained 74xx595 shift registers
(all 595s share SRCLK + RCLK; `QH'` of one feeds `SER` of the next).

```
   ATmega328P                 74xx595  (#1 SEGMENTS)        74xx595  (#2 DIGITS)
 ┌────────────┐             ┌──────────────────┐         ┌──────────────────┐
 │ PB3  D11   │── MOSI ───▶ │ 14 SER     QH' 9 │──────▶  │ 14 SER     QH' 9 │─▶ (#3…)
 │ PB5  D13   │── SCK  ──┬▶ │ 11 SRCLK         │   ┌───▶ │ 11 SRCLK         │
 │ PB2  D10   │── RCLK ─┐└▶ │ 12 RCLK          │   │ ┌─▶ │ 12 RCLK          │
 │ PB4  D12   │  (MISO  │ └───────────┐    ┌───┘   │ │   └──────────────────┘
 │            │  unused)│             │    │       │ │
 └────────────┘         └─────────────┼────┼───────┘ │
                                      └────┼─────────┘   shared SRCLK + RCLK
                                           │
   each 595:  OE(13) → GND   (outputs always enabled)
              MR(10) → VCC   (no reset)

 SER   = data   → PB3 / D11 / MOSI
 SRCLK = shift  → PB5 / D13 / SCK
 RCLK  = latch  → PB2 / D10 / SS
```

Bit order is **LSB-first** (`DORD=1`).

```
 #1 SEGMENTS (CHARSET byte, active-HIGH)      #2 DIGITS (one bit = one digit)
   bit 7 6 5 4 3 2 1 0                          buffer pos : 0    1    2    3
   seg  a b c d e f g dp                        digit bit  : 2    3    4    5
        │ │ │ │ │ │ │ └─ decimal point          (DIG_BIT[] in digit.h)
        │ │ │ │ │ │ └─── g                       reverse the table to mirror
        ... etc                                  the left↔right order
```

Both bytes go out in one burst per update: **segment byte first** (far chip),
**digit byte second** (near chip). If they land on the wrong chip, swap the two
`sr_tx()` calls in `sr_flush()` (`spi595.h`).
