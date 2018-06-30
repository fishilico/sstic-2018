#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Déchiffre le premier flag de validation intermédiaire du code WebAssembly"""
import struct


with open('blockcipher.wasm', 'rb') as fwasm:
    data_segment_data_0 = fwasm.read()[0x34c7:]
assert len(data_segment_data_0) == 361


def trunc_u64(x):
    """Tronque un entier à 64 bits"""
    return x & 0xffffffffffffffff


def ror64(x, r):
    """Rotation de bits vers la droite d'un nombre de 64 bits"""
    assert 0 <= r < 63
    return (x >> r) | trunc_u64(x << (64 - r))


def rol64(x, r):
    """Rotation de bits vers la gauche d'un nombre de 64 bits"""
    assert 0 <= r < 63
    return (x >> (64 - r)) | trunc_u64(x << r)


def speck_schedule_key(key):
    """Algorithme de key scheduling de Speck"""
    l3_lowbytes, l4_highbytes = struct.unpack("<QQ", key)
    keyctx = [None] * 32
    keyctx[0] = l3_lowbytes
    for l1 in range(31):
        l4_highbytes = trunc_u64((ror64(l4_highbytes, 8) + l3_lowbytes)) ^ l1
        l3_lowbytes = l4_highbytes ^ rol64(l3_lowbytes, 3)
        keyctx[l1 + 1] = l3_lowbytes
    return keyctx


def speck_decrypt_block(keyctx, block):
    """Déchiffre un bloc de 16 octets avec Speck"""
    l9_lowbytes, l8_highbytes = struct.unpack("<QQ", block)
    for l0 in range(32, 0, -1):
        l9_lowbytes = ror64(l8_highbytes ^ l9_lowbytes, 3)
        l6 = trunc_u64((keyctx[l0 - 1] ^ l8_highbytes) - l9_lowbytes)
        l8_highbytes = rol64(l6, 8)
    return struct.pack('<QQ', l9_lowbytes, l8_highbytes)


def speck_decrypt_ecb(keyctx, data):
    """Déchiffre des données avec Speck"""
    output = bytearray(len(data))
    for offset in range(0, len(data), 16):
        output[offset:offset + 16] = speck_decrypt_block(
            keyctx,
            data[offset:offset + 16])
    return output.rstrip(b'\0')


flag_data = data_segment_data_0[297:]
assert len(flag_data) == 64
keyctx = speck_schedule_key(flag_data[-16:])
flag = speck_decrypt_ecb(keyctx, flag_data[:-16])
print(flag)

with open('flag1_anomaly_detection.out.txt', 'wb') as fout:
    fout.write(b'Anomaly Detection: ' + flag + b'\n')
