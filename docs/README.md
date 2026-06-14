# Documentation index

| Doc | What it covers |
|-----|----------------|
| **PROJECT.md** | Project title, description, objectives, two-board architecture, **tools used**. |
| **hardware/HARDWARE.md** | Authoritative ATmega32A pin map, two-bus arch, relays, interrupts, ISP (J5), LED-595. **Start here to code firmware.** |
| **hardware/BOM.md** | Full components list (both boards), grouped by type. |
| **hardware/FINAL_CIRCUIT.md** | Master block diagram + every analog stage (as-built ASCII). |
| **hardware/POWER.md** | Supply tree, the regulator-burn post-mortem, buck-pre + linear-post redesign, ±15/±8/+5 details. |
| **hardware/PROTECTION_OUTPUT.md** | Output buffer, dual-rail overcurrent sense, fault interrupts, relay drivers. |
| **hardware/OSCILLOSCOPE_FRONTEND.md** | Scope input path + the two ADC paths + UART commands. |
| **hardware/SCOPE_DIGITIZER.md** | The redirect relay + dual-path digitizer (LM393 SAR + PA0) detail. |
| **design/REQUIREMENTS.md** | Feature/requirement matrix + verification status. |
| **design/output_stage_math.qmd** | Derivations for the gain/offset/centering resistor values. |
| **diagrams/** | Schematic SVGs + interactive HTML visualisations (schemdraw output target). |
| **discussions/** | Design Q&A + decision log — maintained by the **`/qna`** command. |
| **experiments/** | Bench-test notes / WIP ideas (use `TEMPLATE.md`). |

**Conventions** (IC-doc format, net naming, hard rules, schemdraw, `/qna`): see `../CLAUDE.md`.
Hardware docs are **as-built** against the verified KiCad netlist — keep them in sync when the
schematic changes.
