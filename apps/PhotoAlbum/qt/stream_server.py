#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
stream_server.py —— 相册直播/录像 HTTP 服务器（Windows 侧运行）
替代 `python -m http.server`，额外提供：
  - 静态文件服务（含 HTTP Range，支持 MP4 拖进度条 seek）
  - GET  /list           -> 返回 JSON：录像文件列表 record_*.mp4
  - DELETE /<文件名>      -> 删除指定录像
用法：python stream_server.py [端口]   （默认 8000）
把本文件放在要服务的目录（与 out.m3u8 / record_*.mp4 同级）。
"""
import http.server
import json
import os
import re
import sys
import time
import urllib.parse

ROOT = os.path.dirname(os.path.abspath(__file__))


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=ROOT, **kwargs)

    # ---- Range 支持的文件响应 ----
    def _send_file(self, fp, start, end):
        size = os.path.getsize(fp)
        if start is None:
            if end is not None:          # 后缀范围 bytes=-N：取最后 N 字节
                start = max(0, size - end)
                end = size - 1
            else:
                start = 0
                end = size - 1
        if end is None or end >= size:
            end = size - 1
        if start >= size:
            start = size - 1
        length = end - start + 1
        partial = not (start == 0 and end == size - 1)
        self.send_response(206 if partial else 200)
        self.send_header('Content-Type', self.guess_type(fp))
        self.send_header('Accept-Ranges', 'bytes')
        self.send_header('Content-Length', str(length))
        if partial:
            self.send_header('Content-Range', 'bytes %d-%d/%d' % (start, end, size))
        self.end_headers()
        with open(fp, 'rb') as f:
            f.seek(start)
            remaining = length
            while remaining > 0:
                chunk = f.read(min(65536, remaining))
                if not chunk:
                    break
                try:
                    self.wfile.write(chunk)
                except (BrokenPipeError, ConnectionResetError):
                    break
                remaining -= len(chunk)

    def _recordings(self):
        files = []
        for root, dirs, names in os.walk(ROOT):
            for name in sorted(names):
                if name.startswith('record_') and name.endswith('.mp4'):
                    rel = os.path.relpath(os.path.join(root, name), ROOT).replace('\\', '/')
                    files.append({
                        'name': rel,
                        'url': 'http://%s/%s' % (self.headers.get('Host', ''), rel),
                    })
        return files

    # ---- GET ----
    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        path = urllib.parse.unquote(parsed.path)

        if path == '/list':
            body = json.dumps({'files': self._recordings()}).encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        fp = os.path.join(ROOT, path.lstrip('/'))
        if not os.path.isfile(fp):
            self.send_response(404)
            self.end_headers()
            return

        start = end = None
        rng = self.headers.get('Range')
        if rng:
            m = re.match(r'bytes=(\d*)-(\d*)', rng)
            if m:
                if m.group(1):
                    start = int(m.group(1))
                if m.group(2):
                    end = int(m.group(2))
                # 后缀范围 bytes=-N 表示最后 N 字节
                if m.group(1) == '' and m.group(2):
                    start = None
        self._send_file(fp, start, end)

    # ---- DELETE（只允许删录像 record_*.mp4）----
    def do_DELETE(self):
        parsed = urllib.parse.urlparse(self.path)
        path = urllib.parse.unquote(parsed.path)
        fp = os.path.join(ROOT, path.lstrip('/'))
        name = os.path.basename(fp)
        if os.path.isfile(fp) and name.startswith('record_') and name.endswith('.mp4'):
            ok = False
            for _ in range(5):          # Windows 文件可能被短时占用，重试
                try:
                    os.remove(fp)
                    ok = True
                    break
                except PermissionError:
                    time.sleep(0.2)
            if ok:
                self.send_response(200)
            else:
                self.send_response(500)
            self.end_headers()
        else:
            self.send_response(404)
            self.end_headers()


if __name__ == '__main__':
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8000
    server = http.server.ThreadingHTTPServer(('0.0.0.0', port), Handler)
    print('stream_server serving %s on port %d' % (ROOT, port))
    server.serve_forever()