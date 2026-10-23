# Copyright The Phorepoc Authors
# SPDX-License-Identifier: Apache-2.0
#
# Stand-in for Symbian's uidcrc tool (snmtool.pl's WRITER.pm runs it): prints the three UIDs and the UID checksum
# (CRC-16/CCITT over the even and the odd bytes of the 12-byte UID block, as euser's crypt.cpp does).
import struct
import sys


def _crc16(data):
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def uid_checksum(u1, u2, u3):
    block = struct.pack("<III", u1, u2, u3)
    even = _crc16(block[0:12:2])
    odd = _crc16(block[1:12:2])
    return ((odd << 16) | even) & 0xFFFFFFFF


if __name__ == "__main__":
    a = [int(x, 16) for x in sys.argv[1:4]]
    print("0x%08x 0x%08x 0x%08x 0x%08x" % (a[0], a[1], a[2], uid_checksum(*a)))
