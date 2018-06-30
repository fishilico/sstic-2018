#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Décrypte la communication de l'agent avec le pcap"""
import binascii
import os.path
import struct
import sys
from scapy.all import rdpcap, IP, TCP


sys.path.insert(0, os.path.dirname(__file__))
from square_attack import decrypt4rounds


def parse_peer_reply(payload):
    conn_struct = struct.unpack('<QHHBBBBQ', payload[:0x18])
    sockaddr_in = conn_struct[1:8]
    assert sockaddr_in[0] == 2
    assert sockaddr_in[6] == 0
    with open('serveur_racine_infos.out.txt', 'w') as fout:
        fout.write('ID agent de la racine = {}\n'.format(conn_struct[0]))
        fout.write('sockaddr_in.sin_family = {}\n'.format(sockaddr_in[0]))
        fout.write('sockaddr_in.sin_port = {}\n'.format(sockaddr_in[1]))
        fout.write('sockaddr_in.sin_addr = {}.{}.{}.{}\n'.format(
            sockaddr_in[2], sockaddr_in[3], sockaddr_in[4], sockaddr_in[5]))


confidentiel_content = b''
surprise_content = b''


def decode_msg(direction, msg):
    global confidentiel_content, surprise_content

    # Décode l'entête du message
    assert msg[:8] == b'AAAA\xde\xc0\xd3\xd1'
    assert msg[8:16] == b'babar007'
    src_addr, dst_addr, cmd, size = struct.unpack('<QQII', msg[0x10:0x28])
    assert size <= len(msg)
    assert all(x == 0 for x in msg[size:])  # padding
    payload = msg[0x28:size]
    if cmd != 0x3000204 and cmd != 0x3000202:
        print('\033[34m%s %#x - %#x %#x %d %r\033[m' % (direction, src_addr, dst_addr, cmd, len(payload), payload[:40]))

    if direction == 'send':
        assert src_addr == 0x28e48f9f80ddf725
        assert dst_addr == 0
    else:
        assert src_addr in (0, 0xdf8e9f2b91cee2d4)
        assert dst_addr == 0x28e48f9f80ddf725

    if cmd == 0x201:  # cmd
        print('Cmd: %r' % payload)
    elif cmd == 0x3000201:
        print('Response:\n%s' % '\n'.join('    %s' % l for l in payload.decode('ascii').splitlines()))
    elif cmd == 0x204 or cmd == 0x202:
        print('Get file: %r' % payload)
    elif cmd == 0x3000204:  # get
        confidentiel_content += payload
    elif cmd == 0x3000202:  # put
        surprise_content += payload
    elif cmd == 0x1010000:
        assert direction == 'recv'
        parse_peer_reply(payload)
    elif payload:
        print("Unknown cmd %#x %r" % (cmd, payload))


aes_keys = {}
with open('square_attack_keys.out.txt', 'r') as fkeys:
    for line in fkeys:
        direction, hexkey = line.split('=')
        aes_keys[direction] = binascii.unhexlify(hexkey.strip())


tcp_contents = {'send': b'', 'recv': b''}
for pkt in rdpcap('agent.out.pcap'):
    flow = "%s:%s-%s:%s" % (pkt[IP].src, pkt[TCP].sport, pkt[IP].dst, pkt[TCP].dport)
    if flow == '192.168.231.123:49734-192.168.23.213:31337':
        tcp_contents['send'] += bytes(pkt[TCP].payload)
    elif flow == '192.168.23.213:31337-192.168.231.123:49734':
        tcp_contents['recv'] += bytes(pkt[TCP].payload)
    else:
        raise RuntimeError("Unknown flow %r" % flow)


# Découpe et déchiffre chaque message
for direction, content in sorted(tcp_contents.items()):
    # Ignore les 4 premiers paquets de 256 octets
    offset = 512
    while offset < len(content):
        size = struct.unpack('<I', content[offset:offset + 4])[0]
        assert 32 <= size <= 0x4000
        iv = content[offset + 4:offset + 20]
        payload_offset = offset + 20

        decrypted = b''
        for off_in_blk in range(payload_offset, payload_offset + size - 16, 16):
            ciphertext = list(content[off_in_blk:off_in_blk + 16])
            cleartext = decrypt4rounds(ciphertext, aes_keys[direction])
            # CBC mode
            for i in range(16):
                cleartext[i] ^= iv[i]
            iv = ciphertext
            decrypted += bytes(cleartext)
        assert decrypted.startswith(b'AAAA\xde\xc0\xd3\xd1')
        decode_msg(direction, decrypted)
        offset += 4 + size


# Enregistre les archives échangées
with open('confidentiel.out.tgz', 'wb') as f:
    f.write(confidentiel_content)

with open('surprise.out.tgz', 'wb') as f:
    f.write(surprise_content)
