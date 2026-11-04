#!/usr/bin/env python3
"""Log the frame's USB serial output with timestamps, and send it test commands.

Run in the background with PlatformIO's Python (it has pyserial):
    ~/.platformio/penv/bin/python tools/serial_log.py [--log serial.log] [--cmd cmd.txt]
Every line goes to the log as "HH:MM:SS text". To send a command, write it to the command file
(e.g. `printf n > cmd.txt`); it is sent and the file removed. The port is reopened when the frame
re-enumerates or is flashed; stop the logger before `pio run -t upload` (picotool needs the port).
"""
import argparse
import glob
import os
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--log', default='serial.log', help='log file, appended to')
    parser.add_argument('--cmd', default='cmd.txt', help='command file, sent and removed')
    parser.add_argument('--port', default='/dev/cu.usbmodem*', help='serial port glob')
    args = parser.parse_args()

    log = open(args.log, 'a', buffering=1)

    def stamp(text):
        log.write('%s %s\n' % (time.strftime('%H:%M:%S'), text))

    port = None
    while True:
        if port is None:
            names = glob.glob(args.port)
            if not names:
                time.sleep(0.3)
                continue
            try:
                port = serial.Serial(names[0], 115200, timeout=0.2)
                stamp('[open %s]' % names[0])
            except serial.SerialException:
                time.sleep(0.5)
                continue
        try:
            if os.path.exists(args.cmd):
                with open(args.cmd) as f:
                    command = f.read()
                os.remove(args.cmd)
                if command:
                    port.write(command.encode())
                    stamp('[sent %r]' % command)
            data = port.read(4096)
            for line in data.decode(errors='replace').splitlines():
                if line.strip():
                    stamp(line)
        except (serial.SerialException, OSError) as e:
            stamp('[closed: %s]' % e)
            port = None
            time.sleep(0.5)


if __name__ == '__main__':
    main()
