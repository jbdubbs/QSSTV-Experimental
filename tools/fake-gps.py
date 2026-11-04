#!/usr/bin/env python3
"""Fake NMEA GPS receiver on a virtual serial port (Linux), for testing the GPS features without a real receiver.

It opens a pseudo terminal and sends NMEA 0183 sentences (GGA + RMC, once a second) for a vehicle driving along a road:
straight stretches of 0.5 to 2 km, corners where it slows down, optionally a stop at some of them. A symbolic link with a
fixed name points at the pseudo terminal, so the program can be given the same port every time.

    tools/fake-gps.py                       start near Seattle at 90 km/h, port /tmp/ttyFAKEGPS
    tools/fake-gps.py --lat 40.7 --lon -74  start somewhere else (decimal degrees, south and west negative)
    tools/fake-gps.py --speed 300           faster, to see the numbers change at once
    tools/fake-gps.py --no-fix-for 20       no position for the first 20 s (the amber "waiting" state)
    tools/fake-gps.py --dropouts            lose the position for 10 s every minute or so
    tools/fake-gps.py --stops               stop for 5 to 20 s at some corners

In QSSTV-Experimental: Configuration > GPS, Serial port = /tmp/ttyFAKEGPS (type it in: the list only offers the
devices of /dev/serial/by-id), any baud rate, then switch the GPS button on the Transmit tab on. Stop with Ctrl+C.
"""
import argparse
import datetime
import errno
import math
import os
import pty
import random
import signal
import sys
import time
import tty

EARTH = 111_320.0      # metres per degree of latitude
KNOT = 0.514444        # metres per second


def checksum(body):
    x = 0
    for c in body.encode("ascii"):
        x ^= c
    return "%02X" % x


def sentence(body):
    return ("$%s*%s\r\n" % (body, checksum(body))).encode("ascii")


def coordinate(degrees, latitude):
    """Signed degrees to the NMEA ddmm.mmmm / dddmm.mmmm text and hemisphere letter."""
    hemisphere = ("N" if degrees >= 0 else "S") if latitude else ("E" if degrees >= 0 else "W")
    a = abs(degrees)
    d = int(a)
    m = (a - d) * 60.0
    if round(m, 4) >= 60.0:    # 59.99996 rounds to 60.0000: carry into the degrees
        d += 1
        m = 0.0
    return ("%02d%07.4f" if latitude else "%03d%07.4f") % (d, m), hemisphere


class Vehicle:
    """A car on a road made of straight stretches and corners, flat-earth arithmetic (fine for a few hundred km)."""

    def __init__(self, lat, lon, heading, cruise_ms, stops, rng):
        self.lat, self.lon, self.heading = lat, lon, heading % 360.0
        self.cruise = cruise_ms
        self.stops = stops
        self.rng = rng
        self.speed = 0.0
        self.target_heading = self.heading
        self.state = "drive"
        self.left = self.new_stretch()
        self.wait = 0.0

    def new_stretch(self):
        return self.rng.uniform(500.0, 2000.0)

    def step(self, dt):
        # speed: slow for the corner (the last 60 m of a stretch and the turn itself), stop when a stop is on
        want = self.cruise
        if self.state == "stop":
            want = 0.0
        elif self.state == "turn" or self.left < 60.0:
            want = min(self.cruise, 20.0 / 3.6)
        accel = 2.5 * dt if want > self.speed else 4.0 * dt
        self.speed += max(-accel, min(accel, want - self.speed))

        # heading: turn towards the target at up to 45 degrees a second, small wobble on the straight
        diff = (self.target_heading - self.heading + 180.0) % 360.0 - 180.0
        self.heading = (self.heading + max(-45.0 * dt, min(45.0 * dt, diff))) % 360.0
        if self.state == "drive":
            self.heading = (self.heading + self.rng.gauss(0.0, 0.3)) % 360.0    # the pull towards the target keeps it small

        metres = self.speed * dt
        h = math.radians(self.heading)
        self.lat += metres * math.cos(h) / EARTH
        self.lon += metres * math.sin(h) / (EARTH * max(0.05, math.cos(math.radians(self.lat))))

        if self.state == "drive":
            self.left -= metres
            if self.left <= 0.0:
                if self.stops and self.rng.random() < 0.5:
                    self.state, self.wait = "stop", self.rng.uniform(5.0, 20.0)
                else:
                    self.begin_turn()
        elif self.state == "stop":
            self.wait -= dt
            if self.wait <= 0.0 and self.speed < 0.1:
                self.begin_turn()
        elif self.state == "turn":
            if abs((self.target_heading - self.heading + 180.0) % 360.0 - 180.0) < 1.0:
                self.state, self.left = "drive", self.new_stretch()

    def begin_turn(self):
        self.target_heading = (self.heading + self.rng.choice((-90.0, 90.0))) % 360.0
        self.state = "turn"


def build(talker, kinds, now, lat, lon, speed_ms, heading, altitude, fix):
    t = now.strftime("%H%M%S.00")
    d = now.strftime("%d%m%y")
    out = []
    la, ns = coordinate(lat, True)
    lo, ew = coordinate(lon, False)
    for kind in kinds:
        if kind == "GGA":
            if fix:
                body = "%sGGA,%s,%s,%s,%s,%s,1,08,0.9,%.1f,M,-17.0,M,," % (talker, t, la, ns, lo, ew, altitude)
            else:
                body = "%sGGA,%s,,,,,0,00,99.99,,,,,," % (talker, t)
        elif kind == "RMC":
            if fix:
                body = "%sRMC,%s,A,%s,%s,%s,%s,%.2f,%.1f,%s,,,A" % (talker, t, la, ns, lo, ew, speed_ms / KNOT, heading, d)
            else:
                body = "%sRMC,%s,V,,,,,,,%s,,,N" % (talker, t, d)
        elif kind == "GLL":
            if fix:
                body = "%sGLL,%s,%s,%s,%s,%s,A,A" % (talker, la, ns, lo, ew, t)
            else:
                body = "%sGLL,,,,,%s,V,N" % (talker, t)
        else:
            continue
        out.append(sentence(body))
    return b"".join(out)


def main():
    p = argparse.ArgumentParser(description="Fake NMEA GPS receiver on a virtual serial port.")
    p.add_argument("--link", default="/tmp/ttyFAKEGPS", help="symbolic link to create (default %(default)s)")
    p.add_argument("--lat", type=float, default=47.6062, help="start latitude, decimal degrees (default Seattle)")
    p.add_argument("--lon", type=float, default=-122.3321, help="start longitude, decimal degrees (default Seattle)")
    p.add_argument("--heading", type=float, default=None, help="start heading in degrees (default: random)")
    p.add_argument("--speed", type=float, default=90.0, help="cruising speed in km/h (default %(default)s)")
    p.add_argument("--interval", type=float, default=1.0, help="seconds between fixes (default %(default)s)")
    p.add_argument("--sentences", default="GGA,RMC", help="comma separated: GGA, RMC, GLL (default %(default)s)")
    p.add_argument("--talker", default="GP", help="two letter talker id (default %(default)s, GN for a multi-GNSS receiver)")
    p.add_argument("--no-fix-for", type=float, default=0.0, metavar="S", help="send no-fix sentences for the first S seconds")
    p.add_argument("--dropouts", action="store_true", help="lose the fix for 10 s now and then")
    p.add_argument("--stops", action="store_true", help="stop for 5 to 20 s at some corners")
    p.add_argument("--seed", type=int, default=None, help="random seed (the same road again)")
    p.add_argument("--quiet", action="store_true", help="no status line")
    a = p.parse_args()

    if not (-90.0 <= a.lat <= 90.0 and -180.0 <= a.lon <= 180.0):
        p.error("latitude -90..90, longitude -180..180")
    kinds = [k.strip().upper() for k in a.sentences.split(",") if k.strip()]
    if not kinds or any(k not in ("GGA", "RMC", "GLL") for k in kinds):
        p.error("--sentences: GGA, RMC and GLL only")
    if len(a.talker) != 2 or not a.talker.isalpha():
        p.error("--talker: two letters")
    if a.interval < 0.05:
        p.error("--interval: 0.05 s at least")

    rng = random.Random(a.seed)
    heading = rng.uniform(0.0, 360.0) if a.heading is None else a.heading
    car = Vehicle(a.lat, a.lon, heading, a.speed / 3.6, a.stops, rng)
    car.speed = car.cruise     # already on the road at the start

    master, slave = pty.openpty()
    slave_name = os.ttyname(slave)
    tty.setraw(slave)          # no echo, no CR/LF translation, whatever the reader does
    os.set_blocking(master, False)
    os.close(slave)            # nobody holds the slave: when the program closes it, the writes simply fail until the next open

    try:
        os.unlink(a.link)
    except FileNotFoundError:
        pass
    os.symlink(slave_name, a.link)

    stop = []
    for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
        signal.signal(sig, lambda *_: stop.append(1))

    print("Fake GPS on %s  ->  %s" % (a.link, slave_name), file=sys.stderr)
    print("Start %.5f %.5f, heading %.0f, %.0f km/h, %s every %g s.  Ctrl+C to stop." %
          (a.lat, a.lon, heading, a.speed, "+".join(kinds), a.interval), file=sys.stderr)

    started = time.monotonic()
    nxt = started
    last = started
    dropout_until = 0.0
    next_dropout = started + rng.uniform(40.0, 80.0)
    try:
        while not stop:
            now = time.monotonic()
            if now < nxt:
                time.sleep(min(0.05, nxt - now))
                continue
            dt = now - last
            last = now
            nxt += a.interval
            if nxt < now:
                nxt = now + a.interval

            car.step(dt)
            fix = (now - started) >= a.no_fix_for
            if a.dropouts and fix:
                if now >= next_dropout:
                    dropout_until = now + 10.0
                    next_dropout = dropout_until + rng.uniform(40.0, 80.0)
                fix = now >= dropout_until
            altitude = 120.0 + 25.0 * math.sin((now - started) / 90.0)
            data = build(a.talker.upper(), kinds, datetime.datetime.now(datetime.timezone.utc), car.lat, car.lon,
                         car.speed, car.heading, altitude, fix)
            try:
                os.write(master, data)
            except OSError as e:
                if e.errno not in (errno.EIO, errno.EAGAIN):    # EIO: nobody has the port open; EAGAIN: nobody reads
                    raise
            if not a.quiet:
                if fix:
                    line = "%s  %9.5f %10.5f  %5.1f km/h  heading %3.0f  %s" % (
                        time.strftime("%H:%M:%S"), car.lat, car.lon, car.speed * 3.6, car.heading, car.state)
                else:
                    line = "%s  no fix" % time.strftime("%H:%M:%S")
                sys.stderr.write("\r%-78s" % line)
                sys.stderr.flush()
    finally:
        try:
            os.unlink(a.link)
        except FileNotFoundError:
            pass
        os.close(master)
        if not a.quiet:
            sys.stderr.write("\n")
        print("Stopped.", file=sys.stderr)


if __name__ == "__main__":
    main()
