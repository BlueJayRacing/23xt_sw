"""
Real-time viewer for ChannelSampleBatch UDP packets.

Rewritten from realtime_dataview_mag.py.
"""

import socket
import sys
import threading
import time
from collections import deque

import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

sys.path.insert(0, "generated")
import channel_sample_pb2 as cs

# configs
UDP_PORT = 8888         
MAX_PACKET_SIZE = 2048  
PLOT_WINDOW = 1000
STATS_INTERVAL = 1.0
PLOT_UPDATE_INTERVAL = 100   
SOCKET_TIMEOUT = 0.25
SILENCE_TIMEOUT = 5.0

VOLTAGE_MAX = 5.0       
UINT24_MAX = 0xFFFFFF   

CHANNEL_NAMES = {
    0: "AIN0 - Ground reference",
    1: "AIN1 - 5V reference",
    2: "AIN2 - 2.5V reference",
    3: "AIN3 - 2.5V reference (buffered)",
    4: "AIN4 - Strain gauge 2",
    5: "AIN5 - Strain gauge 1",
    6: "AIN6 - Channel 1",
    7: "AIN7 - Channel 6",
    8: "AIN8 - Channel 2",
    9: "AIN9 - Channel 7",
    10: "AIN10 - Channel 3",
    11: "AIN11 - Channel 8",
    12: "AIN12 - Channel 4",
    13: "AIN13 - Channel 9",
    14: "AIN14 - Channel 5",
    15: "AIN15 - Channel 10",
    16: "DIN0 - Digital Input 0",
    17: "DIN1 - Digital Input 1",
    18: "DIN2 - Digital Input 2",
    19: "DIN3 - Digital Input 3",
    20: "DIN4 - Digital Input 4",
    21: "DIN5 - Digital Input 5",
    22: "MISC0 - System temperature",
    23: "MISC1 - Power supply",
    24: "MISC2 - CPU load",
    25: "MISC3 - Memory usage",
    26: "MISC4",
    27: "MISC5",
    28: "MISC6",
    29: "MISC7",
}

ANALOG_CHANNELS = [6, 8, 10, 12, 14, 7, 9, 11, 13, 15]  
DIGITAL_CHANNELS = [16, 17, 18, 19, 20, 21]            


def convert_to_voltage(value):
    """Convert a uint24 reading to volts on a 0-5V scale."""
    return (float(value) / UINT24_MAX) * VOLTAGE_MAX


channel_data = {
    ch: {"timestamps": deque(maxlen=PLOT_WINDOW),
         "values": deque(maxlen=PLOT_WINDOW)}
    for ch in ANALOG_CHANNELS + DIGITAL_CHANNELS
}

# timestamp of first sample
origin = {"timestamp": None}

stats = {"packets": 0, "samples": 0, "bytes": 0}

rate_history = deque(maxlen=300)
last_report = {"packets": 0, "samples": 0, "bytes": 0}

def process_batch(data):
    """Decode one message and file its samples by channel."""
    batch = cs.ChannelSampleBatch()
    batch.ParseFromString(data)

    if origin["timestamp"] is None and batch.samples:
        origin["timestamp"] = batch.samples[0].timestamp

    for s in batch.samples:
        ch = s.internal_channel_id
        if ch not in channel_data:
            continue 
        value = convert_to_voltage(s.value) if ch in ANALOG_CHANNELS else s.value
        channel_data[ch]["timestamps"].append(s.timestamp)
        channel_data[ch]["values"].append(value)

    stats["packets"] += 1
    stats["samples"] += len(batch.samples)
    stats["bytes"] += len(data)


def report_stats(elapsed, interval):
    """Data flow rates over the last interval"""
    d_packets = stats["packets"] - last_report["packets"]
    d_samples = stats["samples"] - last_report["samples"]
    d_bytes = stats["bytes"] - last_report["bytes"]

    print(f"{elapsed:6.1f}s | "
          f"{d_packets / interval:6.1f} pkt/s | "
          f"{d_samples / interval:7.0f} samples/s | "
          f"{d_bytes / interval / 1024:6.1f} KiB/s | "
          f"{stats['packets']} packets total")

    rate_history.append((elapsed,
                         d_packets / interval,
                         d_samples / interval,
                         d_bytes / interval / 1024))

    last_report.update(stats)


def receive_loop(port=UDP_PORT):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("", port))
    sock.settimeout(SOCKET_TIMEOUT)
    print(f"listening on port {port}")

    start = time.time()
    last_time = start
    next_report = start + STATS_INTERVAL

    last_packet = None
    port_silence = False

    while True:
        try:
            data, addr = sock.recvfrom(MAX_PACKET_SIZE)
        except socket.timeout:
            gap = time.time() - (last_packet or start)
            if gap > SILENCE_TIMEOUT and not port_silence:
                print(f"No packets received for {gap:.1f} seconds")
                port_silence = True
            pass
        else: 
            if port_silence:
                port_silence = False
                print(f"Packets resumed after {time.time() - last_packet:.1f} seconds")
            last_packet = time.time()
            try:
                process_batch(data)
            except Exception as e:
                print(f"Error processing batch from {addr}: {e}")

        now = time.time()
        if now >= next_report:
            report_stats(now - start, now - last_time)
            last_time = now
            next_report = now + STATS_INTERVAL


def setup_plots():
    """Three stacked panels, matching the original layout."""
    fig, (ax_analog, ax_digital, ax_speed) = plt.subplots(
        3, 1, figsize=(12, 10), gridspec_kw={"height_ratios": [2, 2, 1]})

    ax_analog.set_title("Analog channels")
    ax_analog.set_ylabel("Volts")
    ax_analog.set_ylim(0, VOLTAGE_MAX)
    ax_analog.grid(True, alpha=0.3)

    ax_digital.set_title("Digital channels (cumulative)")
    ax_digital.set_ylabel("Count")
    ax_digital.set_xlabel("Seconds since first sample")
    ax_digital.grid(True, alpha=0.3)

    # Only these two share an x-axis
    ax_digital.sharex(ax_analog)

    ax_speed.set_title("Data speed")
    ax_speed.set_xlabel("Seconds since listening started")
    ax_speed.set_ylabel("Rate")
    ax_speed.grid(True, alpha=0.3)

    # One Line2D per channel
    lines = {}
    for ch in ANALOG_CHANNELS:
        lines[ch], = ax_analog.plot([], [], lw=1.2, label=CHANNEL_NAMES[ch])
    for ch in DIGITAL_CHANNELS:
        lines[ch], = ax_digital.plot([], [], lw=1.2, label=CHANNEL_NAMES[ch])

    speed_lines = {
        "packets": ax_speed.plot([], [], lw=2, label="Packets/sec")[0],
        "samples": ax_speed.plot([], [], lw=2, label="Samples/sec")[0],
        "bytes": ax_speed.plot([], [], lw=2, label="KiB/sec")[0],
    }

    ax_analog.legend(loc="upper left", fontsize=7, ncol=2)
    ax_digital.legend(loc="upper left", fontsize=7, ncol=2)
    ax_speed.legend(loc="upper right", fontsize=8)
    fig.tight_layout()

    return fig, (ax_analog, ax_digital, ax_speed), lines, speed_lines


def update_plot(frame, axes, lines, speed_lines):
    """Redraw everything from the data that the receiver fills"""
    ax_analog, ax_digital, ax_speed = axes

    t0 = origin["timestamp"]
    if t0 is None:
        return []  # nothing received yet

    for ch, line in lines.items():
        timestamps = list(channel_data[ch]["timestamps"])
        values = list(channel_data[ch]["values"])

        # matplotlib rejects mismatched lengths.
        n = min(len(timestamps), len(values))
        if n == 0:
            continue

        # seconds since the first sample
        line.set_data([(t - t0) / 1e6 for t in timestamps[:n]], values[:n])

    if rate_history:
        history = list(rate_history)
        elapsed = [row[0] for row in history]
        speed_lines["packets"].set_data(elapsed, [row[1] for row in history])
        speed_lines["samples"].set_data(elapsed, [row[2] for row in history])
        speed_lines["bytes"].set_data(elapsed, [row[3] for row in history])
        ax_speed.relim()
        ax_speed.autoscale_view()

    ax_analog.relim()
    ax_analog.autoscale_view(scaley=False)
    ax_digital.relim()
    ax_digital.autoscale_view()

    return list(lines.values()) + list(speed_lines.values())


if __name__ == "__main__":
    # receiving loop runs in background thread
    threading.Thread(target=receive_loop, daemon=True).start()

    fig, axes, lines, speed_lines = setup_plots()

    ani = FuncAnimation(fig, update_plot,
                        fargs=(axes, lines, speed_lines),
                        interval=PLOT_UPDATE_INTERVAL,
                        blit=False, cache_frame_data=False)

    plt.show()
