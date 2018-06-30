Riposte proportionnée
=====================

.. role:: raw-latex(raw)
     :format: latex

Vulnérabilités ! Où êtes-vous ?
-------------------------------

Un réseau très dictatorial
~~~~~~~~~~~~~~~~~~~~~~~~~~

Jusqu'à maintenant, j'ai extrait le programme ``f4ncyn0un0urs`` depuis la trace réseau initiale et l'analyse que j'ai effectuée a permis de comprendre le fonctionnement de cet agent.
J'ai de plus retrouvé les clés de chiffrement utilisées dans la communication qui a été enregistrée dans la trace réseau, ce qui m'a conduit à l'adresse IP et au port du serveur de contrôle de l'attaquant (le « serveur racine », ``195.154.105.12:36735``).

Enfin, en exécutant ``f4ncyn0un0urs -c SSTIC2018{f2ff2a7ed70d4ab72c52948be06fee20}``, le programme se comporte comme un serveur racine, ce qui permet de procéder à des tests sans devoir interagir avec le serveur racine de l'attaquant, ce que je vais faire dans un premier temps.
En exécutant donc ``f4ncyn0un0urs`` en mode « serveur racine », je me rends compte qu'appuyer sur Entrée affiche:

.. code-block:: sh

    --------------------------------------------------------------------------------
    invalid command
    --------------------------------------------------------------------------------

En effet, le programme propose alors une invite de commande interactive, implémentée par la fonction ``prompt``. Cette interface autorise l'utilisateur à entrer les commandes suivantes:

* ``help`` affiche la liste des commandes autorisées, « ``routes|get|put|cmd|ping`` ».
* ``routes`` affiche « ``routing table:`` » puis la liste des agents connectés, s'il y en a.
* ``get`` permet de récupérer un fichier depuis un agent connecté.
* ``put`` permet de déposer un fichier sur un agent connecté.
* ``cmd`` permet d'exécuter une commande *shell* sur un agent connecté.
* ``ping`` permet d'envoyer une requête « ping » à un agent connecté.

En exécutant une seconde instance de ``f4ncyn0un0urs``, cette fois ci en mode « agent » en combinant les options ``-h`` et ``-p`` pour indiquer l'adresse IP et le port TCP que mon serveur racine utilise, je vois son identifiant qui apparaît quand j'entre ensuite ``routes`` dans l'interface, et j'arrive à utiliser les commandes ``get``, ``put``, ``cmd`` et ``ping``.
Le code de la fonction ``prompt`` permet de comprendre que ces quatre commandes correspondent aux messages ``GET``, ``PUT``, ``CMD`` et ``PING`` que j'avais identifié dans la section :raw-latex:`\ref{sect:1-protocole}`.

Est-il possible d'exécuter directement des commandes envoyées par un agent sur le serveur racine ?
La fonction ``mesh_process_message``, qui interprète les messages reçus, appelle ``msg_process_job`` pour exécuter les commandes ``GET``, ``PUT`` et ``CMD``.
Pour cela il faut nécessairement que le message provienne du parent et que l'ID agent source du message soit nul.
Cela impose donc que ces commandes proviennent du serveur racine, et tout message ``GET``, ``PUT`` ou ``CMD`` reçu depuis un agent connecté serait simplement ignoré.

La fonction ``mesh_process_message`` appelle aussi ``msg_process_job`` quand le message reçu correspond à une réponse, ce qui permet par exemple d'afficher le résultat de l'exécution d'une commande *shell* dans l'interface du serveur racine.
En creusant l'implémentation des fonctions ``prompt`` et ``start_execute_job``, je comprends ce qu'il se passe quand le serveur racine lance l'exécution d'une commande *shell* sur un agent:

* Le serveur racine appelle ``add_receiver`` pour écrire dans son interface (``stdout``) les messages de réponse provenant de l'ID agent de l'agent concerné par l'exécution de la commande *shell*.
* Puis le serveur racine envoie un message ``CMD`` (``0x00000201``) à l'agent, avec comme contenu la commande à exécuter.
* Ce message est acheminé par le réseau arborescent jusqu'à l'agent destinataire, qui appelle la fonction ``start_execute_job``. Celle-ci crée un nouveau processus qui exécute la commande donnée, et appelle ``add_transmitter`` pour transmettre au serveur racine la sortie de cette commande.
* Tant que la commande produit du contenu, l'agent appelle ``process_transmission`` pour transmettre ce contenu au serveur racine, par des messages ``CMD_CONTENT`` (``0x03000201``).
* Le serveur racine qui reçoit ces messages appelle ``msg_process_transmission``, qui écrit leurs contenus sur la sortie standard du programme (``stdout``), comme cela avait été défini par ``add_receiver``.
* À la fin de l'exécution de la commande, l'agent appelle ``end_transmission``, qui envoie un message ``CMD_DONE`` (``0x05000201``) au serveur racine et retire le transmetteur.
* Quand le serveur racine reçoit le message ``CMD_DONE``, il appelle ``msg_process_transmission_done``, qui enlève le récepteur associé à l'adresse en question.

Dans un contexte où je contrôle complètement l'agent, la réception d'un message ``CMD`` provenant du serveur racine indique la création d'un canal de communication dans lequel je peux écrire ce que je veux dans l'interface de contrôle du serveur racine.
Et le serveur racine n'a pas de fonctionnalité permettant de fermer un tel canal de contrôle: il attend que j'envoie un message ``CMD_DONE`` pour cela.
Et si j'utilisais cela pour créer un canal de discussion avec l'attaquant ?
J'ai implémenté un client Python du protocole utilisé par ``f4ncyn0un0urs``, testé avec le serveur racine de test, puis l'ai connecté au serveur racine que j'ai trouvé et ai attendu que l'attaquant envoie une commande, pour lui demander gentiment son adresse e-mail...
Et là, le mercredi 4 avril à 00h06, je reçois un message demandant à mon agent d'exécuter la commande ``id``:

.. code-block:: python

    [DEBUG] RCV Recv(b'ceqejeve' 0x0->0x1245780345567817 cmd 0x201 [3] b'id\x00')
    >>> c.reply_cmd(b'coucou ! Quelle est ton adresse e-mail ?\n')
    [DEBUG] SND cmd 0x1245780345567817->0x0 0x3000201 b'coucou ! Quelle est ton adress'(40)
    >>> c.reply_cmd(b'pour repondre, envoie une commande vers 0x1245780345567817 ;)\n')
    [DEBUG] SND cmd 0x1245780345567817->0x0 0x3000201 b'pour repondre, envoie une comm'(62)
    >>> c.recv_encrypted()

L'attaquant n'a pas répondu, dommage. Il ne s'attendait probablement pas à ce que la commande ``id`` lui retourne une question, mais j'ai peut-être été trop direct dans ma question.

Comme il n'est pas possible d'utiliser les commandes ``GET``, ``PUT`` et ``CMD`` depuis un agent vers le serveur racine, il reste les autres. Qu'en est-il de ``PING`` ? Il n'y a pas de filtrage quand ``mesh_process_message`` appelle ``msg_process_ping``, et cette fonction se contente de modifier l'en-tête avant de renvoyer le message avec ``mesh_relay_message``. Cette dernière fonction se fie au champ *taille* de l'en-tête du message pour définir la quantité de données à envoyer, et ce champ a été défini par l'agent qui a émis la requête ``PING`` initiale... Avant de rechercher une vulnérabilité, je teste que le serveur racine que j'utilise répond aux requêtes ``PING``, et c'est bien le cas.

Un tintement qui saigne
~~~~~~~~~~~~~~~~~~~~~~~
:raw-latex:`\label{sect:2-ping-leak}`

Comme vu précédemment dans les sections :raw-latex:`\ref{sect:1-comm-tracee}` et :raw-latex:`\ref{sect:1-protocole}`, quand un agent envoie un message, il précise la taille des données envoyées à deux endroits:

* dans l'en-tête du message qui est chiffré, un champ *taille* indique la taille du message (en-tête de 40 octets et contenu) ;
* une fois le message chiffré, il est envoyé avec le vecteur d'initialisation qui a été utilisé, après que la taille de l'ensemble (encodée par un entier de 32 bits) ait été envoyée.

En temps normal, la seconde taille correspond à la première arrondie au multiple de 16 supérieur ou égal le plus proche, à laquelle a été ajoutée 16 (ce qui permet de prendre en compte le chiffrement par bloc et le vecteur d'initialisation). En pratique, rien n'empêche d'émettre un message dont ces deux tailles sont décorrélées. Toutefois ``f4ncyn0un0urs`` est assez robuste face à des tailles non prévues:

* Quand la fonction ``scomm_recv`` reçoit des données chiffrées, elle commence par recevoir la taille des données, puis tronque cette taille à 16384 octets (``0x4000``) avant de recevoir effectivement les données, dans une zone mémoire de 16384 octets.
* Chaque appel à ``scomm_recv`` s'effectue avec une zone mémoire de 16384 octets qui a été effacée au préalable (en la remplissant de 0 avec la fonction ``memset``). Donc si un paquet définit un champ *taille* supérieur à la taille des données effectivement transmises, cela ne réutilise pas d'éventuelles données précédentes non-effacées.
* Les fonctions qui utilisent ``scomm_recv`` (``mesh_agent_peering`` et ``mesh_process_message``) vérifient que le champ *taille* ne dépasse pas 16384 octets. Ainsi les fonctions qui reposent sur ce champ pour accéder au contenu du message ne peuvent pas dépasser de la zone mémoire allouée de 16384 octets.

Toutefois, en regardant plus en détail, je me rends compte que les zones mémoires temporaires utilisées par ``scomm_recv`` pour recevoir les données chiffrées puis pour accueillir les données déchiffrées ne sont pas effacées avant utilisation. De plus, la fonction n'enlève pas la taille du vecteur d'initialisation (16 octets) à la taille du message avant de le copier dans la zone mémoire donnée en argument. Donc en pratique, ``scomm_recv`` copie un bloc de 16 octets en trop, qui proviennent de valeurs précédentes sur la pile. En utilisant un message ``PING`` dont le champ *taille* englobe ces 16 octets supplémentaires, il est possible de les obtenir.

Est-il possible d'obtenir des données intéressantes avec cette vulnérabilité, comme l'adresse mémoire d'une structure ? Pour répondre, j'ai dessiné sur un même schéma l'utilisation de la pile par les fonctions qui sont appelées par ``mesh_process_message``.

.. code-block:: xorg

    adresses relatives de la pile                               ...
    relatives à mesh_process_message                        +-----------------------------+
    -c098                                                   |  message chiffré            |
                                       ...                  |  envoyé par                 |
                                   +------------------------+  scomm_send                 |
    -c088                          |  message chiffré       |  (16384 octets)             |
              ...                  |  envoyé par            |                             |
          +------------------------+  scomm_send            |                             |
    -c068 |   message chiffré      |  (16384 octets)        |                             |
          |   reçu par scomm_recv  |                        +-----------------------------+
    -8098 |   (16384 octets)       |                        |  message clair à            |
          |                        +------------------------+  envoyer par                |
    -8088 |                        |  message clair à       |  scomm_send                 |
          |                        |  chiffrer par          |  (16384 octets)             |
          +------------------------+  scomm_send            |                             |
    -8068 |   message déchiffré    |  (16384 octets)        |                             |
          |   par scomm_recv       |                        |                             |
          |   (16384 octets)       |                        |                             |
          |                        |                        +-----------------------------+
    -4098 |                        |                        |   (alignement)              |
    -4090 |                        |                        |  rbx sauvegardé             |
          |                        +------------------------+-----------------------------+
    -4088 |                        |   (alignement)         |  rbp sauvegardé             |
    -4080 |                        |  rbx sauvegardé        |  r12 sauvegardé             |
    -4078 |                        |  rbp sauvegardé        |  r13 sauvegardé             |
    -4070 |                        |  r12 sauvegardé        |  r14 sauvegardé             |
          +------------------------+------------------------+-----------------------------+
    -4068 |   (alignement)         |  r13 sauvegardé        |  r15 sauvegardé             |
          +------------------------+------------------------+-----------------------------+
    -4060 |  rbx sauvegardé        |  r14 sauvegardé        |  rip sauv. (0x401550)       |
          |                        |                        |  [    scomm_send    ]       |
          +------------------------+------------------------+-----------------------------+
    -4058 |  rbp sauvegardé        |  r15 sauvegardé        |   (alignement)              |
    -4050 |  r12 sauvegardé        |  rip sauv. (0x401dcc)  |  rbx sauvegardé             |
          |                        |  [    scomm_send    ]  |                             |
          +------------------------+------------------------+-----------------------------+
    -4048 |  r13 sauvegardé        |   (alignement)         |  rbp sauvegardé             |
    -4040 |  r14 sauvegardé        |  rbx sauvegardé        |  r12 sauvegardé             |
    -4038 |  r15 sauvegardé        |  rbp sauvegardé        |  r13 sauvegardé             |
          |                        |  [mesh_relay_message]  |                             |
          +------------------------+------------------------+-----------------------------+
    -4030 |  rip sauv. (0x401ba6)  |  rip sauv. (0x401d7b)  |  rip sauv. (0x401cbe)       |
          | [ appel à scomm_recv ] |  [ msg_process_ping ]  | [mesh_process_agent_peering]|
          +------------------------+------------------------+-----------------------------+
    -4028 |                           message reçu dans                                   |
          |                           mesh_process_message                                |
          |                           (16384 octets)                                      |
          +-------------------------------------------------------------------------------+
    -0028 |                           (alignement)                                        |
    -0020 |                           rbx sauvegardé                                      |
    -0018 |                           rbp sauvegardé                                      |
    -0010 |                           r12 sauvegardé                                      |
    -0008 |                           r13 sauvegardé                                      |
          +-------------------------------------------------------------------------------+
    -0000 |                           rip sauv. (0x4011aa)                                |
          |                          [mesh_process_message]                               |


Ce schéma représente le contenu de la pile du serveur quand ``mesh_process_message`` appelle ``scomm_recv`` (colonne de gauche), ``msg_process_ping`` (colonne du milieu) et ``mesh_process_agent_peering`` (colonne de droite). Ces fonctions appellent d'autres fonctions, qui peuvent agir sur le contenu de la pile, et ce schéma ne représente que les appels à des fonctions qui peuvent agir sur ce que contient la zone « message déchiffré par ``scomm_recv`` ».

La fonction ``msg_process_ping`` modifie les deux derniers blocs de 16 octets de cette zone mémoire quand ``scomm_send`` est appelée, et la fonction ``mesh_process_agent_peering`` modifie les trois derniers blocs de 16 octets.
Par ailleurs, pour qu'il soit possible de recevoir une réponse à la commande ``PING`` utilisée, il faut que le champ *taille* ne dépasse pas 16384-16 = 16368 (``0x3ff0``) octets, à cause d'une vérification opérée par ``scomm_send`` [#]_.
Ceci empêche la lecture du dernier bloc de 16 octets, mais permet de lire:

.. [#] ``scomm_send`` utilisant une zone mémoire de 16384 pour enregistrer le message chiffré constitué d'un vecteur d'initialisation de 16 octets et d'un message de taille celle indiquée dans son champ *taille*, il est cohérent que cette fonction s'assure que le message chiffré ne dépasse pas 16384 octets

* le « ``rbx`` sauvegardé » d'un précédent appel à ``scomm_send`` par ``msg_process_ping`` ;
* le « ``rbx`` sauvegardé » d'un précédent appel à ``scomm_send`` par ``mesh_process_agent_peering`` ;
* le « ``rbp`` sauvegardé » d'un précédent appel à ``scomm_send`` par ``mesh_process_agent_peering`` ;
* le « ``r12`` sauvegardé » d'un précédent appel à ``scomm_send`` par ``mesh_process_agent_peering``.

Un « registre sauvegardé » désigne ici un registre du processeur dont la valeur est mise dans la pile à l'entrée dans une fonction et est rétablie à la sortie de la fonction. En regardant d'où viennent les valeurs, je me rends compte des éléments suivants.

* Le « ``rbx`` sauvegardé » correspond toujours à l'adresse de la zone mémoire « message reçu » dans ``mesh_process_message`` (en ``-4028`` dans le schéma précédent), et permet donc de retrouver l'adresse de base de la pile (qui est définie aléatoirement par le noyau Linux).
* Le « ``rbp`` sauvegardé » peut contenir l'adresse de la structure principale du programme, présente dans la pile aussi.
* Le « ``r12`` sauvegardé » correspond à l'adresse de la structure allouée pour enregistrer les informations de connexion de l'agent auprès du serveur qui traite sa demande. Sa valeur permet donc d'obtenir une adresse dans le tas (« Heap ») du programme.

En résumé, en envoyant deux commandes ``PING`` précédées par des commandes ``PEER`` adéquates, je peux obtenir une adresse dans la pile et une adresse dans le tas du serveur.

Après avoir modifié le script Python que j'ai écrit pour communiquer avec le serveur racine pour exploiter cette vulnérabilité, j'obtiens les valeurs suivantes (qui changent à chaque redémarrage du serveur racine):

* Adresse de la zone « message reçu » dans ``mesh_process_message``: ``0x3ff6c4678e0``
* Adresse de la structure principale du programme: ``0x3ff6c46b9f0``
* Adresse de la structure des informations de connexion de l'agent: ``0x6dfb50``

Ces adresses peuvent être utiles pour par exemple déterminer où écrire en mémoire s'il est possible d'exploiter une vulnérabilité permettant une telle écriture, mais ne suffisent pas à compromettre le serveur racine. Il faut donc maintenant trouver une autre vulnérabilité au programme.

Un routage peu stable
~~~~~~~~~~~~~~~~~~~~~

Dans les messages que peut envoyer un agent au serveur auquel il est connecté (son « parent »), ``GET``, ``PUT`` et ``CMD`` sont ignorés car ne proviennent pas du serveur racine, ``DUP_ID`` est ignoré car ne provient pas du parent du serveur, et ``PING`` est accepté et permet d'obtenir des adresses mémoires.
Il reste à étudier la commande ``PEER``, qui permet à un agent d'annoncer son ID agent à ses ancêtres, comme décrit à la section :raw-latex:`\ref{sect:1-protocole}`.

Quand un serveur reçoit un message ``PEER``, il exécute la fonction ``mesh_process_agent_peering``.
Après avoir vérifié que l'ID agent source du message reçu n'était pas connu, le serveur enregistre cet ID agent dans ces structures internes.
Pour cela, il utilise une « table de routage » qui est un tableau de structures de 24 octets, que j'appelle la « structure ``ROUTE_ENTRY`` ».
Chaque structure correspond à un enfant du serveur et contient les champs suivants (chaque ligne représente 8 octets):

.. code-block:: xorg

    |  0     1     2     3     4     5     6     7  |
    +-----------------------+-----------------------+
    |        nombre         |        alloués        |  (entiers 32 bits petits boutistes)
    +-----------------------+-----------------------+
    |          pointeur vers des ID agents          |
    +-----------------------------------------------+
    |        pointeur vers la structure CONN        |
    +-----------------------------------------------+

Cette structure contient un pointeur vers un tableau contenant les ID agents de la descendance de l'agent qui est décrit. Le champ ``nombre`` est le nombre d'ID agents renseignés dans le tableau, et le champ ``alloués`` est le nombre d'ID agents que peut contenir le tableau avant de devoir être étendu. La fonction ``add_route`` (qui crée une structure ``ROUTE_ENTRY``) alloue initialement un tableau pouvant contenir 6 ID agents, et la fonction ``add_to_route`` (qui ajoute des ID agents à une structure ``ROUTE_ENTRY``) étend le tableau de 5 entrées à chaque fois que c'est nécessaire.

La structure ``CONN`` est la structure qui contient des informations de connexion d'un agent connecté directement: ID agent, adresse IP, numéro de port, clés AES, vecteur d'initialisation à utiliser pour envoyer des données, etc.

La table de routage consiste en pratique en 3 champs dans la structure principale de l'agent, qui reprennent le principe des champs ``nombre``, ``alloués`` et ``pointeur vers des ID agents``, en référençant des structures ``ROUTE_ENTRY`` au lieu d'ID agents. Ainsi, en théorie il n'y a pas de limites au nombre d'agents que supporte un serveur, ni au nombre d'ID agents enregistrés par un serveur qui correspond à la descendance de chacun de ses enfants. En pratique, quand je tente de connecter 13 agents à un agent connecté au serveur racine que j'exécute, ce serveur s'interrompt brutalement:

.. code-block:: xorg

    realloc(): invalid next size
    Bad system call (core dumped)

En exécutant le serveur racine avec le débogueur ``gdb`` afin de déterminer la cause de cette interruption, j'obtiens la trace d'appels suivante:

.. code-block:: xml

    realloc(): invalid next size

    Program received signal SIGSYS, Bad system call.
    0x000000000046278d in sigprocmask ()
    (gdb) backtrace
    #0  0x000000000046278d in sigprocmask ()
    #1  0x00000000004195f8 in abort ()
    #2  0x000000000041e8a7 in __libc_message ()
    #3  0x00000000004241aa in malloc_printerr ()
    #4  0x00000000004281e4 in _int_realloc ()
    #5  0x0000000000429012 in realloc ()
    #6  0x000000000040183b in add_to_route ()
    #7  0x00000000004015a3 in mesh_process_agent_peering ()
    #8  0x0000000000401cbe in mesh_process_message ()
    #9  0x00000000004011aa in agent_main_loop ()
    #10 0x0000000000400672 in main ()

La fonction ``malloc_printerr`` qui apparaît est une fonction de la bibliothèque C qui est utilisée quand la bibliothèque détecte une corruption des structures utilisées par son allocateur mémoire.
Cette fonction affiche un message indiquant le problème qui a été détecté (« realloc(): invalid next size ») et interrompt l'exécution du programme en utilisant la fonction ``abort``.

Le problème est déclenché quand la fonction ``add_to_route`` étend l'espace mémoire alloué aux ID agents, pour passer de 11 à 16 entrées [#]_. Mais pourquoi est-ce que cela se déclenche à la connexion d'un 13:sup:`e` agent ? Normalement l'espace mémoire aurait dû être étendu lorsque le 12:sup:`e` s'était connecté.

.. [#] cet espace mémoire est alloué initialement avec 6 entrées, puis 5 y sont ajoutées pour passer à 11 entrées, puis 5 pour passer à 16 entrées

Pour répondre à cette question, il faut relire le contenu de la fonction ``add_to_route``, dont le code assembleur [#]_ est assez court:

.. [#] le code correspond ici à la sortie de ``objdump -Mintel -rd f4ncyn0un0urs``, avec mes commentaires

.. code-block:: xml

    0000000000401810 <add_to_route>:
      401810:   55              push   rbp
      401811:   53              push   rbx
      401812:   48 89 f5        mov    rbp,rsi   ; rbp et rsi sont l'ID agent à ajouter
      401815:   48 89 fb        mov    rbx,rdi   ; rbx et rdi désignent une ROUTE_ENTRY
      401818:   48 83 ec 08     sub    rsp,0x8
      40181c:   8b 17           mov    edx,DWORD PTR [rdi]      ; edx = rdi->nombre
      40181e:   8b 77 04        mov    esi,DWORD PTR [rdi+0x4]  ; esi = rdi->alloués
      401821:   48 8b 47 08     mov    rax,QWORD PTR [rdi+0x8]
      401825:   39 f2           cmp    edx,esi
      401827:   76 18           jbe    401841 <add_to_route+0x31> ; si edx > esi:

      401829:   83 c6 05        add    esi,0x5                  ; ... étend le tableau
      40182c:   89 77 04        mov    DWORD PTR [rdi+0x4],esi  ; pour 5 ID agents en plus
      40182f:   48 c1 e6 03     shl    rsi,0x3
      401833:   48 89 c7        mov    rdi,rax
      401836:   e8 15 77 02 00  call   428f50 <__libc_realloc>
      40183b:   8b 13           mov    edx,DWORD PTR [rbx]
      40183d:   48 89 43 08     mov    QWORD PTR [rbx+0x8],rax

      401841:   89 d1           mov    ecx,edx                   ; fin du bloc conditionnel
      401843:   83 c2 01        add    edx,0x1
      401846:   48 89 2c c8     mov    QWORD PTR [rax+rcx*8],rbp ; l'ID agent est ajouté
      40184a:   89 13           mov    DWORD PTR [rbx],edx      ; rdi->nombre est incrémenté
      40184c:   48 83 c4 08     add    rsp,0x8
      401850:   5b              pop    rbx
      401851:   5d              pop    rbp
      401852:   c3              ret

Le tableau des ID agents descendants n'est étendu que lorsque le nombre d'ID agents utilisés dépasse déjà le nombre qui a été alloué, au lieu d'être étendu au moment où c'est sur le point de dépasser.
Quand le 12:sup:`e` agent se connecte, la valeur du champ ``nombre`` était 11 et celle du champ ``alloués`` aussi.
Comme la fonction ``add_to_route`` n'étend pas le tableau en cas d'égalité de ces deux champs, l'ID agent du 12:sup:`e` agent est enregistré sur les 8 octets situés après la fin du tableau alloué. Ceci a pour effet d'écraser une structure utilisée par l'allocateur mémoire de la bibliothèque C.
Quand le 13:sup:`e` agent se connecte ensuite, comme le ``nombre`` 12 est supérieur au champ ``alloués`` 11, ``add_to_route`` tente d'étendre la mémoire allouée en appelant ``realloc``, ce qui plante à cause de l'écrasement qui a eu lieu.

Par ailleurs, la fonction ``add_route`` qui ajoute une nouvelle structure ``ROUTE_ENTRY`` à la table de routage est implémentée d'une manière similaire à ``add_to_route``, mais utilise une instruction assembleur ``jb`` au lieu du ``jbe`` présent à l'adresse ``401827``.
Ainsi la zone mémoire des ``ROUTE_ENTRY`` est étendue correctement dans la table de routage.

En résumé, la fonction ``add_to_route`` permet à un agent de corrompre les structures de l'allocateur mémoire du serveur auquel il est connecté, en écrasant les 8 octets présents après une zone mémoire allouée.
Le contenu de cet écrasement est contrôlé par l'agent qui se connecte et présente très peu de contraintes [#]_.
Est-il possible d'exploiter cette vulnérabilité pour exécuter du code arbitraire sur le serveur racine ?
J'espère que la réponse est positive, et vais m'intéresser à cela.

.. [#] il faut surtout que ces 8 octets correspondent à un ID agent qui ne soit pas déjà connu par le serveur

Du crash à l'écriture arbitraire en mémoire
-------------------------------------------

L'allocateur de la glibc
~~~~~~~~~~~~~~~~~~~~~~~~

Afin de comprendre comment transformer l'écrasement effectué par la ``add_to_route`` en écriture arbitraire, il faut au préalable se familiariser avec les structures utilisées par l'allocateur mémoire de la bibliothèque C utilisée par ``f4ncyn0un0urs``.
Ce programme étant liée statiquement, il embarque sa propre bibliothèque C, qui implémente ``malloc``, ``realloc``, ``free``, etc.[#]_
En s'intéressant aux messages qui peuvent être affichés par ces fonctions, je me rends compte que la bibliothèque C correspond à la glibc[#]_ version 2.27[#]_.

.. [#] ces fonctions sont décrites dans http://man7.org/linux/man-pages/man3/malloc.3.html
.. [#] « The GNU C Library », https://www.gnu.org/software/libc/
.. [#] Les appels à la fonction ``_malloc_assert`` contiennent chacun un nom de fichier source, un numéro de ligne ainsi que le nom de la fonction et un extrait du code C qui correspondent à la ligne indiquée dans le fichier indiqué. Ces appels correspondent exactement à la version 2.27 du code de la glibc.

L'allocateur mémoire de la glibc a évolué au fil des versions. Dans la version utilisée par ``f4ncyn0un0urs``, la mémoire gérée par l'allocateur est divisée en *arènes*, elles-mêmes divisées en *chunks*[#]_. Un *chunk* correspond à un bloc mémoire qui peut être renvoyé par ``malloc`` et libéré par ``free``. En particulier, lorsque la fonction ``free`` est utilisée, le *chunk* passé en paramètre est ajouté à une liste de « *chunks* libres », en étant éventuellement fusionné avec des *chunks* libres adjacents.

.. [#] « *chunk* » peut être traduit par « tronçon », mais pour une plus grande clarté, j'utiliserai le mot « *chunk* » pour désigner spécifiquement ce que la glibc appelle « *chunk * »

Un *chunk* contient un en-tête de 16 octets composé de deux nombres entiers de 8 octets chacun:

* ``mchunk_prev_size``, qui est la taille du *chunk* précédent s'il n'est pas utilisé (si ce *chunk* est utilisé, il s'agit de la fin du contenu du bloc précédent) ;
* ``mchunk_size``, qui est la taille du *chunk* dont c'est l'en-tête, à laquelle a été combinée quelques bits indiquant des informations.

La taille d'un *chunk* est alignée sur un multiple de 16 octets, laissant la possibilité d'enregistrer des informations dans les 4 bits de poids faible du champ ``mchunk_size``:

* Le bit de poids faible est nommé ``PREV_INUSE`` et est positionné à 1 si le *chunk* précédent est en cours d'utilisation (i.e. s'il a été alloué par ``malloc`` ou une fonction similaire)è
* Le second bit, nommé ``IS_MMAPPED``, est positionné à 1 si le *chunk* est le résultat de l'utilisation de la fonction ``mmap``. Dans le cas de ``f4ncyn0un0urs``, ce n'est jamais le cas.
* Le troisième bit, nommé ``NON_MAIN_ARENA``, peut être positionné à 1 si l'arène du *chunk* n'est pas l'arène principale. Comme les tailles des allocations mémoires effectuées par ``malloc`` et ``realloc`` dans ``f4ncyn0un0urs`` sont relativement petites, une seule arène est utilisée (l'arène principale, ``main_arena``). Ce bit n'est donc jamais positionné à 1.
* Le quatrième bit n'est pas utilisé, et vaut toujours 0.

Un certain nombre de pages web décrivent de manière assez précise le fonctionnement de l'allocateur de la glibc. Diverses personnes m'ont conseillé des lectures à ce sujet, et voici les trois pages que je trouve les plus pertinentes[#]_:

* https://sensepost.com/blog/2017/painless-intro-to-the-linux-userland-heap/
* https://medium.com/@c0ngwang/the-art-of-exploiting-heap-overflow-part-7-10a788dd7ab
* https://dangokyo.me/2018/01/16/extra-heap-exploitation-tcache-and-potential-exploitation/

.. [#] http://tukan.farm/2017/07/08/tcache/ est aussi intéressant pour aborder le sujet du *tcache*

En lisant de code de la glibc et en effectuant des tests, il y a deux aspects que je trouve important de mentionner pour comprendre comment réaliser l'exploitation ensuite:

* L'allocateur de la glibc utilise un certain nombre de listes de *chunks* libérés correspondant à des *cache*. Le premier est le « *tcache* », qui contient quelques *chunks* qui ont été libérés par ``free`` [#]_. Un *chunk* dans le *tcache* est marqué comme étant encore utilisé (le bit ``PREV_INUSE`` du *chunk* suivant est conservé à 1) et seul ``malloc`` peut extraire un *chunk* du *tcache*. En particulier:

    - ``realloc`` n'extrait pas de *chunk* du *tcache*. Et ce même si les *fast bins*, *small bins*, etc. sont vides, ``realloc`` créée un nouveau *chunk* en étendant l'espace occupé par l'arène utilisée plutôt que d'extraire un *chunk* du *tcache*.[#]_
    - Si le *chunk* qui est étendu par un appel à ``realloc`` est suivi par un *chunk* libéré qui est dans le *tcache*, ``realloc`` ne va pas fusionner les *chunks* mais déplacera le *chunk* ailleurs (en appelant ``_int_malloc``, qui n'utilise pas le *tcache*).

* Quand un *chunk* est extrait d'une liste de *chunks* libres qui n'est pas le *tcache* (par exemple d'un *fast bin*), le champ ``mchunk_size`` du *chunk* est comparé par rapport à ce qui est attendu en fonction de la liste dont il est issu (par exemple les *fast bins* sont ordonnés par taille, ce qui rend possible une telle vérification).

    - En particulier, si une attaque réussit à modifier l'adresse d'un *chunk* dans une telle liste, la valeur de retour de ``malloc`` se retrouve maîtrisée par l'attaquant à partir du moment où les 4 octets précédents[#]_ correspondent à un ``mchunk_size`` valide. En pratique cela complexifie beaucoup une telle attaque (il y a rarement une telle valeur juste à côté de valeurs intéressantes à écraser, comme des pointeurs de fonctions).
    - Le fait que le *chunk* libre ne soit pas dans le *tcache* est important. En effet, quand ``malloc`` renvoie un élément du *tcache*, il ne vérifie rien sur le *chunk* renvoyé[#]_.

.. [#] le *tcache* est concrètement un ensemble de *chunks* propre à un *thread* (chaque *thread* a un *tcache* différent) dans lequel les *chunks* sont ordonnés par taille. Quand il y a plus de 7 *chunks* d'une même taille, les *chunks* de taille identique qui sont libérés ensuite ne sont pas mis dans le *tcache* et utilisent donc les structures habituelles de la glibc (*fast bins*, *unsorted bin*, *small bins*, etc.)
.. [#] en conséquence, pour une taille de *chunk* donnée, tant que le *tcache* n'est pas rempli avec 7 *chunks*, utiliser ``free`` puis ``realloc`` ne permet pas de *récupérer* la mémoire qui vient d'être libérée dans le ``realloc``
.. [#] c'est 4 et non 8, car dans le fichier ``malloc/malloc.c`` des sources de la glibc, la macro ``fastbin_index(sz)`` n'utilise que 4 octets
.. [#] c'est en tout cas ce que je constate dans la version de glibc employée. Une version future de la glibc ajoutera peut-être une vérification, ce qui compliquera la mise en œuvre des attaques utilisant une corruption du *tcache*


Contraintes d'exploitation
~~~~~~~~~~~~~~~~~~~~~~~~~~

L'exploitation d'une vulnérabilité touchant l'allocateur mémoire d'une bibliothèque C nécessite de contrôler assez finement certains appels aux fonctions ``malloc``, ``realloc``, ``free``... Dans le cas présent, l'agent ne peut pas directement appeler ces fonctions dans le serveur racine, et ne peut qu'envoyer des messages qui sont ensuite traités par le serveur. Je m'intéresse donc aux liens entre les actions que réalise un agent et les fonctions relatives à l'allocation mémoire utilisées par le serveur.

* Quand un agent se connecte au serveur, le serveur appelle ``mesh_process_connection``, qui effectue les appels suivants:

    - ``malloc(0x230)``, pour allouer la structure ``CONN`` décrivant la nouvelle connexion ;
    - ``scomm_prepare_channel``, qui utilise une bibliothèque GMP qui alloue et libère beaucoup de petites zones mémoires ;
    - ``add_route``, qui étend éventuellement la table de routage avec ``realloc`` puis initialise une structure ``ROUTE_ENTRY`` avec 6 ID agents alloués, en appelant ``malloc(0x30)``.

        - Donc quand 6 agents sont connectés simultanément et qu'un 7:sup:`e` arrive, ``add_route`` appelle ``realloc(0x108)`` pour étendre la table de routage[#]_.

* Quand un agent connecté transmet son premier message ``PEER``, l'ID agent de la structure ``CONN`` qui lui est associée est mis à jour sans déclencher d'allocation.
* Quand un agent connecté transmet le message ``PEER`` d'un de ses nouveaux descendants (enfants, enfants de ses enfants, etc.) et que l'ID agent source du message n'est pas connu du serveur, celui-ci ajoute l'ID agent au tableau référencé par la structure ``ROUTE_ENTRY`` correspondant à l'agent connecté.

    - Cela se traduit par un appel à ``add_to_route``, qui appelle ``realloc`` pour étendre une zone mémoire à 11, 16, 21... éléments (ce qui correspond à des tailles ``0x58``, ``0x80`` et ``0xa8``).

* Quand un agent se déconnecte, le serveur appelle ``del_route``, qui appelle:

    - ``free`` sur le tableau des ID agents de la structure ``ROUTE_ENTRY`` ;
    - ``free`` sur la structure ``CONN`` décrivant la connexion.

.. [#] la table de routage est étendue à 6 + 5 entrées de ``0x18`` octets, ce qui fait ``0x108`` octets

Il existe aussi une fonction ``del_from_route``, qui ne peut pas être appelée dans le serveur racine, car son seul appelant est la fonction qui traite la réception de messages ``DUP_ID`` provenant du parent. J'ignore donc cette fonction dans l'analyse que j'effectue, ce qui signifie en pratique que le tableau des ID agents de la structure ``ROUTE_ENTRY`` ne peut que croître.

Ainsi, je m'intéresse à des *chunks* issus d'une allocation de ``0x230``, ``0x30``, ``0x108``, ``0x58``, ``0x80`` ou ``0xa8`` octets. Pour calculer la taille des *chunks* correspondants, le code de glibc[#]_ utilise une formule implémentée par la macro ``request2size(req)``:

.. [#] https://sourceware.org/git/?p=glibc.git;a=blob;f=malloc/malloc.c;hb=23158b08a0908f381459f273a984c6fd328363cb#l1219

.. code-block:: c

    // SIZE_SZ est la taille du type size_t, donc 8
    // MALLOC_ALIGNMENT vaut 16 (0x10 en hexadécimal)
    #define MALLOC_ALIGN_MASK (MALLOC_ALIGNMENT - 1)

    // MIN_CHUNK_SIZE vaut 32 (0x20)
    #define MINSIZE  \
      (unsigned long)(((MIN_CHUNK_SIZE+MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK))
    // donc MINSIZE vaut 32 (0x20)

    #define request2size(req)                                         \
      (((req) + SIZE_SZ + MALLOC_ALIGN_MASK < MINSIZE)  ?             \
       MINSIZE :                                                      \
       ((req) + SIZE_SZ + MALLOC_ALIGN_MASK) & ~MALLOC_ALIGN_MASK)

En bref, la taille du *chunk* correspondant à une allocation de :raw-latex:`$req$` octets est :raw-latex:`$(req + 8)$` aligné sur un multiple de 16, si cette valeur est supérieure à ``0x20``, et ``0x20`` sinon. Donc:

- ``malloc(0x230)`` renvoie un *chunk* de taille ``0x240`` (pour la structure ``CONN``) ;
- ``realloc(0x108)`` renvoie un *chunk* de taille ``0x110`` (pour la table de routage) ;
- ``malloc(0x30)`` renvoie un *chunk* de taille ``0x40`` (pour le tableau des ID agents) ;
- ``realloc(0x58)`` renvoie un *chunk* de taille ``0x60`` (pour le tableau des ID agents) ;
- ``realloc(0x80)`` renvoie un *chunk* de taille ``0x90`` (pour le tableau des ID agents) ;
- ``realloc(0xa8)`` renvoie un *chunk* de taille ``0xb0`` (pour le tableau des ID agents).

D'ailleurs, c'est pour cela que le serveur racine ne s'arrête pas quand un 8:sup:`e` ID agent est transmis par un agent: quand le 7:sup:`e` ID agent est enregistré par ``add_to_route``, les 8 octets écrasés sont encore dans les données du *chunk* alloué par ``malloc(0x30)``, car l'alignement sur un multiple de 16 de la taille du *chunk* a créé un vide 8 octets.

Par contre, quand le 12:sup:`e` ID agent est enregistré, cela dépasse la capacité du *chunk* renvoyé par ``realloc(0x58)``, et le champ ``mchunk_size`` du *chunk* suivant se retrouve écrasé. C'est pour cela que lorsque le programme appelle ensuite ``realloc(0x80)`` lorsque le 13:sup:`e` ID agent est reçu, le serveur racine s'interrompt avec le message « realloc(): invalid next size » (la taille du *chunk* suivant est en effet devenue invalide suite à l'écrasement). Toutefois en contrôlant la valeur du 12:sup:`e` ID agent, il est possible d'écrire une valeur de ``mchunk_size`` valide à la place, ce qui permet d'éviter l'arrêt du programme.

Il s'agit maintenant d'assembler les briques[#]_ que j'ai décrites (les actions d'un agent pouvant déclencher l'utilisation de ``malloc``, ``realloc`` et ``free`` sur le serveur racine) afin d'utiliser la corruption liée à l'enregistrement d'un 12:sup:`e` ID agent pour écrire de manière arbitraire en mémoire, ce qui devrait ensuite permettre d'exécuter du code sur le serveur racine.

.. [#] j'aime bien jouer aux Lego ;)

Assemblage des briques
~~~~~~~~~~~~~~~~~~~~~~

J'ai commencé par quelques tentatives ratées qui tentaient de corrompre des listes utilisées par l'allocateur de la libc (dont une qui permettait de contrôler le résultat d'un ``realloc(0x58)`` à partir du moment où les quatre octets précédents contenaient une valeur entre ``0x60`` et ``0x6f``, ce qui s'est révélé être une contrainte trop forte). Au bout de trois jours et de trois nuits, j'ai trouvé un assemblage permettant de contrôler à peu près le contenu de la table de routage !

Pour mettre en œuvre cet assemblage, il faut commencer par établir 6 connexions au serveur racine.
J'appelle ``t0``, ``t1``, ..., ``t5`` les tableaux d'ID agents associés à ces connexions.
Initialement ces tableaux sont issus d'un appel à ``malloc(0x30)``.
En envoyant 8 messages ``PEER`` avec des ID agents différents sur la première connexion, le serveur étend ``t0`` en appelant ``realloc(t0, 0x58)``.
Ce nouveau *chunk* alloué a comme caractéristique de se trouver en haut de l'arène principale[#]_, car il n'y avait précédemment pas de *chunks* libres de taille ``0x60``.
En effectuant une opération similaire sur la seconde connexion, le serveur appelle ``realloc(t1, 0x58)``, ce qui a pour conséquence de déplacer le tableau juste après ``t0``. En poursuivant ainsi, j'arrive à obtenir des *chunks* contigus de tailles ``0x60`` (le champ ``mchunk_size`` est alors ``0x61`` car le *chunk* précédent est alloué[#]_).

.. [#] cela se produit en particulier car d'autres *chunks* alloués se trouvent après ``t0``, l'empêchant ainsi qu'il soit étendu au lieu d'être déplacé
.. [#] en ajoutant à la taille du *chunk* (``0x60``) le bit ``PREV_INUSE`` (``0x01``), le champ ``mchunk_size`` vaut ``0x61``

Il est alors possible de trouver un enchaînement permettant d'étendre ``t5`` à ``0x80`` octets sans déclencher l'interruption du programme.
Une fois que ceci est fait, le schéma suivant décrit les actions que je peux effectuer pour arriver à une écriture arbitraire dans la mémoire du serveur racine.

.. raw:: latex

    \input{images/02_briques_table_routage.tikz.tex}

Dans ce schéma, chaque nombre correspond au champ ``mchunk_size`` du *chunk* situé à sa droite.
Le nombre rouge indique le champ ``mchunk_size`` du *chunk* de ``t5`` après son écrasement et le cadre rouge pointillé représente la taille de ce *chunk* considérée par l'allocateur mémoire.

.. raw:: latex

    \clearpage

Cet enchaînement permet ainsi de créer un chevauchement entre le tableau des ID agents de la 6:sup:`e` connexion (``t5``) et la table de routage.
En ajoutant des ID agents à ``t5``, je parviens donc à écraser la structure ``ROUTE_ENTRY`` de la première connexion, et en particulier l'adresse vers le tableau des ID agents associés à la première connexion.
En écrivant une adresse de la mémoire du serveur racine à cet endroit puis en envoyant un message ``PEER`` sur la première connexion, je peux donc écrire de manière arbitraire au moins 8 octets à un emplacement quelconque de la mémoire.

Il y a toutefois un détail qui empêche que ce soit si simple: quand un 7:sup:`e` agent se connecte au serveur racine, une structure ``CONN`` est allouée avec ``malloc(0x230)``, avant que la table de routage soit déplacée.
Comme en temps normal il n'y a pas de *chunks* libres déjà existant dans l'arène pour répondre à cette demande d'allocation, l'allocateur de la glibc alloue de la mémoire en haut de l'arène.
Un *chunk* de taille ``0x240`` vient donc se positionner entre ``t5`` et la table de routage, ce qui empêche les manipulations décrites d'être effectuées.

Comment empêcher l'apparition d'un *chunk* problématique de taille ``0x240`` au moment où un 7:sup:`e` agent se connecte au serveur racine ?
Il suffit d'en avoir alloué et libéré un au préalable, car dans une telle situation, ``malloc(0x230)`` renvoie le *chunk* présent dans les listes de *chunks* libres au lieu d'en créer un nouveau.
Mais comme les *chunks* de taille ``0x240`` sont alloués quand un agent se connecte au serveur et libérés quand un agent s'y déconnecte, et comme les manipulations que j'utilise pour écrire en mémoire nécessitent d'avoir 6 connexions actives sans jamais avoir eu précédemment 7 connexions actives simultanément[#]_, cela est impossible à réaliser en utilisant uniquement des connexions/déconnexions.
Et avec la vulnérabilité permettant d'écraser la taille du *chunk* suivant ?
Est-il possible de créer artificiellement un faux *chunk* de taille ``0x240``, qui puisse être libéré sans pénaliser le nombre de connexions ?
Le schéma suivant montre comment apporter une réponse positive à cette question, en tirant partie de la décomposition de ``0x240`` suivante::

    0x240 = 0x60 + 0x60 + 0x60 + 0x90 + 0x90

.. [#] pour les lecteurs qui n'auraient pas suivi, c'est la condition fondamentale au déclenchement du ``realloc(table de routage, 0x108)``, sur lequel est fondé le reste de l'assemblage

.. raw:: latex

    \input{images/02_briques_free0x240.tikz.tex}

À l'issue de cet assemblage, un *chunk* de taille ``0x240`` a été ajouté dans le *tcache* correspondant, et ``t4`` et ``t5`` ont été amenés dans un état semblable à celui du schéma précédent.
La superposition des *chunks* libérés fait que je peux m'attendre à quelques instabilités, mais en pratique il est possible de réaliser l'ensemble des opérations décrites sans déclencher de crash intempestif !

Vers l'exécution de code et au delà !
-------------------------------------

Où écrire pour exécuter du code ?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
:raw-latex:`\label{sect:2-ROP}`

Les actions décrites précédemment permettent d'écrire 8 octets à une adresse que je définis en mémoire, en établissant 7 connexions au serveur racine et en envoyant un certain nombre de messages ``PEER``.
Par ailleurs, la section :raw-latex:`\ref{sect:2-ping-leak}` a détaillé comment utiliser un message ``PING`` pour obtenir une adresse mémoire de la pile.
Au moment où l'écriture arbitraire se produit, le serveur racine est dans l'exécution de ``add_to_route`` et le contenu de la pile ressemble à ceci:

.. code-block:: xorg

    adresses relatives de la pile
    relatives à mesh_process_message

          +------------------------------+
    -4078 |         (alignement)         | <- rsp (extrémité de la pile)
    -4070 |        rbx sauvegardé        |
    -4068 |        rbp sauvegardé        |
          +------------------------------+
    -4060 |     rip sauv. (0x4015a3)     |
          |       [ add_to_route ]       |
          +------------------------------+
    -4058 |         (alignement)         |
    -4050 |        rbx sauvegardé        |
    -4048 |        rbp sauvegardé        |
    -4040 |        r12 sauvegardé        |
    -4038 |        r13 sauvegardé        |
          +------------------------------+
    -4030 |     rip sauv. (0x401cbe)     | <- cible de l'écriture arbitraire
          | [mesh_process_agent_peering] |
          +------------------------------+
    -4028 |     message reçu dans        | <- adresse obtenue avec PING
          |     mesh_process_message     |
          |       (16384 octets)         |
          |                              |
          |                              |
          +------------------------------+
    -0028 |         (alignement)         |
    -0020 |        rbx sauvegardé        |
    -0018 |        rbp sauvegardé        |
    -0010 |        r12 sauvegardé        |
    -0008 |        r13 sauvegardé        |
          +------------------------------+
    -0000 |     rip sauv. (0x4011aa)     |
          |   [ mesh_process_message ]   |
          +------------------------------+
          |             ...              |

Les 8 octets qui précèdent l'adresse de la pile qui a été obtenue avec une requête ``PING`` (i.e. ce qui se trouve en ``-4030`` sur le dessin) correspond à la sauvegarde du pointeur d'instruction (registre ``rip``) au moment où ``mesh_process_message`` appelle la fonction ``mesh_process_agent_peering`` pour traiter le message ``PEER`` reçu.
En remplaçant le contenu de ces octets par l'adresse d'une fonction, il est possible de rediriger l'exécution ailleurs.

De plus, juste après cette sauvegarde, en ``-4028``, se trouve le dernier message déchiffré qui a été reçu par le serveur racine, c'est à dire le message ``PEER`` ayant servi à déclencher l'écriture arbitraire.
En temps normal, un message ``PEER`` n'a pas de contenu (seuls les ID agents source et destination importent), mais il est possible d'en ajouter un, qui est simplement ignoré par le programme.
Ceci fournit un moyen simple d'écrire environ 16 Ko de données arbitraires sur la pile.

Comme la pile n'est pas exécutable [#]_ il n'est pas possible de déposer directement du code sur la pile.
Je dois utiliser une indirection pour cela.
En fait, le contexte que j'ai obtenu permet de directement mettre en œuvre ce que la littérature appelle la « Programmation orientée retour » (ROP).
Je peux en effet simplement écrire sur la pile des adresses correspondant à des morceaux de code que je souhaite exécuter (des « *gadgets* ») qui sont déjà présents dans les parties exécutables de la mémoire, et diriger l'exécution sur une instruction ``ret`` qui exécute ces *gadgets* successivement, jusqu'à obtenir l'effet que je souhaite.

.. [#] l'en-tête ELF de ``f4ncyn0un0urs`` définit bien que la pile n'est pas exécutable

Pour trouver des *gadgets*, un outil qui réalise ce travail existe déjà: ROPGadget [#]_.
Dans les *gadgets* qu'il donne, voici ceux que j'utilise:

.. [#] https://github.com/JonathanSalwan/ROPgadget

* ``0x454e89``: ``add rsp, 0x58 ; ret`` (ce qui est le premier *gadget* exécuté, pour sauter l'en-tête du message)
* ``0x454e8d``: ``ret``
* ``0x454e8c``: ``pop rax ; ret``
* ``0x400766``: ``pop rdi ; ret``
* ``0x4017dc``: ``pop rsi ; ret``
* ``0x408f59``: ``pop rcx ; ret``
* ``0x454ee5``: ``pop rdx ; ret``
* ``0x4573d4``: ``pop r10 ; ret``
* ``0x489291``: ``mov qword ptr [rsi], rax ; ret`` (met la valeur de ``rax`` à l'adresse indiquée par ``rsi``)
* ``0x47fa05``: ``syscall ; ret``

Pour effectuer un appel système sur Linux, il faut placer le numéro de l'appel système dans ``rax`` et les arguments dans ``rdi``, ``rsi``, ``rdx``, ``r10``, ``r8`` et ``r9``.
Les *gadgets* dont je dispose permettent donc d'exécuter n'importe quel appel système qui prend 4 arguments ou moins, ce qui est suffisant pour la plupart des situations.
De plus, comme la glibc est présente à une adresse fixe dans la mémoire, il est toujours possible de faire appel à des fonctions de la bibliothèque C pour accéder aux appels systèmes qui prennent plus que 4 arguments[#]_.

.. [#] Il faut par contre faire attention lors de l'utilisation de fonctions de la glibc de minimiser l'utilisation de celles qui peuvent allouer de la mémoire, comme ``fopen``. En effet, pour arriver à l'écriture arbitraire, il a fallu positionner les structures internes de l'allocateur mémoire dans un état plutôt instable...

Cela permet donc d'exécuter des appels systèmes depuis le serveur racine.
Le moyen le plus direct de transformer un tel accès en exécution de commandes arbitraires consiste à utiliser l'appel système ``execve`` pour exécuter un programme présent sur le système, comme ``ls``, ``cat``, etc.
Toutefois lorsque je tente d'utiliser ``execve``, le serveur racine s'interrompt avec le message « Bad system call ».
Le débogueur permet de trouver que cela provient de la réception du signal SIGSYS par le programme, qui signifie que le programme a tenté d'utiliser un appel système non-autorisé.

Donc certains appels systèmes (comme ``accept4`` pour accepter des nouvelles connexions) sont autorisés, d'autres (comme ``execve``) ne le sont pas.
Où est configurée la liste des appels systèmes autorisés ?
Sur un système Linux en général, il y a plusieurs possibilités: filtre SECCOMP [#]_, module de sécurité qui interdit l'appel système, etc.
Dans le cas de ``f4ncyn0un0urs``, j'avais remarqué la mise en place d'un filtre SECCOMP dans la fonction ``agent_init`` quand j'avais commencé l'analyse (section :raw-latex:`\ref{sect:1-ourson-fantaisiste}`).
Il est maintenant temps d'analyser le contenu de ce filtre !

.. [#] http://man7.org/linux/man-pages/man2/seccomp.2.html

Une barrière de plus à franchir
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
:raw-latex:`\label{sect:2-SECCOMP}`

La fonction ``agent_init`` utilise la fonction ``prctl`` pour charger un filtre SECCOMP-BPF présent à l'adresse ``0x4A8A60`` qui contient 53 instructions.
Une instruction BPF occupe 8 octets: un entier de 2 octets encode un « opcode », puis deux entiers de 1 octet permettent d'encoder des sauts conditionnels, et un entier de 4 octets encode une constante.

Afin de lire un programme SECCOMP-BPF à partir d'un éditeur hexadécimal, il faut avoir à l'esprit un certain nombre de notions:

* Le programme SECCOMP-BPF agit comme un filtre qui est exécuté à chaque appel système. Ce filtre peut accéder aux paramètres de l'appel système invoqué et retourne une décision.
* L'opcode ``0x0006`` permet à un filtre de retourner une valeur qui traduit la décision à prendre :

    - ``SECCOMP_RET_KILL_THREAD`` (``0``) pour indiquer au noyau de tuer le thread qui a tenté d'exécuter un appel système ;
    - ``SECCOMP_RET_ALLOW`` (``0x7fff0000``) pour indiquer que l'appel système est autorisé ;
    - ``SECCOMP_RET_TRAP`` (``0x00030000``) pour indiquer au noyau d'envoyer un signal ``SIGSYS`` au processus

* Un filtre SECCOMP-BPF commence habituellement par 4 instructions qui vérifient l'architecture employée et chargent le numéro de l'appel système dans un accumulateur. Ces instructions sont écrites en C de la manière suivante:

    .. code-block:: c

        BPF_STMT(BPF_LD + BPF_W + BPF_ABS, offsetof(struct seccomp_data, arch)),
        BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, AUDIT_ARCH_X86_64, 1, 0),
        BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_KILL_THREAD),
        BPF_STMT(BPF_LD + BPF_W + BPF_ABS, offsetof(struct seccomp_data, nr)),

    Ceci se traduit en hexadécimal par:

    .. code-block:: xorg

        20 00 00 00 04 00 00 00  15 00 01 00 3e 00 00 c0
        06 00 00 00 00 00 00 00  20 00 00 00 00 00 00 00

* Une liste blanche d'appels systèmes est implémentée en deux instructions par appel: un saut conditionnel suivi d'un retour autorisant l'appel système. En C, cela s'écrit (pour autoriser l'appel système *x*):

    .. code-block:: c

        BPF_JUMP(BPF_JMP + BPF_JEQ + BPF_K, __NR_x, 0, 1),
        BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_ALLOW),

    Ceci se traduit en hexadécimal par:

    .. code-block:: xorg

        15 00 00 01 xx xx xx xx  06 00 00 00 00 00 ff 7f

Le programme SECCOMP-BPF de ``f4ncyn0un0urs`` décrit une liste blanche d'appels systèmes sans utiliser les arguments des appels systèmes pour prendre une décision.
Voici son contenu brut, avec le numéro et le nom de l'appel système autorisé quand la ligne correspond à l'autorisation d'un appel système:

.. code-block:: xorg

    004a8a60: 2000 0000 0400 0000 1500 0100 3e00 00c0
    004a8a70: 0600 0000 0000 0000 2000 0000 0000 0000
    004a8a80: 1500 0001 e700 0000 0600 0000 0000 ff7f => 231: exit_group
    004a8a90: 1500 0001 0c00 0000 0600 0000 0000 ff7f =>  12: brk
    004a8aa0: 1500 0001 0900 0000 0600 0000 0000 ff7f =>   9: mmap
    004a8ab0: 1500 0001 0b00 0000 0600 0000 0000 ff7f =>  11: munmap
    004a8ac0: 1500 0001 2900 0000 0600 0000 0000 ff7f =>  41: socket
    004a8ad0: 1500 0001 3100 0000 0600 0000 0000 ff7f =>  49: bind
    004a8ae0: 1500 0001 3200 0000 0600 0000 0000 ff7f =>  50: listen
    004a8af0: 1500 0001 2001 0000 0600 0000 0000 ff7f => 288: accept4
    004a8b00: 1500 0001 3600 0000 0600 0000 0000 ff7f =>  54: setsockopt
    004a8b10: 1500 0001 2c00 0000 0600 0000 0000 ff7f =>  44: sendto
    004a8b20: 1500 0001 2d00 0000 0600 0000 0000 ff7f =>  45: recvfrom
    004a8b30: 1500 0001 1400 0000 0600 0000 0000 ff7f =>  20: writev
    004a8b40: 1500 0001 1700 0000 0600 0000 0000 ff7f =>  23: select
    004a8b50: 1500 0001 1900 0000 0600 0000 0000 ff7f =>  25: mremap
    004a8b60: 1500 0001 4800 0000 0600 0000 0000 ff7f =>  72: fcntl
    004a8b70: 1500 0001 0101 0000 0600 0000 0000 ff7f => 257: openat
    004a8b80: 1500 0001 0200 0000 0600 0000 0000 ff7f =>   2: open
    004a8b90: 1500 0001 0000 0000 0600 0000 0000 ff7f =>   0: read
    004a8ba0: 1500 0001 0300 0000 0600 0000 0000 ff7f =>   3: close
    004a8bb0: 1500 0001 4e00 0000 0600 0000 0000 ff7f =>  78: getdents
    004a8bc0: 1500 0001 d900 0000 0600 0000 0000 ff7f => 217: getdents64
    004a8bd0: 1500 0001 2000 0000 0600 0000 0000 ff7f =>  32: dup
    004a8be0: 1500 0001 0100 0000 0600 0000 0000 ff7f =>   1: write
    004a8bf0: 1500 0001 0500 0000 0600 0000 0000 ff7f =>   5: fstat
    004a8c00: 0600 0000 0000 0300

Le script autorise donc uniquement 24 appels systèmes.
J'y trouve dedans ceux nécessaires au bon fonctionnement d'un serveur TCP (``socket``, ``bind``, ``listen``, ``accept4``, ``recvfrom`` et ``sendto``), ceux utilisés pour la gestion mémoire de la bibliothèque C (``brk``, ``mmap`` et ``munmap``), et ceux utilisés pour accéder à des fichiers et pour implémenter l'interface du serveur racine (``open``, ``close``, ``read`` et ``write``).

Le courriel de la racine
~~~~~~~~~~~~~~~~~~~~~~~~
:raw-latex:`\label{sect:2-courriel-final}`

En revenant la méthode d'exécution décrite dans la section :raw-latex:`\ref{sect:2-ROP}`, je parviens donc à exécuter des appels systèmes sur le serveur racine.
Le filtre SECCOMP-BPF qui est en place ne permet d'utiliser qu'un nombre restreint d'appels systèmes, mais ceux qui permettent d'énumérer le contenu d'un dossier et de lire un fichier sont autorisés.
Je construit donc[#]_:

.. [#] le script Python qui implémente et met en œuvre ces chaînes de *gadgets* se trouve à l'annexe :raw-latex:`\ref{sect:ann-hackback-script}`

* une première chaîne de *gadgets* permettant d'énumérer le contenu d'un dossier et d'envoyer le résultat sur la connexion TCP établie, en utilisant les appels systèmes ``open``, ``getdents64`` et ``sendto`` (au travers de la fonction ``send`` de la glibc) ;
* une seconde chaîne de *gadgets* permettant d'envoyer le contenu d'un fichier sur la connexion TCP, en utilisant ``open``, ``read`` et ``sendto``.

En utilisant la première chaîne pour énumérer le contenu du dossier courant, j'obtiens une structure ``linux_dirent64``[#]_ qui m'informe que le dossier dans lequel est exécuté le serveur racine contient les éléments suivants:

.. [#] http://man7.org/linux/man-pages/man2/getdents.2.html

* un dossier ``..``
* un fichier ``agent.sh``
* un fichier ``.bashrc``
* un fichier ``.lesshst``
* un fichier ``.profile``
* un dossier ``secret``
* un dossier ``.``
* un lien symbolique ``.bash_history``
* un fichier ``.viminfo``
* un dossier ``.ssh``
* un fichier ``agent``
* un fichier ``.bash_logout``

Le dossier ``secret`` m'intrigue... en utilisant la première chaîne de nouveau pour en énumérer le contenu, j'obtiens:

* un dossier ``..``
* un fichier ``sstic2018.flag``
* un dossier ``.``

En utilisant la seconde chaîne pour lire le contenu de ``secret/sstic2018.flag``, j'obtiens:

.. code-block:: xorg

    65r1o0q1380ornqq763p96r74n0r51o816onpp68100s5p4s74955rqqr0p5507o@punyyratr.ffgvp.bet

Cela ressemble à une adresse électronique, mais le domaine est très étrange.
À tout hasard, je tente de décoder cette adresse en ROT13, et obtiens une adresse utilisant le domaine ``challenge.sstic.org``.
Il s'agit de l'adresse e-mail qu'il fallait découvrir, qui permet de témoigner de l'accès au système de fichier du serveur racine qui a servi à réaliser l'attaque initiale.

.. raw:: latex

    \intermediateflagw{1.05\textwidth}{Nation-state Level Botnet}{65e1b0d1380beadd763c96e74a0e51b816bacc68100f5c4f74955edde0c5507b@challenge.sstic.org}
