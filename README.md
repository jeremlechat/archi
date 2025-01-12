# Casse brique

Auteur : Jérémy Couture

# Objectif

L'objectif de ce projet est de réaliser un casse brique émulé sur Mini-Risc Harvey. Il doit être jouable au clavier et proposer différents niveaux : mur, pyramide, pyramide inversée avec un menu pour choisir le niveau. Cet objectif n'a pas été atteint et seul le niveau mur est jouable sans menu.

# Fonctionnalités mise en place

Les principales fonctionnalités mise en place sont l'affichage de rectangle et cercle de couleurs à l'aide de `draw_square` et `draw_disk`. Le déplacement de la bille, du plateau ainsi que l'affichage et effaçage des briques font directement appel à ces deux fonctions. La principale difficulté a été la mise en place du rebond sur les briques et sur le plateau, en effet j'avais initialement des rebonds multiples sur une seule brique ou sur le platea, j'ai donc compris une marge d'erreur sur la position de la bille vis à vis de la brique mais cela n'a pas été suffisant. En effet le problème venait du fait que la bille n'avait pas le temps de quitter la zone de collision de la brique après un rebond ce qui entrainait des rebonds supplémentaires. C'est pour cela qu'il possible de voir de nombreux `vTaskDelay(MS2TICKS(2*delay));` dans les fonctions de collisons. De plus l'augmentation de la vitesse de déplacement du plateau suite à un appui long a été rendu possible grâce à `KEYBOARD_DATA_REPEAT` qui permet de gérer les appuis long sur le clavier.

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
Le projet se compile et s'éxécute avec la ligne de commande `make exec` dans le répertoire source du projet. Le jeu se lance immédiatement donc il faudra être réactif.

# Tentatives infructeuses et problèmes

Lors de la création de ce projet j'ai eu pour idée de rendre la gestion du plateau possible par la souris toutefois la prise en compte de la souris nécessitait un trop grand nombre de ressources que je n'ai pas réussi à synchroniser. En effet au delà du simple déplacement de la souris il fallait enregistrer sa vitesse afin de la retransmettre au plateau et ainsi à la bille. Je pense que mon erreur à été de vouloir créer une gestion complète du premier coup au lieu d'utiliser une méthode incrémentale.
Je me suis donc rabbatu sur la gestion par clavier qui elle est fonctionelle. Cependant le changement d'angle de la bille suite à la collision avec le plateau dans le cas où ce dernier est en déplacement n'est pas fonctionnel. Ce problème est rejoint par une gestion de la vitesse et des angles que je n'ai pas réussi à maitriser entièrement. En effet pour une raison que j'ignore la vitesse de la bille semble augmenter lors de rebond avec le plateau et ralentir lors de collision avec les briques. De plus l'initialisation de `marble_speed` à de grandes répercussions sur l'angle initial de la bille alors que ces deux données ne sont pas liées. Pour une valeur supérieure à 3 le comportement est nominal, pour une valeur entre 0 et 3 la bille à une direction verticale et pour une valeur négative l'angle change en fonction de la valeur de la vitesse. 

Il est possible si l'on augmente le nombre de briques que la bille "rentre" dans une brique et rebondisse à l'intérieure de cette dernière. Cela est dû à un appel trop long de la fonction `collision_brics` j'imagine même si j'ai tenté de reduire le temps de parcourt de cette dernière.

# Idées d'améliorations

Plusieurs idées d'amélioration me sont apparues au cours de ce projet. Tout d'abord il est possible de finaliser le changement d'angle de la bille suite à une collision avec le plateau en mouvement. On pourrait aussi ajouter un son pour le rebond de la bille qui soit différent selon la surface de rebond : plateau; rebords; briques.

# Conclusion

Ce projet m'a permis de me rendre compte à quel point la synchronisation de variables ainsi que l'appel à des fonctions rapides peut être déterminant pour un code. Mon projet est fonctionnel mais il ne pourra pas être amélioré tant que ces deux condtions ne seront pas perfectionnées.