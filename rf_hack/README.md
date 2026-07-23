# Pico RF tester

This firmware can monitor raw OOK/ASK pulses either through a CC1101 or through
a low-cost 315 MHz ASK receiver module.

## 315 MHz receiver wiring

The receiver in the linked kit requires 5 V. A Raspberry Pi Pico GPIO is not
5 V tolerant, so do not connect the receiver DATA output directly.

| Receiver | Connect to |
| --- | --- |
| VCC | Pico VBUS (5 V, only while Pico is USB-powered) |
| GND | Pico GND |
| DATA | 10 kOhm resistor, then GPIO22 |

Also connect a 20 kOhm resistor between GPIO22 and GND. The 10 kOhm/20 kOhm
divider reduces a 5 V DATA signal to about 3.3 V.

The receiver may have two DATA pins; they are normally the same signal, so use
either one. Check the labels printed on the actual board before applying power,
because pin order varies between module revisions.

For better reception, attach a quarter-wave wire antenna approximately 23.8 cm
long to the receiver's ANT pad.

## Selecting a mode

Edit `MODE` near the top of `src/main.cpp`:

- `DirectAsk315Monitor`: 315 MHz receiver DATA on GPIO22
- `PassiveKeyMonitor`: CC1101 asynchronous OOK monitor at 433.92 MHz
- `TestTransmitter` / `TestResponder`: CC1101 packet link test

`DirectAsk315Monitor` is selected by default. Open the Pico USB serial output
and press a 315 MHz remote button. Each received burst is printed as alternating
high/low pulse durations in microseconds. Some output while no transmitter is
active is expected from this inexpensive super-regenerative receiver.

## Transmitter module

The transmitter accepts a 3.3 V GPIO DATA signal directly and can be powered
from 3.3 V to 12 V. Its current firmware path is receive/analysis only; capture
a known remote waveform before adding a transmit/replay path. Observe local
radio regulations and only transmit codes for equipment you own or are
authorized to test.
