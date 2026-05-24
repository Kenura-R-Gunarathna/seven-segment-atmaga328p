### Strucutre 

```
main.c     → clock_tick(), counter_tick()
               ↓
utils.h    → clock, counter logic
               ↓
display.h  → buffer, multiplexing
               ↓
seg.h      → PORTD = pattern
digit.h    → digit select
charset.h  → number → pattern
               ↓
gpio.h     → raw pin control
               ↓
hardware
```
