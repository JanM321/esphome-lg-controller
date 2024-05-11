# LG Wall Controller Protocol

Documentation for the protocol used by wired wall controllers. This is largely based on reverse-engineering the LG PREMTB001 controller (some information is based on the newer PREMTB100).
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
prevent this by ensuring the RX pin is high for at least 500 ms before sending. This works well (I haven't seen any collisions in practice) and the LG
controllers seem to do something similar.

My custom controller also checks that each message we send is transmitted uncorrupted and else it tries to send it again. This works because it's a single wire
bus, so the controller always receives its own messages too.

## Message format
Each message is 13 bytes long. The first byte indicates the message type:
| Bits | Description |
| --- | --- |
| `XXX0_0000` | Source of the message<br>1: slave controller (for example `0x28` or `0x2A`)<br>5: master controller (for example `0xA8` or `0xAA`)<br>6: unit (for example `0xC8` or `0xC9`) |
| `000X_X000` | Product type <br>0: ventilation<br>1: AC<br>2: heat exchanger?|
| `0000_0XXX` | Message type (0-7)<br>0: status message<br>1: capabilities message<br>2: settings<br>... see below ... |

The two "product type" bits are always set to `01` for normal HVAC units. This means the (master) controller sends messages with first byte `0xA8` to `0xAF` and the AC unit will send messages `0xC8` to `0xCF`. You can use this to identify the source of a message.

For instance, `0xC8` is a status message sent by the AC unit. `0xAA` is a settings message sent by the controller.

LG controllers send messages with the product type field set to 0 or 2 only when connected to a ventilation product responding with such messages. For example if a heat exchanger unit responds with `0xD0` messages, the LG controllers will switch from sending `0xA8` to `0xB0` messages. This documentation and the ESPHome controller only cover AC units.

The last byte of each message is a checksum, computed by adding up the other bytes and XOR'ing with `0x55`.

## Type 0: Status message (0xA8/0xC8/0x28)
Status messages are used to control all of the basic settings such as operation mode, fan mode, room temperature, etc.

The controller and AC unit will send this when a setting changes. The AC unit will also send this every 60 seconds and 'master' controllers send this every 20 seconds.
| Byte | Bits | Description |
| --- | --- | --- |
| 0 | `XXXX_XXXX` | Message type, `0xA8/0xC8/0x28` |
| 1 | `0000_000X` | Set if settings were changed |
|   | `0000_00X0` | On/off<br>0: unit is off<br>1: unit is on |
|   | `000X_XX00` | Operation mode<br>0: cooling<br>1: dehumidify<br>2: fan<br>3: auto<br>4: heating |
|   | `XXX0_0000` | Fan speed<br>0: low<br>1: medium<br>2: high<br>3: auto<br>4: slow<br>5: low-medium<br>6: medium-high<br>7: power
| 2 | `0000_000X` | If set, controller shows Filter sign |
|   | `0000_00X0` | Controller sets this when clearing Filter in settings? |
|   | `0000_0X00` | Plasma / air-purifier |
|   | `0000_X000` | Humidifier |
|   | `000X_0000` | Resistive heater |
|   | `00X0_0000` | Swirl |
|   | `0X00_0000` | Horizontal swing |
|   | `X000_0000` | Vertical swing |
| 3 | `0000_000X` | External ventilation is on |
|   | `0000_00X0` | Fan Auto function |
|   | `0000_0X00` | Defrost |
|   | `0000_X000` | Preheat |
|   | `000X_0000` | Active reservation |
|   | `0XX0_0000` | Elevation grill setting<br>0: default<br>1: stop<br>2: up<br>3: down |
|   | `X000_0000` | Unknown |
| 4 | `0000_XXXX` | Unknown (seems related to central control) |
|   | `000X_0000` | Plasma sign blinks if plasma on (unclear what this is for) |
|   | `00X0_0000` | Auto addressing (shows IDU address on PREMTB100) |
|   | `0X00_0000` | Set by unit in heating mode |
|   | `X000_0000` | Related to central control |
| 5 | `0000_000X` | Add 0.5 to setpoint in byte 6 |
|   | `0000_00X0` | Energy saving function |
|   | `0000_0X00` | Outdoor unit is active |
|   | `0000_X000` | Zone 4 on |
|   | `000X_0000` | Zone 3 on |
|   | `00X0_0000` | Zone 2 on |
|   | `0X00_0000` | Zone 1 on |
|   | `X000_0000` | Set if installer setting Zone Type is set to New |
| 6 | `0000_XXXX` | Setpoint, stored as `temperature - 15` (example: 4 => 19°C or 19.5°C depending on byte 5)<br>In AI mode: -2 to +2 stored as 0-4 |
|   | `00XX_0000` | Thermistor installer setting<br>0: unit<br>1: controller<br>2: 2TH |
|   | `XX00_0000` | Ceiling height installer setting<br>0: medium<br>1: low<br>2: high<br>3: very high |
| 7 | `00XX_XXXX` | Room temperature, stored as `(temperature - 10) * 2` (example: 27 => `27 / 2 + 10` => 23.5°C) |
|   | `0X00_0000` | Set by AC when any indoor unit is in cooling mode |
|   | `X000_0000` | Set by AC when any indoor unit is in heating mode |
| 8 | `0000_0XXX` | Reservation/timer number of minutes, upper bits (see next section) |
|   | `00XX_X000` | Reservation/timer type (see next section)<br>0: none<br>1: turn-on reservation<br>2: turn-off reservation<br>3: sleep timer<br>4: clear all reservation data<br>5: simple timer (on/off after N minutes)<br>6-7: unknown |
|   | `0X00_0000` | If set, the AC unit will send a batch with all of its settings (other message types below) and clear it. LG controllers set this when powered on. |
|   | `X000_0000` | Set temporarily for "release 3 minute delay" installer setting 10 |
| 9 | `XXXX_XXXX` | Reservation/timer number of minutes, lower bits (see next section) |
| 10 | `0000_000X` | Set temporarily for installer setting 1 (test run) |
|    | `0000_00X0` | Zone 8 on |
|    | `0000_0X00` | Unknown, but always set by PREMTB100 (PREMTB001 sets it too but lets the unit clear it) |
|    |             | Or for duct units with > 4 zones: zone 7 on |
|    | `0000_X000` | Active robot clean |
|    |             | Or for duct units with > 4 zones: zone 6 on |
|    | `000X_0000` | Active auto clean (auto dry) |
|    |             | Or for duct units with > 4 zones: zone 5 on |
|    | `00X0_0000` | Unknown |
|    | `0X00_0000` | Set by unit when initializing? |
|    | `X000_0000` | Set by controller when initializing or for reservation/Fahrenheit settings (see next sections) |
| 11 | `XXXX_XXXX` | Error value, 0 = no error |
| 12 | `XXXX_XXXX` | Checksum |

### Reservation settings
Timer-related settings use the reservation flag in byte 3 with type + number of minutes stored in bytes 8 and 9.

For example when the controller sets a "simple reservation" to turn on/off after N hours, it stores the reservation type in byte 8 and the number of minutes in byte 9 + the three low bits of byte 8 if value > 255:
```
1 hour : a8 43 00 10 00 00 03 1d 28 3c 00 00 => 0x3c => 60 minutes
7 hours: a8 43 00 10 00 00 03 1d 29 a4 00 00 => 0x100 + 0xa4 => 420 minutes
                  ^              ^^ ^^
```
If a wired controller is connected, it looks like the unit makes it the controller's responsibility to implement these timers and turn the unit on/off.

The sleep timer option uses a different reservation type (3) in byte 8. The sleep timer is the only reservation type that the controller keeps sending regularly with the updated number of minutes. Unlike the other reservation types, the sleep timer can't be emulated with Home Assistant automations because some units use an extra-low fan speed and reduce power usage when the sleep timer is on. This is why the ESPHome controller supports sleep timers natively but not any of the other timers.

The turn-off/turn-on reservations are set based on the target time, but the controller always converts this to number of minutes relative to the current time.

To disable a single reservation or timer, the number of minutes can be set to 0. Reservation type 4 will clear all reservations.

Because multiple reservation types can be active at the same time, the controller also has a way to disable individual ones: byte 10 has bit `0x80` set and then byte 9 has a single bit for each of the following if they're enabled: turn-off reservation (`0x4`), turn-on reservation (`0x8`), sleep timer (`0x10`), and simple timer (`0x20`). My AC unit doesn't send messages like this, it just uses the reservation type with number of minutes > 0 to enable a reservation or timer, and 0 minutes to disable it.

### Fahrenheit mode
Temperature values sent/received are always in Celsius. To switch the unit to Fahrenheit (this affects the displayed temperature), the controller can temporarily set bit `0x80` of byte 10 and then set byte 9 to `0x40`. (Note that this is similar to the mechanism in the previous paragraph.)

Unfortunately LG uses a Fahrenheit/Celsius mapping that's different from what you'd expect. For example, 78°F is ~25.5°C, but LG controllers and units will instead send 26°C for 78°F (even though 26°C is more like 79°F). A value of 25.5°C is treated by the AC as 77°F. The ESPHome controller has code to convert between Fahrenheit, Celsius and "LG-Celsius" in Fahrenheit mode to account for these differences. See [issue #27](https://github.com/JanM321/esphome-lg-controller/issues/27) for more information on this.

## Type 1: Capabilities message (0xC9)
The AC sends this to the controller when it's powered on to tell it which features are supported.
| Byte | Bits | Description |
| --- | --- | --- |
| 0 | `XXXX_XXXX` | Message type, `0xC9` |
| 1 | `0000_0XXX` | Kind of unit?<br>1: cassette<br>2: duct (enables zone settings)<br>4: wall unit<br>(other values unknown) |
|   | `0000_X000` | Unknown |
|   | `000X_0000` | Supports zone state installer setting |
|   | `00X0_0000` | Supports swirl |
|   | `0X00_0000` | Supports horizontal swing |
|   | `X000_0000` | Supports vertical swing |
| 2 | `0000_000X` | Supports Fan Auto sub function |
|   | `0000_00X0` | Supports plasma (air purifier) |
|   | `0000_0X00` | Supports humidifier |
|   | `0000_X000` | Supports operation mode Auto |
|   | `000X_0000` | Supports operation mode AI |
|   | `00X0_0000` | Supports operation mode Heating |
|   | `0X00_0000` | Supports operation mode Fan |
|   | `X000_0000` | Supports operation mode Dehumidify |
| 3 | `0000_000X` | Supports fan speed Auto |
|   | `0000_00X0` | Supports fan speed Power |
|   | `0000_0X00` | Supports fan speed High |
|   | `0000_X000` | Supports fan speed Medium |
|   | `000X_0000` | Supports fan speed Low |
|   | `00X0_0000` | Supports fan speed Slow |
|   | `0X00_0000` | Unknown |
|   | `X000_0000` | Supports fan speed Power in heating mode |
| 4 | `0000_000X` | Supports vertical vane control |
|   | `0000_00X0` | Supports ESP value installer setting |
|   | `0000_0X00` | Supports static pressure installer setting |
|   | `0000_X000` | Supports ceiling height installer setting |
|   | `00XX_0000` | Unknown |
|   | `0X00_0000` | Supports robot clean setting |
|   | `X000_0000` | Supports auto clean (auto dry) setting |
| 5 | `0000_000X` | Supports energy saving sub function |
|   | `0000_00X0` | Supports override master/slave installer setting |
|   | `0000_0X00` | Supports auto change temperature setting |
|   | `000X_X000` | Unknown |
|   | `00X0_0000` | Half degrees C not supported |
|   | `0X00_0000` | Has single vane |
|   | `X000_0000` | Has two vanes |
| 6 | `0000_000X` | Supports extra airflow option (presence based?) |
|   | `0000_0XX0` | Unknown |
|   | `0000_X000` | Supports low-medium fan option |
|   | `000X_0000` | Supports medium-high fan option |
|   | `00X0_0000` | Supports minimum cooling target temperature of 16 instead of 18 |
|   | `XX00_0000` | Unknown |
| 7 | `0000_0X00` | Supports auxiliary heater, installer setting |
|   | `000X_X000` | Supports setting for zone = 1 to N (this value) in settings. Unclear what this is for |
|   | `X000_0000` | Supports over heating, installer setting 15 |
| 8 | `0000_000X` | Supports pipe temperature setting (installer setting 16 on PREMTB001) |
|   | `0000_00X0` | Supports centrigrade installer setting (0.5C or 1C) |
|   | `0000_0X00` | Unknown |
|   | `0000_X000` | Supports 5-8 zones |
|   | `000X_0000` | Supports emergency heater installer setting |
|   | `00X0_0000` | Supports group control installer setting |
|   | `0X00_0000` | Supports unit information setting |
|   | `X000_0000` | Supports clear filter timer in settings |
| 9 | `0000_00X0` | Supports indoor unit address check, installer setting 26 |
|   | `0000_0X00` | Supports over cooling, installer setting 27 |
|   | `000X_0000` | Supports energy usage setting |
|   | `X000_0000` | Supports refrigerant leak detector installation, installer setting 29 |
| 10 | `0000_000X` | Supports DRED (demand response enabling device) |
|    | `0000_00X0` | Supports static pressure step, installer setting 32 |
|    | `0000_0X00` | Supports indoor unit Wifi AP setting |
|    | `0X00_0000` | Supports fan cooling mode thermal off, installer setting 35 |
|    | `X000_0000` | Supports use primary heater control, installer setting 36 |
| 11 | `0000_000X` | Supports indoor unit auto start installer setting |
|    | `0000_00X0` | Supports AC fan interlocked with ventilation installer setting |
|    | `0000_0X00` | Supports himalaya cool sub function |
|    | `0000_X000` | Supports monsoon comfort option |
|    | `000X_0000` | Supports mosquito away sub function |
|    | `00X0_0000` | Supports hum+e sub function (called 'comfort cooling' on PREMTB100) |
|    | `0X00_0000` | Unknown |
|    | `X000_0000` | Supports simple dry contact installer setting 41 |
| 12 | `XXXX_XXXX` | Checksum |

## Type 2: Settings (0xAA/0xCA)
Type 0xA messages store more advanced settings and are only sent when the AC is powered on or when changing settings.

| Byte | Bits | Description |
| --- | --- | --- |
| 0 | `XXXX_XXXX` | Message type, `0xAA` or `0xCA` |
| 1 | `XXXX_XXXX` | Unit address (see installer setting 2) |
| 2 | `XXXX_XXXX` | Fan speed for Slow fan (see installer setting 3)<br>0: default |
| 3 | `XXXX_XXXX` | Fan speed for Low fan (see installer setting 3)<br>0: default |
| 4 | `XXXX_XXXX` | Fan speed for Medium fan (see installer setting 3)<br>0: default |
| 5 | `XXXX_XXXX` | Fan speed for High fan (see installer setting 3)<br>0: default |
| 6 | `XXXX_XXXX` | Fan speed for Power fan (see installer setting 3)<br>0: default |
| 7 | `0000_XXXX` | Vertical vane position for vane 1<br>0: default for operation mode<br>1: up<br>2-5: ...<br>6: down  |
|   | `XXXX_0000` | Vertical vane position for vane 2 |
| 8 | `0000_XXXX` | Vertical vane position for vane 3 |
|   | `XXXX_0000` | Vertical vane position for vane 4 |
| 9 | `0000_0XXX` | Auto change temperature setting 1-7 |
|   | `X000_0000` | Auxiliary heater, installer setting 25<br>0: not installed<br>1: installed |
| 10 | `XXXX_0000` | Maximum setpoint value, stored as `temperature - 15`. Typically `0xf` => 30°C. |
|    | `0000_XXXX` | Minimum setpoint value, stored as `temperature - 15`. Typically `0x1` => 16°C. |
| 11 | `0000_000X` | Dry contact mode installer setting 9<br>1: auto |
|    | `0000_00X0` | Zone state installer setting 11<br>0: variable<br>1: fixed |
|    | `0000_0X00` | Robot clean auto |
|    | `0000_X000` | Auto clean (auto dry) enabled |
|    | `XXXX_0000` | Unknown |
| 12 | `XXXX_XXXX` | Checksum |

## Type 3: More settings (0xAB/0xCB)
0xB stores more information and settings (whether Wifi AP mode is on, installer settings, etc).

| Byte | Bits | Description |
| --- | --- | --- |
| 0 | `XXXX_XXXX` | Message type, `0xAB` or `0xCB` |
| 1 | `0000_00XX` | DRED, demand response mode<br>0: off<br>1: DRM 1<br>2: DRM 2<br>3: DRM 3 |
|   | `X000_0000` | Set to request this message type from the other side (works both ways). For example, if the controller sends `AB 80 ..`, the unit will immediately send `CB 00 ..`. Messages are otherwise the same. |
| 2 | `0000_00XX` | Setting zone = 1-3 in settings menu will set these bits to that number. Unclear what this setting is for. |
|   | `00XX_X000` | Over heating, installer setting 15<br>0: default for unit<br>1: +4°C / +6°C<br>2: +2°C / +4°C<br>3: -1°C / +1°C<br>4: -0.5°C / +0.5°C<br>Note: the LG controllers only show option 4 in Fahrenheit mode (where it's -1F/+1F) and some only in dual setpoint mode, but a custom controller can send it in Celsius mode too. |
|   | `XX00_0000` | Over cooling, installer setting 27<br>0: +0.5°C / -0.5°C<br>1: +6°C / +4°C<br>2: +4°C / +2°C<br>3: +1°C / -1°C |
| 3 | `XXXX_XXXX` | Pipe temperature In (see table below) |
| 4 | `XXXX_XXXX` | Pipe temperature Out (see table below) |
| 5 | `XXXX_XXXX` | Pipe temperature Mid (see table below) |
| 6 | `0000_00X0` | Oil change warning |
|   | `0000_0X00` | Centrigrade control, installer setting 17<br>0: 1°C<br>1: 0.5°C |
|   | `0000_X000` | Emergency heater, installer setting 18<br>0: don't use<br>1: use |
|   | `0X00_0000` | Emergency heater, installer setting 18, fan speed<br>0: fan off<br>1: fan on |
|   | `X000_0000` | Function setting for group control, installer setting 19<br>0: not in use<br>1: in use |
| 7 | `0X00_0000` | Indoor unit Wifi Access Point (AP) mode |
| 8 | `0000_XXXX` | Model info, outdoor unit (see table in PREMTB001 manual) |
|   | `XXXX_0000` | Model info, indoor unit (see table in PREMTB001 manual) |
| 9 | `0000_XXXX` | Model info, capacity field (see table in PREMTB001 manual) |
|   | `000X_0000` | Refrigerant leak detector, installer setting 29<br>0: not installed<br>1: installed |
|   | `XX00_0000` | Fan operation in cooling mode and thermal off conditions, installer setting 35<br>0: fan low<br>1: fan off<br>2: fan setting |
| 10| `0000_XXXX` | Static pressure step, installer setting 32 (0-11) |
|   | `XXXX_0000` | Number of zones configured for duct units (Zone Number installer setting) |
| 11| `0000_XXXX` | Emergency heater, installer setting 18, low ambient heating operation (0-15) |
| 12 | `XXXX_XXXX` | Checksum |

### Pipe Temperature Values
Bytes 3-5 contain pipe temperature values. A higher value indicates a lower temperature. Below are the byte values mapped to °C (from PREMTB100). For example, a value of 0x78 indicates 23°C. 
|      | 0x0  | 0x1  | 0x2  | 0x3  | 0x4  | 0x5  | 0x6  | 0x7  | 0x8  | 0x9  | 0xA  | 0xB  | 0xC  | 0xD  | 0xE  | 0xF  |
| ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 0x00 | \-   | \-   | \-   | \-   | \-   | \-   | \-   | \-   | \-   | \-   | 108  | 104  | 101  | 100  | 98   | 95   |
| 0x10 | 93   | 91   | 89   | 87   | 85   | 84   | 82   | 81   | 79   | 78   | 76   | 75   | 74   | 73   | 72   | 71   |
| 0x20 | 70   | 68   | 68   | 67   | 66   | 65   | 64   | 63   | 62   | 61   | 60   | 60   | 59   | 58   | 57   | 57   |
| 0x30 | 56   | 55   | 55   | 54   | 53   | 53   | 52   | 52   | 51   | 50   | 50   | 49   | 49   | 48   | 47   | 47   |
| 0x40 | 46   | 46   | 45   | 45   | 44   | 44   | 43   | 43   | 42   | 42   | 41   | 41   | 40   | 40   | 39   | 39   |
| 0x50 | 39   | 38   | 38   | 37   | 37   | 36   | 36   | 36   | 35   | 35   | 34   | 34   | 33   | 33   | 33   | 32   |
| 0x60 | 32   | 31   | 31   | 31   | 30   | 30   | 30   | 29   | 29   | 29   | 28   | 28   | 27   | 27   | 27   | 26   |
| 0x70 | 26   | 26   | 25   | 25   | 24   | 24   | 24   | 23   | 23   | 23   | 22   | 22   | 22   | 21   | 21   | 21   |
| 0x80 | 20   | 20   | 20   | 19   | 19   | 19   | 18   | 18   | 18   | 17   | 17   | 17   | 16   | 16   | 16   | 15   |
| 0x90 | 15   | 15   | 14   | 14   | 14   | 13   | 13   | 13   | 12   | 12   | 12   | 11   | 11   | 11   | 10   | 10   |
| 0xA0 | 10   | 9    | 9    | 9    | 8    | 8    | 8    | 7    | 7    | 6    | 6    | 6    | 5    | 5    | 5    | 4    |
| 0xB0 | 4    | 4    | 3    | 3    | 3    | 2    | 2    | 2    | 1    | 1    | 0    | 0    | 0    | 0    | 0    | \-1  |
| 0xC0 | \-1  | \-2  | \-2  | \-2  | \-3  | \-3  | \-4  | \-4  | \-5  | \-5  | \-5  | \-6  | \-6  | \-7  | \-7  | \-8  |
| 0xE0 | \-8  | \-9  | \-9  | \-9  | \-10 | \-10 | \-11 | \-11 | \-12 | \-12 | \-13 | \-14 | \-14 | \-15 | \-15 | \-16 |
| 0xE0 | \-16 | \-17 | \-18 | \-18 | \-19 | \-20 | \-20 | \-21 | \-22 | \-22 | \-23 | \-24 | \-25 | \-26 | \-27 | \-28 |
| 0xF0 | \-29 | \-   | \-   | \-   | \-   | \-   | \-   | \-   | \-   | \-   | \-   | \-   | \-   | \-   | \-   | \-   |

## Type 4: More status information (0xAC/0xCC)
The wall controller sends these regularly and apparently some AC units do this too, but I've never seen this from my unit.

| Byte | Bits | Description |
| --- | --- | --- |
| 0 | `XXXX_XXXX` | Message type, `0xAC/0xCC/2C` |
| 1 | `XXXX_XXXX` | Filter time left in hours (low bits). Combined with value in byte 2.<br>Example: `CC.AB.01 => 0x1AB => 427 hours left` |
| 2 | `0000_XXXX` | Filter time left in hours (high bits) |
|   | `0X00_0000` | Occupancy status |
|   | `X000_0000` | Dual setpoint mode |
| 3-5 | `XXXX_XXXX` | Power Consumption in kWh. Example: `64.91.29 => 64912.9 kWh` |
| 6 | `0XXX_XXXX` | Setpoint in degrees C, integer part. Used for the high setpoint in dual setpoint mode |
| 7 | `0XXX_XXXX` | Setpoint in degrees C, integer part. Used for the low setpoint in dual setpoint mode |
|   | `X000_0000` | Set if settings were changed. This is similar to the bit in byte 1 for Type 0 status messages |
| 8 | `0000_XXXX` | Fractional part for setpoint in byte 7 (for example `5 => 0.5C`) |
|   | `XXXX_0000` | Fractional part for setpoint in byte 6 |
| 9 | `XXXX_XXXX` | Room temperature value (from controller) or unit temperature (from AC). Stored as `temperature * 2` (example: `0x2B => 21.5C`). PREMTB100 sends this but PREMTB001 always sends 0. |
| 10 | `XXXX_XXXX` | Deadband for dual setpoint mode (example: `0x25 => 2.5C`) |
| 11 | `0000_000X` | Himalaya Cooling |
|    | `0000_00X0` | Mosquito Away |
|    | `0000_0X00` | Comfort Cooling |
| 12 | `XXXX_XXXX` | Checksum |

## Type 5: Advanced settings (0xAD/0xCD)
My unit sends this after power on with all zeroes. The wall controller uses it for installer settings.
```
Set 25 (auxiliary heater) to duct-type:          ad 04 ...
Set 36 (use primary heater control) to 1:        ad 40 ...
Set 38 (fan interlocked with ventilation) to 1:  ad 02 ...
Set 39 (indoor unit auto start) to 1:            ad 01
Set 41 (simple dry contact setting) to 3:        ad 00 00 00 00 00 03 00 00 00 00 00 ..
```

## Type 6: More settings or status information (0xAE/0xCE)
The format here is a bit different, with the second byte as additional message type (probably because they ran out of message types).
```
CE.00... => extended capabilities message? unit sends this with available settings

CE.10... => used to set settings
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

CE 11... => used for air quality monitoring values?
```
The `AE 80` / `CE 80` message is a newer, additional status message that the PREMTB100 controller and some units send regularly.
```
AE.80.3C.0F.17.00.2A.74.02.00.12.04.13
- byte 2:      humidity (0x3C => 60%)
- bytes 3-4:   fan operation time (0x0f17 => 3863 hours)
- bytes 6-7:   IDU (indoor unit) operation time (0x2a74 => 10868 hours)
- byte 8:      PREMTB100 always sets bit 2 (unknown)
- bytes 10-11: room temperature with 0.1 degrees precision (0x12 + 0x04 => 18 + 0.4 => 18.4C)
- byte 12:     checksum
```

## Type 7: More status information (0xAF/0xCF)
Used by newer units and controllers. Contains unit's current power usage among other things. Some examples from PREMTB100:
```
CF.00.12.34.56.00.00.00.00.00.00.00.3E => 123.5 kW
CF.00.98.76.54.00.00.00.00.00.00.00.64 => 987.7 kW
CF.00.AB.CD.EF.00.00.00.00.00.00.00.63 => 1123.5 kW
```
