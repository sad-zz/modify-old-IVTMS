"""KiCad v9 schematic generator for IVTMS modular boards.

Outputs valid .kicad_sch files containing:
  * inline lib_symbols (so the file is self-contained — no external lib needed)
  * placed symbol instances on a grid
  * global labels for every connection (no wires drawn — labels carry the net)

Run:  python3 circuits/tools/gen_kicad.py
"""
from __future__ import annotations
import uuid as _uuid
import os
import textwrap
from dataclasses import dataclass, field
from typing import Dict, List, Tuple

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def U() -> str:
    return str(_uuid.uuid4())


# ──────────────────────────────────────────────────────────────────────────
# Library symbol definitions (compact, sufficient for KiCad to render)
# ──────────────────────────────────────────────────────────────────────────

LIB_R = '''(symbol "Device:R"
\t\t\t(pin_numbers (hide yes))
\t\t\t(pin_names (offset 0))
\t\t\t(exclude_from_sim no)
\t\t\t(in_bom yes)
\t\t\t(on_board yes)
\t\t\t(property "Reference" "R" (at 2.032 0 90))
\t\t\t(property "Value" "R" (at 0 0 90))
\t\t\t(property "Footprint" "" (at -1.778 0 90) (effects (hide yes)))
\t\t\t(property "Datasheet" "~" (at 0 0 0) (effects (hide yes)))
\t\t\t(property "Description" "Resistor" (at 0 0 0) (effects (hide yes)))
\t\t\t(symbol "R_0_1"
\t\t\t\t(rectangle (start -1.016 -2.54) (end 1.016 2.54)
\t\t\t\t\t(stroke (width 0.254) (type default))
\t\t\t\t\t(fill (type none))))
\t\t\t(symbol "R_1_1"
\t\t\t\t(pin passive line (at 0 3.81 270) (length 1.27)
\t\t\t\t\t(name "~" (effects (font (size 1.27 1.27))))
\t\t\t\t\t(number "1" (effects (font (size 1.27 1.27)))))
\t\t\t\t(pin passive line (at 0 -3.81 90) (length 1.27)
\t\t\t\t\t(name "~" (effects (font (size 1.27 1.27))))
\t\t\t\t\t(number "2" (effects (font (size 1.27 1.27)))))))'''

LIB_C = '''(symbol "Device:C"
\t\t\t(pin_numbers (hide yes))
\t\t\t(pin_names (offset 0.254))
\t\t\t(exclude_from_sim no)
\t\t\t(in_bom yes)
\t\t\t(on_board yes)
\t\t\t(property "Reference" "C" (at 0.635 2.54 0))
\t\t\t(property "Value" "C" (at 0.635 -2.54 0))
\t\t\t(property "Footprint" "" (at 0.9652 -3.81 0) (effects (hide yes)))
\t\t\t(property "Datasheet" "~" (at 0 0 0) (effects (hide yes)))
\t\t\t(property "Description" "Capacitor" (at 0 0 0) (effects (hide yes)))
\t\t\t(symbol "C_0_1"
\t\t\t\t(polyline (pts (xy -2.032 -0.762) (xy 2.032 -0.762))
\t\t\t\t\t(stroke (width 0.508) (type default))
\t\t\t\t\t(fill (type none)))
\t\t\t\t(polyline (pts (xy -2.032 0.762) (xy 2.032 0.762))
\t\t\t\t\t(stroke (width 0.508) (type default))
\t\t\t\t\t(fill (type none))))
\t\t\t(symbol "C_1_1"
\t\t\t\t(pin passive line (at 0 3.81 270) (length 2.794)
\t\t\t\t\t(name "~" (effects (font (size 1.27 1.27))))
\t\t\t\t\t(number "1" (effects (font (size 1.27 1.27)))))
\t\t\t\t(pin passive line (at 0 -3.81 90) (length 2.794)
\t\t\t\t\t(name "~" (effects (font (size 1.27 1.27))))
\t\t\t\t\t(number "2" (effects (font (size 1.27 1.27)))))))'''

LIB_CP = '''(symbol "Device:C_Polarized"
\t\t\t(pin_names (offset 0.254))
\t\t\t(exclude_from_sim no)
\t\t\t(in_bom yes)
\t\t\t(on_board yes)
\t\t\t(property "Reference" "C" (at 0.635 2.54 0))
\t\t\t(property "Value" "C_Polarized" (at 0.635 -2.54 0))
\t\t\t(property "Footprint" "" (at 0.9652 -3.81 0) (effects (hide yes)))
\t\t\t(property "Datasheet" "~" (at 0 0 0) (effects (hide yes)))
\t\t\t(symbol "C_Polarized_0_1"
\t\t\t\t(polyline (pts (xy -2.032 -0.762) (xy 2.032 -0.762))
\t\t\t\t\t(stroke (width 0.508) (type default)) (fill (type none)))
\t\t\t\t(polyline (pts (xy -2.032 1.524) (xy -1.016 0.508)) (stroke (width 0.3) (type default)) (fill (type none)))
\t\t\t\t(polyline (pts (xy -1.524 1.016) (xy -1.524 1.016)) (stroke (width 0.3) (type default)) (fill (type none)))
\t\t\t\t(arc (start 2.032 -1.524) (mid 0 -0.5572) (end -2.032 -1.524) (stroke (width 0.0) (type default)) (fill (type none))))
\t\t\t(symbol "C_Polarized_1_1"
\t\t\t\t(pin passive line (at 0 3.81 270) (length 2.794)
\t\t\t\t\t(name "~" (effects (font (size 1.27 1.27))))
\t\t\t\t\t(number "1" (effects (font (size 1.27 1.27)))))
\t\t\t\t(pin passive line (at 0 -3.81 90) (length 2.286)
\t\t\t\t\t(name "~" (effects (font (size 1.27 1.27))))
\t\t\t\t\t(number "2" (effects (font (size 1.27 1.27)))))))'''

LIB_L = '''(symbol "Device:L"
\t\t\t(pin_numbers (hide yes))
\t\t\t(pin_names (offset 1.016) (hide yes))
\t\t\t(exclude_from_sim no)
\t\t\t(in_bom yes)
\t\t\t(on_board yes)
\t\t\t(property "Reference" "L" (at -1.27 0 90))
\t\t\t(property "Value" "L" (at 1.905 0 90))
\t\t\t(property "Footprint" "" (at 0 0 0) (effects (hide yes)))
\t\t\t(property "Datasheet" "~" (at 0 0 0) (effects (hide yes)))
\t\t\t(symbol "L_0_1"
\t\t\t\t(arc (start 0 -2.54) (mid 0.6323 -1.905) (end 0 -1.27) (stroke (width 0) (type default)) (fill (type none)))
\t\t\t\t(arc (start 0 -1.27) (mid 0.6323 -0.635) (end 0 0) (stroke (width 0) (type default)) (fill (type none)))
\t\t\t\t(arc (start 0 0) (mid 0.6323 0.635) (end 0 1.27) (stroke (width 0) (type default)) (fill (type none)))
\t\t\t\t(arc (start 0 1.27) (mid 0.6323 1.905) (end 0 2.54) (stroke (width 0) (type default)) (fill (type none))))
\t\t\t(symbol "L_1_1"
\t\t\t\t(pin passive line (at 0 3.81 270) (length 1.27) (name "~" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))
\t\t\t\t(pin passive line (at 0 -3.81 90) (length 1.27) (name "~" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))))'''

LIB_D = '''(symbol "Device:D"
\t\t\t(pin_numbers (hide yes))
\t\t\t(pin_names (offset 1.016) (hide yes))
\t\t\t(exclude_from_sim no)
\t\t\t(in_bom yes)
\t\t\t(on_board yes)
\t\t\t(property "Reference" "D" (at 0 2.54 0))
\t\t\t(property "Value" "D" (at 0 -2.54 0))
\t\t\t(property "Footprint" "" (at 0 0 90) (effects (hide yes)))
\t\t\t(property "Datasheet" "~" (at 0 0 0) (effects (hide yes)))
\t\t\t(symbol "D_0_1"
\t\t\t\t(polyline (pts (xy -1.27 1.27) (xy -1.27 -1.27)) (stroke (width 0.254) (type default)) (fill (type none)))
\t\t\t\t(polyline (pts (xy 1.27 1.27) (xy 1.27 -1.27) (xy -1.27 0) (xy 1.27 1.27)) (stroke (width 0.254) (type default)) (fill (type none))))
\t\t\t(symbol "D_1_1"
\t\t\t\t(pin passive line (at -3.81 0 0) (length 2.54) (name "K" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))
\t\t\t\t(pin passive line (at 3.81 0 180) (length 2.54) (name "A" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))))'''

LIB_LED = '''(symbol "Device:LED"
\t\t\t(pin_numbers (hide yes))
\t\t\t(pin_names (offset 1.016) (hide yes))
\t\t\t(exclude_from_sim no)
\t\t\t(in_bom yes)
\t\t\t(on_board yes)
\t\t\t(property "Reference" "D" (at 0 2.54 0))
\t\t\t(property "Value" "LED" (at 0 -2.54 0))
\t\t\t(property "Footprint" "" (at 0 0 0) (effects (hide yes)))
\t\t\t(property "Datasheet" "~" (at 0 0 0) (effects (hide yes)))
\t\t\t(symbol "LED_0_1"
\t\t\t\t(polyline (pts (xy -1.27 -1.27) (xy -1.27 1.27)) (stroke (width 0.254) (type default)) (fill (type none)))
\t\t\t\t(polyline (pts (xy -1.27 0) (xy 1.27 -1.27) (xy 1.27 1.27) (xy -1.27 0)) (stroke (width 0.254) (type default)) (fill (type outline))))
\t\t\t(symbol "LED_1_1"
\t\t\t\t(pin passive line (at -3.81 0 0) (length 2.54) (name "K" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))
\t\t\t\t(pin passive line (at 3.81 0 180) (length 2.54) (name "A" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))))'''

LIB_QNMOS = '''(symbol "Device:Q_NMOS_GSD"
\t\t\t(pin_names (offset 0))
\t\t\t(exclude_from_sim no)
\t\t\t(in_bom yes)
\t\t\t(on_board yes)
\t\t\t(property "Reference" "Q" (at 5.08 1.27 0))
\t\t\t(property "Value" "Q_NMOS_GSD" (at 5.08 -1.27 0))
\t\t\t(property "Footprint" "" (at 0 0 0) (effects (hide yes)))
\t\t\t(property "Datasheet" "~" (at 0 0 0) (effects (hide yes)))
\t\t\t(symbol "Q_NMOS_GSD_0_1"
\t\t\t\t(polyline (pts (xy 0.508 0) (xy 1.778 0)) (stroke (width 0) (type default)) (fill (type none)))
\t\t\t\t(polyline (pts (xy 1.27 -1.778) (xy 1.27 1.778)) (stroke (width 0.508) (type default)) (fill (type none)))
\t\t\t\t(polyline (pts (xy 0 -2.54) (xy 0 0) (xy 2.54 0)) (stroke (width 0) (type default)) (fill (type none))))
\t\t\t(symbol "Q_NMOS_GSD_1_1"
\t\t\t\t(pin input line (at -5.08 2.54 0) (length 5.08) (name "G" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))
\t\t\t\t(pin passive line (at 2.54 -2.54 180) (length 1.27) (name "S" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))
\t\t\t\t(pin passive line (at 2.54 2.54 180) (length 1.27) (name "D" (effects (font (size 1.27 1.27)))) (number "3" (effects (font (size 1.27 1.27)))))))'''

LIB_QPMOS = '''(symbol "Device:Q_PMOS_GSD"
\t\t\t(pin_names (offset 0))
\t\t\t(exclude_from_sim no)
\t\t\t(in_bom yes)
\t\t\t(on_board yes)
\t\t\t(property "Reference" "Q" (at 5.08 1.27 0))
\t\t\t(property "Value" "Q_PMOS_GSD" (at 5.08 -1.27 0))
\t\t\t(property "Footprint" "" (at 0 0 0) (effects (hide yes)))
\t\t\t(property "Datasheet" "~" (at 0 0 0) (effects (hide yes)))
\t\t\t(symbol "Q_PMOS_GSD_0_1"
\t\t\t\t(polyline (pts (xy 0.508 0) (xy 1.778 0)) (stroke (width 0) (type default)) (fill (type none)))
\t\t\t\t(polyline (pts (xy 1.27 -1.778) (xy 1.27 1.778)) (stroke (width 0.508) (type default)) (fill (type none)))
\t\t\t\t(polyline (pts (xy 0 -2.54) (xy 0 0) (xy 2.54 0)) (stroke (width 0) (type default)) (fill (type none)))
\t\t\t\t(circle (center 0 0) (radius 0.508) (stroke (width 0) (type default)) (fill (type none))))
\t\t\t(symbol "Q_PMOS_GSD_1_1"
\t\t\t\t(pin input line (at -5.08 2.54 0) (length 5.08) (name "G" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))
\t\t\t\t(pin passive line (at 2.54 -2.54 180) (length 1.27) (name "S" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))
\t\t\t\t(pin passive line (at 2.54 2.54 180) (length 1.27) (name "D" (effects (font (size 1.27 1.27)))) (number "3" (effects (font (size 1.27 1.27)))))))'''

LIB_CRYSTAL = '''(symbol "Device:Crystal"
\t\t\t(pin_names (offset 1.016) (hide yes))
\t\t\t(exclude_from_sim no)
\t\t\t(in_bom yes)
\t\t\t(on_board yes)
\t\t\t(property "Reference" "Y" (at 0 -3.81 0))
\t\t\t(property "Value" "Crystal" (at 0 3.81 0))
\t\t\t(property "Footprint" "" (at 0 0 0) (effects (hide yes)))
\t\t\t(property "Datasheet" "~" (at 0 0 0) (effects (hide yes)))
\t\t\t(symbol "Crystal_0_1"
\t\t\t\t(rectangle (start -1.27 -1.524) (end 1.27 1.524) (stroke (width 0.254) (type default)) (fill (type none)))
\t\t\t\t(polyline (pts (xy -1.778 -2.032) (xy -1.778 2.032)) (stroke (width 0.508) (type default)) (fill (type none)))
\t\t\t\t(polyline (pts (xy 1.778 -2.032) (xy 1.778 2.032)) (stroke (width 0.508) (type default)) (fill (type none))))
\t\t\t(symbol "Crystal_1_1"
\t\t\t\t(pin passive line (at -3.81 0 0) (length 2.032) (name "1" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))
\t\t\t\t(pin passive line (at 3.81 0 180) (length 2.032) (name "2" (effects (font (size 1.27 1.27)))) (number "2" (effects (font (size 1.27 1.27)))))))'''


def make_generic_ic_lib(libname: str, pins: List[Tuple[str, str]], width: int = 12, label: str = "") -> str:
    """Build a generic rectangle-shaped IC symbol with named pins.

    `pins` is a list of (number, name). They're placed alternating L/R going down.
    libname:  e.g. "IVTMS:LM339"
    """
    label = label or libname.split(":")[-1]
    n = len(pins)
    half = (n + 1) // 2
    height = max(half * 2.54, 5.08)
    # Place pins: first half on left, second half on right (top-to-bottom)
    pin_blocks = []
    # Left side
    for i, (num, name) in enumerate(pins[:half]):
        y = (height / 2) - 2.54 - i * 2.54
        pin_blocks.append(
            f'\t\t\t\t(pin passive line (at {-width/2 - 2.54} {y} 0) (length 2.54) '
            f'(name "{name}" (effects (font (size 1.27 1.27)))) '
            f'(number "{num}" (effects (font (size 1.27 1.27)))))'
        )
    # Right side
    for i, (num, name) in enumerate(pins[half:]):
        y = (height / 2) - 2.54 - i * 2.54
        pin_blocks.append(
            f'\t\t\t\t(pin passive line (at {width/2 + 2.54} {y} 180) (length 2.54) '
            f'(name "{name}" (effects (font (size 1.27 1.27)))) '
            f'(number "{num}" (effects (font (size 1.27 1.27)))))'
        )
    body = (
        f'(symbol "{libname}"\n'
        f'\t\t\t(pin_names (offset 1.016))\n'
        f'\t\t\t(exclude_from_sim no)\n'
        f'\t\t\t(in_bom yes)\n'
        f'\t\t\t(on_board yes)\n'
        f'\t\t\t(property "Reference" "U" (at 0 {height/2 + 2.54} 0))\n'
        f'\t\t\t(property "Value" "{label}" (at 0 {-(height/2 + 2.54)} 0))\n'
        f'\t\t\t(property "Footprint" "" (at 0 0 0) (effects (hide yes)))\n'
        f'\t\t\t(property "Datasheet" "~" (at 0 0 0) (effects (hide yes)))\n'
        f'\t\t\t(symbol "{libname.split(":")[-1]}_0_1"\n'
        f'\t\t\t\t(rectangle (start {-width/2} {-height/2}) (end {width/2} {height/2}) '
        f'(stroke (width 0.254) (type default)) (fill (type background))))\n'
        f'\t\t\t(symbol "{libname.split(":")[-1]}_1_1"\n'
        + "\n".join(pin_blocks) + "))"
    )
    return body


def make_pinheader_lib(libname: str, npins: int) -> str:
    """A simple 1-row pin header symbol, pins along the right side."""
    height = npins * 2.54
    pin_blocks = []
    for i in range(npins):
        y = (height / 2) - 1.27 - i * 2.54
        pin_blocks.append(
            f'\t\t\t\t(pin passive line (at -5.08 {y} 0) (length 2.54) '
            f'(name "Pin_{i+1}" (effects (font (size 1.27 1.27)))) '
            f'(number "{i+1}" (effects (font (size 1.27 1.27)))))'
        )
    return (
        f'(symbol "{libname}"\n'
        f'\t\t\t(pin_names (offset 1.016) (hide yes))\n'
        f'\t\t\t(exclude_from_sim no)\n'
        f'\t\t\t(in_bom yes)\n'
        f'\t\t\t(on_board yes)\n'
        f'\t\t\t(property "Reference" "J" (at 0 {height/2 + 2.54} 0))\n'
        f'\t\t\t(property "Value" "Conn_1x{npins:02d}" (at 0 {-(height/2 + 2.54)} 0))\n'
        f'\t\t\t(property "Footprint" "" (at 0 0 0) (effects (hide yes)))\n'
        f'\t\t\t(property "Datasheet" "~" (at 0 0 0) (effects (hide yes)))\n'
        f'\t\t\t(symbol "{libname.split(":")[-1]}_0_1"\n'
        f'\t\t\t\t(rectangle (start -2.54 {-height/2}) (end 0 {height/2}) '
        f'(stroke (width 0.254) (type default)) (fill (type background))))\n'
        f'\t\t\t(symbol "{libname.split(":")[-1]}_1_1"\n'
        + "\n".join(pin_blocks) + "))"
    )


def make_pinheader2x_lib(libname: str, nrows: int) -> str:
    """2-row pin header symbol; odd pins left, even pins right."""
    height = nrows * 2.54
    pin_blocks = []
    for i in range(nrows):
        y = (height / 2) - 1.27 - i * 2.54
        odd = 2 * i + 1
        even = 2 * i + 2
        pin_blocks.append(
            f'\t\t\t\t(pin passive line (at -5.08 {y} 0) (length 2.54) '
            f'(name "P{odd}" (effects (font (size 1.27 1.27)))) '
            f'(number "{odd}" (effects (font (size 1.27 1.27)))))'
        )
        pin_blocks.append(
            f'\t\t\t\t(pin passive line (at 5.08 {y} 180) (length 2.54) '
            f'(name "P{even}" (effects (font (size 1.27 1.27)))) '
            f'(number "{even}" (effects (font (size 1.27 1.27)))))'
        )
    return (
        f'(symbol "{libname}"\n'
        f'\t\t\t(pin_names (offset 1.016) (hide yes))\n'
        f'\t\t\t(exclude_from_sim no)\n'
        f'\t\t\t(in_bom yes)\n'
        f'\t\t\t(on_board yes)\n'
        f'\t\t\t(property "Reference" "J" (at 0 {height/2 + 2.54} 0))\n'
        f'\t\t\t(property "Value" "Conn_2x{nrows:02d}" (at 0 {-(height/2 + 2.54)} 0))\n'
        f'\t\t\t(property "Footprint" "" (at 0 0 0) (effects (hide yes)))\n'
        f'\t\t\t(property "Datasheet" "~" (at 0 0 0) (effects (hide yes)))\n'
        f'\t\t\t(symbol "{libname.split(":")[-1]}_0_1"\n'
        f'\t\t\t\t(rectangle (start -2.54 {-height/2}) (end 2.54 {height/2}) '
        f'(stroke (width 0.254) (type default)) (fill (type background))))\n'
        f'\t\t\t(symbol "{libname.split(":")[-1]}_1_1"\n'
        + "\n".join(pin_blocks) + "))"
    )


# ──────────────────────────────────────────────────────────────────────────
# Schematic builder
# ──────────────────────────────────────────────────────────────────────────

@dataclass
class Sch:
    title: str
    paper: str = "A3"
    rev: str = "1.0"
    company: str = "IVTMS"
    sch_uuid: str = field(default_factory=U)
    libs: Dict[str, str] = field(default_factory=dict)         # libname -> lib body
    instances: List[str] = field(default_factory=list)         # symbol instance s-exprs
    labels: List[str] = field(default_factory=list)            # global labels

    def add_lib(self, libname: str, body: str):
        if libname not in self.libs:
            self.libs[libname] = body

    def place(self, lib_id: str, ref: str, value: str, footprint: str,
              x: float, y: float, rotation: int = 0,
              pin_nets: Dict[str, str] = None):
        """Place a symbol instance at (x, y) with optional global-label net stubs.

        pin_nets maps pin number -> net name. For each, we'll emit a global label
        at the pin's effective location (assumed +0/-0 offsets matching the
        rectangle origin — the user can drag them once opened).
        For simplicity, labels are placed near the symbol with the net name."""
        pin_nets = pin_nets or {}
        u = U()
        body = (
            f'\t(symbol\n'
            f'\t\t(lib_id "{lib_id}")\n'
            f'\t\t(at {x} {y} {rotation})\n'
            f'\t\t(unit 1)\n'
            f'\t\t(exclude_from_sim no)\n'
            f'\t\t(in_bom yes)\n'
            f'\t\t(on_board yes)\n'
            f'\t\t(dnp no)\n'
            f'\t\t(uuid "{u}")\n'
            f'\t\t(property "Reference" "{ref}" (at {x + 2.54} {y - 2.54} 0))\n'
            f'\t\t(property "Value" "{value}" (at {x + 2.54} {y + 2.54} 0))\n'
            f'\t\t(property "Footprint" "{footprint}" (at {x} {y} 0) (effects (hide yes)))\n'
            f'\t\t(property "Datasheet" "~" (at {x} {y} 0) (effects (hide yes)))\n'
            f'\t\t(instances\n'
            f'\t\t\t(project "board"\n'
            f'\t\t\t\t(path "/{self.sch_uuid}"\n'
            f'\t\t\t\t\t(reference "{ref}")\n'
            f'\t\t\t\t\t(unit 1)))))'
        )
        self.instances.append(body)
        # Drop a label *near* the component for each net assignment
        for pin, net in pin_nets.items():
            self.label(net, x + 5.08, y + (int(pin) - 1) * 2.54 - 5.08)

    def label(self, net: str, x: float, y: float, rotation: int = 0):
        u = U()
        body = (
            f'\t(global_label "{net}"\n'
            f'\t\t(shape input)\n'
            f'\t\t(at {x} {y} {rotation})\n'
            f'\t\t(fields_autoplaced yes)\n'
            f'\t\t(effects (font (size 1.27 1.27)) (justify left))\n'
            f'\t\t(uuid "{u}"))'
        )
        self.labels.append(body)

    def text(self, txt: str, x: float, y: float):
        u = U()
        # Escape quotes in text
        safe = txt.replace('"', '\\"')
        self.instances.append(
            f'\t(text "{safe}"\n'
            f'\t\t(at {x} {y} 0)\n'
            f'\t\t(effects (font (size 1.5 1.5)) (justify left))\n'
            f'\t\t(uuid "{u}"))'
        )

    def render(self) -> str:
        libs_block = "\n\t\t".join(self.libs.values())
        instances_block = "\n".join(self.instances)
        labels_block = "\n".join(self.labels)
        return (
            f'(kicad_sch\n'
            f'\t(version 20250114)\n'
            f'\t(generator "eeschema")\n'
            f'\t(generator_version "9.0")\n'
            f'\t(uuid "{self.sch_uuid}")\n'
            f'\t(paper "{self.paper}")\n'
            f'\t(title_block\n'
            f'\t\t(title "{self.title}")\n'
            f'\t\t(date "2026-05-10")\n'
            f'\t\t(rev "{self.rev}")\n'
            f'\t\t(company "{self.company}"))\n'
            f'\t(lib_symbols\n'
            f'\t\t{libs_block})\n'
            f'{instances_block}\n'
            f'{labels_block}\n'
            f'\t(sheet_instances\n'
            f'\t\t(path "/" (page "1")))\n'
            f'\t(embedded_fonts no)\n'
            f')\n'
        )


# ──────────────────────────────────────────────────────────────────────────
# Common library setup
# ──────────────────────────────────────────────────────────────────────────

def add_passives(sch: Sch):
    sch.add_lib("Device:R", LIB_R)
    sch.add_lib("Device:C", LIB_C)
    sch.add_lib("Device:C_Polarized", LIB_CP)
    sch.add_lib("Device:L", LIB_L)
    sch.add_lib("Device:D", LIB_D)
    sch.add_lib("Device:LED", LIB_LED)
    sch.add_lib("Device:Q_NMOS_GSD", LIB_QNMOS)
    sch.add_lib("Device:Q_PMOS_GSD", LIB_QPMOS)
    sch.add_lib("Device:Crystal", LIB_CRYSTAL)


# ──────────────────────────────────────────────────────────────────────────
# Board generators
# ──────────────────────────────────────────────────────────────────────────

def gen_psu():
    sch = Sch("PSU — IVTMS 12V to 5V/2A", "A3")
    add_passives(sch)
    # LM2596 5V fixed buck regulator (5-pin TO-263)
    LM2596 = make_generic_ic_lib("IVTMS:LM2596S-5",
        [("1", "Vin"), ("2", "OUT"), ("3", "GND"), ("4", "FB"), ("5", "ON_OFF")],
        width=12, label="LM2596S-5.0")
    sch.add_lib("IVTMS:LM2596S-5", LM2596)
    # 2x8 backplane bus
    JBUS = make_pinheader2x_lib("IVTMS:Conn_2x08", 8)
    sch.add_lib("IVTMS:Conn_2x08", JBUS)
    JIN = make_pinheader_lib("IVTMS:Conn_1x02", 2)
    sch.add_lib("IVTMS:Conn_1x02", JIN)

    # Place components on a grid; using global labels for nets.
    # Grid origin around (50, 50) mm. KiCad units are mm.
    sch.text("PSU 12V→5V — IVTMS Power Module", 50, 30)
    sch.text("Input: 12V via J1; Buck: U1 LM2596S-5.0; Output: 5V to backplane", 50, 35)

    # J1 — input jack (use a 1x2 header for simplicity)
    sch.place("IVTMS:Conn_1x02", "J1", "DC_Jack_12V",
              "Connector_BarrelJack:BarrelJack_Horizontal", 50, 80,
              pin_nets={"1": "+12V_RAW", "2": "GND"})

    # F1 polyfuse (modeled as R)
    sch.place("Device:R", "F1", "1A_Polyfuse",
              "Fuse:Fuse_Bourns_MF-RX_TH", 70, 80, rotation=90,
              pin_nets={"1": "+12V_RAW", "2": "+12V_FUSED"})

    # D1 reverse-polarity protection
    sch.place("Device:D", "D1", "SS24",
              "Diode_SMD:D_SMA", 90, 80,
              pin_nets={"1": "+12V_PROT", "2": "+12V_FUSED"})

    # TVS1
    sch.place("Device:D", "TVS1", "SMAJ24CA",
              "Diode_SMD:D_SMA", 110, 90, rotation=90,
              pin_nets={"1": "+12V_PROT", "2": "GND"})

    # C1 input bulk
    sch.place("Device:C_Polarized", "C1", "470uF/25V",
              "Capacitor_THT:CP_Radial_D10.0mm_P5.00mm", 110, 80,
              pin_nets={"1": "+12V_PROT", "2": "GND"})

    # L1 ferrite bead between C1 and U1.Vin
    sch.place("Device:L", "L1", "FB_600R/2A",
              "Inductor_SMD:L_1210_3225Metric", 130, 80,
              pin_nets={"1": "+12V_PROT", "2": "+12V_BUS"})

    # C2 pre-reg
    sch.place("Device:C", "C2", "100nF",
              "Capacitor_SMD:C_0805_2012Metric", 150, 80,
              pin_nets={"1": "+12V_BUS", "2": "GND"})

    # U1 LM2596S-5.0
    sch.place("IVTMS:LM2596S-5", "U1", "LM2596S-5.0",
              "Package_TO_SOT_SMD:TO-263-5_TabPin3", 170, 100,
              pin_nets={
                  "1": "+12V_BUS",   # Vin
                  "2": "SW_NODE",    # Output (switch node)
                  "3": "GND",
                  "4": "+5V",        # FB tied to output for fixed version
                  "5": "GND",        # ON/OFF active-low → GND for always-on
              })

    # D2 freewheel: cathode to SW_NODE, anode to GND
    sch.place("Device:D", "D2", "SS34",
              "Diode_SMD:D_SMA", 195, 110, rotation=90,
              pin_nets={"1": "SW_NODE", "2": "GND"})

    # L2 output inductor
    sch.place("Device:L", "L2", "33uH/3A",
              "Inductor_SMD:L_Bourns_SRR1280", 215, 100,
              pin_nets={"1": "SW_NODE", "2": "+5V"})

    # C3 output bulk
    sch.place("Device:C_Polarized", "C3", "220uF/10V",
              "Capacitor_SMD:CP_Elec_8x5.4", 235, 100,
              pin_nets={"1": "+5V", "2": "GND"})

    # C4 HF decouple
    sch.place("Device:C", "C4", "10uF",
              "Capacitor_SMD:C_0805_2012Metric", 250, 100,
              pin_nets={"1": "+5V", "2": "GND"})

    # LED1 power indicator (with R1)
    sch.place("Device:R", "R1", "1K",
              "Resistor_SMD:R_0805_2012Metric", 50, 130,
              pin_nets={"1": "+12V_BUS", "2": "LED1_A"})
    sch.place("Device:LED", "LED1", "Green",
              "LED_SMD:LED_0805_2012Metric", 50, 145,
              pin_nets={"1": "GND", "2": "LED1_A"})

    # LED2 status (driven by MCU)
    sch.place("Device:R", "R2", "1K",
              "Resistor_SMD:R_0805_2012Metric", 70, 130,
              pin_nets={"1": "LED_STATUS", "2": "LED2_A"})
    sch.place("Device:LED", "LED2", "Red",
              "LED_SMD:LED_0805_2012Metric", 70, 145,
              pin_nets={"1": "GND", "2": "LED2_A"})

    # J_BUS — 2x8 backplane
    sch.place("IVTMS:Conn_2x08", "J_BUS", "Backplane",
              "Connector_PinHeader_2.54mm:PinHeader_2x08_P2.54mm_Vertical", 280, 100,
              pin_nets={
                  "1": "+12V_BUS", "2": "+12V_BUS",
                  "3": "GND",      "4": "GND",
                  "5": "+5V",      "6": "+5V",
                  "7": "GND",      "8": "MUX_A",
                  "9": "MUX_B",   "10": "MUX_C",
                  "11":"MUX_INH", "12": "LOOP_IO_COM_U51",
                  "13":"LOOP_IO_COM_U52","14":"CMP_OUT",
                  "15":"LED_STATUS","16":"RESERVED"
              })

    return sch.render()


def gen_loop():
    sch = Sch("LOOP Detector — IVTMS 4-channel", "A2")
    add_passives(sch)
    # LM339 quad comparator (14-pin SOIC)
    LM339 = make_generic_ic_lib("IVTMS:LM339",
        [("1","OUT_B"),("2","OUT_A"),("3","VCC"),("4","IN-_A"),
         ("5","IN+_A"),("6","IN-_B"),("7","IN+_B"),
         ("8","IN-_C"),("9","IN+_C"),("10","IN+_D"),("11","IN-_D"),
         ("12","GND"),("13","OUT_D"),("14","OUT_C")],
        width=14, label="LM339")
    sch.add_lib("IVTMS:LM339", LM339)
    # HCF4051 8:1 mux (16-pin)
    HCF4051 = make_generic_ic_lib("IVTMS:HCF4051BEY",
        [("1","IO4"),("2","IO6"),("3","IO_COM"),("4","IO7"),
         ("5","IO5"),("6","INH"),("7","GND"),("8","GND2"),
         ("9","C"),("10","B"),("11","A"),("12","IO3"),
         ("13","IO0"),("14","IO1"),("15","IO2"),("16","VCC")],
        width=14, label="HCF4051BEY")
    sch.add_lib("IVTMS:HCF4051BEY", HCF4051)
    # 1x8 input/output headers
    H8 = make_pinheader_lib("IVTMS:Conn_1x08", 8)
    sch.add_lib("IVTMS:Conn_1x08", H8)
    # 2x8 backplane
    JBUS = make_pinheader2x_lib("IVTMS:Conn_2x08", 8)
    sch.add_lib("IVTMS:Conn_2x08", JBUS)

    sch.text("LOOP Detector — 4 channels reverse-engineered from main loop pcb3.SchDoc", 50, 30)
    sch.text("Per-ch: 2N7000 protect / LM339 quarter / 2SJ196(BSS92) load switch", 50, 35)

    # Per-channel parameters: (ch, x_base, lm339_pins (out, in+, in-))
    channels = [
        (11,  60, {"out":"2", "ip":"5",  "im":"4"}),
        (12, 130, {"out":"1", "ip":"7",  "im":"6"}),
        (13, 200, {"out":"14","ip":"9",  "im":"8"}),
        (14, 270, {"out":"13","ip":"10", "im":"11"}),
    ]
    for ch, xb, lm in channels:
        s = f"{ch:02d}"
        # Header text
        sch.text(f"Channel {ch}", xb, 55)
        # Input filter R##A (0R)
        sch.place("Device:R", f"R{s}A", "0R",
                  "Resistor_SMD:R_0805_2012Metric", xb, 70, rotation=90,
                  pin_nets={"1": f"LOOP_IN_RAW_{ch}", "2": f"LOOP_FILT_{ch}"})
        # C##B (47nF to GND on input side)
        sch.place("Device:C", f"C{s}B", "47nF",
                  "Capacitor_SMD:C_0805_2012Metric", xb + 10, 70,
                  pin_nets={"1": f"LOOP_IN_RAW_{ch}", "2": "GND"})
        # 2N7000 protection trio Q##A/B/C (protection clamps)
        sch.place("Device:Q_NMOS_GSD", f"Q{s}A", "2N7000",
                  "Package_TO_SOT_SMD:SOT-23", xb, 90,
                  pin_nets={"1": f"LOOP_FILT_{ch}", "2": "GND", "3": f"LOOP_IN_{ch}"})
        sch.place("Device:Q_NMOS_GSD", f"Q{s}B", "2N7000",
                  "Package_TO_SOT_SMD:SOT-23", xb + 15, 90,
                  pin_nets={"1": f"LOOP_IN_{ch}", "2": "GND", "3": f"LOOP_IN_{ch}"})
        sch.place("Device:Q_NMOS_GSD", f"Q{s}C", "2N7000",
                  "Package_TO_SOT_SMD:SOT-23", xb + 30, 90,
                  pin_nets={"1": f"LOOP_IN_{ch}", "2": "GND", "3": f"LOOP_IN_{ch}"})
        # C##A (47nF additional)
        sch.place("Device:C", f"C{s}A", "47nF",
                  "Capacitor_SMD:C_0805_2012Metric", xb + 45, 90,
                  pin_nets={"1": f"LOOP_IN_{ch}", "2": "GND"})
        # D##A (1N5819 schottky clamp, anode to GND, cathode to LOOP_IN)
        sch.place("Device:D", f"D{s}A", "1N5819",
                  "Diode_SMD:D_SOD-123", xb + 55, 90, rotation=90,
                  pin_nets={"1": f"LOOP_IN_{ch}", "2": "GND"})
        # Resistor divider
        sch.place("Device:R", f"R{s}C", "330K",
                  "Resistor_SMD:R_0805_2012Metric", xb, 110, rotation=90,
                  pin_nets={"1": "VCC", "2": f"LOOP_IN_{ch}"})
        sch.place("Device:R", f"R{s}B", "10K",
                  "Resistor_SMD:R_0805_2012Metric", xb + 15, 110, rotation=90,
                  pin_nets={"1": f"LOOP_IN_{ch}", "2": f"CMPIN_{ch}"})
        sch.place("Device:R", f"R{s}D", "3.3K",
                  "Resistor_SMD:R_0805_2012Metric", xb + 30, 110, rotation=90,
                  pin_nets={"1": f"CMPIN_{ch}", "2": "GND"})
        # R##E (1K gate series for Q##D)
        sch.place("Device:R", f"R{s}E", "1K",
                  "Resistor_SMD:R_0805_2012Metric", xb + 45, 130, rotation=90,
                  pin_nets={"1": f"LM339_OUT_{ch}", "2": f"PMOS_GATE_{ch}"})
        # Q##D (BSS92 P-channel)
        sch.place("Device:Q_PMOS_GSD", f"Q{s}D", "BSS92",
                  "Package_TO_SOT_SMD:SOT-23", xb + 30, 150,
                  pin_nets={"1": f"PMOS_GATE_{ch}", "2": "VCC", "3": f"LOOP_OUT_{ch}"})

    # LM339 (single placement, 4 channels share)
    sch.place("IVTMS:LM339", "U50", "LM339",
              "Package_SO:SOIC-14_3.9x8.7mm_P1.27mm", 50, 200,
              pin_nets={
                  "1": "LM339_OUT_12", "2": "LM339_OUT_11",
                  "3": "VCC",          "4": "CMPIN_11",
                  "5": "VTHR",         "6": "CMPIN_12",
                  "7": "VTHR",         "8": "CMPIN_13",
                  "9": "VTHR",        "10": "VTHR",
                 "11": "CMPIN_14",   "12": "GND",
                 "13": "LM339_OUT_14","14":"LM339_OUT_13",
              })
    # LM339 bypass cap
    sch.place("Device:C", "C_BYP_50", "100nF",
              "Capacitor_SMD:C_0805_2012Metric", 75, 200,
              pin_nets={"1": "VCC", "2": "GND"})
    # Threshold reference net (VTHR) — tied to a divider; for now expose as label
    sch.text("VTHR = mid-rail reference (from external divider, e.g. 2x10K to VCC/GND)", 50, 220)

    # U51 + U52 muxes
    sch.place("IVTMS:HCF4051BEY", "U51", "HCF4051BEY",
              "Package_SO:SOIC-16_3.9x9.9mm_P1.27mm", 130, 200,
              pin_nets={
                  "1": "LOOP_IN_11", "2": "LOOP_IN_13",
                  "3": "LOOP_IO_COM_U51", "4": "LOOP_IN_14",
                  "5": "LOOP_IN_12", "6": "MUX_INH",
                  "7": "GND", "8": "GND",
                  "9": "MUX_C", "10": "MUX_B", "11": "MUX_A",
                 "12": "LOOP_OUT_14","13": "LOOP_OUT_11",
                 "14": "LOOP_OUT_12","15": "LOOP_OUT_13",
                 "16": "VCC",
              })
    sch.place("Device:C", "C_BYP_51", "100nF",
              "Capacitor_SMD:C_0805_2012Metric", 155, 200,
              pin_nets={"1": "VCC", "2": "GND"})

    sch.place("IVTMS:HCF4051BEY", "U52", "HCF4051BEY",
              "Package_SO:SOIC-16_3.9x9.9mm_P1.27mm", 200, 200,
              pin_nets={
                  "1": "LOOP_IN_11", "2": "LOOP_IN_13",
                  "3": "LOOP_IO_COM_U52", "4": "LOOP_IN_14",
                  "5": "LOOP_IN_12", "6": "MUX_INH",
                  "7": "GND", "8": "GND",
                  "9": "MUX_C", "10": "MUX_B", "11": "MUX_A",
                 "12": "LOOP_OUT_14","13": "LOOP_OUT_11",
                 "14": "LOOP_OUT_12","15": "LOOP_OUT_13",
                 "16": "VCC",
              })
    sch.place("Device:C", "C_BYP_52", "100nF",
              "Capacitor_SMD:C_0805_2012Metric", 225, 200,
              pin_nets={"1": "VCC", "2": "GND"})

    # Headers: P1 (inputs) and pl (outputs)
    sch.place("IVTMS:Conn_1x08", "P1", "Loop_Input",
              "Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical", 50, 250,
              pin_nets={
                  "1": "LOOP_IN_RAW_11", "2": "LOOP_IN_RAW_11_R",
                  "3": "LOOP_IN_RAW_12", "4": "LOOP_IN_RAW_12_R",
                  "5": "LOOP_IN_RAW_13", "6": "LOOP_IN_RAW_13_R",
                  "7": "LOOP_IN_RAW_14", "8": "LOOP_IN_RAW_14_R",
              })
    sch.place("IVTMS:Conn_1x08", "PL", "Loop_Output",
              "Connector_PinHeader_2.54mm:PinHeader_1x08_P2.54mm_Vertical", 130, 250,
              pin_nets={
                  "1": "LOOP_OUT_11", "2": "LOOP_OUT_12",
                  "3": "LOOP_OUT_13", "4": "LOOP_OUT_14",
                  "5": "RSV5", "6": "RSV6", "7": "RSV7", "8": "RSV8",
              })
    # All LOOP_IN_RAW_xx_R lines tie to GND (loop second wire is loop return)
    sch.text("Note: P1 even pins (loop returns) tie to GND on this board.", 50, 275)

    # Backplane bus
    sch.place("IVTMS:Conn_2x08", "J_BUS", "Backplane",
              "Connector_PinHeader_2.54mm:PinHeader_2x08_P2.54mm_Vertical", 220, 250,
              pin_nets={
                  "1": "+12V_BUS", "2": "+12V_BUS",
                  "3": "GND", "4": "GND",
                  "5": "VCC", "6": "VCC",
                  "7": "GND", "8": "MUX_A",
                  "9": "MUX_B", "10": "MUX_C",
                 "11": "MUX_INH", "12": "LOOP_IO_COM_U51",
                 "13": "LOOP_IO_COM_U52", "14": "CMP_OUT",
                 "15": "LED_STATUS", "16": "RESERVED",
              })
    return sch.render()


def gen_mcu_a():
    sch = Sch("MCU-A — dsPIC30F4011 + SIM900", "A3")
    add_passives(sch)
    DSPIC = make_generic_ic_lib("IVTMS:dsPIC30F4011",
        [("1","RB6/MDM_STAT"),("2","RB5/MMC_ERR"),("3","RB4"),("4","RB3"),
         ("5","RB2/MODEM_PWR"),("6","RB1"),("7","RB0/AN0"),("8","CN1"),
         ("9","RC15/MUX_C"),("10","RC14/MUX_B"),("11","MCLR"),("12","VSS"),
         ("13","AN0/LOOP_COM1"),("14","AN1/LOOP_COM2"),("15","AN2/CMP_OUT"),("16","RC13/MUX_A"),
         ("17","RF0/RTC_CS"),("18","RF1/MMC_CS"),("19","SDO1/MOSI"),("20","RD0/MUX_INH"),
         ("21","U2RX"),("22","U2TX"),("23","SCK1"),("24","SDI1/MISO"),
         ("25","RE0/onloop0"),("26","RE1/onloop1"),("27","RE2/onloop2"),("28","RE3/onloop3"),
         ("29","RE4/STATUS"),("30","RE5/CONN"),("31","OSC1"),("32","OSC2"),
         ("33","U1RX"),("34","U1TX"),("35","AVDD"),("36","AVSS"),
         ("37","RB7/PWRKEY"),("38","PGC"),("39","PGD"),("40","VDD")],
        width=20, label="dsPIC30F4011")
    sch.add_lib("IVTMS:dsPIC30F4011", DSPIC)
    SIM900 = make_generic_ic_lib("IVTMS:SIM900_Module",
        [("1","Vbat"),("2","Vbat"),("3","GND"),("4","GND"),
         ("5","PWRKEY"),("6","STATUS"),("7","NETLIGHT"),("8","UART_TXD"),
         ("9","UART_RXD"),("10","SIM_VDD"),("11","SIM_RST"),("12","SIM_CLK"),
         ("13","SIM_DATA"),("14","SIM_GND"),("15","ANT"),("16","RI")],
        width=14, label="SIM900_Module")
    sch.add_lib("IVTMS:SIM900_Module", SIM900)
    DS1305 = make_generic_ic_lib("IVTMS:DS1305",
        [("1","X1"),("2","X2"),("3","Vbat"),("4","GND"),
         ("5","INT1"),("6","SDO"),("7","SDI"),("8","SCLK"),
         ("9","CE"),("10","INT0"),("11","Vcc2"),("12","Vcc1"),
         ("13","NC"),("14","NC"),("15","NC"),("16","NC")],
        width=12, label="DS1305")
    sch.add_lib("IVTMS:DS1305", DS1305)
    BUCK = make_generic_ic_lib("IVTMS:MP1584_Buck",
        [("1","BS"),("2","IN"),("3","SW"),("4","GND"),
         ("5","FB"),("6","COMP"),("7","EN"),("8","FREQ")],
        width=10, label="MP1584")
    sch.add_lib("IVTMS:MP1584_Buck", BUCK)
    JBUS = make_pinheader2x_lib("IVTMS:Conn_2x08", 8)
    sch.add_lib("IVTMS:Conn_2x08", JBUS)
    H5 = make_pinheader_lib("IVTMS:Conn_1x05", 5)
    sch.add_lib("IVTMS:Conn_1x05", H5)

    sch.text("MCU-A: dsPIC30F4011 (5V) + SIM900 (legacy 2G)", 50, 30)
    sch.text("Buck 12V→4.0V for SIM900 Vbat, level-shifters on UART", 50, 35)

    # Buck for SIM900 Vbat (4.0V)
    sch.place("IVTMS:MP1584_Buck", "U2", "MP1584EN",
              "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm", 60, 60,
              pin_nets={"1":"BS_U2","2":"+12V_BUS","3":"SW_U2","4":"GND",
                        "5":"FB_U2","6":"COMP_U2","7":"+12V_BUS","8":"FREQ_U2"})
    sch.place("Device:L", "L_BUCK", "10uH",
              "Inductor_SMD:L_Bourns_SRR1280", 85, 60,
              pin_nets={"1":"SW_U2","2":"+4V0_RF"})
    sch.place("Device:D", "D_BUCK", "SS34",
              "Diode_SMD:D_SMA", 85, 75, rotation=90,
              pin_nets={"1":"SW_U2","2":"GND"})
    sch.place("Device:C_Polarized", "C_RF_BULK", "470uF/16V",
              "Capacitor_SMD:CP_Elec_8x10.5", 105, 60,
              pin_nets={"1":"+4V0_RF","2":"GND"})
    sch.place("Device:C", "C_BUCK_IN", "22uF/25V",
              "Capacitor_SMD:C_1210_3225Metric", 45, 60,
              pin_nets={"1":"+12V_BUS","2":"GND"})

    # Crystal Y1 16MHz + load caps
    sch.place("Device:Crystal", "Y1", "16MHz",
              "Crystal:Crystal_SMD_HC49-SD", 130, 60,
              pin_nets={"1":"OSC1","2":"OSC2"})
    sch.place("Device:C", "C_OSC1", "22pF",
              "Capacitor_SMD:C_0805_2012Metric", 125, 75,
              pin_nets={"1":"OSC1","2":"GND"})
    sch.place("Device:C", "C_OSC2", "22pF",
              "Capacitor_SMD:C_0805_2012Metric", 140, 75,
              pin_nets={"1":"OSC2","2":"GND"})

    # dsPIC30F4011
    sch.place("IVTMS:dsPIC30F4011", "U1", "dsPIC30F4011-30I/PT",
              "Package_QFP:TQFP-44_10x10mm_P0.8mm", 80, 110,
              pin_nets={
                  "1":"MDM_STAT","2":"MMC_ERR","11":"MCLR","12":"GND",
                  "13":"LOOP_IO_COM_U51","14":"LOOP_IO_COM_U52","15":"CMP_OUT","16":"MUX_A",
                  "17":"RTC_CS","18":"MMC_CS","19":"SPI_MOSI","20":"MUX_INH",
                  "23":"SPI_SCK","24":"SPI_MISO","9":"MUX_C","10":"MUX_B",
                  "25":"LED_ONLOOP0","26":"LED_ONLOOP1","27":"LED_ONLOOP2","28":"LED_ONLOOP3",
                  "29":"LED_STATUS","30":"LED_CONN_STATE","31":"OSC1","32":"OSC2",
                  "33":"DSPIC_U1RX","34":"DSPIC_U1TX","35":"+5V","36":"GND",
                  "37":"AIR_PWRKEY","38":"PGC","39":"PGD","40":"+5V",
                  "5":"MODEM_PWR_EN",
              })
    # MCU bypass caps (3 ×100nF + 10uF)
    for i, x in enumerate([60, 80, 100, 120]):
        sch.place("Device:C", f"C_BYP_{i+1}", "100nF",
                  "Capacitor_SMD:C_0805_2012Metric", x, 145,
                  pin_nets={"1":"+5V","2":"GND"})
    sch.place("Device:C", "C_BULK_5V", "10uF",
              "Capacitor_SMD:C_0805_2012Metric", 140, 145,
              pin_nets={"1":"+5V","2":"GND"})

    # MCLR pull-up + button
    sch.place("Device:R", "R_MCLR", "10K",
              "Resistor_SMD:R_0805_2012Metric", 50, 110, rotation=90,
              pin_nets={"1":"+5V","2":"MCLR"})

    # SIM900 module
    sch.place("IVTMS:SIM900_Module", "U4", "SIM900A",
              "Module:SIM900", 200, 80,
              pin_nets={
                  "1":"+4V0_RF","2":"+4V0_RF","3":"GND","4":"GND",
                  "5":"AIR_PWRKEY","6":"MDM_STAT","7":"NETLIGHT_LED","8":"SIM_TXD",
                  "9":"SIM_RXD","10":"SIM_VDD","11":"SIM_RST","12":"SIM_CLK",
                  "13":"SIM_DATA","14":"GND","15":"ANT_OUT","16":"RING_INDICATE",
              })
    sch.place("Device:C", "C_RF_BYP1", "100nF",
              "Capacitor_SMD:C_0805_2012Metric", 180, 65,
              pin_nets={"1":"+4V0_RF","2":"GND"})

    # Level-shifter resistors (5V→2.8V via voltage divider)
    sch.place("Device:R", "R_LVL_TX1", "10K",
              "Resistor_SMD:R_0805_2012Metric", 165, 110, rotation=90,
              pin_nets={"1":"DSPIC_U1TX","2":"SIM_RXD"})
    sch.place("Device:R", "R_LVL_TX2", "15K",
              "Resistor_SMD:R_0805_2012Metric", 165, 125, rotation=90,
              pin_nets={"1":"SIM_RXD","2":"GND"})
    # SIM TX→dsPIC RX direct (2.8V is high enough for VIH on 5V dsPIC if SIM TX is 2.8V CMOS = ~OK)
    # but add buffer resistor:
    sch.place("Device:R", "R_LVL_RX", "1K",
              "Resistor_SMD:R_0805_2012Metric", 175, 110, rotation=90,
              pin_nets={"1":"SIM_TXD","2":"DSPIC_U1RX"})

    # PWRKEY pull-up
    sch.place("Device:R", "R_PWRKEY", "10K",
              "Resistor_SMD:R_0805_2012Metric", 185, 110, rotation=90,
              pin_nets={"1":"+5V","2":"AIR_PWRKEY"})
    # STATUS pull-up
    sch.place("Device:R", "R_STATUS", "10K",
              "Resistor_SMD:R_0805_2012Metric", 195, 110, rotation=90,
              pin_nets={"1":"+5V","2":"MDM_STAT"})

    # NETLIGHT LED
    sch.place("Device:R", "R_NETLED", "1K",
              "Resistor_SMD:R_0805_2012Metric", 210, 110, rotation=90,
              pin_nets={"1":"+5V","2":"NETLIGHT_A"})
    sch.place("Device:LED", "LED_NET", "Yellow",
              "LED_SMD:LED_0805_2012Metric", 210, 125,
              pin_nets={"1":"NETLIGHT_LED","2":"NETLIGHT_A"})

    # DS1305 RTC
    sch.place("IVTMS:DS1305", "U3", "DS1305",
              "Package_SO:SOIC-16_3.9x9.9mm_P1.27mm", 260, 80,
              pin_nets={
                  "1":"RTC_X1","2":"RTC_X2","3":"VBAT_RTC","4":"GND",
                  "5":"INT1","6":"SPI_MISO","7":"SPI_MOSI","8":"SPI_SCK",
                  "9":"RTC_CS","10":"INT0","11":"+5V","12":"+5V",
                  "13":"NC","14":"NC","15":"NC","16":"NC"
              })
    sch.place("Device:Crystal", "Y2", "32.768kHz",
              "Crystal:Crystal_SMD_3215-2Pin_3.2x1.5mm", 245, 100,
              pin_nets={"1":"RTC_X1","2":"RTC_X2"})

    # ICSP header
    sch.place("IVTMS:Conn_1x05", "J_ICSP", "ICSP",
              "Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Vertical", 50, 180,
              pin_nets={"1":"MCLR","2":"+5V","3":"GND","4":"PGD","5":"PGC"})

    # Backplane
    sch.place("IVTMS:Conn_2x08", "J_BUS", "Backplane",
              "Connector_PinHeader_2.54mm:PinHeader_2x08_P2.54mm_Vertical", 280, 180,
              pin_nets={
                  "1":"+12V_BUS","2":"+12V_BUS",
                  "3":"GND","4":"GND",
                  "5":"+5V","6":"+5V",
                  "7":"GND","8":"MUX_A",
                  "9":"MUX_B","10":"MUX_C",
                 "11":"MUX_INH","12":"LOOP_IO_COM_U51",
                 "13":"LOOP_IO_COM_U52","14":"CMP_OUT",
                 "15":"LED_STATUS","16":"RESERVED",
              })
    return sch.render()


def gen_mcu_b():
    sch = Sch("MCU-B — dsPIC30F4011 + AIR780E + W25Q80", "A3")
    add_passives(sch)
    DSPIC = make_generic_ic_lib("IVTMS:dsPIC30F4011",
        [("1","RB6/MDM_STAT"),("2","RB5/MMC_ERR"),("3","RB4"),("4","RB3"),
         ("5","RB2/MODEM_PWR"),("6","RB1"),("7","RB0/AN0"),("8","CN1"),
         ("9","RC15/MUX_C"),("10","RC14/MUX_B"),("11","MCLR"),("12","VSS"),
         ("13","AN0/LOOP_COM1"),("14","AN1/LOOP_COM2"),("15","AN2/CMP_OUT"),("16","RC13/MUX_A"),
         ("17","RF0/RTC_CS"),("18","RF1/FLASH_CS"),("19","SDO1/MOSI"),("20","RD0/MUX_INH"),
         ("21","U2RX"),("22","U2TX"),("23","SCK1"),("24","SDI1/MISO"),
         ("25","RE0/onloop0"),("26","RE1/onloop1"),("27","RE2/onloop2"),("28","RE3/onloop3"),
         ("29","RE4/STATUS"),("30","RE5/CONN"),("31","OSC1"),("32","OSC2"),
         ("33","U1RX"),("34","U1TX"),("35","AVDD"),("36","AVSS"),
         ("37","RB7/PWRKEY"),("38","PGC"),("39","PGD"),("40","VDD")],
        width=20, label="dsPIC30F4011")
    sch.add_lib("IVTMS:dsPIC30F4011", DSPIC)
    AIR780 = make_generic_ic_lib("IVTMS:AIR780E_Module",
        [("1","Vbat"),("2","Vbat"),("3","GND"),("4","GND"),
         ("5","PWRKEY"),("6","RESET_N"),("7","STATUS"),("8","NETLIGHT"),
         ("9","UART_TXD"),("10","UART_RXD"),("11","USIM_VDD"),("12","USIM_RST"),
         ("13","USIM_CLK"),("14","USIM_DATA"),("15","ANT_MAIN"),("16","ANT_DIV")],
        width=14, label="AIR780E")
    sch.add_lib("IVTMS:AIR780E_Module", AIR780)
    W25Q = make_generic_ic_lib("IVTMS:W25Q80",
        [("1","CS_n"),("2","DO"),("3","WP_n"),("4","GND"),
         ("5","DI"),("6","CLK"),("7","HOLD_n"),("8","Vcc")],
        width=10, label="W25Q80DV")
    sch.add_lib("IVTMS:W25Q80", W25Q)
    AMS = make_generic_ic_lib("IVTMS:AMS1117-3.3",
        [("1","GND"),("2","Vout"),("3","Vin")],
        width=8, label="AMS1117-3.3")
    sch.add_lib("IVTMS:AMS1117-3.3", AMS)
    BUCK = make_generic_ic_lib("IVTMS:MP1584_Buck",
        [("1","BS"),("2","IN"),("3","SW"),("4","GND"),
         ("5","FB"),("6","COMP"),("7","EN"),("8","FREQ")],
        width=10, label="MP1584")
    sch.add_lib("IVTMS:MP1584_Buck", BUCK)
    DS1305 = make_generic_ic_lib("IVTMS:DS1305",
        [("1","X1"),("2","X2"),("3","Vbat"),("4","GND"),
         ("5","INT1"),("6","SDO"),("7","SDI"),("8","SCLK"),
         ("9","CE"),("10","INT0"),("11","Vcc2"),("12","Vcc1"),
         ("13","NC"),("14","NC"),("15","NC"),("16","NC")],
        width=12, label="DS1305")
    sch.add_lib("IVTMS:DS1305", DS1305)
    JBUS = make_pinheader2x_lib("IVTMS:Conn_2x08", 8)
    sch.add_lib("IVTMS:Conn_2x08", JBUS)
    H5 = make_pinheader_lib("IVTMS:Conn_1x05", 5)
    sch.add_lib("IVTMS:Conn_1x05", H5)

    sch.text("MCU-B: dsPIC30F4011 + AIR780E (4G) + W25Q80 (1MB SPI flash)", 50, 30)
    sch.text("Buck 12V→3.8V for AIR780, LDO 5V→3.3V for W25Q80 + level shifters", 50, 35)

    # Buck for AIR780 Vbat (3.8V)
    sch.place("IVTMS:MP1584_Buck", "U2", "MP1584EN",
              "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm", 60, 60,
              pin_nets={"1":"BS_U2","2":"+12V_BUS","3":"SW_U2","4":"GND",
                        "5":"FB_U2","6":"COMP_U2","7":"+12V_BUS","8":"FREQ_U2"})
    sch.place("Device:L", "L_BUCK", "10uH",
              "Inductor_SMD:L_Bourns_SRR1280", 85, 60,
              pin_nets={"1":"SW_U2","2":"+V_BAT_RF"})
    sch.place("Device:D", "D_BUCK", "SS34",
              "Diode_SMD:D_SMA", 85, 75, rotation=90,
              pin_nets={"1":"SW_U2","2":"GND"})
    sch.place("Device:C_Polarized", "C_RF_BULK", "470uF/16V",
              "Capacitor_SMD:CP_Elec_8x10.5", 105, 60,
              pin_nets={"1":"+V_BAT_RF","2":"GND"})

    # AMS1117-3.3 LDO (5V → 3.3V)
    sch.place("IVTMS:AMS1117-3.3", "U7", "AMS1117-3.3",
              "Package_TO_SOT_SMD:SOT-223-3_TabPin2", 130, 60,
              pin_nets={"1":"GND","2":"+3V3","3":"+5V"})
    sch.place("Device:C", "C_LDO_IN", "10uF",
              "Capacitor_SMD:C_0805_2012Metric", 120, 75,
              pin_nets={"1":"+5V","2":"GND"})
    sch.place("Device:C", "C_LDO_OUT", "10uF",
              "Capacitor_SMD:C_0805_2012Metric", 145, 75,
              pin_nets={"1":"+3V3","2":"GND"})

    # Crystal
    sch.place("Device:Crystal", "Y1", "16MHz",
              "Crystal:Crystal_SMD_HC49-SD", 170, 60,
              pin_nets={"1":"OSC1","2":"OSC2"})
    sch.place("Device:C", "C_OSC1", "22pF",
              "Capacitor_SMD:C_0805_2012Metric", 165, 75,
              pin_nets={"1":"OSC1","2":"GND"})
    sch.place("Device:C", "C_OSC2", "22pF",
              "Capacitor_SMD:C_0805_2012Metric", 180, 75,
              pin_nets={"1":"OSC2","2":"GND"})

    # dsPIC
    sch.place("IVTMS:dsPIC30F4011", "U1", "dsPIC30F4011-30I/PT",
              "Package_QFP:TQFP-44_10x10mm_P0.8mm", 80, 120,
              pin_nets={
                  "1":"MDM_STAT","2":"MMC_ERR","11":"MCLR","12":"GND",
                  "13":"LOOP_IO_COM_U51","14":"LOOP_IO_COM_U52","15":"CMP_OUT","16":"MUX_A",
                  "17":"RTC_CS","18":"FLASH_CS_5V","19":"SPI_MOSI_5V","20":"MUX_INH",
                  "23":"SPI_SCK_5V","24":"SPI_MISO_5V","9":"MUX_C","10":"MUX_B",
                  "25":"LED_ONLOOP0","26":"LED_ONLOOP1","27":"LED_ONLOOP2","28":"LED_ONLOOP3",
                  "29":"LED_STATUS","30":"LED_CONN_STATE","31":"OSC1","32":"OSC2",
                  "33":"DSPIC_U1RX","34":"DSPIC_U1TX","35":"+5V","36":"GND",
                  "37":"AIR_PWRKEY","38":"PGC","39":"PGD","40":"+5V",
                  "5":"MODEM_PWR_EN",
              })
    for i, x in enumerate([60, 80, 100, 120]):
        sch.place("Device:C", f"C_BYP_{i+1}", "100nF",
                  "Capacitor_SMD:C_0805_2012Metric", x, 155,
                  pin_nets={"1":"+5V","2":"GND"})

    sch.place("Device:R", "R_MCLR", "10K",
              "Resistor_SMD:R_0805_2012Metric", 50, 120, rotation=90,
              pin_nets={"1":"+5V","2":"MCLR"})

    # AIR780E
    sch.place("IVTMS:AIR780E_Module", "U8", "AIR780E",
              "Module:AIR780E_LCC", 200, 95,
              pin_nets={
                  "1":"+V_BAT_RF","2":"+V_BAT_RF","3":"GND","4":"GND",
                  "5":"AIR_PWRKEY","6":"AIR_RESET","7":"MDM_STAT","8":"NETLIGHT",
                  "9":"AIR_TXD","10":"AIR_RXD","11":"USIM_VDD","12":"USIM_RST",
                  "13":"USIM_CLK","14":"USIM_DATA","15":"ANT","16":"ANT_DIV",
              })
    # UART level shifters: dsPIC TX (5V) → AIR RXD (3.3V)
    sch.place("Device:R", "R_LVL_TX1", "10K",
              "Resistor_SMD:R_0805_2012Metric", 175, 100, rotation=90,
              pin_nets={"1":"DSPIC_U1TX","2":"AIR_RXD"})
    sch.place("Device:R", "R_LVL_TX2", "20K",
              "Resistor_SMD:R_0805_2012Metric", 175, 115, rotation=90,
              pin_nets={"1":"AIR_RXD","2":"GND"})
    sch.place("Device:R", "R_LVL_RX", "1K",
              "Resistor_SMD:R_0805_2012Metric", 185, 100, rotation=90,
              pin_nets={"1":"AIR_TXD","2":"DSPIC_U1RX"})
    sch.place("Device:R", "R_PU_PK", "10K",
              "Resistor_SMD:R_0805_2012Metric", 195, 100, rotation=90,
              pin_nets={"1":"+5V","2":"AIR_PWRKEY"})

    # W25Q80 on +3V3
    sch.place("IVTMS:W25Q80", "U4", "W25Q80DV",
              "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm", 260, 110,
              pin_nets={
                  "1":"FLASH_CS_3V3","2":"FLASH_MISO_3V3","3":"+3V3","4":"GND",
                  "5":"FLASH_MOSI_3V3","6":"FLASH_SCK_3V3","7":"+3V3","8":"+3V3",
              })
    sch.place("Device:C", "C_FLASH_BYP", "100nF",
              "Capacitor_SMD:C_0805_2012Metric", 280, 110,
              pin_nets={"1":"+3V3","2":"GND"})

    # SPI level shifters (5V → 3.3V) for CS, MOSI, SCK using resistor dividers (10k+18k)
    for i, (sig5, sig33, x) in enumerate([
        ("FLASH_CS_5V","FLASH_CS_3V3", 235),
        ("SPI_MOSI_5V","FLASH_MOSI_3V3", 245),
        ("SPI_SCK_5V","FLASH_SCK_3V3", 255),
    ]):
        sch.place("Device:R", f"R_LVL_S{i}A", "10K",
                  "Resistor_SMD:R_0805_2012Metric", x, 130, rotation=90,
                  pin_nets={"1":sig5,"2":sig33})
        sch.place("Device:R", f"R_LVL_S{i}B", "18K",
                  "Resistor_SMD:R_0805_2012Metric", x, 145, rotation=90,
                  pin_nets={"1":sig33,"2":"GND"})
    # MISO 3V3 → 5V — use direct (3V3 ≥ VIH=2.0V on 5V dsPIC) with series 1K
    sch.place("Device:R", "R_LVL_MISO", "1K",
              "Resistor_SMD:R_0805_2012Metric", 225, 130, rotation=90,
              pin_nets={"1":"FLASH_MISO_3V3","2":"SPI_MISO_5V"})

    # DS1305 RTC
    sch.place("IVTMS:DS1305", "U3", "DS1305",
              "Package_SO:SOIC-16_3.9x9.9mm_P1.27mm", 60, 200,
              pin_nets={
                  "1":"RTC_X1","2":"RTC_X2","3":"VBAT_RTC","4":"GND",
                  "5":"INT1","6":"SPI_MISO_5V","7":"SPI_MOSI_5V","8":"SPI_SCK_5V",
                  "9":"RTC_CS","10":"INT0","11":"+5V","12":"+5V",
                  "13":"NC","14":"NC","15":"NC","16":"NC"
              })
    sch.place("Device:Crystal", "Y2", "32.768kHz",
              "Crystal:Crystal_SMD_3215-2Pin_3.2x1.5mm", 45, 220,
              pin_nets={"1":"RTC_X1","2":"RTC_X2"})

    # ICSP
    sch.place("IVTMS:Conn_1x05", "J_ICSP", "ICSP",
              "Connector_PinHeader_2.54mm:PinHeader_1x05_P2.54mm_Vertical", 130, 200,
              pin_nets={"1":"MCLR","2":"+5V","3":"GND","4":"PGD","5":"PGC"})

    # Backplane
    sch.place("IVTMS:Conn_2x08", "J_BUS", "Backplane",
              "Connector_PinHeader_2.54mm:PinHeader_2x08_P2.54mm_Vertical", 280, 200,
              pin_nets={
                  "1":"+12V_BUS","2":"+12V_BUS",
                  "3":"GND","4":"GND",
                  "5":"+5V","6":"+5V",
                  "7":"GND","8":"MUX_A",
                  "9":"MUX_B","10":"MUX_C",
                 "11":"MUX_INH","12":"LOOP_IO_COM_U51",
                 "13":"LOOP_IO_COM_U52","14":"CMP_OUT",
                 "15":"LED_STATUS","16":"RESERVED",
              })
    return sch.render()


def gen_mcu_c():
    sch = Sch("MCU-C — STM32F103C8 + AIR780E + W25Q80", "A3")
    add_passives(sch)
    STM32 = make_generic_ic_lib("IVTMS:STM32F103C8",
        [("1","VBAT"),("2","PC13"),("3","PC14"),("4","PC15"),
         ("5","PD0/OSC_IN"),("6","PD1/OSC_OUT"),("7","NRST"),("8","VSSA"),
         ("9","VDDA"),("10","PA0"),("11","PA1"),("12","PA2"),
         ("13","PA3"),("14","PA4"),("15","PA5"),("16","PA6"),
         ("17","PA7"),("18","PB0"),("19","PB1"),("20","PB2/BOOT1"),
         ("21","PB10"),("22","PB11"),("23","VSS_1"),("24","VDD_1"),
         ("25","PB12"),("26","PB13"),("27","PB14"),("28","PB15"),
         ("29","PA8"),("30","PA9"),("31","PA10"),("32","PA11"),
         ("33","PA12"),("34","PA13"),("35","VSS_2"),("36","VDD_2"),
         ("37","PA14"),("38","PA15"),("39","PB3"),("40","PB4"),
         ("41","PB5"),("42","PB6"),("43","PB7"),("44","BOOT0"),
         ("45","PB8"),("46","PB9"),("47","VSS_3"),("48","VDD_3")],
        width=22, label="STM32F103C8")
    sch.add_lib("IVTMS:STM32F103C8", STM32)
    AIR780 = make_generic_ic_lib("IVTMS:AIR780E_Module",
        [("1","Vbat"),("2","Vbat"),("3","GND"),("4","GND"),
         ("5","PWRKEY"),("6","RESET_N"),("7","STATUS"),("8","NETLIGHT"),
         ("9","UART_TXD"),("10","UART_RXD"),("11","USIM_VDD"),("12","USIM_RST"),
         ("13","USIM_CLK"),("14","USIM_DATA"),("15","ANT_MAIN"),("16","ANT_DIV")],
        width=14, label="AIR780E")
    sch.add_lib("IVTMS:AIR780E_Module", AIR780)
    W25Q = make_generic_ic_lib("IVTMS:W25Q80",
        [("1","CS_n"),("2","DO"),("3","WP_n"),("4","GND"),
         ("5","DI"),("6","CLK"),("7","HOLD_n"),("8","Vcc")],
        width=10, label="W25Q80DV")
    sch.add_lib("IVTMS:W25Q80", W25Q)
    AMS = make_generic_ic_lib("IVTMS:AMS1117-3.3",
        [("1","GND"),("2","Vout"),("3","Vin")],
        width=8, label="AMS1117-3.3")
    sch.add_lib("IVTMS:AMS1117-3.3", AMS)
    BUCK = make_generic_ic_lib("IVTMS:MP1584_Buck",
        [("1","BS"),("2","IN"),("3","SW"),("4","GND"),
         ("5","FB"),("6","COMP"),("7","EN"),("8","FREQ")],
        width=10, label="MP1584")
    sch.add_lib("IVTMS:MP1584_Buck", BUCK)
    SD = make_generic_ic_lib("IVTMS:microSD",
        [("1","CS"),("2","MOSI"),("3","GND1"),("4","Vdd"),
         ("5","SCK"),("6","GND2"),("7","MISO"),("8","NC1"),("9","NC2")],
        width=10, label="microSD")
    sch.add_lib("IVTMS:microSD", SD)
    JBUS = make_pinheader2x_lib("IVTMS:Conn_2x08", 8)
    sch.add_lib("IVTMS:Conn_2x08", JBUS)
    H4 = make_pinheader_lib("IVTMS:Conn_1x04", 4)
    sch.add_lib("IVTMS:Conn_1x04", H4)

    sch.text("MCU-C: STM32F103C8 + AIR780E + W25Q80 + microSD (3.3V native)", 50, 30)
    sch.text("Pin map matches stm32-firmware/Core/Src/main.c", 50, 35)

    # Buck 12V→3.8V
    sch.place("IVTMS:MP1584_Buck", "U2", "MP1584EN",
              "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm", 60, 60,
              pin_nets={"1":"BS_U2","2":"+12V_BUS","3":"SW_U2","4":"GND",
                        "5":"FB_U2","6":"COMP_U2","7":"+12V_BUS","8":"FREQ_U2"})
    sch.place("Device:L", "L_BUCK", "10uH",
              "Inductor_SMD:L_Bourns_SRR1280", 85, 60,
              pin_nets={"1":"SW_U2","2":"+V_BAT_RF"})
    sch.place("Device:D", "D_BUCK", "SS34",
              "Diode_SMD:D_SMA", 85, 75, rotation=90,
              pin_nets={"1":"SW_U2","2":"GND"})
    sch.place("Device:C_Polarized", "C_RF_BULK", "470uF/16V",
              "Capacitor_SMD:CP_Elec_8x10.5", 105, 60,
              pin_nets={"1":"+V_BAT_RF","2":"GND"})

    # 5V→3.3V LDO
    sch.place("IVTMS:AMS1117-3.3", "U3", "AMS1117-3.3",
              "Package_TO_SOT_SMD:SOT-223-3_TabPin2", 130, 60,
              pin_nets={"1":"GND","2":"+3V3","3":"+5V"})
    sch.place("Device:C", "C_LDO_IN", "10uF",
              "Capacitor_SMD:C_0805_2012Metric", 120, 75,
              pin_nets={"1":"+5V","2":"GND"})
    sch.place("Device:C", "C_LDO_OUT", "10uF",
              "Capacitor_SMD:C_0805_2012Metric", 145, 75,
              pin_nets={"1":"+3V3","2":"GND"})

    # HSE 8MHz crystal
    sch.place("Device:Crystal", "Y1", "8MHz",
              "Crystal:Crystal_SMD_HC49-SD", 170, 60,
              pin_nets={"1":"HSE_IN","2":"HSE_OUT"})
    sch.place("Device:C", "C_HSE1", "18pF",
              "Capacitor_SMD:C_0805_2012Metric", 165, 75,
              pin_nets={"1":"HSE_IN","2":"GND"})
    sch.place("Device:C", "C_HSE2", "18pF",
              "Capacitor_SMD:C_0805_2012Metric", 180, 75,
              pin_nets={"1":"HSE_OUT","2":"GND"})

    # LSE 32.768kHz
    sch.place("Device:Crystal", "Y2", "32.768kHz",
              "Crystal:Crystal_SMD_3215-2Pin_3.2x1.5mm", 200, 60,
              pin_nets={"1":"LSE_IN","2":"LSE_OUT"})
    sch.place("Device:C", "C_LSE1", "6pF",
              "Capacitor_SMD:C_0805_2012Metric", 195, 75,
              pin_nets={"1":"LSE_IN","2":"GND"})
    sch.place("Device:C", "C_LSE2", "6pF",
              "Capacitor_SMD:C_0805_2012Metric", 210, 75,
              pin_nets={"1":"LSE_OUT","2":"GND"})

    # STM32F103C8
    sch.place("IVTMS:STM32F103C8", "U1", "STM32F103C8T6",
              "Package_QFP:LQFP-48_7x7mm_P0.5mm", 80, 130,
              pin_nets={
                  "1":"+3V3","2":"LED_HB","3":"LSE_IN","4":"LSE_OUT",
                  "5":"HSE_IN","6":"HSE_OUT","7":"NRST","8":"GND",
                  "9":"+3V3A","10":"VBAT_SENSE","11":"SOLAR_SENSE","12":"LOOP_IN",
                  "13":"PA3_SP","14":"FLASH_CS","15":"FLASH_SCK","16":"FLASH_MISO",
                  "17":"FLASH_MOSI","18":"MUX_A","19":"MUX_B","20":"FLASH_WP",
                  "21":"AIR_RXD","22":"AIR_TXD","23":"GND","24":"+3V3",
                  "25":"SD_CS","26":"SD_SCK","27":"SD_MISO","28":"SD_MOSI",
                  "29":"AIR_PWRKEY","30":"DEBUG_TX","31":"DEBUG_RX","32":"AIR_STATUS",
                  "33":"PA12_SP","34":"SWDIO","35":"GND","36":"+3V3",
                  "37":"SWCLK","38":"PA15_SP","39":"FLASH_HOLD","40":"MUX_C",
                  "41":"LED_LOOP0","42":"LED_LOOP1","43":"LED_LOOP2","44":"BOOT0",
                  "45":"LED_LOOP3","46":"LED_CONN","47":"GND","48":"+3V3",
              })
    # Bypass caps on VDD pins
    for i, x in enumerate([55, 75, 95, 115]):
        sch.place("Device:C", f"C_BYP_{i+1}", "100nF",
                  "Capacitor_SMD:C_0805_2012Metric", x, 165,
                  pin_nets={"1":"+3V3","2":"GND"})
    # VDDA filter
    sch.place("Device:L", "L_VDDA", "FB_600R",
              "Inductor_SMD:L_0805_2012Metric", 130, 165,
              pin_nets={"1":"+3V3","2":"+3V3A"})
    sch.place("Device:C", "C_VDDA", "10nF",
              "Capacitor_SMD:C_0805_2012Metric", 145, 165,
              pin_nets={"1":"+3V3A","2":"GND"})

    # NRST circuit
    sch.place("Device:R", "R_NRST", "10K",
              "Resistor_SMD:R_0805_2012Metric", 50, 130, rotation=90,
              pin_nets={"1":"+3V3","2":"NRST"})
    sch.place("Device:C", "C_NRST", "100nF",
              "Capacitor_SMD:C_0805_2012Metric", 50, 145,
              pin_nets={"1":"NRST","2":"GND"})

    # AIR780E
    sch.place("IVTMS:AIR780E_Module", "U5", "AIR780E",
              "Module:AIR780E_LCC", 200, 95,
              pin_nets={
                  "1":"+V_BAT_RF","2":"+V_BAT_RF","3":"GND","4":"GND",
                  "5":"AIR_PWRKEY","6":"AIR_RESET","7":"AIR_STATUS","8":"NETLIGHT",
                  "9":"AIR_TXD","10":"AIR_RXD","11":"USIM_VDD","12":"USIM_RST",
                  "13":"USIM_CLK","14":"USIM_DATA","15":"ANT","16":"ANT_DIV",
              })
    sch.place("Device:C", "C_AIR_BYP", "100nF",
              "Capacitor_SMD:C_0805_2012Metric", 180, 95,
              pin_nets={"1":"+V_BAT_RF","2":"GND"})

    # W25Q80
    sch.place("IVTMS:W25Q80", "U4", "W25Q80DV",
              "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm", 260, 100,
              pin_nets={
                  "1":"FLASH_CS","2":"FLASH_MISO","3":"FLASH_WP","4":"GND",
                  "5":"FLASH_MOSI","6":"FLASH_SCK","7":"FLASH_HOLD","8":"+3V3",
              })

    # microSD
    sch.place("IVTMS:microSD", "J_SD", "microSD",
              "Connector_Card:microSD_HC_Hirose_DM3BT-DSF-PEJS", 260, 150,
              pin_nets={
                  "1":"SD_CS","2":"SD_MOSI","3":"GND","4":"+3V3",
                  "5":"SD_SCK","6":"GND","7":"SD_MISO","8":"NC","9":"NC",
              })
    sch.place("Device:C", "C_SD_BYP", "10uF",
              "Capacitor_SMD:C_0805_2012Metric", 240, 150,
              pin_nets={"1":"+3V3","2":"GND"})

    # SWD header
    sch.place("IVTMS:Conn_1x04", "J_SWD", "SWD",
              "Connector_PinHeader_1.27mm:PinHeader_1x04_P1.27mm_Vertical", 50, 200,
              pin_nets={"1":"+3V3","2":"SWDIO","3":"SWCLK","4":"GND"})

    # Status LEDs
    for i, sig in enumerate(["LED_HB","LED_LOOP0","LED_LOOP1","LED_LOOP2","LED_LOOP3","LED_CONN"]):
        sch.place("Device:R", f"R_LED_{i}", "1K",
                  "Resistor_SMD:R_0805_2012Metric", 100 + i*15, 200, rotation=90,
                  pin_nets={"1":"+3V3","2":f"{sig}_A"})
        sch.place("Device:LED", f"LED_{sig}", sig.split('_')[1] if '_' in sig else "Blue",
                  "LED_SMD:LED_0805_2012Metric", 100 + i*15, 215,
                  pin_nets={"1":sig,"2":f"{sig}_A"})

    # VBAT divider 100K/10K (1:11)
    sch.place("Device:R", "R_VBAT_HI", "100K",
              "Resistor_SMD:R_0805_2012Metric", 50, 245, rotation=90,
              pin_nets={"1":"+12V_BUS","2":"VBAT_SENSE"})
    sch.place("Device:R", "R_VBAT_LO", "10K",
              "Resistor_SMD:R_0805_2012Metric", 50, 260, rotation=90,
              pin_nets={"1":"VBAT_SENSE","2":"GND"})

    # Backplane
    sch.place("IVTMS:Conn_2x08", "J_BUS", "Backplane",
              "Connector_PinHeader_2.54mm:PinHeader_2x08_P2.54mm_Vertical", 280, 230,
              pin_nets={
                  "1":"+12V_BUS","2":"+12V_BUS",
                  "3":"GND","4":"GND",
                  "5":"+5V","6":"+5V",
                  "7":"GND","8":"MUX_A",
                  "9":"MUX_B","10":"MUX_C",
                 "11":"MUX_INH","12":"LOOP_IO_COM_U51",
                 "13":"LOOP_IO_COM_U52","14":"LOOP_IN",
                 "15":"LED_CONN","16":"RESERVED",
              })
    return sch.render()


# ──────────────────────────────────────────────────────────────────────────
# Main
# ──────────────────────────────────────────────────────────────────────────

def write_sch(rel_path: str, content: str):
    abs_path = os.path.join(ROOT, rel_path)
    os.makedirs(os.path.dirname(abs_path), exist_ok=True)
    with open(abs_path, "w") as f:
        f.write(content)
    print(f"  wrote {rel_path} ({len(content)} bytes)")


def main():
    print("Generating KiCad schematics...")
    write_sch("00-power-supply/schematic/board.kicad_sch", gen_psu())
    write_sch("01-loop-detector/schematic/board.kicad_sch", gen_loop())
    write_sch("02-mcu-dspic-sim900/schematic/board.kicad_sch", gen_mcu_a())
    write_sch("03-mcu-dspic-air780-w25q80/schematic/board.kicad_sch", gen_mcu_b())
    write_sch("04-mcu-stm32-air780-w25q80/schematic/board.kicad_sch", gen_mcu_c())
    print("Done.")


if __name__ == "__main__":
    main()
