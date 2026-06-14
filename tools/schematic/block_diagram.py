#!/usr/bin/env python3
"""Example schemdraw figure — the FG/scope signal-flow block diagram.

Pattern to copy for new doc figures. Renders to docs/diagrams/block_diagram.svg.
    pip install schemdraw[matplotlib]
    python tools/schematic/block_diagram.py
"""
from pathlib import Path

import schemdraw
import schemdraw.elements as elm
from schemdraw import flow

OUT = Path(__file__).resolve().parents[2] / "docs" / "diagrams" / "block_diagram.svg"


def build() -> schemdraw.Drawing:
    d = schemdraw.Drawing()
    d.config(fontsize=11)

    mcu = d.add(flow.Box(w=3, h=1).label("ATmega32A\n(DDS + SAR + UART)"))
    d.add(flow.Arrow().right().at(mcu.E).label("HW-SPI", "top"))
    dac = d.add(flow.Box(w=2.6, h=1).label("16-bit R-2R\nsignal DAC"))
    d.add(flow.Arrow().right().at(dac.E))
    buf = d.add(flow.Box(w=2.2, h=1).label("buffer\n(DACV)"))
    d.add(flow.Arrow().right().at(buf.E))
    gain = d.add(flow.Box(w=2.8, h=1).label("center → LPF → mux\n→ gain + offset"))
    d.add(flow.Arrow().right().at(gain.E))
    out = d.add(flow.Box(w=2.4, h=1).label("output buf\n+ protect"))
    d.add(flow.Arrow().right().at(out.E).label("BNC", "top"))

    # scope tap off the buffer
    d.add(flow.Arrow().down().at(buf.S).length(1.2).label("DACV", "right"))
    sar = d.add(flow.Box(w=2.6, h=1).anchor("N").label("SAR: LM393 U31\n→ PD2 / PA0"))

    return d


if __name__ == "__main__":
    OUT.parent.mkdir(parents=True, exist_ok=True)
    d = build()
    d.save(str(OUT))
    print(f"wrote {OUT}")
