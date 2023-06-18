# Very basic AC emulator for testing controllers. Requires the pyserial module.
#
# Typical usage on Linux is like this:
#
#    $ python3 ac-emulator.py /dev/ttyUSB0
#
# This has been tested with the LG PREMTB001 controller connected via a LIN bus
# transceiver to a PL2303 TTL USB Serial Port adapter.

import datetime
import time
import sys
import serial

class ACEmulator:
    _last_sent_time = None
    _last_received_time = None
    _last_status_response = None

    def __init__(self, serial_port):
        self._serial = serial.Serial(serial_port, baudrate=104, timeout=0.3)

    def calc_checksum(self, msg):
        assert len(msg) == 12
        return (sum(msg) & 0xFF) ^ 0x55

    def loop(self):
        buffer = b""
        while True:
            newpkt = self._serial.read(1)
            if not newpkt:
                buffer = b""
                if self._last_received_time and time.time() - self._last_received_time > 2:
                    self.send_timed()
            else:
                buffer += newpkt
            if len(buffer) == 13:
                self.process(buffer)
                buffer = b""
            assert len(buffer) < 13

    def process(self, buffer):
        self._last_received_time = time.time()
        print(datetime.datetime.now().time(), "read ", buffer.hex())

        if self.calc_checksum(buffer[0:12]) != buffer[12]:
            print("Invalid checksum!")
            return

        if buffer[0] == 0xA8:
            ints = [i for i in buffer]
            self.print_status(ints)
            ints[1] = ints[1] & 0xfe # clear changed flag
            self._last_status_response = ints

    def print_status(self, ints):
        def p(a, b):
            print((a + ":").ljust(15),  b)
        assert (ints[0] & 0x0f) == 0x08

        p("On/off", "on" if (ints[1] & 0x02) else "off")

        modes = ["cool", "dehumidify", "fan", "auto", "heat"]
        p("Mode", modes[(ints[1] >> 2) & 0b111])

        fans = ["low", "med", "high", "auto", "slow", "low-med", "med-high", "power"]
        p("Fan", fans[ints[1] >> 5])

        p("Horiz swing", "on" if (ints[2] & 0x40) else "off")

        p("Plasma", "on" if (ints[2] & 0x04) else "off")

        room_temp = (ints[7] & 0x3f) / 2 + 10
        p("Room temp", str(room_temp))

        setpoint = float((ints[6] & 0xf) + 15)
        if ints[5] & 0x1:
            setpoint += 0.5
        p("Setpoint", str(setpoint))

        thermistor = (ints[6] >> 4) & 0b11
        p("Thermistor", ["unit", "controller", "2th"][thermistor])

    def send_timed(self):
        # Send capabilities message first.
        if self._last_sent_time is None:
            self.send(b"\xC9\xC4\xEA\x1F\x81\x71\x00\x80\x02\x40\x04\x81")
            return

        # Else send a status message every 30 seconds.
        if self._last_status_response and time.time() - self._last_sent_time > 30:
            status = self._last_status_response
            status[0] = 0xc8
            status[7] = int((25 - 10) * 2) & 0xff # room temp = 25C

            status_new = status[:]

            # Make changes to status_new here, for example:
            status_new[5] |= 0x4 # Outdoor unit on.

            if status != status_new:
                status_new[1] |= 0x01 # changed flag

            self.send(bytes(status_new[0:12]))
            time.sleep(0.1)
            self.send(bytes(status_new[0:12]))

    def send(self, message):
        assert len(message) == 12
        message += bytes([self.calc_checksum(message)])

        print(datetime.datetime.now().time(), "write", message.hex())
        self._serial.write(message)
        self._serial.flush()

        self._last_sent_time = time.time()

        # Read our own message back. Only works if this is a single-wire connection.
        read_msg = b""
        while len(read_msg) < 13:
            read_msg += self._serial.read(1)
        if read_msg != message:
            print("Didn't match:", read_msg, message)

if len(sys.argv) != 2:
    print("Usage: python3 ac-emulator.py /dev/ttyUSB0")
    exit(1)

serial_port = sys.argv[1]
emulator = ACEmulator(serial_port)
emulator.loop()