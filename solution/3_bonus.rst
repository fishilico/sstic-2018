Riposte finale, compromission totale
====================================

.. role:: raw-latex(raw)
     :format: latex

Du bac à sable à la coquille
----------------------------

Le fait que le serveur racine soit encore actif témoigne certainement du fait que l'attaquant prévois de sévir de nouveau.
Afin de découvrir les prochaines victimes de l'attaquant et de les prévenir, il me semble pertinent de chercher un moyen permettant de tracer les actions de l'attaquant, de préférence sans qu'il s'en aperçoive.
Pour cela, le plus simple serait de pouvoir exécuter des commandes *shell*.
Toutefois un filtre SECCOMP restreint mes possibilités pour l'instant (cf. section :raw-latex:`\ref{sect:2-SECCOMP}`): j'arrive actuellement uniquement à énumérer des dossiers et à lire des fichiers, en utilisant des chaînes de *gadgets*.

Cela est tout de même un accès suffisant pour recueillir un certain nombre d'informations.

En lisant ``/proc/self/status``, j'obtiens le contexte d'exécution du serveur racine:

.. code-block:: xorg

    Name:       agent
    Umask:      0022
    State:      R (running)
    Tgid:       23614
    Ngid:       0
    Pid:        23614
    PPid:       29811
    TracerPid:  0
    Uid:        1000    1000    1000    1000
    Gid:        1000    1000    1000    1000
    FDSize:     256
    Groups:     24 25 29 30 44 46 108 1000 64040 64042
    NStgid:     23614
    NSpid:      23614
    NSpgid:     23614
    NSsid:      29811
    VmPeak:     3212 kB
    VmSize:     3212 kB
    VmLck:         0 kB
    VmPin:         0 kB
    VmHWM:         4 kB
    VmRSS:         4 kB
    RssAnon:       4 kB
    RssFile:       0 kB
    RssShmem:      0 kB
    VmData:      168 kB
    VmStk:       132 kB
    VmExe:      2896 kB
    VmLib:         8 kB
    VmPTE:        16 kB
    VmPMD:         8 kB
    VmSwap:        0 kB
    HugetlbPages:  0 kB
    Threads:    1
    SigQ:       0/15637
    SigPnd:     0000000000000000
    ShdPnd:     0000000000000000
    SigBlk:     0000000000000000
    SigIgn:     0000000000000000
    SigCgt:     0000000000000000
    CapInh:     0000000000000000
    CapPrm:     0000000000000000
    CapEff:     0000000000000000
    CapBnd:     0000003fffffffff
    CapAmb:     0000000000000000
    Seccomp:    2
    Cpus_allowed:       3
    Cpus_allowed_list:  0-1
    Mems_allowed:       00000000,00000001
    Mems_allowed_list:  0
    voluntary_ctxt_switches:    153
    nonvoluntary_ctxt_switches: 312
    PaX:        pemrs

Le serveur racine s'appelle donc ``agent``, s'exécute avec l'utilisateur ``1000`` et le groupe ``1000``.
De plus la ligne ``PaX: pemrs`` semble indiquer la présence du patch de PaX Team[#]_ dans le système.
Quel noyau utilise le système qui exécute le serveur racine ?
Pour le savoir, je télécharge ``/proc/version``:

.. [#] https://pax.grsecurity.net/

.. code-block:: xml

    Linux version 4.9.0-4-grsec-amd64 (corsac@debian.org) (gcc version 6.3.0 20170516
    (Debian 6.3.0-18) ) #1 SMP Debian 4.9.65-2+grsecunoff1~bpo9+1 (2017-12-09)

Le système utilise donc le noyau dérivé de grsecurity[#]_ qui est fourni par Debian[#]_.
Le système utilise donc probablement la distribution Debian pour fonctionner. Quelle version ? Qu'indique ``/etc/os-release`` ?

.. [#] https://grsecurity.net/ (Le patch grsecurity intègre le patch de PaX Team)
.. [#] https://packages.debian.org/stretch-backports/linux-headers-4.9.0-4-grsec-amd64 indique par ailleurs que la version 4.9.65-2+grsecunoff1~bpo9+1 est la plus récente

.. code-block:: xml

    PRETTY_NAME="Debian GNU/Linux 9 (stretch)"
    NAME="Debian GNU/Linux"
    VERSION_ID="9"
    VERSION="9 (stretch)"
    ID=debian
    HOME_URL="https://www.debian.org/"
    SUPPORT_URL="https://www.debian.org/support"
    BUG_REPORT_URL="https://bugs.debian.org/"

Il s'agit donc d'un système Debian 9, qui correspond à la version appelée « stable » de Debian [#]_.
En revenant au serveur racine, qui est exécuté avec l'utilisateur ``1000``, je me demande comment s'appelle cet utilisateur.
La liste des comptes utilisateurs est généralement contenue dans ``/etc/passwd`` dans un système Linux[#]_:

.. [#] https://www.debian.org/releases/stable/ indique que Debian 9.0 a été publié en juillet 2017.
.. [#] cela n'est pas le cas quand un mécanisme comme un annuaire LDAP est intégré

.. code-block:: xorg

    root:x:0:0:root:/root:/bin/bash
    daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin
    bin:x:2:2:bin:/bin:/usr/sbin/nologin
    sys:x:3:3:sys:/dev:/usr/sbin/nologin
    sync:x:4:65534:sync:/bin:/bin/sync
    games:x:5:60:games:/usr/games:/usr/sbin/nologin
    man:x:6:12:man:/var/cache/man:/usr/sbin/nologin
    lp:x:7:7:lp:/var/spool/lpd:/usr/sbin/nologin
    mail:x:8:8:mail:/var/mail:/usr/sbin/nologin
    news:x:9:9:news:/var/spool/news:/usr/sbin/nologin
    uucp:x:10:10:uucp:/var/spool/uucp:/usr/sbin/nologin
    proxy:x:13:13:proxy:/bin:/usr/sbin/nologin
    www-data:x:33:33:www-data:/var/www:/usr/sbin/nologin
    backup:x:34:34:backup:/var/backups:/usr/sbin/nologin
    list:x:38:38:Mailing List Manager:/var/list:/usr/sbin/nologin
    irc:x:39:39:ircd:/var/run/ircd:/usr/sbin/nologin
    gnats:x:41:41:Gnats Bug-Reporting System (admin):/var/lib/gnats:/usr/sbin/nologin
    nobody:x:65534:65534:nobody:/nonexistent:/usr/sbin/nologin
    systemd-timesync:x:100:102:systemd Time Synchronization,,,:/run/systemd:/bin/false
    systemd-network:x:101:103:systemd Network Management,,,:/run/systemd/netif:/bin/false
    systemd-resolve:x:102:104:systemd Resolver,,,:/run/systemd/resolve:/bin/false
    _apt:x:104:65534::/nonexistent:/bin/false
    Debian-exim:x:105:109::/var/spool/exim4:/bin/false
    messagebus:x:106:110::/var/run/dbus:/bin/false
    sshd:x:107:65534::/run/sshd:/usr/sbin/nologin
    ntpd:x:108:112::/var/run/openntpd:/bin/false
    bind:x:109:113::/var/cache/bind:/bin/false
    sstic:x:1000:1000:Sstic,,,:/home/sstic:/bin/bash

L'utilisateur ``1000`` s'appelle donc ``sstic``, et c'est le seul utilisateur, avec ``root``, à pouvoir obtenir un *shell* sur le système.
En énumérant le dossier dans lequel est exécuté le serveur racine, j'avais trouvé à côté du dossier ``secret`` un dossier ``.ssh``.
Ce dossier contient un fichier ``authorized_keys``, avec des 4 clés publiques de connexion.

Avec un peu de chance, ce fichier est accessible en écriture et je peux y ajouter une clé SSH que je génère pour l'occasion.
Je crée une nouvelle chaîne de *gadgets* qui ouvre le fichier en mode ajout (grâce à l'appel système ``open`` avec les options ``O_WRONLY`` et ``O_APPEND``), y ajoute ma clé, et envoie « ok » sur le canal de communication TCP.
Je teste, j'envoie, et... ça fonctionne !

.. code-block:: sh

    $ ssh -i id_rsa_sstic sstic@195.154.105.12
    Linux sd-133901 4.9.0-4-grsec-amd64 #1 SMP Debian 4.9.65-2+grsecunoff1~bpo9+1
    (2017-12-09) x86_64

    The programs included with the Debian GNU/Linux system are free software;
    the exact distribution terms for each program are described in the
    individual files in /usr/share/doc/*/copyright.

    Debian GNU/Linux comes with ABSOLUTELY NO WARRANTY, to the extent
    permitted by applicable law.
    You have mail.
    Last login: Thu Apr  5 18:19:38 2018

    sstic@sd-133901:~$  w
     20:11:03 up 30 days,  1:54,  1 user,  load average: 0.02, 0.02, 0.00
    USER     TTY      FROM             LOGIN@   IDLE   JCPU   PCPU WHAT
    sstic@sd-133901:~$  who
    sstic    pts/0        2018-04-06 20:10
    sstic@sd-133901:~$  uname -a
    Linux sd-133901 4.9.0-4-grsec-amd64 #1 SMP Debian 4.9.65-2+grsecunoff1~bpo9+1
    (2017-12-09) x86_64 GNU/Linux
    sstic@sd-133901:~$  cat .bash_history
    sstic@sd-133901:~$  ls -al
    total 3040
    drwxr-xr-x 4 root  root     4096 Mar 29 15:44 .
    drwxr-xr-x 3 root  root     4096 Mar  7 16:26 ..
    -rwxr-xr-x 1 root  root  3069416 Mar 29 16:55 agent
    -rw-r--r-- 1 root  root      118 Mar 29 15:39 agent.sh
    lrwxrwxrwx 1 sstic sstic       9 Mar 16 10:56 .bash_history -> /dev/null
    -rw-r--r-- 1 sstic sstic     220 Mar  7 16:26 .bash_logout
    -rw-r--r-- 1 sstic sstic    3561 Mar 13 15:50 .bashrc
    -rw------- 1 sstic sstic      76 Mar 12 18:00 .lesshst
    -rw-r--r-- 1 sstic sstic     675 Mar  7 16:26 .profile
    drwxr-xr-x 2 root  root     4096 Mar 29 15:46 secret
    drwx------ 2 sstic sstic    4096 Mar 29 11:48 .ssh
    -rw------- 1 sstic sstic    3227 Mar 29 12:03 .viminfo
    sstic@sd-133901:~$  id
    uid=1000(sstic) gid=1000(sstic) groups=1000(sstic),24(cdrom),25(floppy),29(audio),
    30(dip),44(video),46(plugdev),108(netdev),64040(grsec-tpe),64042(grsec-sock-clt)
    sstic@sd-133901:~$  cat agent.sh
    while true; do
    /home/sstic/agent -c "SSTIC2018{f2ff2a7ed70d4ab72c52948be06fee20}" -l 36735 -i ceqejeve
    sleep 4
    done
    sstic@sd-133901:~$  ps -efHZ
    LABEL UID        PID  PPID  C STIME TTY          TIME CMD
    -     sstic    25160 25154  0 20:10 ?        00:00:00 sshd: sstic@pts/0
    -     sstic    25161 25160  0 20:10 pts/0    00:00:00   -bash
    -     sstic    25180 25161  0 20:11 pts/0    00:00:00     ps -efHZ
    -     sstic    29810     1  0 Mar29 ?        00:00:01 SCREEN
    -     sstic    29811 29810  0 Mar29 pts/3    00:00:00   /bin/bash
    -     sstic    25153 29811  1 20:09 pts/3    00:00:01     /home/sstic/agent -c SSTIC2018
    {f2ff2a7ed70d4ab72c52948be06fee20} -l 36735 -i ceqejeve
    -     sstic    29027     1  0 Mar29 ?        00:00:00 /lib/systemd/systemd --user
    -     sstic    29028 29027  0 Mar29 ?        00:00:00   (sd-pam)

J'ai donc un *shell* sur le système qui exécute le serveur racine !
Je vérifie au passage que l'exécutable du serveur racine (``/home/sstic/agent``) correspond à ``f4ncyn0un0urs``, ce qui est le cas.
Ceci permet donc de vraiment m'assurer que le serveur racine exécute le même code que celui extrait de la trace réseau initiale.

L'arborescence des processus permet de constater que le serveur est exécuté dans un *screen*[#]_.
En m'y attachant avec ``screen -x``, je tombe sur une fenêtre remplie de messages d'erreur du serveur racine, entre lesquels quelqu'un a exécuté la commande ``routes`` à plusieurs reprises:

.. [#] *screen* est un système de multiplexage de commandes permettant également d'exécuter des commandes sur un système distant sans devoir maintenir une session active, https://www.gnu.org/software/screen/manual/screen.html

.. code-block:: xorg

    Incorrect RSASSA padding start
    double free or corruption (!prev)
    Bad system call
    Segmentation fault
    Incorrect RSASSA padding PS
    Incorrect RSASSA padding start
    double free or corruption (!prev)
    Bad system call
    Incorrect RSASSA padding PS
    Segmentation fault
    Incorrect RSASSA padding PS
    Segmentation fault
    routes
    --------------------------------------------------------------------------------
    routing table:
    --------------------------------------------------------------------------------
    routes
    --------------------------------------------------------------------------------
    routing table:
    --------------------------------------------------------------------------------
    Incorrect RSASSA padding start
    routes
    --------------------------------------------------------------------------------
    routing table:
    --------------------------------------------------------------------------------
    realloc(): invalid next size
    Bad system call
    Bad system call

Pour regarder s'il y a un flag de validation intermédiaire sur ce système différent de celui utilisé par l'agent pour être lancé en mode « serveur racine », je lance une commande ``grep SSTIC`` récursive.
Celle-ci renvoie un résultat dans ``/home/sstic/.viminfo``:

.. code-block:: sh

    # Registers:
    ""1	LINE	0
        dd763c96e74a0e51b816bacc68100f5c4f74955edde0c5507b@challenge.sstic.org
    |3,1,1,1,1,0,1522317814,"dd763c96e74a0e51b816bacc68100f5c4f74955edde0c5507b@challenge.
    sstic.org"
    "2	LINE	0
        SSTIC2018{264b400d1640ce89a58ecab023df3be5}
    |3,0,2,1,1,0,1522317784,"SSTIC2018{264b400d1640ce89a58ecab023df3be5}"


.. raw:: latex

    \intermediateflag{.viminfo}{SSTIC2018\{264b400d1640ce89a58ecab023df3be5\}}

À la recherche du compte racine (ou pas)
----------------------------------------

Avoir un accès *shell* est suffisant pour prendre le contrôle du serveur racine (en attachant le *screen* qui est utilisé).
Mais cela est peu discret, et l'utilisateur légitime du serveur (l'attaquant) peut s'en rendre compte.
Pour éviter cela, l'idéal est d'obtenir un accès au compte ``root``.

Sur un système Linux, il y a assez peu de manières usuelles d'obtenir un accès au compte ``root``:

* en trouvant son mot de passe [#]_ et en utilisant la commande ``su`` ;
* en trouvant une clé privée SSH ayant accès au compte ``root``[#]_ ;
* en utilisant une faiblesse dans la configuration de la commande ``sudo`` si elle est trop permissive[#]_ ;
* en exploitant une configuration des tâches planifiées trop permissive[#]_ ;
* en écrivant un fichier qui est exécuté par l'utilisateur ``root``[#]_ ;
* en exploitant une vulnérabilité dans un programme qui est exécuté en tant que ``root`` (par exemple CVE-2018-0492[#]_[#]_ qui concerne la commande ``beep`` sur les systèmes Debian) ;
* en exploitant une vulnérabilité qui concerne le noyau (par exemple CVE-2016-5195[#]_).

.. [#] il arrive que le mot de passe du compte ``root`` soit simplement écrit dans un fichier accessible, ou qu'il soit simplement devinable à partir d'autres informations
.. [#] si le serveur SSH est configuré pour autoriser les connexions en tant que ``root`` en utilisant une clé SSH
.. [#] le fichier ``/etc/sudoers`` peut contenir des directives ``NOPASSWD`` autorisant un utilisateur à exécuter un ensemble défini de commandes. Cet ensemble est parfois mal restreint et permet indirectement d'obtenir un *shell* ``root``.
.. [#] par exemple si l'utilisateur peut créer un *cron* exécuté par l'utilisateur ``root``
.. [#] par exemple si ``/root/.bashrc`` est un lien symbolique vers ``/home/user/.bashrc``
.. [#] https://holeybeep.ninja/
.. [#] https://security-tracker.debian.org/tracker/CVE-2018-0492
.. [#] https://dirtycow.ninja/

Dans le cas présent, il ne semble pas qu'il y ait de mot de passe ou de clé privée SSH présents sur le système, les commandes ``sudo`` et ``beep`` ne sont pas installées et l'utilisateur ``sstic`` peut écrire un nombre très limité de fichiers.

En cherchant les exécutables présents sur le système possédant le bit ``SUID`` et appartenant à ``root`` (qui sont donc exécutés en tant que ``root``), je trouve:

.. code-block:: sh

    $ find / -perm -4000 -exec ls -ld {} \;
    -rwsr-xr-x 1 root root 59680 May 17  2017 /usr/bin/passwd
    -rwsr-xr-x 1 root root 50040 May 17  2017 /usr/bin/chfn
    -rwsr-xr-x 1 root root 75792 May 17  2017 /usr/bin/gpasswd
    -rwsr-xr-x 1 root root 40504 May 17  2017 /usr/bin/chsh
    -rwsr-xr-x 1 root root 40312 May 17  2017 /usr/bin/newgrp
    -rwsr-xr-x 1 root root 440728 Nov 18 10:37 /usr/lib/openssh/ssh-keysign
    -rwsr-xr-- 1 root messagebus 42992 Oct 1 2017 /usr/lib/dbus-1.0/dbus-daemon-launch-helper
    -rwsr-xr-x 1 root root 1019656 Feb 10 09:26 /usr/sbin/exim4
    -rwsr-xr-x 1 root root 40536 May 17  2017 /bin/su
    -rwsr-xr-x 1 root root 44304 Mar 22  2017 /bin/mount
    -rwsr-xr-x 1 root root 31720 Mar 22  2017 /bin/umount
    -rwsr-xr-x 1 root root 61240 Nov 10  2016 /bin/ping

Il y a donc uniquement des commandes qui peuvent être trouvées classiquement sur un système Linux, et a priori aucune ne me permet d'obtenir un *shell* ``root``.
Il est étrange que la commande ``ping`` face partie des résultats, compte tenu du fait que depuis des années il est possible de la configurer pour utiliser des « capabilities » au lieu du bit ``SUID``, mais cela n'est pas plus gênant que ça.

Une autre piste concerne les services qui sont exécutés sur le système.
À cause de l'utilisation de fonctionnalités présentes dans le noyau grsecurity, l'utilisateur ne peut voir que ses processus dans le dossier ``/proc`` (et donc dans la commande ``ps -e``).
Heureusement, le gestionnaire de services utilisé, ``systemd``, intègre une commande permettant de contourner cette limitation, ``systemctl`` [#]_:

.. [#] en pratique, il est possible d'obtenir les informations concernant les services exécutés en énumérant l'arborescence de ``/sys/fs/cgroup/pids/``, car systemd utilise les *cgroups* de Linux pour gérer les services du système

.. code-block:: sh

    $ systemctl status
    * sd-133901
        State: running
         Jobs: 0 queued
       Failed: 0 units
        Since: Wed 2018-03-07 17:16:28 CET; 4 weeks 2 days ago
       CGroup: /
               |-    2 n/a
               |-    3 n/a
               |-    5 n/a
               |-    7 n/a
    [...]
               |-25215 n/a
               |-user.slice
               | `-user-1000.slice
               |   |-session-896.scope
               |   | |-25154 n/a
               |   | |-25160 sshd: sstic@pts/0
               |   | |-25161 -bash
               |   | |-26063 systemctl status
               |   | `-26064 pager
               |   |-session-664.scope
               |   | |-25153 /home/sstic/agent -c
    SSTIC2018{f2ff2a7ed70d4ab72c52948be06fee20} -l 36735 -i ceqejeve
               |   | |-29810 SCREEN
               |   | `-29811 /bin/bash
               |   `-user@1000.service
               |     `-init.scope
               |       |-29027 /lib/systemd/systemd --user
               |       `-29028 (sd-pam)
               |-init.scope
               | `-1 n/a
               `-system.slice
                 |-irqbalance.service
                 | `-314 n/a
                 |-lvm2-lvmetad.service
                 | `-203 n/a
                 |-ifup@enp1s0.service
                 | `-638 n/a
                 |-dbus.service
                 | `-315 n/a
                 |-ssh.service
                 | `-368 n/a
                 |-system-getty.slice
                 | `-getty@tty1.service
                 |   `-365 n/a
                 |-systemd-logind.service
                 | `-309 n/a
                 |-openntpd.service
                 | |-660 n/a
                 | |-661 n/a
                 | `-665 n/a
                 |-cron.service
                 | `-312 n/a
                 |-systemd-udevd.service
                 | `-24763 n/a
                 |-rsyslog.service
                 | `-30236 n/a
                 |-systemd-journald.service
                 | `-19918 n/a
                 |-bind9.service
                 | `-30598 n/a
                 `-exim4.service
                   `-624 n/a

En résumé, il s'agit d'un système assez minimal, qui présente une surface d'attaque plutôt réduite.
Le seul service qui ne provient pas de la distribution Debian semble être le programme qui exécute le serveur racine, dans ``/home/sstic``.

Quelque jours après avoir ajouté ma clé SSH au fichier ``.ssh/authorized_keys``, je n'ai plus réussi à me connecter.
En téléchargeant de nouveau ce fichier, il semble qu'il ait été réinitialisé et soit maintenant en lecture seule.
Une porte d'entrée est donc fermée, mais quelqu'un m'a par ailleurs indiqué que les permissions sur le dossier ``.ssh`` permettent d'y ajouter un fichier et que la configuration par défaut sur serveur OpenSSH utilise également le fichier ``.ssh/authorized_keys2``[#]_ pour définir les clés SSH autorisées...

.. [#] c'est écrit dans la documentation, https://www.freebsd.org/cgi/man.cgi?sshd(8)#AUTHORIZED_KEYS%09FILE_FORMAT
