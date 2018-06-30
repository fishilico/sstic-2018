#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Implémente l'attaque intégrale sur un AES 4 tours"""
import itertools
import json
import binascii


# Implémentation d'AES 4 tours copiée de
# https://github.com/p4-team/ctf/blob/master/2016-03-12-0ctf/peoples_square/integral.py
aes_sbox = (
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7,
    0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf,
    0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5,
    0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a,
    0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e,
    0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed,
    0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf, 0xd0, 0xef,
    0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff,
    0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d,
    0x64, 0x5d, 0x19, 0x73, 0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee,
    0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
    0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5,
    0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08, 0xba, 0x78, 0x25, 0x2e,
    0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e,
    0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55,
    0x28, 0xdf, 0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f,
    0xb0, 0x54, 0xbb, 0x16)

aes_invsbox = [0] * 256
for i in range(256):
    aes_invsbox[aes_sbox[i]] = i

aes_rcon = (
    [0x01, 0x00, 0x00, 0x00], [0x02, 0x00, 0x00, 0x00], [0x04, 0x00, 0x00, 0x00],
    [0x08, 0x00, 0x00, 0x00], [0x10, 0x00, 0x00, 0x00], [0x20, 0x00, 0x00, 0x00],
    [0x40, 0x00, 0x00, 0x00], [0x80, 0x00, 0x00, 0x00], [0x1B, 0x00, 0x00, 0x00],
    [0x36, 0x00, 0x00, 0x00])


def SubBytes(state):
    state = [list(c) for c in state]
    for i in range(len(state)):
        row = state[i]
        for j in range(len(row)):
            state[i][j] = aes_sbox[state[i][j]]
    return state


def InvSubBytes(state):
    state = [list(c) for c in state]
    for i in range(len(state)):
        row = state[i]
        for j in range(len(row)):
            state[i][j] = aes_invsbox[state[i][j]]
    return state


def rowsToCols(state):
    return [
        [state[0][0], state[1][0], state[2][0], state[3][0]],
        [state[0][1], state[1][1], state[2][1], state[3][1]],
        [state[0][2], state[1][2], state[2][2], state[3][2]],
        [state[0][3], state[1][3], state[2][3], state[3][3]]]


def colsToRows(state):
    return [
        [state[0][0], state[1][0], state[2][0], state[3][0]],
        [state[0][1], state[1][1], state[2][1], state[3][1]],
        [state[0][2], state[1][2], state[2][2], state[3][2]],
        [state[0][3], state[1][3], state[2][3], state[3][3]]]


def RotWord(word):
    return [word[1], word[2], word[3], word[0]]


def SubWord(word):
    return [aes_sbox[word[0]], aes_sbox[word[1]], aes_sbox[word[2]], aes_sbox[word[3]]]


def XorWords(word1, word2):
    return [w1 ^ w2 for w1, w2 in zip(word1, word2)]


def KeyExpansion(key):
    Nk = Nb = Nr = 4
    temp = [0, 0, 0, 0]
    w = [0, 0, 0, 0] * (Nb * (Nr + 1))
    for i in range(Nk):
        w[i] = [key[4 * i], key[4 * i + 1], key[4 * i + 2], key[4 * i + 3]]
    for i in range(Nk, Nb * (Nr + 1)):
        temp = w[i - 1]
        if (i % Nk) == 0:
            temp = XorWords(SubWord(RotWord(temp)), aes_rcon[i // Nk - 1])
        w[i] = XorWords(w[i - Nk], temp)
    return w


def Shiftrows(state):
    state = colsToRows(state)
    state[1].append(state[1].pop(0))
    state[2].append(state[2].pop(0))
    state[2].append(state[2].pop(0))
    state[3].append(state[3].pop(0))
    state[3].append(state[3].pop(0))
    state[3].append(state[3].pop(0))
    return rowsToCols(state)


def InvShiftrows(state):
    state = colsToRows(state)
    state[1].insert(0, state[1].pop())
    state[2].insert(0, state[2].pop())
    state[2].insert(0, state[2].pop())
    state[3].insert(0, state[3].pop())
    state[3].insert(0, state[3].pop())
    state[3].insert(0, state[3].pop())
    return rowsToCols(state)


def galoisMult(a, b):
    p = 0
    for i in range(8):
        if b & 1 == 1:
            p ^= a
        a <<= 1
        if a & 0x100:
            a ^= 0x1b
        b >>= 1
    return p & 0xff


def mixColumn(column):
    temp = [c for c in column]
    column[0] = galoisMult(temp[0], 2) ^ galoisMult(temp[3], 1) ^ \
        galoisMult(temp[2], 1) ^ galoisMult(temp[1], 3)
    column[1] = galoisMult(temp[1], 2) ^ galoisMult(temp[0], 1) ^ \
        galoisMult(temp[3], 1) ^ galoisMult(temp[2], 3)
    column[2] = galoisMult(temp[2], 2) ^ galoisMult(temp[1], 1) ^ \
        galoisMult(temp[0], 1) ^ galoisMult(temp[3], 3)
    column[3] = galoisMult(temp[3], 2) ^ galoisMult(temp[2], 1) ^ \
        galoisMult(temp[1], 1) ^ galoisMult(temp[0], 3)
    return column


def MixColumns(cols):
    return [mixColumn(c) for c in cols]


def mixColumnInv(column):
    temp = [c for c in column]
    column[0] = galoisMult(temp[0], 0xE) ^ galoisMult(temp[3], 0x9) ^ \
        galoisMult(temp[2], 0xD) ^ galoisMult(temp[1], 0xB)
    column[1] = galoisMult(temp[1], 0xE) ^ galoisMult(temp[0], 0x9) ^ \
        galoisMult(temp[3], 0xD) ^ galoisMult(temp[2], 0xB)
    column[2] = galoisMult(temp[2], 0xE) ^ galoisMult(temp[1], 0x9) ^ \
        galoisMult(temp[0], 0xD) ^ galoisMult(temp[3], 0xB)
    column[3] = galoisMult(temp[3], 0xE) ^ galoisMult(temp[2], 0x9) ^ \
        galoisMult(temp[1], 0xD) ^ galoisMult(temp[0], 0xB)
    return column


def InvMixColumns(cols):
    return [mixColumnInv(c) for c in cols]


def AddRoundKey(s, ks, r):
    for i in range(len(s)):
        for j in range(len(s[i])):
            s[i][j] = s[i][j] ^ ks[r * 4 + i][j]
    return s


def oneRound(s, ks, r):
    s = SubBytes(s)
    s = Shiftrows(s)
    s = MixColumns(s)
    s = AddRoundKey(s, ks, r)
    return s


def oneRoundDecrypt(s, ks, r):
    s = AddRoundKey(s, ks, r)
    s = InvMixColumns(s)
    s = InvShiftrows(s)
    s = InvSubBytes(s)
    return s


def finalRound(s, ks, r):
    s = SubBytes(s)
    s = Shiftrows(s)
    s = AddRoundKey(s, ks, r)
    return s


def finalRoundDecrypt(s, ks, r):
    s = AddRoundKey(s, ks, r)
    s = InvShiftrows(s)
    s = InvSubBytes(s)
    return s


def encrypt4rounds(message, key):
    s = [message[:4], message[4:8], message[8:12], message[12:16]]
    ks = KeyExpansion(key)
    s = AddRoundKey(s, ks, 0)
    c = oneRound(s, ks, 1)
    c = oneRound(c, ks, 2)
    c = oneRound(c, ks, 3)
    c = finalRound(c, ks, 4)
    output = []
    for i in range(len(c)):
        for j in range(len(c[i])):
            output.append(c[i][j])
    return output


def decrypt4rounds(message, key):
    s = [message[:4], message[4:8], message[8:12], message[12:16]]
    ks = KeyExpansion(key)
    s = finalRoundDecrypt(s, ks, 4)
    c = oneRoundDecrypt(s, ks, 3)
    c = oneRoundDecrypt(c, ks, 2)
    c = oneRoundDecrypt(c, ks, 1)
    c = AddRoundKey(c, ks, 0)
    output = []
    for i in range(len(c)):
        for j in range(len(c[i])):
            output.append(c[i][j])
    return output


def check_aes_implementation():
    testCt = list(range(16))
    testState = []
    testState.append(testCt[:4])
    testState.append(testCt[4:8])
    testState.append(testCt[8:12])
    testState.append(testCt[12:16])

    key = [
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
    ]

    ks = KeyExpansion(key)

    textData = [0]*16
    assert AddRoundKey(AddRoundKey(testState, ks, 1), ks, 1) == testState
    assert InvMixColumns(MixColumns(testState)) == testState
    assert InvShiftrows(Shiftrows(testState)) == testState
    assert InvSubBytes(SubBytes(testState)) == testState
    assert oneRoundDecrypt(oneRound(testState, ks, 1), ks, 1) == testState
    assert finalRoundDecrypt(finalRound(testState, ks, 1), ks, 1) == testState
    assert decrypt4rounds(encrypt4rounds(textData, key), key) == textData


def round2master(rk):
    """Calcule la clé associée à la sous-clé du round final"""
    Nr = 4
    Nk = 4
    Nb = 4
    w = [[0, 0, 0, 0] for i in range(Nb * (Nr + 1))]
    for i in range(Nk):
        w[i] = [rk[4 * i], rk[4 * i + 1], rk[4 * i + 2], rk[4 * i + 3]]

    for j in range(Nk, Nb * (Nr + 1)):
        if (j % Nk) == 0:
            w[j][0] = w[j-Nk][0] ^ aes_sbox[w[j-1][1] ^ w[j-2][1]] ^ aes_rcon[Nr - j//Nk][0]
            for i in range(1, 4):
                w[j][i] = w[j-Nk][i] ^ aes_sbox[w[j-1][(i+1) % 4] ^ w[j-2][(i+1) % 4]]
        else:
            w[j] = XorWords(w[j-Nk], w[j-Nk-1])

    m = []
    for i in range(16, 20):
        for j in range(4):
            m.append(w[i][j])
    return m


def attack_backup(ct, byte_guess, byte_index):
    # We just need to check sums
    # There is no mixColumns in the last round, so skip it.
    # shiftRows just changes the byte's position. We don't care, so skip it.
    # All we need is a single xor for the guessed byte, and InvSubBytes
    t = ct[byte_index] ^ byte_guess
    return aes_invsbox[t]


def get_potential_for_index(ciphertexts, index):
    """Perform the integral attack (square attack) to compute candidates"""
    assert len(ciphertexts) == 256
    potential = []
    for candidate_byte in range(256):
        sum_dec = 0
        for ciph in ciphertexts:
            one_round_decrypt = attack_backup(ciph, candidate_byte, index)
            sum_dec ^= one_round_decrypt
        if sum_dec == 0:
            potential.append(candidate_byte)
    return potential


def integral_attack(ciphertexts):
    candidates = [get_potential_for_index(ciphertexts, i) for i in range(16)]
    print("candidates: %r" % candidates)
    for round_key in itertools.product(*candidates):
        master_key = round2master(round_key)
        plain = ''.join(chr(c) for c in decrypt4rounds(ciphertexts[0], master_key))
        if plain.startswith('AAAA\xde\xc0\xd3\xd1'):
            print('solved: %r' % master_key)
            print('plaintext: %r' % plain)
            return master_key


if __name__ == '__main__':
    check_aes_implementation()
    with open('capture_firstblocks.out.json', 'r') as f:
        first_blocks = json.load(f)

    keys = {}
    for direction in ('recv', 'send'):
        print("Attacking direction %r" % direction)
        hex_ciphertexts = first_blocks[direction][:256]
        ciphertexts = [[c for c in binascii.unhexlify(x)] for x in hex_ciphertexts]
        key = integral_attack(ciphertexts)
        keys[direction] = ''.join('{:02x}'.format(b) for b in key)
        print("AES key: {}".format(keys[direction]))

    with open('square_attack_keys.out.txt', 'w') as fout:
        for direction in ('recv', 'send'):
            fout.write("{}={}\n".format(direction, keys[direction]))
