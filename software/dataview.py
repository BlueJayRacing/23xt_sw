"""
Real-time viewer for digital channel samples.

Feed data through add_samples() 
"""

from collections import deque

import argparse
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# configs
PLOT_WINDOW = 1000
PLOT_UPDATE_INTERVAL = 100

CHANNEL_NAMES = {
    16: "DIN0 - Digital Input 0",
    17: "DIN1 - Digital Input 1",
    # 18: "DIN2 - Digital Input 2",
    # 19: "DIN3 - Digital Input 3",
    # 20: "DIN4 - Digital Input 4",
    # 21: "DIN5 - Digital Input 5",
}

channel_data = {
    ch: {"timestamps": deque(maxlen=PLOT_WINDOW),
         "values": deque(maxlen=PLOT_WINDOW)}
    for ch in CHANNEL_NAMES
}

origin = {"timestamp": None}
origin_data = {}

# newest timestamp seen per channel
last_ts = {}

def add_samples(samples):
    """File samples by channel. Samples should have channel, timestamp, and value"""
    for s in samples:
        ch = s.channel
        if ch not in channel_data:
            continue

        # drop repeats, out-of-order samples, and the zeroed trailing slot
        if s.timestamp <= last_ts.get(ch, 0):
            continue
        last_ts[ch] = s.timestamp

        if origin["timestamp"] is None:
            origin["timestamp"] = s.timestamp

        if ch not in origin_data and origin["timestamp"] is not None:
            origin_data[ch] = s.value

        channel_data[ch]["timestamps"].append(s.timestamp)
        channel_data[ch]["values"].append(s.value)


def setup_plot():
    fig, ax = plt.subplots(figsize=(12, 6))

    ax.set_title("Digital channels")
    ax.set_ylabel("Count")
    ax.set_xlabel("Seconds since first sample")
    ax.grid(True, alpha=0.3)

    lines = {}
    for ch in CHANNEL_NAMES:
        lines[ch], = ax.plot([], [], lw=1.2, label=CHANNEL_NAMES[ch])
        lines[ch].set_label(f"Channel {ch}: No data received")
    fig.tight_layout()

    ax.legend(loc="upper left")

    return fig, ax, lines


def update_plot(frame, ax, lines, running_total):
    """Redraw plot."""
    t0 = origin["timestamp"]
    if t0 is None:
        return []  # nothing received yet

    for ch, line in lines.items():
        timestamps = list(channel_data[ch]["timestamps"])
        values = list(channel_data[ch]["values"])

        counts = [x - origin_data[ch] for x in values]
        if running_total:
            counts = values

        if (len(counts)) > 0:
            cur_y = counts[-1]
            if running_total:
                line.set_label(f"Channel {ch} total: {cur_y}")
            else:
                line.set_label(f"Channel {ch}: {cur_y}")
        # matplotlib rejects mismatched lengths.
        n = min(len(timestamps), len(values))
        if n == 0:
            continue

        # seconds since the first sample
        line.set_data([(t - t0) / 1e6 for t in timestamps[:n]], counts)
    ax.legend(loc="upper left")
    ax.relim()
    ax.autoscale_view()

    return list(lines.values())


def start():
    """Open the window for plot"""
    parser = argparse.ArgumentParser()
    parser.add_argument("--running_total", action = argparse.BooleanOptionalAction, default = True)
    args = parser.parse_args()
    fig, ax, lines = setup_plot()
    running_total = False
    ani = FuncAnimation(fig, update_plot,
                        fargs=(ax, lines, args.running_total),
                        interval=PLOT_UPDATE_INTERVAL,
                        blit=False, cache_frame_data=False)
    plt.show()