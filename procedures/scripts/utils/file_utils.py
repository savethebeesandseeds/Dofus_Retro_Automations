import pandas as pd
import locale
from pathlib import Path
import curses
from curses import wrapper

def load_data(path: Path) -> (pd.DataFrame, Path):
    path = Path(path)
    if not path.exists():
        raise FileNotFoundError(f"File not found: {path}")
    ext = path.suffix.lower()
    if ext == '.xlsx':
        df = pd.read_excel(path, keep_default_na=False)
    elif ext == '.csv':
        df = pd.read_csv(path, keep_default_na=False)
    elif ext == '.json':
        df = pd.read_json(path, orient='records')
    else:
        raise ValueError(f"Unsupported file type: {ext}")
    return df, path


def save_data(df: pd.DataFrame, path: Path) -> None:
    ext = path.suffix.lower()
    if ext == '.xlsx':
        df.to_excel(path, index=False)
    elif ext == '.csv':
        df.to_csv(path, index=False)
    elif ext == '.json':
        df.to_json(path, orient='records', indent=2)
    else:
        raise ValueError(f"Unsupported file type: {ext}")




# Enable Unicode input (tildes, accents, etc.)
try:
    locale.setlocale(locale.LC_ALL, '')
except locale.Error:
    pass


def interactive_edit(mgr, path: Path) -> None:
    df = mgr.subset_df
    num_rows, num_cols = df.shape
    col_names = list(df.columns)

    def _main(stdscr):
        nonlocal df 
        # Curses setup
        curses.curs_set(0)
        curses.noecho()
        curses.cbreak()
        stdscr.keypad(True)
        curses.start_color()
        curses.use_default_colors()
        # green
        curses.init_pair(10, curses.COLOR_GREEN, -1)                 # green
        GREEN_COLOR = curses.color_pair(10)

        # headers
        curses.init_pair(11, curses.COLOR_WHITE, curses.COLOR_BLUE)
        HEADER_COLOR = curses.color_pair(11)

        # dots
        curses.init_pair(12, curses.COLOR_RED, -1)   # red on default
        DOT_COLOR    = curses.color_pair(12)

        # row highlight
        curses.init_pair(13, curses.COLOR_WHITE, curses.COLOR_BLACK)
        ROW_COLOR    = curses.color_pair(13)

        current_row, current_col = 0, 0
        while True:
            stdscr.clear()
            max_y, max_x = stdscr.getmaxyx()
            rows_visible = max_y - 6

            # Instructions
            instr = ' [←↑→↓ navigate ] [Enter edit]  [F2 save]  [Esc quit] '
            stdscr.addstr(0, 0, instr[:max_x])

            # Determine visible columns with frozen first column
            desired_cols = 5
            
            # Column width: make sure we can only fit ~desired_cols of them
            max_hdr = max(len(c) for c in col_names)
            col_w   = max(max_hdr + 2, max_x // desired_cols)

            max_fit = max_x // col_w
            cols_vis = min(num_cols, max(desired_cols, max_fit))
            # Number of scrollable slots excluding frozen col
            scroll_slots = max(1, cols_vis - 1)
            # Determine scroll window on cols [1..]
            col_start = min(max(1, current_col - scroll_slots//2), max(1, num_cols - scroll_slots))
            col_end = min(num_cols, col_start + scroll_slots)
            visible_cols = [0] + list(range(col_start, col_end))

            # Draw headers
            for j, col in enumerate(visible_cols):
                x = j * col_w
                hdr = col_names[col][:col_w-1].ljust(col_w-1)
                stdscr.attron(HEADER_COLOR)
                # stdscr.addstr(1, x, hdr[:max_x-x])
                stdscr.addstr(1, x, hdr[:max_x-x])
                stdscr.attroff(HEADER_COLOR)

            # Determine visible rows
            row_start = max(0, current_row - rows_visible//2)
            row_end = min(num_rows, row_start + rows_visible)

            # Draw cells
            for i, row in enumerate(range(row_start, row_end), start=2):
                y = i
                if y >= max_y:
                    break
                for j, col in enumerate(visible_cols):
                    x     = j*col_w
                    cell  = str(df.iat[row, col])
                    avail = max_x - x
                    if avail <= 0:
                        continue
                    width = min(col_w-1, avail)

                    # pick your row color + reverse for cursor
                    attr = ROW_COLOR if row == current_row else 0
                    if row == current_row and col == current_col:
                        attr |= curses.A_REVERSE

                    # too long?
                    if len(cell) > width:
                        if width >= 4:
                            base = cell[:width-3]
                            stdscr.addstr(y, x, base, attr)
                            stdscr.addstr(y, x+len(base), '...', attr | DOT_COLOR)
                        else:
                            # narrow sliver: just show what fits
                            stdscr.addstr(y, x, cell[:width], attr)
                    else:
                        text = cell.ljust(width)
                        stdscr.addstr(y, x, text, attr)

            # Status line
            status = f"Row {current_row+1}/{num_rows}, Col {current_col+1}/{num_cols} ({col_names[current_col]})"
            stdscr.addstr(rows_visible+3, 0, status[:max_x])
            stdscr.refresh()

            key = stdscr.get_wch() if hasattr(stdscr, 'get_wch') else stdscr.getch()
            # Navigation
            if key == curses.KEY_DOWN:
                current_row = min(num_rows-1, current_row+1)
            elif key == curses.KEY_UP:
                current_row = max(0, current_row-1)
            elif key == curses.KEY_RIGHT:
                current_col = min(num_cols-1, current_col+1)
            elif key == curses.KEY_LEFT:
                current_col = max(0, current_col-1)

            # Enter to edit
            elif key in (curses.KEY_ENTER, 10, 13) or (isinstance(key, str) and key in ('\n','\r')):
                prompt = f"Edit {col_names[current_col]}: "
                stdscr.move(rows_visible+4, 0)
                stdscr.clrtoeol()
                stdscr.addstr(rows_visible+4, 0, prompt[:max_x])
                win = curses.newwin(1, max_x - len(prompt), rows_visible+4, len(prompt))
                win.keypad(True)
                curses.curs_set(1)
                # Inline edit buffer
                buf = list(str(df.iat[current_row, current_col]))
                pos = len(buf)
                win.clear(); win.addstr(0, 0, ''.join(buf)); win.move(0, pos); win.refresh()
                while True:
                    ch = win.get_wch() if hasattr(win, 'get_wch') else win.getch()
                    if ch in (10, 13) or (isinstance(ch, str) and ch in ('\n','\r')):
                        break
                    elif ch == curses.KEY_LEFT and pos>0:
                        pos -= 1
                    elif ch == curses.KEY_RIGHT and pos<len(buf):
                        pos += 1
                    elif ch in (curses.KEY_BACKSPACE, '\b', '\x7f') and pos>0:
                        del buf[pos-1]; pos -= 1
                    elif isinstance(ch, str) and ch.isprintable():
                        buf.insert(pos, ch); pos += 1
                    # redraw
                    win.clear(); win.addstr(0,0,''.join(buf)); win.move(0,pos); win.refresh()
                curses.curs_set(0)
                df.iat[current_row, current_col] = ''.join(buf)

            # F2 to save
            elif key == curses.KEY_F2:
                stdscr.addstr(rows_visible+5, 0, f"Updating values...", GREEN_COLOR)
                stdscr.refresh(); curses.napms(800)
                mgr.update_fabrication_prices()
                mgr.save()
                stdscr.addstr(rows_visible+5, 0, f"Saved to {path}"[:max_x], GREEN_COLOR)
                stdscr.refresh(); curses.napms(800)
                break

            # Esc to exit
            elif key in (27, curses.KEY_EXIT) or (isinstance(key, str) and key=='\x1b'):
                break

    wrapper(_main)

