# Schematic drawing — schemdraw

We draw block diagrams / explanatory schematics for the docs with **schemdraw**
(https://schemdraw.readthedocs.io/en/stable/). Scripts here render to **`../../docs/diagrams/`**
(SVG/PNG) so the docs reference a generated, regenerable image instead of hand-drawn ASCII when a
real diagram helps.

> Authoritative wiring still lives in the KiCad project (`../../../function_generator.kicad_sch`)
> and the as-built `docs/hardware/`. schemdraw is for **explanatory** figures, not the source of truth.

## Setup
```sh
python -m venv .venv && . .venv/bin/activate     # optional
pip install schemdraw[matplotlib]
```

## Usage
```sh
python tools/schematic/block_diagram.py          # writes docs/diagrams/block_diagram.svg
```
Each script: build a `schemdraw.Drawing()`, then `d.save("../../docs/diagrams/<name>.svg")`.
Copy `block_diagram.py` as the starting pattern for a new figure. Keep one script per figure,
named after its output.
