LATEXMK ?= latexmk
PANDOC ?= pandoc
RM ?= rm -f
SED ?= sed

RST_FILES := $(wildcard *.rst)
OUT_TEX_FILES := $(RST_FILES:%.rst=%.out.tex)
IMAGE_FILES := $(wildcard images/*)

all: solution.pdf

clean:
	$(RM) solution.pdf
	$(RM) *.out.tex
	$(LATEXMK) -C

solution.pdf: solution.tex $(OUT_TEX_FILES) $(IMAGE_FILES)
	$(LATEXMK) -pdf $< < /dev/null
	$(LATEXMK) -pdf $< < /dev/null
	touch $@

%.out.tex: %.rst Makefile
	@# make some blocks shaded verbatim before running pandoc
	$(PANDOC) -o numfig=True $< -o $@.tmp.tex
	@# remove spaces before footnote and replace space after quote
	$(SED) $@.tmp.tex > $@ \
		-e 's/\s\+\(\\footnote\)/\1/g' \
		-e 's/« /«~/g' \
		-e 's/ »/~»/g' \
		-e 's/\(\\begin{Shaded}\)/\1\\setlength{\\parskip}{0pt}/g' \
		-e 's/«~\(hack-back\)~»/« \1 »/g'
	$(RM) $<.tmp.md $@.tmp.tex

# Does not work well :(
inotify-loop:
	busybox inotifyd - $(RST_FILES) Makefile | \
		grep --line-buffered -v '^r' | \
		while IFS= read -r L ; \
		do \
			$(MAKE) -s ; \
		done

# Build continuously, without writing what is done is nothing changed
loop:
	while sleep .1 ; do $(MAKE) -s && continue ; sleep 8 ; done

9b_decrypt_payload_js.rst: decrypt_payload.for_writeup.py Makefile
	echo 'Script de déchiffrement de payload.js' > $@.tmp
	echo '=====================================' >> $@.tmp
	echo '' >> $@.tmp
	echo '.. raw:: latex' >> $@.tmp
	echo '' >> $@.tmp
	echo '     \label{sect:ann-decrypt-payload}' >> $@.tmp
	echo "     Voici le script Python que j'ai utilisé pour décrypter le contenu de \texttt{payload.js} et ainsi extraire \texttt{/tmp/.f4ncyn0un0urs} (cf. section \ref{sect:1-chiffrement-russe})." >> $@.tmp
	echo '' >> $@.tmp
	echo '.. code-block:: python' >> $@.tmp
	echo '' >> $@.tmp
	sed 's/^/    /' $< >> $@.tmp
	mv $@.tmp $@

9c_exploit.rst: exploit.for_writeup.py Makefile
	echo "Script d'exécution sur le serveur racine" > $@.tmp
	echo '========================================' >> $@.tmp
	echo '' >> $@.tmp
	echo '.. raw:: latex' >> $@.tmp
	echo '' >> $@.tmp
	echo '     \label{sect:ann-hackback-script}' >> $@.tmp
	echo "     Voici le script Python que j'ai utilisé pour exécuter des chaînes de \emph{gadgets} sur le serveur racine, permettant ainsi d'énumérer le contenu de dossiers et de lire le contenu de fichiers (cf. section \ref{sect:2-courriel-final})." >> $@.tmp
	echo '' >> $@.tmp
	echo '.. code-block:: python' >> $@.tmp
	echo '' >> $@.tmp
	sed 's/^/    /' $< >> $@.tmp
	mv $@.tmp $@

.PHONY: clean all inotify-loop loop
