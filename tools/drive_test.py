#!/usr/bin/env python3
"""USB drive mode test on macOS: repeated enter / mount / eject cycles.

Needs tools/serial_log.py running with the same log and command files; start it first and let the
frame settle. Each cycle sends `d`, waits for the card to mount (normally 1-3 s, longer right after
plugging in), optionally writes a file and verifies it after the next remount, ejects and waits
for the next photo. Stops at the first reboot (a new "work ..." boot line in the log).

    python3 tools/drive_test.py [-n cycles] [-w MB] [-x 'command'] [-v volume]
"""
import argparse
import filecmp
import glob
import os
import subprocess
import sys
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('-n', type=int, default=10, help='cycles (default 10)')
    parser.add_argument('-w', type=int, default=0, metavar='MB',
                        help='write a random file of this size per cycle, verify it after the remount')
    parser.add_argument('-x', metavar='COMMAND',
                        help='shell command to run while mounted, $V = volume path')
    parser.add_argument('-v', default='sd4gb', metavar='VOLUME',
                        help='volume name of the card (default sd4gb)')
    parser.add_argument('-l', default='serial.log', metavar='LOG', help='serial_log.py log file')
    parser.add_argument('-c', default='cmd.txt', metavar='CMD', help='serial_log.py command file')
    args = parser.parse_args()
    volume = '/Volumes/' + args.v

    def count(text):
        try:
            with open(args.l, errors='replace') as f:
                return sum(text in line for line in f)
        except FileNotFoundError:
            return 0

    def wait_until(condition, timeout_s):
        end = time.time() + timeout_s
        while not condition() and time.time() < end:
            time.sleep(0.2)
        return condition()

    refreshes = lambda: count('Goto Sleep') # one per finished panel refresh
    boots = lambda: count('work ...')
    entered = lambda: count(': drive mode') # the frame's answer to `d`

    if not glob.glob('/dev/cu.usbmodem*'):
        sys.exit('frame not connected (no /dev/cu.usbmodem*)')
    if os.path.isdir(volume):
        sys.exit(volume + ' is already mounted: eject it first')

    data = None
    if args.w:
        data = tempfile.NamedTemporaryFile(delete=False)
        data.write(os.urandom(args.w * 1024 * 1024))
        data.close()
    test_file = os.path.join(volume, 'drive_test.bin')
    boots0 = boots()
    failed = verified = 0
    pending = False

    for i in range(1, args.n + 1):
        n, e = refreshes(), entered()
        with open(args.c, 'w') as f:
            f.write('d')
        start = time.time()
        if not wait_until(lambda: not os.path.exists(args.c), 5):
            os.remove(args.c)
            sys.exit('serial_log.py not running (or not on %s)' % args.c)
        if not wait_until(lambda: entered() > e, 5):
            sys.exit('cycle %d: frame did not answer d (busy, or other firmware)' % i)
        mounted = wait_until(lambda: os.path.isdir(volume), 60)
        if boots() > boots0:
            sys.exit('cycle %d: REBOOT' % i)
        if not mounted:
            # The frame leaves drive mode by itself when the computer does not read the drive
            failed += 1
            print('cycle %d: not mounted, waiting for the frame to give up' % i)
            wait_until(lambda: refreshes() > n + 1, 160)
            continue

        line = 'cycle %d: mounted after %.0f s' % (i, time.time() - start)
        time.sleep(3) # macOS refuses writes for a moment after mounting
        if pending:
            ok = filecmp.cmp(test_file, data.name, shallow=False)
            verified += ok
            line += ', verified' if ok else ', VERIFY FAILED'
            os.remove(test_file)
            pending = False
        if data and i < args.n:
            t = time.time()
            subprocess.run(['cp', data.name, test_file], check=True)
            subprocess.run(['sync'], check=True)
            t = time.time() - t
            line += ', wrote %d MB in %.1f s (%.0f KB/s)' % (args.w, t, args.w * 1024 / t)
            pending = True
        if args.x:
            subprocess.run(args.x, shell=True, env=dict(os.environ, V=volume))
        wait_until(lambda: refreshes() > n, 70) # drive screen done
        n = refreshes()
        subprocess.run(['diskutil', 'eject', volume], check=True, stdout=subprocess.DEVNULL)
        wait_until(lambda: refreshes() > n, 90) # next photo done
        time.sleep(1)
        print(line, flush=True)

    if data:
        os.remove(data.name)
    print('done: %d cycles, %d mounted, %d not mounted, %d verified, %d reboots'
          % (args.n, args.n - failed, failed, verified, boots() - boots0))


if __name__ == '__main__':
    main()
