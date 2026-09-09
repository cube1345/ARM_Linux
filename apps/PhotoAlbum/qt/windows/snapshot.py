#!/usr/bin/env python3
# ============================================
# snapshot.py - grab one JPEG frame per camera channel
# from mediamtx RTSP, for the board's 3x3 snapshot wall.
# Usage: python snapshot.py
# ============================================
import concurrent.futures
import os
import subprocess
import sys
import time

BASE = r"E:\EmbeddedDevelopment\WorkSpace\ARM_Linux\apps\PhotoAlbum"
SNAP = os.path.join(BASE, "snap")
RTSP_BASE = "rtsp://127.0.0.1:8554/live/"
CHANNELS = ["cam%d" % i for i in range(1, 10)]   # cam1..cam9
SCALE = "320:180"
SLEEP = 3


def grab(name):
    out = os.path.join(SNAP, name + ".jpg")
    try:
        subprocess.run(
            ["ffmpeg", "-rtsp_transport", "tcp", "-i", RTSP_BASE + name,
             "-frames:v", "1", "-vf", "scale=" + SCALE, "-y", out],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=8)
        return name, True
    except Exception:
        return name, False


def main():
    os.makedirs(SNAP, exist_ok=True)
    print("snapshot generator: writing to %s" % SNAP)
    while True:
        t0 = time.time()
        with concurrent.futures.ThreadPoolExecutor(max_workers=len(CHANNELS)) as ex:
            results = list(ex.map(grab, CHANNELS))
        ok = sum(1 for _, s in results if s)
        elapsed = time.time() - t0
        print("snap cycle: %d/%d ok in %.1fs" % (ok, len(CHANNELS), elapsed))
        time.sleep(max(0.5, SLEEP - elapsed))


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("stopped")