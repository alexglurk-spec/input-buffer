"""Input Buffer Tuner

A small settings window that runs next to Geometry Dash. It writes config.txt,
which the Input Buffer mod re-reads while you play, so changes apply instantly.
It also reads status.txt from the mod to show whether the game is connected.

Needs Python 3 (tkinter is included with the standard Windows installer).
Run it with:  python buffer_tuner.py
"""

import os
import time

# Same folder the mod uses (see buffer_config.hpp).
if os.name == "nt":
    APP_DIR = os.path.join(os.environ.get("APPDATA", os.path.expanduser("~")), "GDInputBuffer")
else:
    APP_DIR = os.path.join(os.path.expanduser("~"), ".gd-input-buffer")

CONFIG_PATH = os.path.join(APP_DIR, "config.txt")
STATUS_PATH = os.path.join(APP_DIR, "status.txt")

MIN_MS, MAX_MS = 0, 150
DEFAULT = {"enabled": True, "window_ms": 50}
FRAME_MS_60 = 1000 / 60  # one frame at 60 FPS


def read_pairs(path):
    """Read a key=value text file into a dict. Returns {} if missing or unreadable."""
    pairs = {}
    try:
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                if "=" in line:
                    key, value = line.split("=", 1)
                    pairs[key.strip()] = value.strip()
    except OSError:
        pass
    return pairs


def load_config():
    cfg = dict(DEFAULT)
    pairs = read_pairs(CONFIG_PATH)
    if "enabled" in pairs:
        cfg["enabled"] = pairs["enabled"] == "1"
    if "window_ms" in pairs:
        try:
            cfg["window_ms"] = max(MIN_MS, min(MAX_MS, int(pairs["window_ms"])))
        except ValueError:
            pass
    return cfg


def save_config(cfg):
    """Write the config in one atomic step so the mod never reads half a file."""
    os.makedirs(APP_DIR, exist_ok=True)
    tmp = CONFIG_PATH + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        f.write("enabled=%d\n" % (1 if cfg["enabled"] else 0))
        f.write("window_ms=%d\n" % cfg["window_ms"])
    os.replace(tmp, CONFIG_PATH)


def read_status():
    """Returns (connected, buffered_count). Connected = mod wrote a heartbeat recently."""
    pairs = read_pairs(STATUS_PATH)
    try:
        heartbeat = int(pairs.get("heartbeat", "0"))
        buffered = int(pairs.get("buffered", "0"))
    except ValueError:
        return False, 0
    return (time.time() - heartbeat) < 3, buffered


def describe(ms):
    frames = ms / FRAME_MS_60
    return "%d ms  (about %.1f frames at 60 FPS)" % (ms, frames)


def main():
    import tkinter as tk
    from tkinter import ttk

    cfg = load_config()

    root = tk.Tk()
    root.title("GD Input Buffer")
    root.resizable(False, False)

    frame = ttk.Frame(root, padding=16)
    frame.grid(sticky="nsew")

    enabled_var = tk.BooleanVar(value=cfg["enabled"])
    window_var = tk.IntVar(value=cfg["window_ms"])
    top_var = tk.BooleanVar(value=False)
    label_var = tk.StringVar(value=describe(cfg["window_ms"]))
    status_var = tk.StringVar(value="Waiting for Geometry Dash...")
    count_var = tk.StringVar(value="Buffered clicks this session: 0")

    save_job = {"id": None}

    def do_save():
        save_job["id"] = None
        try:
            save_config({"enabled": enabled_var.get(), "window_ms": window_var.get()})
        except OSError as err:
            status_var.set("Could not save settings: %s" % err)

    def changed(*_):
        label_var.set(describe(window_var.get()))
        # Wait a moment so dragging the slider does not write the file 100 times.
        if save_job["id"] is not None:
            root.after_cancel(save_job["id"])
        save_job["id"] = root.after(150, do_save)

    def set_preset(ms):
        window_var.set(ms)
        changed()

    ttk.Checkbutton(frame, text="Enable input buffer", variable=enabled_var,
                    command=changed).grid(row=0, column=0, columnspan=4, sticky="w")

    ttk.Label(frame, text="Buffer length").grid(row=1, column=0, columnspan=4,
                                                sticky="w", pady=(14, 0))
    ttk.Label(frame, textvariable=label_var).grid(row=2, column=0, columnspan=4, sticky="w")

    slider = ttk.Scale(frame, from_=MIN_MS, to=MAX_MS, orient="horizontal", length=320,
                       command=lambda v: (window_var.set(int(float(v))), changed()))
    slider.set(cfg["window_ms"])
    slider.grid(row=3, column=0, columnspan=4, sticky="ew", pady=(4, 8))

    for col, frames in enumerate((1, 2, 3, 4)):
        ms = round(frames * FRAME_MS_60)
        text = "%d frame%s" % (frames, "" if frames == 1 else "s")
        ttk.Button(frame, text=text,
                   command=lambda m=ms: (slider.set(m), set_preset(m))).grid(
            row=4, column=col, padx=(0 if col == 0 else 4, 0), sticky="ew")

    ttk.Separator(frame).grid(row=5, column=0, columnspan=4, sticky="ew", pady=14)
    ttk.Label(frame, textvariable=status_var).grid(row=6, column=0, columnspan=4, sticky="w")
    ttk.Label(frame, textvariable=count_var).grid(row=7, column=0, columnspan=4, sticky="w")

    ttk.Checkbutton(frame, text="Keep this window on top", variable=top_var,
                    command=lambda: root.attributes("-topmost", top_var.get())).grid(
        row=8, column=0, columnspan=4, sticky="w", pady=(12, 0))

    def poll_status():
        connected, buffered = read_status()
        if connected:
            status_var.set("Connected: mod is running in Geometry Dash")
        else:
            status_var.set("Waiting for Geometry Dash (open a level)...")
        count_var.set("Buffered clicks this session: %d" % buffered)
        root.after(500, poll_status)

    # Write the settings once at startup so the mod and the window always agree.
    do_save()
    poll_status()
    root.mainloop()


if __name__ == "__main__":
    main()
