# LG Wall Controller Protocol

Some notes and documentation on the protocol used by wired wall controllers. This is largely based on reverse-engineering the LG PREMTB001 controller.
Most of these things haven't been tested with an actual AC unit.

NOTE: as explained in the [README](./README.md), this controller actually supports two different protocols. This only documents the more modern protocol that's used by my own AC.

Some information is based on:
* This post and comments: https://www.instructables.com/Hacking-an-LG-Ducted-Split-for-Home-Automation/
* Corresponding source code: https://github.com/AussieMakerGeek/LG_Aircon_MQTT_interface

## Overview
LG controllers use a (very slow) 104 bps 8N1 serial connection over a three-wire cable (Red = 12V, Yellow = Signal, Black = GND) connected to the CN-REMO port on the indoor unit's PCB.
The HVAC unit and controller will often send the same message twice (or even four times) with a short delay between them, but this is not required.

## Collisions
Collisions on the bus are possible because devices can send at arbitrary times, for example immediately after a setting is changed. My custom controller tries to
prevent this by ensuring the RX pin is high for at least 500 ms before sending. This works well (I haven't seen any collisions in practice) and the PREMTB001
controller seems to do something similar.

My custom controller also checks that each message we send is transmitted uncorrupted and else it tries to send it again. This works because it's a single wire
bus, so the controller always receives its own messages too.

## Message format
Each message is 13 bytes long. The upper four bits of the first byte indicate the sender:
* Master controller uses 0xAx, for example 0xA8 or 0xAA.
* Slave controller uses 0x2x.
* AC unit uses 0xCx.

The lower four bits of this byte are used for the message type. For example 0xC8 means it's a status message sent by the AC unit. 

The last byte of each message is a checksum, computed by adding up the other bytes and XOR'ing with 0x55.

## Status message (0xA8/0xC8/0x28)
Status messages are used to control all of the basic settings such as operation mode, fan mode, room temperature, etc.

The controller will send this when a setting changes or else every 20 seconds. For the AC unit this is every 60 seconds.

Format:
| Byte | Description |
| --- | --- |
| 0 | Message type, 0xA8/0xC8/0x28 |
| 1 | 0000_000X: set when settings were changed |
|   | 0000_00X0: 1: unit is on, 0: unit is off |
|   | 000X_XX00: operation mode (0: cooling, 1: dehumidify, 2: fan, 3: auto, 4: heating) |
|   | XXX0_0000: fan mode (0: low, 1: medium, 2: high, 3: auto, 4: slow, 5: low-medium, 6: medium-high, 7: power)
| 2 | 0000_000X: if set, controller shows Filter sign |
|   | 0000_00X0: controller sets this when clearing Filter in settings? |
|   | 0000_0X00: plasma / air-purifier is on |
|   | 0000_X000: humidifier |
|   | 000X_0000: resistive heater |
|   | 00X0_0000: swirl |
|   | 0X00_0000: horizontal swing |
|   | X000_0000: vertical swing |
| 3 | 0000_000X: external ventilation is on |
|   | 0000_00X0: Fan Auto function |
|   | 0000_0X00: defrost |
|   | 0000_X000: preheat |
|   | 000X_0000: active reservation |
|   | XXX0_0000: unknown (might be elevation grill state) |
| 4 | 000X_0000: plasma sign blinks if plasma on |
|   | other bits unknown (seems related to linked control) |
| 5 | 0000_000X: add 0.5 to target temperature in byte 6 |
|   | 0000_00X0: energy saving function |
|   | 0000_0X00: outdoor unit is active |
|   | 0XXX_X000: unknown (instructables post mentions one bit per zone 1-4) |
|   | X000_0000: unknown |
| 6 | 0000_XXXX: `target_temperature - 15` (example: 4 => 19C or 19.5C depending on byte 5) In AI mode: -2 to +2 stored as 0-4 |
|   | 00XX_0000: thermistor installer setting (0: unit, 1: controller, 2: 2TH) |
|   | XX00_0000: ceiling height installer setting (0: medium, 1: low, 2: high, 3: very high) |
| 7 | 00XX_XXXX: `(room_temperature - 10) * 2` (example: 27 => `27 / 2 + 10` => 23.5C) |
|   | 0X00_0000: set by AC when any indoor unit is in cooling mode |
|   | X000_0000: set by AC when any indoor unit is in heating mode |
| 8 | used for reservation data and other things (see below) |
|   | 0X00_0000: if set, the AC unit will send a batch with all of its settings (other message types below) and clear it. LG controller sets this when powered on. |
|   | X000_0000: set temporarily for "release 3 minute delay" installer setting 10 |
| 9 | used for reservation data and other things (see below) |
| 10 | 0000_000X: set temporarily for installer setting 1 (test run) |
|    | 0000_X000: active robot clean |
|    | 000X_0000: active auto clean |
|    | other bits unknown |
| 11 | error value, 0 = no error |
| 12 | Checksum |

### Reservation settings
Timer-related settings use the reservation flag in byte 3 with details in bytes 8 and 9. For example when the controller sets a "simple reservation" to turn on/off after N hours, it stores the number of minutes in byte 9 (and low bit of byte 8 if value > 255):
```
1 hour : a8 43 00 10 00 00 03 1d 28 3c 00 00 => 0x3c => 60 minutes
7 hours: a8 43 00 10 00 00 03 1d 29 a4 00 00 => 0x100 + 0xa4 => 420 minutes
```
The sleep timer option is similar, but sets byte 8 to 0x18/0x19 instead of 0x28/0x29.

## Capabilities message (0xC9)
The AC sends this to the controller when it's powered on to tell it which features are supported.
| Byte | Description |
| --- | --- |
| 0 | Message type, 0xC9 |
| 1 | 0000_XXXX: unknown |
|   | 000X_0000: supports zone state installer setting |
|   | 00X0_0000: supports swirl |
|   | XX00_0000: supports horizontal/vertical swing if both are set |
| 2 | 0000_000X: supports Fan Auto sub function |
|   | 0000_00X0: supports plasma (air purifier) |
|   | 0000_0X00: supports humidifier |
|   | 0000_X000: supports operation mode Auto |
|   | 000X_0000: supports operation mode AI |
|   | 00X0_0000: supports operation mode Heating |
|   | 0X00_0000: supports operation mode Fan |
|   | X000_0000: supports operation mode Dehumidify |
| 3 | supported fan options. auto (0x1), power (0x2, available in heating mode: 0x80?), medium (0x8), low (0x10?), slow (0x20?), high (always available?) |
| 4 | 0000_000X: supports vertical vane control |
|   | 0000_00X0: supports ESP value installer setting |
|   | 0000_0X00: supports static pressure installer setting |
|   | 0000_X000: supports ceiling height installer setting |
|   | 00XX_0000: unknown
|   | 0X00_0000: supports robot clean setting |
|   | X000_0000: supports auto clean setting |
| 5 | 0000_000X: supports energy saving sub function |
|   | 0000_00X0: supports override master/slave installer setting |
|   | 0000_0X00: supports auto change temperature setting |
|   | 000X_X000: unknown
|   | 00X0_0000: half degrees C not supported |
|   | 0X00_0000: has single vane |
|   | X000_0000: has two vanes |
| 6 | 0000_000X: supports extra airflow option (presence based?) |
|   | 0000_0XX0: unknown
|   | 0000_X000: supports low-medium fan option |
|   | 000X_0000: supports medium-high fan option |
|   | 00X0_0000: supports minimum cooling target temperature of 16 instead of 18 |
|   | XX00_0000: unknown |
| 7 | 0000_0X00: supports auxiliary heater, installer setting |
|   | 000X_X000: supports setting for zone = 1 to N (this value) in settings. Unclear what this is for |
|   | X000_0000: supports over heating, installer setting 15 |
| 8 | 0000_000X: supports unknown installer setting 16 |
|   | 0000_00X0: supports centrigrade installer setting (0.5C or 1C) |
|   | 0000_XX00: unknown |
|   | 000X_0000: supports emergency heater installer setting |
|   | 00X0_0000: supports group control installer setting |
|   | 0X00_0000: supports unit information setting |
|   | X000_0000: supports clear filter timer in settings |
| 9 | 0000_00X0: supports indoor unit address check, installer setting 26 |
|   | 0000_0X00: supports over cooling, installer setting 27 |
|   | 000X_0000: supports energy usage setting |
|   | X000_0000: supports refrigerant leak detector installation, installer setting 29 |
| 10 | 0000_00X0: supports static pressure step, installer setting 32 |
|    | 0000_0X00: supports indoor unit Wifi AP setting |
|    | 0X00_0000: supports fan cooling mode thermal off, installer setting 35 |
|    | X000_0000: supports use primary heater control, installer setting 36 |
| 11 | 0000_000X: supports indoor unit auto start installer setting |
|    | 0000_00X0: supports AC fan interlocked with ventilation installer setting |
|    | 0000_0X00: supports himalaya cool sub function |
|    | 0000_X000: supports monsoon comfort option |
|    | 000X_0000: supports mosquito away sub function |
|    | 00X0_0000: supports hum+e sub function |
|    | 0X00_0000: unknown |
|    | X000_0000: supports simple dry contact installer setting 41 |
| 12 | Checksum |

## Advanced settings (0xAA/0xCA)
Type 0xA messages store more advanced settings and are only sent when the AC is powered on or when changing settings.

| Byte | Description |
| --- | --- |
| 0 | Message type, 0xAA or 0xCA |
| 1 | unit address (see installer setting 2) |
| 2-6 | fan speeds for slow/low/med/high/power (see installer setting 3) |
| 7-8 | vertical vane positions from 1 to 6: 0x5555 for 4 vanes set to position 5 | 
| 9 | 0000_0XXX: auto change temperature setting 1-6 |
|   | X000_0000: auxiliary heater, installer setting 25 (0: not installed, 1: installed) |
| 10 | unknown |
| 11 | 0000_000X: dry contact mode installer setting 9 (1: auto) |
|    | 0000_00X0: zone state installer setting 11 (0: variable, 1: fixed) |
|    | 0000_0X00: robot clean auto |
|    | 0000_X000: auto clean enabled |
|    | XXXX_0000: unknown |
| 12 | Checksum |

## More settings (0xAB/0xCB)
0xB stores more information and settings (whether Wifi AP mode is on, installer settings, etc). There's some information in the comments of https://www.instructables.com/Hacking-an-LG-Ducted-Split-for-Home-Automation/
Unlike the information there, my unit and controller don't send this regularly but only when changing some installer settings or after power on (AC unit).

I'm mainly curious about bytes 3-4 because they change sometimes but have a similar value:
```
CB.00.00.8A.8B.FF...
CB.00.00.86.87.FF...
CB.00.00.85.87.FF...
CB.00.00.78.78.FF...
```
Maybe two different timers or (temperature) sensors? It's hard for me to figure out because my unit doesn't send these regularly.

| Byte | Description |
| --- | --- |
| 0 | Message type, 0xAB or 0xCB |
| 1 | 0000_00XX: DRED, demand response mode (0: off, 1: DRM 1, 2: DRM 2, 3: DRM 3) |
| 2 | 0000_00XX: setting zone = 1-3 in settings menu will set these bits to that number. Unclear what this setting is for. |
|   | 00XX_X000: over heating, installer setting 15 (0: default for unit, 1: +4C/+6C, 2: +2C/+4C, 3: -1C/+1C, 4: -0.5C/+0.5C). Note: the LG controller only shows option 4 in Fahrenheit mode (where it's -1F/+1F) but a custom controller can send it in Celsius mode and it works well with my unit. |
|   | XX00_0000: over cooling, installer setting 27 (0: +0.5C/-0.5C, 1: +6C/+4C, 2: +4C/+2C, 3: +1C/-1C) |
| 3 | unknown |
| 4 | unknown |
| 5 | unknown |
| 6 | 0000_00X0: oil change warning |
|   | 0000_0X00: centrigrade control, installer setting 17 (0: 1C, 1: 0.5C) |
|   | 0000_X000: emergency heater, installer setting 18 (0: don't use, 1: use) |
|   | 0X00_0000: emergency heater, installer setting 18, fan speed (0: fan off, 1: fan on) |
|   | X000_0000: function setting for group control, installer setting 19 (0: not in use, 1: in use) |
| 7 | 0X00_0000: indoor unit Wifi Access Point (AP) mode |
| 8 | 0000_XXXX: model info, outdoor unit (see table in manual) |
|   | XXXX_0000: model info, indoor unit (see table in manual) |
| 9 | 0000_XXXX: model info, capacity field (see table in manual) |
|   | 000X_0000: refrigerant leak detector, installer setting 29 (0: not installed, 1: installed) |
|   | XX00_0000: fan operation in cooling mode and thermal off conditions, installer setting 35 (0: fan low, 1: fan off, 2: fan setting) |
| 10| 0000_XXXX: static pressure step, installer setting 32 (0-11) |
|   | XXXX_0000: number of zones (1-8) according to comment on instructables post |
| 11| 0000_XXXX: emergency heater, installer setting 18, low ambient heating operation (0-15) |
| 12 | Checksum |

## Type 0xC (0xAC/0xCC)
The wall controller sends these regularly and apparently some AC units do this too, but I've never seen this from my unit. Maybe this is only used by single-split systems.
Example from my notes, but I haven't checked if this is accurate:
```
CC.00.09.64.91.29...
byte 2 (0x9): filter time (256 + 2048 = 2304)
bytes 3-5: energy usage 06:49, 12.9 kWh
```
Byte 12 controls some functions like mosquito away.

## Type 0xD settings (0xAD/0xCD)
My unit sends this after power on with all zeroes. The wall controller uses it for some installer settings.
```
Set 25 (auxiliary heater) to duct-type:          ad 04 ...
Set 36 (use primary heater control) to 1:        ad 40 ...
Set 38 (fan interlocked with ventilation) to 1:  ad 02 ...
Set 39 (indoor unit auto start) to 1:            ad 01
Set 41 (simple dry contact setting) to 3:        ad 00 00 00 00 00 03 00 00 00 00 00 ..
```

## Type 0xE settings (0xAE/0xCE)
Also used for installer settings. The format here seems a bit different:
```
CE.00... => available settings?

CE.10... => used to set certain settings
Set 46 (fan continuous) to 1:                        ae 10 00 00 02 00 00 00 00 00 00 00 ..
Set 47 (outdoor unit function) to 1:                 ae 10 00 00 00 10 00 00 00 00 00 00 ..
Set 48 (silent mode) to 2:                           ae 10 00 02 00 00 00 00 00 00 00 00 ..
Set 49 (defrost mode) to 3:                          ae 10 00 30 00 00 00 00 00 00 00 00 ..
Set 51 (temperature-based fan speed auto) to 1:      ae 10 00 00 04 00 00 00 00 00 00 00 ..
Set 52 (CN_EXT setting) to 5:                        ae 10 00 00 50 00 00 00 00 00 00 00 .. 
Set 56 (outdoor unit cycle priority) to 1 (standby): ae 10 00 00 00 00 10 00 00 00 00 00 ..
Set it to 2 (cool) with step 3:                      ae 10 00 00 00 00 07 00 00 00 00 00 ..
Set it to 2 (cool) with step 4:                      ae 10 00 00 00 00 09 00 00 00 00 00 ..
Set 60 (outdoor unit cycle priority) to 1 (special): ae 10 00 00 00 00 20 00 00 00 00 00 ..
Set 57 (outdoor temp for heating stages) to 1:       ae 10 00 00 00 00 40 00 00 00 00 00 ..
Set it to 3 with 27.5C:                              ae 10 00 00 00 00 00 00 5b 00 00 00 .. => 0x1b = 27, other bit is for 0.5
```

## Other
Types 0x0 and 0x1 seem related to external ventilation products.
