"""
Curses two‑column dashboard for DataManager.
Filters on top, actions below a separator. Inline editing; returns cleanly
from nested `interactive_edit` without swallowing keys.

Keymap ────────────────────────────────────────────────────────────────
 ↑/↓   move cursor (skips separator)
 →/Enter edit / trigger
 Esc   cancel inline edit
 c/←   clear current filter value
 r     reset all filters
 e     launch interactive table editor (returns)
 s     save full DataFrame & quit
 q     quit without saving
"""
from __future__ import annotations

import curses
import locale
import re
import sys
from pathlib import Path
from typing import Callable, List

from utils.data_body import DataManager

# locale.setlocale(locale.LC_ALL, "")

# ───────────────────────── helpers ──────────────────────────

def unique_strings(mgr: DataManager, col: str) -> List[str]:
    return sorted(set(mgr.full_df[col].dropna().astype(str)))


# ────────────────────────── row model ──────────────────────
class Row:
    def __init__(
        self,
        label: str,
        tag: str,
        apply: Callable[[DataManager, object], None] | None = None,
        needs_arg: bool = False,
    ):
        self.label, self.tag, self.apply, self.needs_arg = label, tag, apply or (lambda *_: None), needs_arg
        self.value: str = ""

    def render(self, total_w: int, label_w: int) -> str:
        if self.tag == "sep":
            return "─" * (total_w - 4)
        left = f"{self.label:<{label_w}} │ "
        return (left + self.value)[: total_w - 4]


# Build the list of rows ------------------------------------------------

def build_rows() -> List[Row]:
    m = lambda lbl, tag, fn: Row(lbl, tag, lambda mgr, v: getattr(mgr, fn)(v), True)
    filters = [
        m("Name", "name", "filter_by_name"),
        m("Level", "level", "filter_by_level"),
        m("Category", "category", "filter_by_category"),
        m("Recipe contains", "recipe_contains", "filter_contains_recipe"),
        m("Full name", "full_name", "filter_by_full_name"),
        m("Recipe exact", "recipe_exact", "filter_exact_recipe"),
        Row("Pods range", "pods", lambda m, t: m.filter_by_pods(*t), True),
        Row("Price range", "price", lambda m, t: m.filter_by_price(*t), True),
    ]
    actions = [
        Row("(r) Reset filters", "reset"),
        Row("(e) Launch editor", "edit"),
        Row("(s) Save & quit", "save"),
        Row("(q) Quit", "quit"),
    ]
    return filters + [Row("", "sep")] + actions

# ─────────────────────────── main UI ───────────────────────────

def run_menu(mgr: DataManager):
    rows = build_rows()
    label_w = max(len(r.label) for r in rows if r.tag != "sep") + 1

    # ------------------------------------------------ filtering logic
    def recalc():
        mgr.reset()
        for r in rows:
            if r.needs_arg and r.value:
                r.apply(mgr, parse_value(r))

    def parse_value(r: Row):
        if r.tag in ("pods", "price") and r.value:
            p = re.split(r"\s*,\s*", r.value)
            lo = float(p[0]) if p and p[0] else None
            hi = float(p[1]) if len(p) > 1 and p[1] else None
            return (lo, hi)
        return r.value

    # ------------------------------------------------ drawing
    def draw(stdscr, cur):
        stdscr.erase()
        h, w = stdscr.getmaxyx()
        # header
        stdscr.attron(curses.color_pair(2))
        stdscr.addstr(0, 0, f"Rows: {len(mgr.subset_df):,}   File: {mgr.path.name}")
        stdscr.attroff(curses.color_pair(2))
        # rows
        for i, r in enumerate(rows, start=2):
            if i >= h - 2:
                break
            if r.tag == "sep":
                stdscr.attron(curses.color_pair(4))
                stdscr.addstr(i, 2, r.render(w, label_w))
                stdscr.attroff(curses.color_pair(4))
                continue
            if i - 2 == cur:
                stdscr.attron(curses.color_pair(1))
            stdscr.addstr(i, 2, r.render(w, label_w))
            if i - 2 == cur:
                stdscr.attroff(curses.color_pair(1))
        # footer
        stdscr.attron(curses.color_pair(3))
        stdscr.addstr(h - 1, 0, "↑↓ move  → edit  c clear  r reset  e editor  s save  q quit")
        stdscr.attroff(curses.color_pair(3))
        stdscr.refresh()

    # ------------------------------------------------ inline edit
    def inline_edit(stdscr, idx):
        r = rows[idx]
        y = 2 + idx; x0 = 2 + label_w + 3
        buf, pos = list(r.value), len(r.value)
        curses.curs_set(1)
        while True:
            stdscr.move(y, x0); stdscr.clrtoeol(); stdscr.addstr(y, x0, "".join(buf))
            stdscr.move(y, x0 + pos); stdscr.refresh()
            ch = stdscr.get_wch()
            if ch in ("\n", "\r", curses.KEY_ENTER):
                r.value = "".join(buf).strip(); recalc(); break
            if ch == "\x1b":
                break
            if ch in (curses.KEY_BACKSPACE, "\b", "\x7f") and pos:
                buf.pop(pos - 1); pos -= 1
            elif ch == curses.KEY_LEFT and pos:
                pos -= 1
            elif ch == curses.KEY_RIGHT and pos < len(buf):
                pos += 1
            elif ch == "\t" and r.tag in ("name", "level", "category", "full_name"):
                pref = "".join(buf[:pos])
                sugg = [s for s in unique_strings(mgr, r.tag) if s.lower().startswith(pref.lower())]
                if sugg: buf, pos = list(sugg[0]), len(sugg[0])
            elif isinstance(ch, str) and ch.isprintable():
                buf.insert(pos, ch); pos += 1
        curses.curs_set(0)

    # ------------------------------------------------ main loop
    def _curses(stdscr):
        curses.curs_set(0); curses.use_default_colors()
        curses.init_pair(1, curses.COLOR_BLACK, curses.COLOR_WHITE)   # highlight
        curses.init_pair(2, curses.COLOR_GREEN, -1)                 # header
        curses.init_pair(3, curses.COLOR_YELLOW, -1)                # footer
        curses.init_pair(4, curses.COLOR_WHITE, -1)               # separator
        cur = 0; draw(stdscr, cur)
        while True:
            key = stdscr.get_wch()
            # navigation
            if key == curses.KEY_UP:
                cur = (cur - 1) % len(rows); cur = cur - 1 if rows[cur].tag == "sep" else cur
            elif key == curses.KEY_DOWN:
                cur = (cur + 1) % len(rows); cur = cur + 1 if rows[cur].tag == "sep" else cur
            # edit / trigger
            elif key in (curses.KEY_RIGHT, "\n", "\r") and rows[cur].tag != "sep":
                r = rows[cur]
                if r.needs_arg:
                    inline_edit(stdscr, cur)
                else:
                    if r.tag == "reset":
                        for rr in rows: rr.value = "" if rr.needs_arg else rr.value; mgr.reset()
                    elif r.tag == "edit":
                        curses.curs_set(0); curses.echo(False)
                        mgr.interactive_edit();
                        stdscr.keypad(True); curses.noecho(); curses.cbreak(); curses.flushinp()
                    elif r.tag == "save":
                        mgr.save(); break
                    elif r.tag == "quit":
                        break
            # clear
            elif key in ("c", curses.KEY_LEFT) and rows[cur].needs_arg:
                rows[cur].value = ""; recalc()
            # shortcuts
            elif key == "r":
                for rr in rows: rr.value = "" if rr.needs_arg else rr.value; mgr.reset()
            elif key == "e":
                mgr.interactive_edit();
                stdscr.keypad(True); curses.noecho(); curses.cbreak(); curses.flushinp()
            elif key == "s":
                mgr.save(); break
            elif key in ("q", "Q"):
                break
            draw(stdscr, cur)

    curses.wrapper(_curses)

