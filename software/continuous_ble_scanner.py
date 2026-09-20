import asyncio
from bleak import BleakScanner
from dataclass import dataclass

SAMPLE_SIZE = 13
SAMPLES_PER_MESSAGE = 19

@dataclass
class ChannelSample:
    channel: int
    timestamp: int
    value: int

def deserialize_sample(sample) -> ChannelSample:
    ret = ChannelSample()

    ret.channel = int.from_bytes(sample[0])
    ret.timestamp = int.from_bytes(sample[1:9], "little")
    ret.value = int.from_bytes(sample[9:13], "little")

    return ret

def process_samples(sample: ChannelSample):
    for sample in samples:
        print(sample)

def on_device_discovery_callback(device, advertisement_data):
    # Print details about device and the advertisement packet it sent out

    samples = []
    for key, val in advertisement_data.manufacturer_data.items():
        if key == 0x8747:
            for i in range(SAMPLES_PER_MESSAGE):
                samples.append(deserialize_sample(advertisement.manufacturer_data[key][i * SAMPLE_SIZE: (i + 1) * SAMPLE_SIZE]))

    process_samples(samples)


async def main():
    # When BleakScanner finds a device, it will send the device data
    # as arguments to on_device_discovery_callback
    scanner = BleakScanner(on_device_discovery_callback)

    # Start the scanner, print results continuously for 60 seconds, then stop.
    # You can change the sleep() value to however many seconds you want.
    await scanner.start()
    await asyncio.Future()
    # await asyncio.sleep(60.0)
    # await scanner.stop()


asyncio.run(main())

