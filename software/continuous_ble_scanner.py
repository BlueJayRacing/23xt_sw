import asyncio
from bleak import BleakScanner
from dataclasses import dataclass

from bleak.args.bluez import BlueZScannerArgs, OrPattern
from bleak.assigned_numbers import AdvertisementDataType

filter_settings = BlueZScannerArgs(
    or_patterns = [OrPattern(0, AdvertisementDataType.MANUFACTURER_SPECIFIC_DATA, b'\x47\x87'), ] #[OrPattern(0, 0xFF, b'\x47\x87')]
)

SAMPLE_SIZE = 13
SAMPLES_PER_MESSAGE = 10

@dataclass
class ChannelSample:
    channel: int
    timestamp: int
    value: int

def deserialize_sample(sample) -> ChannelSample:
    ret = ChannelSample(sample[0], int.from_bytes(sample[1:9], "little"), int.from_bytes(sample[9:13], "little"))

    return ret

def process_samples(samples):
    for sample in samples:
        # pass
        print(sample)


def on_device_discovery_callback(device, advertisement_data):
    # Print details about device and the advertisement packet it sent out

    all_bytes = b''
    samples = []
    for key, val in advertisement_data.manufacturer_data.items():
        if key == 0x8747:
            print(device)
            all_bytes += val
    
    for i in range(4, len(all_bytes), SAMPLE_SIZE):
        samples.append(deserialize_sample(all_bytes[i: i + SAMPLE_SIZE]))

    process_samples(samples)


async def main():
    # When BleakScanner finds a device, it will send the device data
    # as arguments to on_device_discovery_callback
    scanner = BleakScanner(on_device_discovery_callback)#, scanning_mode = "passive", bluez = filter_settings)

    # Start the scanner, print results continuously for 60 seconds, then stop.
    # You can change the sleep() value to however many seconds you want.
    await scanner.start()
    await asyncio.Future()
    # await asyncio.sleep(60.0)
    # await scanner.stop()


asyncio.run(main())

