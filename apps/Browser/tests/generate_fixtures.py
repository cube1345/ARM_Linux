#!/usr/bin/env python3
"""Generate small deterministic image and WAV fixtures for media_smoke."""

import base64
import struct
import sys
import wave
import zlib
from pathlib import Path


def bmp(path: Path, bits: int, compression: int = 0) -> None:
    width, height = 2, 2
    palette_count = 1 << bits if bits <= 8 else 0
    palette = b"".join(
        struct.pack("<BBBB", (index * 73) & 255, (index * 41) & 255,
                    (index * 97) & 255, 0)
        for index in range(palette_count)
    )
    if compression == 2:
        pixels = bytes((2, 0x01, 0, 0, 2, 0x23, 0, 0, 0, 1))
    elif compression == 1:
        pixels = bytes((2, 1, 0, 0, 2, 2, 0, 0, 0, 1))
    else:
        stride = ((width * bits + 31) // 32) * 4
        rows = []
        for row in range(height):
            if bits == 4:
                row_data = bytes((0x01 if row == 0 else 0x23,))
            elif bits == 8:
                row_data = bytes((0, 1) if row == 0 else (2, 3))
            elif bits == 24:
                row_data = bytes((0, 0, 255, 0, 255, 0))
            else:
                row_data = bytes((0, 0, 255, 255, 0, 255, 0, 255))
            rows.append(row_data + b"\0" * (stride - len(row_data)))
        pixels = b"".join(reversed(rows))
    dib = struct.pack("<IiiHHIIiiII", 40, width, height, 1, bits,
                      compression, len(pixels), 2835, 2835,
                      palette_count, 0)
    offset = 14 + len(dib) + len(palette)
    header = struct.pack("<2sIHHI", b"BM", offset + len(pixels), 0, 0, offset)
    path.write_bytes(header + dib + palette + pixels)


def png(path: Path) -> None:
    width, height = 2, 2
    rows = [bytes((0, 255, 0, 0, 0, 255, 0)),
            bytes((0, 0, 0, 255, 255, 255, 255))]

    def chunk(kind: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff))

    data = (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(b"".join(rows))) +
            chunk(b"IEND", b""))
    path.write_bytes(data)


def gif(path: Path) -> None:
    data = base64.b64decode(
        "R0lGODlhAgACAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAICRAEAOw=="
    )
    path.write_bytes(data)


def wav(path: Path) -> None:
    with wave.open(str(path), "wb") as stream:
        stream.setnchannels(1)
        stream.setsampwidth(2)
        stream.setframerate(8000)
        stream.writeframes(b"\0\0" * 800)


def main() -> int:
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <output-directory>", file=sys.stderr)
        return 2
    output = Path(sys.argv[1])
    output.mkdir(parents=True, exist_ok=True)
    bmp(output / "uncompressed-4.bmp", 4)
    bmp(output / "uncompressed-8.bmp", 8)
    bmp(output / "uncompressed-24.bmp", 24)
    bmp(output / "uncompressed-32.bmp", 32)
    bmp(output / "rle4.bmp", 4, 2)
    bmp(output / "rle8.bmp", 8, 1)
    png(output / "sample.png")
    gif(output / "sample.gif")
    wav(output / "sample.wav")
    (output / "corrupt.png").write_bytes(b"not a valid png")
    (output / "README.txt").write_text("empty test document\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
