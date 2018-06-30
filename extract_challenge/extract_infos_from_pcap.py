#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Extrait les informations du pcap permettant d'en casser le chiffrement"""
import binascii
import json
import struct
from scapy.all import rdpcap, IP, TCP

# Parcourt les paquets du fichier agent.pcap et extrait le contenu des paquets TCP
# de chaque sens de la communication agent-serveur
tcp_contents = {'send': b'', 'recv': b''}
for pkt in rdpcap('agent.out.pcap'):
    # Détermine si l'agent envoie ou reçoit des informations
    flow = "%s:%s-%s:%s" % (pkt[IP].src, pkt[TCP].sport, pkt[IP].dst, pkt[TCP].dport)
    if flow == '192.168.231.123:49734-192.168.23.213:31337':
        tcp_contents['send'] += bytes(pkt[TCP].payload)
    elif flow == '192.168.23.213:31337-192.168.231.123:49734':
        tcp_contents['recv'] += bytes(pkt[TCP].payload)
    else:
        raise RuntimeError("Unknown flow %r" % flow)

# Sauvegarde les 4 premiers paquets de 256 octets séparemment
handshake = []
for direction, offset in (('send', 0), ('recv', 0), ('send', 256), ('recv', 256)):
    hexblk = binascii.hexlify(tcp_contents[direction][offset:offset + 256])
    handshake.append(hexblk.decode('ascii'))

with open('rsa2048_handshake.out.json', 'w') as fp:
    json.dump(handshake, fp=fp, indent=2)

# Extrait l'IV et le premier bloc de chaque message
fistblocks = {direction: [] for direction in tcp_contents.keys()}
for direction, content in tcp_contents.items():
    # Ignore les 4 premiers paquets de 256 octets
    offset = 512
    while offset < len(content):
        size = struct.unpack('<I', content[offset:offset + 4])[0]
        assert 32 <= size <= 0x4000
        iv = content[offset + 4:offset + 20]
        first_block = content[offset + 20:offset + 36]
        offset += 4 + size

        # Le vecteur d'initialisation est incrémental donc correspond à la
        # position à laquelle est insérée le bloc
        assert struct.unpack('>QQ', iv) == (0, len(fistblocks[direction]))
        fistblocks[direction].append(binascii.hexlify(first_block).decode('ascii'))

with open('capture_firstblocks.out.json', 'w') as fp:
    json.dump(fistblocks, fp=fp, indent=2)
