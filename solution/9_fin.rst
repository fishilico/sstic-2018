Conclusion
==========

.. role:: raw-latex(raw)
     :format: latex

Cette année le challenge du SSTIC a été très orienté sur le côté « vulnérabilités » de la sécurité informatique.
En effet, il commence par l'analyse d'une machine qui a été infectée par l'exploitation d'une vulnérabilité dans un navigateur web, puis demande de casser l'algorithme de chiffrement utilisé pour déchiffrer le programme qui a été déposé sur la machine (un dérivé de Kuznyechik sans S-Box).
Il faut ensuite étudier l'algorithme de chiffrement des communications de ce programme, qui utilise un AES 4 tours en mode CBC dont les clés, récupérables en mettant en œuvre une attaque intégrale, sont échangées en utilisant RSA-2048.
Comme la fonction qui génère les nombres premiers utilisés génère des nombres avec des propriétés qui permettent la factorisation des clés publiques RSA en quelques secondes (en utilisant une variable de l'attaque ROCA), il est également possible de directement obtenir les clés AES en déchiffrement les messages où elles sont échangées.
Ensuite des erreurs de calcul et de comparaison dans le programme avec lequel a communiqué la machine infectée permettent d'obtenir quelques informations au sujet de la mémoire de ce programme, d'y écrire des données arbitraires et au final d'y exécuter des instructions permettant de trouver l'adresse e-mail marquant la fin du challenge.
Enfin le manque de durcissement de l'environnement dans lequel est exécuté le programme permet d'obtenir une invite de commande directement sur la machine qui a été utilisée lors de l'attaque.

Remerciements
=============

Je remercie tout d'abord ma fiancée et ma famille, qui m'ont soutenu pendant toute la durée du challenge.

Je remercie mes collègues de l'ANSSI, qui m'ont continuellement mis la pression pour résoudre le challenge rapidement, et ce malgré le fait que j'avais aussi des sujets sur lesquels travailler par ailleurs.
Je remercie tout particulièrement Raphaël Sanchez, dont les longues discussions que nous avons partagées au sujet de la manière d'exploiter l'allocateur de la glibc ont été indispensables pour mon arrivée à la seconde place du classement rapidité.

Je remercie également les concepteurs et organisateurs du challenge pour avoir réalisé un challenge très intéressant, qui m'a donné l'occasion d'approfondir des sujets tels que le WebAssembly et l'exploitation d'une vulnérabilité de corruption de tas avec une version récente de la bibliothèque glibc.
Je les remercie également d'avoir un peu tardé dans la mise en ligne du challenge[#]_, ce qui m'a permis de profiter pleinement de Pâques en famille, sans être trop préoccupé par le challenge.

.. [#] le challenge a été publié le samedi après-midi au lieu du vendredi, comme ça avait été le cas les années précédentes

Je remercie enfin les personnes qui ont créé, écrit, développé et maintenu à jour les outils et les documents (articles blogs, papiers de recherche, solutions de challenge de sécurité) que j'ai utilisés pour résoudre ce challenge en moins de 6 jours.
