#!/usr/bin/env python3
# SPDX-License-Identifier: CC0-1.0
"""Generate this fork's original Atum monogram and status variants."""
from pathlib import Path
import os
import subprocess
import sys

theme = Path(__file__).resolve().parent.parent / "theme"
def artwork(background, foreground, state=None):
    marker = "" if state is None else f'<circle cx="198" cy="198" r="31" fill="{background}"/><circle cx="198" cy="198" r="22" fill="{state}"/>'
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256"><rect x="8" y="8" width="240" height="240" rx="55" fill="{background}"/><path d="M57 181 111 66h34l54 115h-37l-10-25h-50l-10 25Zm58-55h25l-12-32Z" fill="{foreground}"/>{marker}</svg>\n'

for variant, colors in {"colored": ("#142638", "#8DE4D3"), "universal": ("#142638", "#8DE4D3"), "dark": ("#142638", "#8DE4D3"), "black": ("#FFFFFF", "#000000"), "white": ("#142638", "#FFFFFF")}.items():
    target = theme / variant
    target.mkdir(parents=True, exist_ok=True)
    for name in ("atum-icon", "wizard_logo"):
        (target / f"{name}.svg").write_text(artwork(*colors))
    for state, color in {"ok": "#8DE4D3", "error": "#F07178", "information": "#85B7EF", "offline": "#8493A4", "pause": "#EDC981", "sync": "#85B7EF"}.items():
        (target / f"state-{state}.svg").write_text(artwork(*colors, color))
subprocess.run([sys.argv[1], str(theme / "colored/atum-icon.svg"), str(theme / "colored")], check=True,
               env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
