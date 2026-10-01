"""
Real-time viewer for digital channel samples.

Feed data through add_samples() 
"""

from collections import deque

import argparse
import csv
import matplotlib.pyplot as plt
import math
import os
import time
from matplotlib.animation import FuncAnimation

# configs
PLOT_WINDOW = 1000
PLOT_UPDATE_INTERVAL = 100
COUNT_TO_RPM = 60 / 33 
RPM_WINDOW_S = 5 # in seconds
RPM_TO_MPH = math.pi * 23 * 60 / 63360

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

DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")
csv_writers = {}

def open_csvs():
    """Create csv files."""
    os.makedirs(DATA_DIR, exist_ok=True)
    run_name = time.strftime("%Y-%m-%d_%H-%M-%S")
    for ch in CHANNEL_NAMES:
        f = open(os.path.join(DATA_DIR, f"{run_name}_ch{ch}.csv"), "w", newline="", buffering=1)
        csv_writers[ch] = csv.writer(f)
        csv_writers[ch].writerow(["timestamp_us", "value"])

def add_samples(samples):
    """File samples by channel. Samples should have channel, timestamp, and value."""
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

        if ch in csv_writers:
            csv_writers[ch].writerow([s.timestamp, s.value])


def setup_plot():
    fig, ax = plt.subplots(figsize=(12, 6))

    ax.set_title("Digital channels")
    ax.set_ylabel("Count")
    ax.set_xlabel("Seconds since first sample")
    ax.grid(True, alpha=0.3)

    lines = {}
    for ch in CHANNEL_NAMES:
        lines[ch], = ax.plot([], [], lw=1.2)
        lines[ch].set_label(f"Channel {ch}: No data received")
    fig.tight_layout()

    ax.legend(loc="upper left")

    return fig, ax, lines

def setup_speed_plot(unit):
    """Set up plot for both mph and rpm."""
    fig, ax = plt.subplots(figsize=(12, 6))

    ax.set_title(f"{unit} Plot")
    ax.set_ylabel(unit)
    ax.set_xlabel("Seconds since first sample")
    ax.grid(True, alpha=0.3)

    lines = {}
    for ch in {17}:
        lines[ch], = ax.plot([], [], lw=1.2, label=CHANNEL_NAMES[ch])
        lines[ch].set_label(f"{unit}: No data received")
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

def compute_rpm(timestamps, values):
    """Computer RPM for each sample."""
    window_us = RPM_WINDOW_S * 1e6

    rpm_times = []
    rpms = []

    j = 0
    for i in range(len(timestamps)):
        # calculates derivative over a window of time
        while timestamps[i] - timestamps[j] > window_us:
            j += 1

        if i == j:
            continue

        delta_counts = values[i] - values[j]
        delta_us = timestamps[i] - timestamps[j]

        counts_per_sec = delta_counts / (delta_us / 1e6)

        rpm_times.append(timestamps[i])
        rpms.append(counts_per_sec * COUNT_TO_RPM)

    return rpm_times, rpms


def update_rpm_plot(frame, ax, lines):
    """Redraw rpm plot."""
    t0 = origin["timestamp"]
    if t0 is None:
        return []  # nothing received yet

    for ch, line in lines.items():
        timestamps = list(channel_data[ch]["timestamps"])
        values = list(channel_data[ch]["values"])

        rpm_times, rpms = compute_rpm(timestamps, values)
        if len(rpms) == 0:
            continue

        line.set_label(f"RPM: {rpms[-1]:.0f}")

        # seconds since the first sample
        line.set_data([(t - t0) / 1e6 for t in rpm_times], rpms)

    ax.legend(loc="upper left")
    ax.relim()
    ax.autoscale_view()

    return list(lines.values())

def update_mph_plot(frame, ax, lines):
    """Redraw mph plot."""
    t0 = origin["timestamp"]
    if t0 is None:
        return []  # nothing received yet

    for ch, line in lines.items():
        timestamps = list(channel_data[ch]["timestamps"])
        values = list(channel_data[ch]["values"])

        rpm_times, rpms = compute_rpm(timestamps, values)
        if len(rpms) == 0:
            continue

        mph = [x * RPM_TO_MPH for x in rpms]
        line.set_label(f"MPH: {mph[-1]:.2f}")

        # seconds since the first sample
        line.set_data([(t - t0) / 1e6 for t in rpm_times], mph)

    ax.legend(loc="upper left")
    ax.relim()
    ax.autoscale_view()

    return list(lines.values())



def start():
    """Open the window for plot."""
    parser = argparse.ArgumentParser()
    # Run with python dataview.py --no-running-total to see counts per sample
    parser.add_argument("--running_total", action = argparse.BooleanOptionalAction, default = True)
    # Run with --no-save_csv to skip writing csvs to software/data/
    parser.add_argument("--save_csv", action = argparse.BooleanOptionalAction, default = True)
    args = parser.parse_args()

    if args.save_csv:
        open_csvs()

    fig, ax, lines = setup_plot()
    ani = FuncAnimation(fig, update_plot,
                        fargs=(ax, lines, args.running_total),
                        interval=PLOT_UPDATE_INTERVAL,
                        blit=False, cache_frame_data=False)

    fig2, ax2, lines2 = setup_speed_plot("RPM")
    ani2 = FuncAnimation(fig2, update_rpm_plot,
                        fargs=(ax2, lines2),
                        interval=PLOT_UPDATE_INTERVAL,
                        blit=False, cache_frame_data=False)

    fig3, ax3, lines3 = setup_speed_plot("MPH")
    ani3 = FuncAnimation(fig3, update_mph_plot,
                        fargs=(ax3, lines3),
                        interval=PLOT_UPDATE_INTERVAL,
                        blit=False, cache_frame_data=False)
    plt.show()