# 23XT CAN Bus Spec

Machine-readable version: [23xt.dbc](23xt.dbc). It loads in SavvyCAN, cantools (Python) and most CAN tools. **Update both files together.**

## Bus

| Setting | Value |
|---|---|
| Bitrate | 125 kbit/s |
| IDs | Standard 11-bit |
| Byte order | Little-endian (Intel) for all multi-byte signals |
| Signed values | Two's complement |

Lower IDs win arbitration, so time-critical data gets lower IDs.

## ID allocation

| Range | Owner |
|---|---|
| `0x120`–`0x12F` | mag_board |
| `0x7A0`–`0x7AF` | mag_board CAN test suite (test builds only, never on the car) |

Claim a range here before adding a new board.

## mag_board

Source: [firmware/stm32/prod/mag_board](../stm32/prod/mag_board). Sensor: TMAG5170A2, ±150 mT range.

Up to 4 boards share the bus, all running the same firmware. Each board's node number (0–3) is stored in the STM32 option byte `Data0` and picks its IDs:

| Node | MAG_FIELD | MAG_STATUS |
|---|---|---|
| 0 | `0x120` | `0x128` |
| 1 | `0x121` | `0x129` |
| 2 | `0x122` | `0x12A` |
| 3 | `0x123` | `0x12B` |

Set it once per board, in bootloader mode (hold BOOT, tap RESET, release BOOT):

```
STM32_Programmer_CLI -c port=usb1 -ob Data0=<node>
```

It survives reflashing. A board whose `Data0` is not 0–3 (a new chip reads `0xFF`) sends no MAG_FIELD, and sends MAG_STATUS on `0x12F` with `StartupFailMask` bit 15 (`NODE_ID`) set, so it never collides with a configured board.

The app retransmits frames that lose arbitration and recovers from bus-off on its own.

### `0x120`–`0x123` MAG_FIELD: 100 Hz, 8 bytes

Average of 10 X/Y/Z samples taken at 1 kHz.

| Byte | Signal | Type | Scale | Range |
|---|---|---|---|---|
| 0–1 | `Bx` | int16 | 0.01 mT | ±327.67 mT |
| 2–3 | `By` | int16 | 0.01 mT | ±327.67 mT |
| 4–5 | `Bz` | int16 | 0.01 mT | ±327.67 mT |
| 6 | `Counter` | uint8 | — | +1 per frame, wraps. A gap means dropped frames |
| 7 | `status` | bits | — | see below |

`status` bits:

| Bit | Name | Meaning |
|---|---|---|
| 0 | `SensorFault` | Sensor reported an error, reset, or is not configured. Field values may be stale or 0 |
| 1 | `SpiError` | SPI transfer or CRC error this window |
| 2 | `SamplesMissing` | Fewer than 10 valid samples averaged. 0 valid samples gives 0 mT |
| 3 | `Saturated` | An axis hit full scale (±150 mT) in at least one sample. The value is clipped |
| 4 | `StartupFailed` | MCU configuration checks failed at boot, see `MAG_STATUS.StartupFailMask` |
| 5–7 | — | Reserved, 0 |

**Use the field values only when bits 0–2 are clear.**

### `0x128`–`0x12B` MAG_STATUS: 1 Hz, 8 bytes (`0x12F` if no node number)

| Byte | Signal | Type | Meaning |
|---|---|---|---|
| 0–3 | `StartupFailMask` | uint32 | `startup_fail_t` bits from [startup_checks.h](../stm32/prod/mag_board/include/startup_checks.h). 0 = OK |
| 4–5 | `RangeFullScale` | uint16 | Configured full scale in mT (150) |
| 6 | `SpiErrors` | uint8 | SPI errors in the last second, saturates at 255 |
| 7 | `MissedSamples` | uint8 | 1 kHz slots with no valid sample in the last second, saturates at 255 |

### `0x7A0`–`0x7A3` CAN test suite

Only sent by firmware built with `-DMAG_BOARD_TEST_SUITE=CAN`.

| ID | Message | When |
|---|---|---|
| `0x7A0` | Summary: `[0..3]` startup_fail_mask, `[4]` failed count, `[5]` test count, `[6]` sensor power-up flag | 1 Hz |
| `0x7A1` | Result: `[0]` test id, `[1]` result code, `[2..5]` detail | 1 Hz, one per test that ran |
| `0x7A2` | Loopback test frame | Internal only (silent loopback), never on the bus |
| `0x7A3` | Bus test frame, data `CA FE` | Once at boot |

Test ids and result codes: [hw_results.h](../stm32/prod/mag_board/include/hw_results.h), also in the DBC.

## Bus load

| Message | Rate | Load at 125 kbit/s |
|---|---|---|
| MAG_FIELD | 100 Hz per board | ~11% per board (8-byte frame ≈ 135 bits worst case with stuffing) |
| MAG_STATUS | 1 Hz per board | ~0.1% per board |
| **4 boards** | | **~44%** |

44% leaves little headroom for other boards at 125 kbit/s. Before adding more traffic, raise the bitrate or lower the MAG_FIELD rate.
