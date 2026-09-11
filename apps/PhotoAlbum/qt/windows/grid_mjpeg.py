# -*- coding: utf-8 -*-
"""
grid_mjpeg.py - 九宫格 MJPEG 服务器
为每个通道从本机 mediamtx(RTSP 8554) 拉流，压成低分辨率 JPEG，
以 multipart/x-mixed-replace 流式提供, 供板卡九宫格同时显示多路画面。

用法:  python grid_mjpeg.py [port]
默认端口 8010，路径 /cam1 ... /cam9
"""
import socket
import subprocess
import sys
import threading
import time

PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8010
W, H, FPS, Q = 240, 135, 5, 10
CHANNELS = [1, 2, 3, 4, 5, 6, 8]
BOUNDARY = b"frame"

frames = {}          # ch -> latest JPEG bytes
frame_lock = threading.Lock()
frame_cond = threading.Condition(frame_lock)


def read_jpeg(pipe):
    """从 ffmpeg stdout 读一个完整 JPEG (FFD8..FFD9)。"""
    data = b""
    while True:
        if len(data) < 2:
            chunk = pipe.read(1)
            if not chunk:
                return None
            data += chunk
            continue
        if data[:2] != b"\xff\xd8":
            data = data[1:]
            continue
        if len(data) < 4:
            chunk = pipe.read(1)
            if not chunk:
                return None
            data += chunk
            continue
        idx = data.find(b"\xff\xd9", 2)
        if idx >= 0:
            return data[:idx + 2]
        chunk = pipe.read(4096)
        if not chunk:
            return None
        data += chunk


def reader_loop(ch):
    url = "rtsp://127.0.0.1:8554/live/cam%d" % ch
    while True:
        try:
            proc = subprocess.Popen(
                ["ffmpeg", "-rtsp_transport", "tcp", "-i", url,
                 "-vf", "scale=%d:%d,fps=%d" % (W, H, FPS),
                 "-q:v", str(Q), "-an",
                 "-f", "image2pipe", "-vcodec", "mjpeg", "-"],
                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
            while True:
                jpg = read_jpeg(proc.stdout)
                if jpg is None:
                    break
                with frame_cond:
                    frames[ch] = jpg
                    frame_cond.notify_all()
            proc.terminate()
        except Exception:
            pass
        time.sleep(2)  # 拉流失败/断流重连


class Handler:
    def __init__(self, conn, addr, ch):
        self.conn = conn
        self.ch = ch
        self.addr = addr

    def serve(self):
        try:
            self.conn.sendall(
                b"HTTP/1.1 200 OK\r\n"
                b"Content-Type: multipart/x-mixed-replace; boundary=" + BOUNDARY + b"\r\n"
                b"Cache-Control: no-cache\r\n"
                b"Connection: close\r\n\r\n")
            while True:
                with frame_cond:
                    while self.ch not in frames:
                        frame_cond.wait(3.0)
                        if self.ch not in frames:
                            # 无帧也发一次空 keep-alive 边界, 客户端可据此判断离线
                            self.conn.sendall(b"--" + BOUNDARY + b"\r\n\r\n")
                            continue
                    jpg = frames[self.ch]
                self.conn.sendall(
                    b"--" + BOUNDARY + b"\r\n"
                    b"Content-Type: image/jpeg\r\n"
                    b"Content-Length: " + str(len(jpg)).encode() + b"\r\n\r\n" + jpg + b"\r\n")
        except Exception:
            pass
        finally:
            try:
                self.conn.close()
            except Exception:
                pass


def main():
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", PORT))
    srv.listen(16)
    for ch in CHANNELS:
        threading.Thread(target=reader_loop, args=(ch,), daemon=True).start()
    print("grid_mjpeg listening on %d, channels=%s" % (PORT, CHANNELS))
    while True:
        conn, addr = srv.accept()
        try:
            req = conn.recv(4096)
            line = req.split(b"\r\n")[0].decode(errors="ignore")
            parts = line.split(" ")
            if len(parts) < 2:
                conn.close()
                continue
            path = parts[1]
            import re
            m = re.match(r"^/cam(\d+)$", path)
            if not m:
                conn.sendall(b"HTTP/1.1 404 Not Found\r\n\r\n")
                conn.close()
                continue
            ch = int(m.group(1))
            threading.Thread(target=Handler(conn, addr, ch).serve, daemon=True).start()
        except Exception:
            try:
                conn.close()
            except Exception:
                pass


if __name__ == "__main__":
    main()