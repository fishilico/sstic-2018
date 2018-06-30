CAT ?= cat
FLAKE8 ?= flake8
GREP ?= grep
MV ?= mv
# Use "make PYTHON3=pypy3" to be faster"
PYTHON3 ?= python3
RM ?= rm -f
SED ?= sed
SHA256SUM ?= sha256sum
STRINGS ?= strings
TAR ?= tar
TSHARK ?= tshark
WGET ?= wget
XXD ?= xxd


all: serveur_racine_infos.out.txt all_flags.out.txt

clean:
	$(RM) ./*.out.* ./*.js ./*.wasm ./*.tmp
	$(RM) -r __pycache__

challenge_SSTIC_2018.pcapng.gz: challenge_SSTIC_2018.pcapng.gz.sha256
	$(WGET) -O $@.tmp https://static.sstic.org/challenge2018/challenge_SSTIC_2018.pcapng.gz
	$(MV) $@.tmp $@
	if ! $(SHA256SUM) -c $< ; then $(MV) $@ $@.out.bad_checksum && false ; fi
	touch $@

# Extract downloaded files from the capture
stage1.js: challenge_SSTIC_2018.pcapng.gz
	$(TSHARK) -r $< -Tfields -e http.file_data 'tcp.stream == 249 and http and tcp.srcport == 8080' > $@

utils.js: challenge_SSTIC_2018.pcapng.gz
	$(TSHARK) -r $< -Tfields -e http.file_data 'tcp.stream == 254 and http and tcp.srcport == 8080' > $@

blockcipher.js: challenge_SSTIC_2018.pcapng.gz
	$(TSHARK) -r $< -Tfields -e http.file_data 'tcp.stream == 856 and http and tcp.srcport == 8080' > $@

blockcipher.wasm.out.hex.txt: challenge_SSTIC_2018.pcapng.gz
	$(TSHARK) -r $< -Tfields -e media.type 'tcp.stream == 857 and http and tcp.srcport == 8080' > $@

blockcipher.wasm: blockcipher.wasm.out.hex.txt
	$(XXD) -p -r < $< > $@

payload.js: challenge_SSTIC_2018.pcapng.gz
	$(TSHARK) -r $< -Tfields -e http.file_data 'tcp.stream == 858 and http and tcp.srcport == 8080' > $@

stage2.js: challenge_SSTIC_2018.pcapng.gz
	$(TSHARK) -r $< -Tfields -e http.file_data 'tcp.stream == 859 and http and tcp.srcport == 8080' > $@

https_password.out.pcap: challenge_SSTIC_2018.pcapng.gz
	$(TSHARK) -r $< -w $@ 'tcp.stream == 861'

agent.out.pcap: challenge_SSTIC_2018.pcapng.gz
	$(TSHARK) -r $< -w $@ 'tcp.stream == 862 and ip.addr == 192.168.23.213 and tcp.port == 31337 and !tcp.analysis.retransmission'

# Flag 1: use blockcipher.wasm
flag1_anomaly_detection.out.txt: get_flag1_from_wasm.py stage1.js utils.js blockcipher.js blockcipher.wasm payload.js stage2.js
	$(PYTHON3) $<

f4ncyn0un0urs.out.elf: decrypt_payload_js.py payload.js https_password.out.pcap
	$(PYTHON3) $<

flag2_disruptive_javascript.out.txt: f4ncyn0un0urs.out.elf
	$(STRINGS) $< | $(SED) -n 's/^\(SSTIC\)/Disruptive JavaScript: \1/p' > $@.tmp
	$(GREP) SSTIC $@.tmp
	$(MV) $@.tmp $@

rsa2048_handshake.out.json capture_firstblocks.out.json: extract_infos_from_pcap.py agent.out.pcap
	$(PYTHON3) $<

square_attack_keys.out.txt: square_attack.py capture_firstblocks.out.json
	$(PYTHON3) $<

serveur_racine_infos.out.txt confidentiel.out.tgz surprise.out.tgz: decrypt_agent_communication.py agent.out.pcap square_attack_keys.out.txt
	$(PYTHON3) $<

flag3_battle_tested_encryption.out.txt: confidentiel.out.tgz
	$(TAR) -O -xzf $< home/user/confidentiel/super_secret| $(SED) -n 's/^\(SSTIC\)/Battle-tested Encryption: \1/p' > $@.tmp
	$(MV) $@.tmp $@

all_flags.out.txt: flag1_anomaly_detection.out.txt flag2_disruptive_javascript.out.txt flag3_battle_tested_encryption.out.txt
	$(CAT) $^ > $@

lint:
	if $(FLAKE8) --max-line-length=120 . | $(GREP) -v \
		-e 'E302 expected 2 blank lines' \
		-e 'E305 expected 2 blank lines after class or function definition' \
		-e 'E402 module level import not at top of file' \
		; then false ; else true ; fi

.PHONY: all clean lint
