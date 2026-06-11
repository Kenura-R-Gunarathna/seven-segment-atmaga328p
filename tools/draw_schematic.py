#!/usr/bin/env python3
"""
Clean component-level schematics for the function-generator output stage.

Uses SchemDraw (auto-routes proper symbols — no overlapping labels).

    pip install schemdraw
    python tools/draw_schematic.py

Outputs:
    schematic_A_input.svg     DAC -> buffer -> U7 centering
    schematic_B_outputamp.svg output amp: gain bank + offset
    schematic_C_protect.svg   dual-rail overcurrent protection (+15V half)
    schematic_D_scope.svg     oscilloscope comparator front-end

Values from output_stage_math.qmd (recalculated):
    R7=R7a=10k, R_cen=32k, Rf=51k, gain bank 51k/25.5k/12.75k/10k, R_off=10k.
"""

import schemdraw
import schemdraw.elements as elm


def stage_a():
    with schemdraw.Drawing(file="schematic_A_input.svg", show=False) as d:
        d.config(fontsize=11, unit=2.4)

        dac = elm.Ic(size=(2.4, 2.0),
                     pins=[elm.IcPin(name="VOUT", side="right", slot="2/3")]
                     ).label("16-bit\nR-2R DAC\n2×74HC595\n0-5V", "center", fontsize=9)
        d += dac

        d += elm.Line().right(0.6).at(dac.VOUT)
        buf = elm.Opamp(leads=True).anchor("in2")
        d += buf
        d += elm.Line().up(0.9).at(buf.out)
        d += elm.Line().tox(buf.in1)
        d += elm.Line().toy(buf.in1).dot()
        d += elm.Line().right(0.25).to(buf.in1)
        d += elm.Dot().at(buf.out)
        d += elm.Label().at(buf.center).label("U-BUF", "bottom")

        d += elm.Line().right(0.8).at(buf.out)
        dacv = elm.Dot().label("DACV", "top")
        d += dacv

        d.push()
        d += elm.Line().down(2.2)
        d += elm.Line().right(2.2)
        d += elm.Label().label("-> SAR comparator", "right", fontsize=9)
        d.pop()

        d += elm.Resistor().right().at(dacv.center).label("R7a 10k")
        summ = elm.Dot()
        d += summ

        d.push()
        d += elm.Resistor().down().label("R_cen 32k")
        d += elm.Vss().label("-8V")
        d.pop()

        op7 = elm.Opamp(leads=True).anchor("in1")
        d += op7
        d += elm.Ground().at(op7.in2)
        d += elm.Label().at(op7.center).label("U7", "bottom")

        d += elm.Line().up(1.3).at(op7.out)
        d += elm.Resistor().tox(summ.center).label("R7 10k")
        d += elm.Line().toy(summ.center).to(summ.center)

        d += elm.Line().right(1.0).at(op7.out)
        d += elm.Dot(open=True).label("V1 ±2.5V -> filter", "right", fontsize=9)


def stage_b():
    with schemdraw.Drawing(file="schematic_B_outputamp.svg", show=False) as d:
        d.config(fontsize=11, unit=2.4)

        op = elm.Opamp(leads=True)
        d += op
        d += elm.Ground().at(op.in2)
        d += elm.Label().at(op.center).label("OUTPUT AMP", "bottom")
        summ = elm.Dot().at(op.in1)
        d += summ

        d += elm.Line().up(1.9).at(op.out)
        d += elm.Resistor().tox(op.in1).label("Rf 51k (||22pF)")
        d += elm.Line().toy(op.in1).to(op.in1)

        # gain bank bus to the left of the summing node
        d += elm.Line().left(1.0).at(summ.center)
        bus = elm.Dot()
        d += bus
        for i, lbl in enumerate(["51k ×1", "25.5k ×2", "12.75k ×4", "10k ×5.1"]):
            d.push()
            d += elm.Line().up(i * 1.0).at(bus.center)
            d += elm.Switch().left()
            d += elm.Resistor().left().label(lbl, "top", fontsize=9)
            d += elm.Line().down(i * 1.0)
            d.pop()
        d += elm.Dot(open=True).at(bus.center).label("SELSIG ±2.5V\nCD4066 U17", "left", fontsize=9)

        # offset via fixed R_off
        d += elm.Line().down(1.4).at(summ.center)
        d += elm.Resistor().left().label("R_off 10k")
        d += elm.Dot(open=True).label("OFFSET ±2.5V", "left", fontsize=9)

        d += elm.Line().right(1.2).at(op.out)
        d += elm.Dot(open=True).label("VOUT ±13V -> protect", "right", fontsize=9)


def stage_c():
    with schemdraw.Drawing(file="schematic_C_protect.svg", show=False) as d:
        d.config(fontsize=11, unit=2.0)

        # ---- +15V high-side sense + PNP ----
        d += elm.Vdd().label("+15V")
        d += elm.Resistor().down().label("Rs1 30Ω")
        vplus = elm.Dot()
        d += vplus
        d.push()
        d += elm.Line().right(1.8)
        d += elm.Dot(open=True).label("to OUTPUT AMP V+", "right", fontsize=9)
        d.pop()

        q1 = elm.BjtPnp(circle=True).anchor("emitter").at(vplus.center)
        d += q1
        d += elm.Label().at(q1.center).label("BC557", "right", fontsize=9)
        d += elm.Resistor().left().at(q1.base).label("Rb 1k", fontsize=9)
        d += elm.Resistor().down().at(q1.collector).label("Rc 10k")
        d += elm.Ground()
        nodeA = elm.Dot().at(q1.collector)
        d += nodeA

        # ---- LM393 + fault out (well to the right) ----
        d += elm.Line().right(3.0).at(nodeA.center)
        lm = elm.Ic(size=(2.8, 2.6),
                    pins=[elm.IcPin(name="A", side="left", slot="3/4"),
                          elm.IcPin(name="B", side="left", slot="1/4"),
                          elm.IcPin(name="OUT", side="right", slot="2/4")]
                    ).anchor("A").label("LM393\ndual\nopen-coll", "center", fontsize=9)
        d += lm
        d += elm.Line().right(0.9).at(lm.OUT)
        fnode = elm.Dot()
        d += fnode
        d.push()
        d += elm.Resistor().up().label("10k")
        d += elm.Vdd().label("+5V")
        d.pop()
        d += elm.Line().right(1.1).at(fnode.center)
        d += elm.Dot(open=True).label("to PD3/INT1\n(active-LOW)", "right", fontsize=9)
        d += elm.Line().left(1.2).at(lm.B)
        d += elm.Dot(open=True).label("nodeB (from -15V\nBC547 NPN mirror)", "left", fontsize=8)

        # ---- separate OUTPUT block (VOUT = op-amp output, NOT a supply node) ----
        ox = vplus.center[0] - 1.0
        oy = vplus.center[1] - 6.5
        d += elm.Dot(open=True).at((ox, oy)).label("from OUTPUT AMP\n(VOUT)", "left", fontsize=9)
        d += elm.Resistor().right().label("R_out 100-220Ω")
        cn = elm.Dot()
        d += cn
        d.push()
        d += elm.Diode().up().label("D+ 1N4148", fontsize=8)
        d += elm.Vdd().label("+15V")
        d.pop()
        d.push()
        d += elm.Diode().down().reverse().label("D- 1N4148", fontsize=8)
        d += elm.Vss().label("-15V")
        d.pop()
        d += elm.Line().right(1.2).at(cn.center)
        d += elm.Dot(open=True).label("BNC (high-Z)", "right", fontsize=9)


def stage_d():
    with schemdraw.Drawing(file="schematic_D_scope.svg", show=False) as d:
        d.config(fontsize=11, unit=2.0)

        d += elm.Dot(open=True).label("PROBE", "left")
        d += elm.Fuse().right().label("F1 100mA")
        d += elm.Resistor().right().label("R_in 10k 1W")
        pn = elm.Dot()
        d += pn
        d.push()
        d += elm.Diode().up().label("+5V clamp", "right", fontsize=8)
        d += elm.Vdd().label("+5V")
        d.pop()
        d.push()
        d += elm.Diode().down().reverse().label("GND clamp", "right", fontsize=8)
        d += elm.Ground()
        d.pop()
        d += elm.Line().right(0.8).at(pn.center)
        bias = elm.Dot()
        d += bias
        d += elm.Label().at(bias.center).label("probe node\n2.5V bias", "top", fontsize=8)

        d.push()
        d += elm.Line().down(2.0)
        d += elm.Line().right(2.0)
        d += elm.Label().label("-> PA0 (10-bit ADC)", "right", fontsize=9)
        d.pop()

        d += elm.Line().right(0.8).at(bias.center)
        comp = elm.Opamp(leads=True).anchor("in2")
        d += comp
        d += elm.Label().at(comp.center).label("NE5532-B", "bottom", fontsize=8)
        d += elm.Line().left(0.5).at(comp.in1)
        d += elm.Line().down(2.4)
        d += elm.Line().left(2.0)
        d += elm.Label().label("DACV (ref)", "left", fontsize=9)

        d += elm.Resistor().right().at(comp.out).label("820Ω")
        d += elm.Diode().right().label("1N4148")
        opto = elm.Ic(size=(2.4, 1.8),
                      pins=[elm.IcPin(name="IN", side="left", slot="2/3"),
                            elm.IcPin(name="OUT", side="right", slot="2/3")]
                      ).anchor("IN").label("6N137\nVCC+5\nVE->GND", "center", fontsize=9)
        d += opto
        d += elm.Line().right(0.8).at(opto.OUT)
        on = elm.Dot()
        d += on
        d.push()
        d += elm.Resistor().up().label("4.7k")
        d += elm.Vdd().label("+5V")
        d.pop()
        d += elm.Line().right(1.0).at(on.center)
        d += elm.Dot(open=True).label("-> PD2/INT0", "right", fontsize=9)


if __name__ == "__main__":
    stage_a()
    stage_b()
    stage_c()
    stage_d()
    print("Wrote schematic_A_input.svg, _B_outputamp.svg, _C_protect.svg, _D_scope.svg")
