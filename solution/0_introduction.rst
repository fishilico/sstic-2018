Cette année, le challenge du SSTIC concerne l'étude d'une attaque informatique qui a eu lieu sur une machine.
Les participants, dont je fais partie, sont chargés d'analyser cette attaque à partir d'une trace réseau correspondant à l'activité de la machine afin de trouver le serveur utilisé pour cette attaque.
Cela conduit à la découverte d'un programme déposé sur la machine compromise qui permet à l'attaquant d'y exécuter des commandes et d'en exfiltrer des données.

Ensuite les participants ont pour mission de s'introduire sur le serveur utilisé lors de l'attaque, dont l'adresse IP est obtenue en décryptant une communication présente dans la trace réseau.
Pour cela, il est nécessaire d'exploiter des vulnérabilités présentes dans le serveur utilisé par l'attaquant.
Ces vulnérabilités permettent au final de lire le contenu des fichiers présents sur la machine utilisée par l'attaquant.
Un de ces fichiers contient l'adresse e-mail à laquelle il faut envoyer un message afin de résoudre le challenge.

Ce document relate la démarche que j'ai suivie afin de trouver cette adresse e-mail.
Il s'organise ainsi en trois parties: investigation de l'attaque à partir de l'analyse de la trace réseau, recherche de vulnérabilités dans le programme découvert et compromission de la machine de l'attaquant à partir des vulnérabilités découvertes.
