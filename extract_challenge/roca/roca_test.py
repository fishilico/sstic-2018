#!/usr/bin/env python
import binascii
import logging
import struct
import sys


logging.basicConfig(level=logging.DEBUG)

sys.path.insert(0, '.')
from roca_detect import DlogFprint


def decode_bigint_be(data):
    """Decode a Big-Endian big integer"""
    return int(binascii.hexlify(data).decode('ascii'), 16)


def decode_bigint_be_from_hex(hexdata):
    """Decode a Big-Endian big integer"""
    return int(hexdata.replace(' ', '').replace('\n', ''), 16)

def encode_bigint_be(value, bytelen=None):
    """Encode a Big-Endian big integer"""
    if bytelen is None:
        bytelen = (value.bit_length() + 7) // 8
    hexval = '{{:0{:d}x}}'.format(bytelen * 2).format(value)
    return binascii.unhexlify(hexval.encode('ascii'))


GENP_DATA = """
    0f389b45 5d0e5c30 08aaf2d3 305ed5bc
    5aad78aa 5de8b6d1 bb87edf1 1e2655b6
    a8ec19b8 9c3a3004 e48e955d 4ef05be4
    defd119a 49124877 e6ffa3a9 d4d1
"""

GEN_P = decode_bigint_be_from_hex(GENP_DATA)

PRIMES_DATA = """
    02000000 00000000 03000000 00000000
    05000000 00000000 07000000 00000000
    0b000000 00000000 0d000000 00000000
    11000000 00000000 13000000 00000000
    17000000 00000000 1d000000 00000000
    1f000000 00000000 25000000 00000000
    29000000 00000000 2b000000 00000000
    2f000000 00000000 35000000 00000000
    3b000000 00000000 3d000000 00000000
    43000000 00000000 47000000 00000000
    49000000 00000000 4f000000 00000000
    53000000 00000000 59000000 00000000
    61000000 00000000 65000000 00000000
    67000000 00000000 6b000000 00000000
    6d000000 00000000 71000000 00000000
    7f000000 00000000 83000000 00000000
    89000000 00000000 8b000000 00000000
    95000000 00000000 97000000 00000000
    9d000000 00000000 a3000000 00000000
    a7000000 00000000 ad000000 00000000
    b3000000 00000000 b5000000 00000000
    bf000000 00000000 c1000000 00000000
    c5000000 00000000 c7000000 00000000
    d3000000 00000000 df000000 00000000
    e3000000 00000000 e5000000 00000000
    e9000000 00000000 ef000000 00000000
    f1000000 00000000 fb000000 00000000
    01010000 00000000 07010000 00000000
    0d010000 00000000 0f010000 00000000
    15010000 00000000 19010000 00000000
    1b010000 00000000 25010000 00000000
    33010000 00000000 37010000 00000000
    39010000 00000000 3d010000 00000000
    4b010000 00000000 51010000 00000000
    5b010000 00000000 5d010000 00000000
    61010000 00000000 67010000 00000000
    6f010000 00000000 75010000 00000000
    7b010000 00000000 7f010000 00000000
    85010000 00000000 8d010000 00000000
    91010000 00000000 99010000 00000000
    a3010000 00000000 a5010000 00000000
    af010000 00000000 b1010000 00000000
    b7010000 00000000 bb010000 00000000
    c1010000 00000000 c9010000 00000000
    cd010000 00000000 cf010000 00000000
    d3010000 00000000 df010000 00000000
    e7010000 00000000 eb010000 00000000
    f3010000 00000000 f7010000 00000000
    fd010000 00000000 09020000 00000000
    0b020000 00000000 1d020000 00000000
    23020000 00000000 2d020000 00000000
    33020000 00000000 39020000 00000000
    3b020000 00000000 41020000 00000000
    4b020000 00000000 51020000 00000000
    57020000 00000000 59020000 00000000
    5f020000 00000000 65020000 00000000
    69020000 00000000 6b020000 00000000
    77020000 00000000 81020000 00000000
    83020000 00000000 87020000 00000000
    8d020000 00000000 93020000 00000000
    95020000 00000000 a1020000 00000000
    a5020000 00000000 ab020000 00000000
    b3020000 00000000 bd020000 00000000
"""
PRIMES_BIN = binascii.unhexlify(PRIMES_DATA.replace(' ', '').replace('\n', '').encode('ascii'))
PRIMES = struct.unpack('<%dQ' % (len(PRIMES_BIN) // 8), PRIMES_BIN)

product_of_primes = 1
phi_product_of_primes = 1
for p in PRIMES:
    product_of_primes *= p
    phi_product_of_primes *= p - 1

MY_RSA_N = decode_bigint_be_from_hex("""
    a0e1 cdfc c314 1ec0 a071 247e df25 1a4a
    118d c878 9e1c 44f5 ba63 e4b6 b3f3 4210
    7964 4657 5b12 bddc 0d73 ecc3 a5b3 98fc
    bdc0 dcc7 1b2d facf 01be 1250 0ac6 a572
    f282 9d2b fa9a f28b f873 dc4a 299a d8d0
    3345 c5ff c9c0 7a86 bdd0 1c30 bbea c413
    bddd 3e92 8ae8 6e8c 2a2a da44 e4f0 353e
    8d2e 9924 4656 9d96 769e 4054 17e8 2108
    2a19 6fb5 c895 d98b 6d26 9214 9843 9361
    7b86 0b25 5d1d 0c62 a5e1 f171 7bc7 7726
    14d8 7e56 7329 59ca ea30 000d 1b59 5729
    4e7a 5cab 70e5 988b c1e2 06e7 e6d0 ca09
    5f68 e341 4ece 1ddb 0e88 ce76 67cc a91b
    7c98 8829 976e 1455 f984 3a5e 7da1 a2b3
    6a2a 2387 65e8 d5d4 2187 6a52 eb4e 077d
    8622 66f7 b6b0 dda7 a1f2 d02d 430e 311d
""")
PEER_RSA_N = decode_bigint_be_from_hex("""
    df3b c349 ea89 004a 1b5f 7902 8c0a 4a63
    e83b 6262 cb1c 301d 77da 0d68 292b de3f
    1a38 6620 11d8 e3e2 4491 2c6e da9e 1712
    d2e6 94e0 8d28 cf14 8cac c756 150c d007
    3d67 e34a d9e4 b812 4fdd 5527 b325 be26
    26a8 d468 a742 d16e 7dea 738d ea66 576b
    92b3 1eb0 8b6a a74c 8653 e597 6124 6305
    9059 789a bea4 ee09 010a ba67 c6d2 71d6
    8bd0 b125 5c2e eb5b aa92 009b 6cd4 ad6e
    ce8c 3fdd 60c0 c30e acf1 c7dd 72b0 d7dd
    eef1 3b33 ca65 dc62 49f7 25f6 7d01 d3fc
    9dbf 5325 0e04 f294 b5fe 3074 bf28 8294
    7998 3af7 86b1 dd48 7dd2 fbf8 3056 f033
    f511 90a9 00b0 3db4 741f dafa 0512 645a
    4146 b0d3 cdab fd3b 1686 8a79 31e0 d289
    3e5f 90c5 e614 c11a c901 2cdb f402 5845
""")
MODULI = (
    ('my', MY_RSA_N),
    ('peer', PEER_RSA_N),
)
MODULI_WITH_PQ = (
    ('my', MY_RSA_N,
        140593188526543551856544248498623691571805608006398212249068967519026500044666156244837178813023773052913867123122996627405078597338048758716754823967228918954989671473114184022769392969918846611532764299976570398102662495596242353036018351708302865413048048747210168285751154740226608508609267688025737461861,
    144455627078908803014064356071183236556558321644894887618692464352582057409809026683982976363245726375981082523215500170562487226261595895528788746806528380434989686204958165593070995847858489850573647257907019560330791126415620512706824199310099842292997704993709863389102344365270290677553896233832622650969,
    ),
    ('peer', PEER_RSA_N,
        175594539091517144724957514072216753115834462316800503946621764118107067308148843115901811265802559556177342032383279608486296002246193089662279579027652035495435685921342801996553681124814626652367775512835288505338864396612445547188766860423629582750660328630357010120446787949651400700198649254108739122189,
        160486836955563953148034602726430783938305016569227799211684765497845735236979621423275356590022273477329630249894632395222238390887771722348265702154853866477552652992391629178086461844358814950642678677999089441850332880613920342214757975211899731130342273481253673579676065468850596794332296662099027627801,
    ),
)
AES_DATA = {
    'peer': decode_bigint_be_from_hex("""
        36f5 52e8 e281 65c1 96dc 56cb 2837 a322
        dd47 aa00 d998 8f3b 33fd f0ff 68dc adec
        c910 2f67 5920 442f 2f98 c290 9589 dd8d
        a026 3b5f 4b2a 203b d383 7e92 51fc 6833
        9711 c366 7dad 52d8 a76c d878 601e 39fe
        9edf a430 5152 423e 006c ba25 4ae7 1986
        078d 79b6 d314 dc35 7104 f17d ccfc 7261
        9ab0 38c7 5d46 9ab3 62b3 3ed6 e387 7ca7
        6747 aa66 5ab4 213e 723a 7090 9a62 def2
        d31b 543c ffa7 6d93 4c24 c224 4028 2814
        954b 6c3e 2a89 2e34 d0ff 0b1b 6082 fa58
        092f a7ce bb08 1b69 e561 0e7c 5ef3 e300
        2693 cfbd e165 7439 c67c 1b09 d1db 6eef
        0bb3 8141 0c52 7977 1634 41ed e128 ff09
        a523 5213 9fc6 7895 d3d7 86c3 1c03 7715
        8d56 04c0 5f9b 42a8 13f5 5369 044b 61fb
"""),
    'my': decode_bigint_be_from_hex("""
        1d64 1a6d 9941 887a 0f64 7f80 9c12 f80a
        5d57 a7cd 6535 8af9 d348 79e7 b8fc eb24
        4808 d0cb 1350 686a e0e3 bab6 7f83 4b80
        a2c3 19af 8e17 e739 517e cd08 95f8 917d
        0bac 6da8 f7b4 e3ea a460 d6d4 21c1 edcf
        41d9 12f5 825a 49bf c413 c241 f9eb 23fe
        3966 6afc 2a1a 5a69 77aa ce9e 2e4c 942d
        7832 55d0 ce42 2331 c1a3 699f e6e8 c54b
        cffb 0c68 335d ca22 4439 ddbb 4f60 4b0c
        2123 5666 f780 c3c3 38a4 c1ff c95d b703
        4691 ef4a c15d 2a38 23bc bc4c 4899 d682
        32a2 4d0e b852 3c70 f7e4 3d08 91a0 70a1
        61a8 0f31 cca9 e16b a4d2 2092 11e8 dd39
        f8bd 7aac 5ebe d1ea 3ef7 9b11 d8d9 b619
        2be2 2989 350e a320 d78b a2c4 1fef ebc6
        d964 ca74 e873 c53d de83 1d20 225e 4ada
"""),
}
AES_KEYS = {
    'peer': bytes((76, 26, 105, 54, 47, 224, 3, 54, 246, 168, 70, 15, 243, 61, 255, 213)),
    'my': bytes((114, 255, 128, 54, 217, 32, 7, 119, 209, 233, 122, 91, 225, 211, 245, 20)),
}

if __name__ == '__main__':
    print("gen_p (%d bits) = %#x" % (GEN_P.bit_length(), GEN_P))
    print("primes[%d] = %r" % (len(PRIMES), PRIMES))  # n = 126, primes until 701
    print("product_of_primes[%d bits = %s bytes] = %#x" % (
        product_of_primes.bit_length(), product_of_primes.bit_length() / 8, product_of_primes))

    print("6925650131069390 ^2 = %#x" % (6925650131069390 ** 2))
    print("Crafting primes of %d bits" % ((6925650131069390 * product_of_primes).bit_length()))  # Crafting primes of 1024 bits

    obj = DlogFprint(max_prime=PRIMES[-1], generator=GEN_P)
    for name, modulus, p, q in MODULI_WITH_PQ:
        print("")

        d = DlogFprint.discrete_log(modulus, obj.generator, obj.generator_order, obj.generator_order_decomposition, obj.m)
        print("%s RSA modulus: %#x" % (name, modulus))
        print("Fingerprinting %s RSA: d = %r" % (name, d))
        assert d is not None  # Yes, vulnerable

        assert p * q == modulus
        print("p = %d bits, q = %d bits" % (p.bit_length(), q.bit_length()))
        a = DlogFprint.discrete_log(p, obj.generator, obj.generator_order, obj.generator_order_decomposition, obj.m)
        b = DlogFprint.discrete_log(q, obj.generator, obj.generator_order, obj.generator_order_decomposition, obj.m)
        print("a = %d, b = %d" % (a, b))
        print("a + b %% ord= %r" % ((a + b) % obj.generator_order))
        assert (a + b) % obj.generator_order == d
        assert (p - pow(obj.generator, a, obj.m)) % obj.m == 0
        assert (q - pow(obj.generator, b, obj.m)) % obj.m == 0
        print("p = (CST+%d) * M + pow(gen, a, M)" % ((p - pow(obj.generator, a, obj.m)) // obj.m - 6925650131069390))
        print("q = (CST+%d) * M + pow(gen, b, M)" % ((q - pow(obj.generator, b, obj.m)) // obj.m - 6925650131069390))

        privkey = DlogFprint.mul_inv(0x10001, (p - 1) * (q - 1))
        print("private key = %d" % privkey)
        # test
        for msg in [2, 4545534, 543647867658]:
            encrypted = pow(msg, 0x10001, modulus)
            decrypted = pow(encrypted, privkey, modulus)
            assert msg == decrypted, "%d -> %d" % (msg, decrypted)

        # decrypt!
        encrypted = AES_DATA[name]
        print("encrypted(%d): %r" % (len(encode_bigint_be(encrypted)), binascii.hexlify(encode_bigint_be(encrypted))))
        decrypted = encode_bigint_be(pow(encrypted, privkey, modulus))
        print("decrypted(%d): %r" % (len(decrypted), binascii.hexlify(decrypted)))
        print("vs %r" % binascii.hexlify(AES_KEYS[name]))
        x, y = decrypted.split(b'\0')
        assert len(decrypted) == 255
        assert decrypted[:1] == b'\x02'
        assert y == AES_KEYS[name]

