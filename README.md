Template de projet C pour le Mini-RSIC, avec utilisation des interruptions.

Mon envie est de faire un casse brique classique avec une variation de la trajectoire de la bille en fonction de la vitesse de la souris
un truc fou serait de récupérer un bonus qui s'active quand on appuie sur espace

L'objectif est d'avoir le programme classique qui se run avec le mouvmement de la bille avec deux types d'interuptions
Les mouvements de la souris sont des interuptions
Les collisions sont des interuptions.
faire une fonction pause

il faut lire sur la liaison série et non pas sur le clavier 

FIn de la séance du 9dec :
    Actuellement les interruptions fonctionnent bien toutefois le décalage du clavier ne se fait pas bien car j'appelle deux fois KEYBOARD->DATA donc l'instruction qui indiut l'interruption n'est pas la même que celle gérée par l'interrutption il faut que j'utilise une pile de message pour cela 

La tache du 16 est de géré les interruptions avec la souris :
Bon la bille rebondie mais elle traverse le plateau
La j'ai un probleme, quand je clique il y a un overflow et je ne vois pas d'ou il sort 

faire une fonction pour ne lancer la vérif des casses birque que si la bille est dans la zone supérieure

Il y a un problème car la condition de rebond permet aux billes de rebondir à l'infini et donc de ne pas s'extraire du bas de l'image -> Je mets un delay pour éviter les appels consécutifs à la fonction

De plus pour éviter que la boule ne voit pas un brique je vais utiliser un rectangle dans lequel la boule doit être pour enclencher le rebond Ou alors je vais dispacher les taches sur des coeurs différents 

peut etre devrais je mettre plus de distnace entre les tuies
Je pense qu'il y a un décalage entre l'affiche et les tuiles à cause de la variable écart


Pour éviter les collisions à l'intérieur il faut gérer les collisions non pas sur la position actuelle mais sur la position future.

EN fait je suis trop con je n'ai pas pris en compte le rayon de la bille pour les collisions

J'ai un problème sur la gestion des angles c'est pas possible 


Les points à améliorer dans l'ordre :
    - Introduire le rayon dans le collision_plate
    - Reprendre la gestion de l'affichage pour vérifier que les temps de calculs et d'affichage soient cohérents entre eux.
    - Résoudre les problèmes d'angles et de vitesse qui ne respectent pas la physique
    - gérer les collisions verticales et horizontales
    - utilser les interruptions souris -> sinon rester sur le clavier avec augmentation de la vitesse en fonction du temps d'appui
    - Faire des bonus
    - Mettre un affichage de fin de niveau et de début de jeu
  
Revoir ici les taches concurentes : ie bloquer la vidéo pour gérér la collision puis rendre la main


# Casse brique

Auteur : Jérémy Couture

# Objectif

L'objectif de ce projet est de réaliser un casse brique émulé sur Mini-Risc Harvey. Il doit être jouable au clavier et proposer différents niveaux : mur, pyramide, pyramide inversée avec un menu pour choisir le niveau. Cet objectif n'a pas été atteint et seul le niveau mur est jouable sans menu.

# Fonctionnalités mise en place

Les principales fonctionnalités mise en place sont l'affichage de rectangle et cercle de couleurs à l'aide de `draw_square` et `draw_disk`. Le déplacement de la bille, du plateau ainsi que l'affichage et effaçage des briques font directement appel à ces deux fonctions.

# Mécanique du projet
Le projet est organisé autour de plusieurs fonctions lancées en parallèle qui gèrent les mouvements, les collisions et l'affichage de la balle et du plateau.
L'exécution du main initialise le périphérique vidéo et autorise les interruptions clavier/souris. Ensuite il initialise le mode de jeu, la bille et le plateau. A partir de cet instant ce sont les trois tâches et une fonction suivantes qui deviennent actives ainsi que deux interruptions qui se lancent :
  - `collision_plate`
  - `collision_brics`
  - `move_marble`
  - `move_plate`

Voyons plus en détail chacune d'entre elles :
- `collision_plate` : Cette fonction impose à la balle un rebond horizontal en cas de contact avec le rebord supérieur du plateau. Un délai a été introduit pour éviter deux appels immédiats de la fonction. De plus l'objectif initial été de changer l'angle de rebond de la bille en fonction du mouvement du plateau mais il semble que cette fonctionnalité bien que définie ne soit pas fonctionnelle.

- `collision_brics` : Première des deux fonctions principales du programme, elle prend en argument la position future de la bille et vérifie si il y aura une collision. Elle renvoie le numéro de la brique responsable de la collision. Afin de ne pas utiliser inutillement des ressources elle ne s'active que si la bille est dans la zone d'existence des briques et parcourt la liste des briques en commençant par les lignes du bas. 

- `move_marble` : Deuxième fonction principale du programme, elle calcule la position de la bille et l'affiche. Elle fait appel à `collision_brics` pour savoir si une collision aura lieu avec une brique. Le cas échéant elle appel la fonction de rebond associée, rend inactive et efface la brique responsable de la collision.

 - `move_plate` : Cette fonction sert à déplacer le plateau à l'aide du clavier. Pour cela on vérifie dans l'ordre si il y a eu un appui long sur les flèches directionnelles ou une simple pression. Dans le premier cas on augmente/diminue la position du plateau en x en prenant en compte le temps d'appuie sur la touche. Dans le second cas on augmente/diminue simplement la position du plateau en x d'une constante.


# Compilation et éxectution
Le projet se compile et s'éxécute avec la ligne de commande `make exec` dans le répertoire source du projet.

# Tentatives infructeuses 

# Idées d'améliorations

# Conclusion