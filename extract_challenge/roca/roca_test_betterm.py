#!/usr/bin/env python
"""Find a better modulus to use

https://crocs.fi.muni.cz/_media/public/papers/nemec_roca_ccs17_preprint.pdf
(1) primes (p,q) are still of the form (1) – M0 must be a divisor of M;
(2) Coppersmith’s algorithm will find k0 for correct guess of a0 – enough bits must be known (log2 (M0) > log2 (N)/4);
(3) overall time of the factorization will be minimal – number of attempts (ordM0 (65537)) and time per attempt (running time of Coppersmith’s algorithm) should result in a minimal time.

En primes décroissants ou croissants:

Final M = 3760521612380916345048248036465914040505038286724779677484652961679034681273556134732998926971941209335901202681827127086369010449740069641128520265646511536990250330559437958993581185688496097067797726693263764630
Final phi(M) = 338751992361839103757913831646640400994371081216046845152117164887948163614898472210765185843738279364851565592497066513567977048544195912492148513772168336428185686657328096896614400000000000000000000000000000000
Final log2(M) = 709.481617
Generator order: 27663515880
Final fingerprinting my RSA: d=11910002217
Final fingerprinting peer RSA: d=7829338907

Enlève 2 primes:
    683 373 => 892371480 (= 27663515880/31)

Final M = 14761094259205430799493827642854282048936596103473399084957363475594717679350115735785581380724297117416464983305112388910181820660860144847202729896280451473707505252255810232390538452767109688245744906728570
Final phi(M) = 1335225271820070254146224859074513610326881252231130944534249223062892834227676631865343809493497459105302106362126992532904396653360593102561049545029516036121565630251506073600000000000000000000000000000000
Final log2(M) = 691.522843
Generator order: 892371480
Final fingerprinting my RSA: d=309172977
Final fingerprinting peer RSA: d=690367067

Enlève 3 primes:
    461 599 691 => 38798760 (= 892371480/23)
    443 521 547 => 2984520
    409 613 647 => 175560
    457 601 617 => 87780
    421 571 661 => 17556
    419 673 701 => 924

Final M = 709664881851089264818029587997515866787058444759468871442482047636096353604452219252631670308075095802395413871693625645462943534181109793273668860623304725590
Final phi(M) = 66370577520239097751412702804243116415645348096215069702324202990319556302160774373892003339951392677017867736705706441438324710849904640000000000000000000000
Final log2(M) = 527.691777
Generator order: 924
Final fingerprinting my RSA: d=729
Final fingerprinting peer RSA: d=467


(possibilité de 7 19 397 ; et 131 397 1 462)

=>
My N
successful factorization [140593188526543551856544248498623691571805608006398212249068967519026500044666156244837178813023773052913867123122996627405078597338048758716754823967228918954989671473114184022769392969918846611532764299976570398102662495596242353036018351708302865413048048747210168285751154740226608508609267688025737461861, 144455627078908803014064356071183236556558321644894887618692464352582057409809026683982976363245726375981082523215500170562487226261595895528788746806528380434989686204958165593070995847858489850573647257907019560330791126415620512706824199310099842292997704993709863389102344365270290677553896233832622650969]

Peer N: :(

Un cran au dessus:
    Final M = 140281505998018719071287438310889510216105257013334232404667280801898691627164926233224684617429383600061082975577526560835898499919563153700379304761409264588118078330
    Final phi(M) = 13050260820187557121386575796830472100082380989184099753372841526218161460936227014970137719125290478182527683038780281601981760585226369630208000000000000000000000000
    Final log2(M) = 555.250317
    Generator order: 17556
    Final fingerprinting my RSA: d=11817
    Final fingerprinting peer RSA: d=12479

successful factorization [175594539091517144724957514072216753115834462316800503946621764118107067308148843115901811265802559556177342032383279608486296002246193089662279579027652035495435685921342801996553681124814626652367775512835288505338864396612445547188766860423629582750660328630357010120446787949651400700198649254108739122189, 160486836955563953148034602726430783938305016569227799211684765497845735236979621423275356590022273477329630249894632395222238390887771722348265702154853866477552652992391629178086461844358814950642678677999089441850332880613920342214757975211899731130342273481253673579676065468850596794332296662099027627801]

"""
import math
import sys

sys.path.insert(0, '.')
from roca_test import *


def get_genp_order(new_m, new_phi_m):
    max_prime = 701
    new_phi_m_decomposition = DlogFprint.small_factors(new_phi_m, max_prime)
    return DlogFprint.element_order(GEN_P, new_m, new_phi_m, new_phi_m_decomposition)


def get_betterm():
    print("gen = %d" % GEN_P)
    print("M = %d" % product_of_primes)
    print("log2(M) = %f" % math.log2(product_of_primes))
    print("log2(N)/4 = %f and %f" % (math.log2(MY_RSA_N) / 4, math.log2(PEER_RSA_N) / 4))

    max_prime = 701
    generator = GEN_P
    m = product_of_primes
    phi_m = phi_product_of_primes
    phi_m_decomposition = DlogFprint.small_factors(phi_m, max_prime)
    generator_order = DlogFprint.element_order(generator, m, phi_m, phi_m_decomposition)
    generator_order_decomposition = DlogFprint.small_factors(generator_order, max_prime)
    print("Generator order: %d" % generator_order)
    assert pow(generator, generator_order, m) == 1
    if 0:
        for name, modulus in MODULI:
            d = DlogFprint.discrete_log(modulus, generator, generator_order, generator_order_decomposition, m)
            #print("%s RSA modulus: %#x" % (name, modulus))
            print("Fingerprinting %s RSA: d=%r" % (name, d))
            #print("check:\n    %d\n    %d" % (pow(generator, d, m), modulus % m))
            assert pow(generator, d, m) == modulus % m

    # Try to reduce the generator order by reducing M
    if 0:
        new_m = m
        new_phi_m = phi_m
        new_genorder = generator_order
        for iteration in range(5):
            for p in PRIMES[::-1]:
            #for p in PRIMES:
                if (new_m % p) != 0:
                    continue
                assert (new_m % (p ** 2)) != 0
                assert (new_phi_m % (p - 1)) == 0
                try_m = new_m // p
                try_phi_m = new_phi_m // (p - 1)
                try_genorder = get_genp_order(try_m, try_phi_m)
                print(p, try_genorder, math.log2(try_m))
                if try_genorder < new_genorder and math.log2(try_m) > 512:
                    new_m = try_m
                    new_phi_m = try_phi_m
                    new_genorder = try_genorder
                    print("Trying m = %d (log2 = %d)" % (new_m, math.log2(new_m)))

            m = new_m
            phi_m = new_phi_m
            print("Final M = %d" % m)
            print("Final phi(M) = %d" % phi_m)
            print("Final log2(M) = %f" % math.log2(m))
            phi_m_decomposition = DlogFprint.small_factors(phi_m, max_prime)
            generator_order = DlogFprint.element_order(generator, m, phi_m, phi_m_decomposition)
            generator_order_decomposition = DlogFprint.small_factors(generator_order, max_prime)
            print("Generator order: %d" % generator_order)
            assert pow(generator, generator_order, m) == 1
            if 1:
                for name, modulus in MODULI:
                    d = DlogFprint.discrete_log(modulus, generator, generator_order, generator_order_decomposition, m)
                    #print("%s RSA modulus: %#x" % (name, modulus))
                    print("Final fingerprinting %s RSA: d=%r" % (name, d))
                    #print("check:\n    %d\n    %d" % (pow(generator, d, m), modulus % m))
                    assert pow(generator, d, m) == modulus % m

    # Try removing two primes
    new_m = 3760521612380916345048248036465914040505038286724779677484652961679034681273556134732998926971941209335901202681827127086369010449740069641128520265646511536990250330559437958993581185688496097067797726693263764630
    new_phi_m = 338751992361839103757913831646640400994371081216046845152117164887948163614898472210765185843738279364851565592497066513567977048544195912492148513772168336428185686657328096896614400000000000000000000000000000000
    new_genorder = 27663515880

    new_m = new_m // (683 * 373)
    new_phi_m = new_phi_m // ((683-1)*(373-1))
    new_genorder = get_genp_order(new_m, new_phi_m)
    assert new_genorder == 892371480

    # Remove 3 primes
    assert new_m % (461*599*691) == 0
    assert new_phi_m % ((461-1)*(599-1)*(691-1)) == 0
    new_m = new_m // (461*599*691)
    new_phi_m = new_phi_m // ((461-1)*(599-1)*(691-1))
    new_genorder = get_genp_order(new_m, new_phi_m)
    assert new_genorder == 38798760

    # Remove 3 primes
    assert new_m % (443*521*547) == 0
    assert new_phi_m % ((443-1)*(521-1)*(547-1)) == 0
    new_m = new_m // (443*521*547)
    new_phi_m = new_phi_m // ((443-1)*(521-1)*(547-1))
    new_genorder = get_genp_order(new_m, new_phi_m)
    assert new_genorder == 2984520

    # Remove 3 primes
    assert new_m % (409*613*647) == 0
    assert new_phi_m % ((409-1)*(613-1)*(647-1)) == 0
    new_m = new_m // (409*613*647)
    new_phi_m = new_phi_m // ((409-1)*(613-1)*(647-1))
    new_genorder = get_genp_order(new_m, new_phi_m)
    assert new_genorder == 175560

    # Remove 3 primes
    assert new_m % (457*601*617) == 0
    assert new_phi_m % ((457-1)*(601-1)*(617-1)) == 0
    new_m = new_m // (457*601*617)
    new_phi_m = new_phi_m // ((457-1)*(601-1)*(617-1))
    new_genorder = get_genp_order(new_m, new_phi_m)
    assert new_genorder == 87780

    # Remove 3 primes
    assert new_m % (421*571*661) == 0
    assert new_phi_m % ((421-1)*(571-1)*(661-1)) == 0
    new_m = new_m // (421*571*661)
    new_phi_m = new_phi_m // ((421-1)*(571-1)*(661-1))
    new_genorder = get_genp_order(new_m, new_phi_m)
    assert new_genorder == 17556

    # Remove 3 primes
    # or not
    if 0:
        assert new_phi_m % ((419-1)*(673-1)*(701-1)) == 0
        new_m = new_m // (419*673*701)
        new_phi_m = new_phi_m // ((419-1)*(673-1)*(701-1))
        new_genorder = get_genp_order(new_m, new_phi_m)
        assert new_genorder == 924

    if 1:
        m = new_m
        phi_m = new_phi_m
        print("Final M = %d" % m)
        print("Final phi(M) = %d" % phi_m)
        print("Final log2(M) = %f" % math.log2(m))
        phi_m_decomposition = DlogFprint.small_factors(phi_m, max_prime)
        generator_order = DlogFprint.element_order(generator, m, phi_m, phi_m_decomposition)
        generator_order_decomposition = DlogFprint.small_factors(generator_order, max_prime)
        print("Generator order: %d" % generator_order)
        assert pow(generator, generator_order, m) == 1
        for name, modulus in MODULI:
            d = DlogFprint.discrete_log(modulus, generator, generator_order, generator_order_decomposition, m)
            #print("%s RSA modulus: %#x" % (name, modulus))
            print("Final fingerprinting %s RSA: d=%r" % (name, d))
            #print("check:\n    %d\n    %d" % (pow(generator, d, m), modulus % m))
            assert pow(generator, d, m) == modulus % m

    if 0:

        known_primes = set(PRIMES)
        assert pow(generator, new_genorder, new_m) == 1
        assert new_phi_m % new_genorder == 0
        for iteration in range(5):
            for p1 in sorted(known_primes, reverse=True):
                if (new_m % p1) != 0:
                    continue
                for p2 in sorted(known_primes, reverse=True):
                    if p2 <= p1:
                        break
                    if (new_m % p2) != 0:
                        continue
                    #for p3 in [1]:
                    for p3 in sorted(known_primes, reverse=True):
                        if 1 < p3 <= p2:
                            break
                        if (new_m % p3) != 0:
                            known_primes.remove(p3)
                            continue
                        p3_min1 = (p3 - 1) if p3 != 1 else 1
                        assert new_m % (p1 * p2 * p3) == 0
                        assert new_phi_m % ((p1 - 1) * (p2 - 1) * p3_min1) == 0
                        try_m = new_m // (p1 * p2 * p3)
                        try_phi_m = new_phi_m // (p1 - 1) // (p2 - 1) // p3_min1
                        try_genorder = get_genp_order(try_m, try_phi_m)
                        print(p1, p2, p3, try_genorder, math.log2(try_m))
                        if try_genorder < new_genorder and math.log2(try_m) > 512:
                            new_m = try_m
                            new_phi_m = try_phi_m
                            new_genorder = try_genorder
                            print("Trying m = %d (log2 = %d)" % (new_m, math.log2(new_m)))
                            break

            m = new_m
            phi_m = new_phi_m
            print("Final M = %d" % m)
            print("Final phi(M) = %d" % phi_m)
            print("Final log2(M) = %f" % math.log2(m))
            phi_m_decomposition = DlogFprint.small_factors(phi_m, max_prime)
            generator_order = DlogFprint.element_order(generator, m, phi_m, phi_m_decomposition)
            generator_order_decomposition = DlogFprint.small_factors(generator_order, max_prime)
            print("Generator order: %d" % generator_order)
            assert pow(generator, generator_order, m) == 1
            if 1:
                for name, modulus in MODULI:
                    d = DlogFprint.discrete_log(modulus, generator, generator_order, generator_order_decomposition, m)
                    #print("%s RSA modulus: %#x" % (name, modulus))
                    print("Final fingerprinting %s RSA: d=%r" % (name, d))
                    #print("check:\n    %d\n    %d" % (pow(generator, d, m), modulus % m))
                    assert pow(generator, d, m) == modulus % m


def better_stronger():
    """Try heuristic with factors of the order, VERY SLOW!!!"""
    new_m = 3760521612380916345048248036465914040505038286724779677484652961679034681273556134732998926971941209335901202681827127086369010449740069641128520265646511536990250330559437958993581185688496097067797726693263764630
    new_phi_m = 338751992361839103757913831646640400994371081216046845152117164887948163614898472210765185843738279364851565592497066513567977048544195912492148513772168336428185686657328096896614400000000000000000000000000000000
    new_genorder = 27663515880

    if 1:
        new_m = new_m // (683 * 373)
        new_phi_m = new_phi_m // ((683-1)*(373-1))
        new_genorder = get_genp_order(new_m, new_phi_m)
        assert new_genorder == 892371480

        assert new_m % (461*599*691) == 0
        assert new_phi_m % ((461-1)*(599-1)*(691-1)) == 0
        new_m = new_m // (461*599*691)
        new_phi_m = new_phi_m // ((461-1)*(599-1)*(691-1))
        new_genorder = get_genp_order(new_m, new_phi_m)
        assert new_genorder == 38798760

        # Remove 3 primes
        assert new_m % (443*521*547) == 0
        assert new_phi_m % ((443-1)*(521-1)*(547-1)) == 0
        new_m = new_m // (443*521*547)
        new_phi_m = new_phi_m // ((443-1)*(521-1)*(547-1))
        new_genorder = get_genp_order(new_m, new_phi_m)
        assert new_genorder == 2984520

    for p in PRIMES[::-1]:
        if (new_genorder % p) != 0:
            continue
        print("Trying to simplify with %d (pow %d)" % (p, new_genorder // p))
        bignum = pow(GEN_P, new_genorder // p) - 1
        newgcd = math.gcd(bignum, new_m)
        print("Trying to simplify with %d: %f" % (p, math.log2(newgcd)))
        

if __name__ == '__main__':
    get_betterm()
    #better_stronger()
