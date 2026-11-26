#!/usr/bin/env python3
"""The browser emulator's page: the computer keyboard plays the pad (arrows), F5 (Enter or Space) and G5 (Backspace
or Escape), as the FM-1's lowest keys and F5 / G5 do. Added to fm1-base's generated page after the build."""
import sys
from pathlib import Path

ADD = """<script>
// Isle of Bears: the arrows are the pad (F3 left, F#3 up, G3 down, A3 right), Enter or Space is F5, Backspace or Esc G5
(() => {
  const MAP = { ArrowLeft: 0, ArrowUp: 1, ArrowDown: 2, ArrowRight: 4, Enter: 24, " ": 24, Backspace: 26, Escape: 26 };
  // fm1-base's page: holdDown / letGo press and release a key by an id of the caller's (its own shortcuts use "k...")
  for (const [type, on] of [["keydown", 1], ["keyup", 0]])
    addEventListener(type, (e) => {
      if (!(e.key in MAP) || e.metaKey || e.ctrlKey || e.altKey || e.target.closest?.("input,textarea,select")) return;
      e.preventDefault(); e.stopImmediatePropagation();
      if (on && e.repeat) return;
      if (on) holdDown("iob" + e.key, "key", MAP[e.key]); else letGo("iob" + e.key);
    }, true);
})();
</script>
"""

page = Path(sys.argv[1])
s = page.read_text()
if "Isle of Bears: the arrows" not in s:
    s = s.replace("</body>", ADD + "</body>", 1)
    page.write_text(s)
print("web keys added")
