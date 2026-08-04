#!/usr/bin/env python3
"""Exercise the public converter with synthetic boundary-value samples."""

from pathlib import Path
import struct
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True

from make_test_yuv10 import pack


def read_ifd(path: Path) -> tuple[dict[int, tuple[int, int, int]], bytes]:
    data = path.read_bytes()
    if data[:4] != b"II\x2a\x00":
        raise AssertionError("expected little-endian TIFF")
    ifd_offset = struct.unpack_from("<I", data, 4)[0]
    count = struct.unpack_from("<H", data, ifd_offset)[0]
    tags: dict[int, tuple[int, int, int]] = {}
    offset = ifd_offset + 2
    for _ in range(count):
        tag, field_type, value_count, value = struct.unpack_from("<HHII", data, offset)
        tags[tag] = (field_type, value_count, value)
        offset += 12
    return tags, data


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} CONVERTER", file=sys.stderr)
        return 2

    converter = Path(sys.argv[1]).resolve()

    # Known packed word: samples 0x001, 0x155, 0x3ff followed by two zero bits.
    assert pack([0x001, 0x155, 0x3FF]) == bytes.fromhex("00555ffc")

    # Six 4:2:2 pairs: grayscale boundaries followed by BT.709 primaries.
    samples = [
        512, 64, 512, 64,
        512, 940, 512, 940,
        512, 512, 512, 512,
        409, 250, 960, 250,  # red
        167, 691, 105, 691,  # green
        960, 127, 471, 127,  # blue
    ]
    raw = pack(samples)
    assert len(raw) == 32
    assert all((int.from_bytes(raw[i:i + 4], "big") & 0x3) == 0
               for i in range(0, len(raw), 4))

    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        input_path = root / "boundary.yuv10"
        output_path = root / "boundary.tif"
        input_path.write_bytes(raw)
        subprocess.run(
            [str(converter), str(input_path), str(output_path), "12", "1"],
            check=True,
        )

        tags, tiff = read_ifd(output_path)
        assert tags[256][2] == 12  # ImageWidth
        assert tags[257][2] == 1  # ImageLength
        bits_offset = tags[258][2]
        assert struct.unpack_from("<HHH", tiff, bits_offset) == (8, 8, 8)
        assert tags[259][2] == 1  # Compression: none
        assert tags[262][2] == 2  # PhotometricInterpretation: RGB
        assert tags[277][2] == 3  # SamplesPerPixel
        assert tags[284][2] == 1  # PlanarConfiguration: chunky

        pixel_offset = tags[273][2]
        byte_count = tags[279][2]
        pixels = tiff[pixel_offset:pixel_offset + byte_count]
        grayscale = bytes(
            [0, 0, 0] * 2 +
            [255, 255, 255] * 2 +
            [130, 130, 130] * 2
        )
        assert pixels[:len(grayscale)] == grayscale

        primaries = [pixels[index:index + 3] for index in range(len(grayscale), len(pixels), 3)]
        for actual, expected in zip(
            primaries,
            [(255, 0, 0)] * 2 + [(0, 255, 0)] * 2 + [(0, 0, 255)] * 2,
            strict=True,
        ):
            assert all(abs(value - target) <= 1 for value, target in zip(actual, expected, strict=True))

    print("packing, TIFF tags, BT.709 boundaries, and primaries: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
