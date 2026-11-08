/**
  ******************************************************************************
  * @file    reseau.h
  * @brief   Initialisation de la pile reseau, partagee entre les parties.
  *
  *  Ces fonctions sont definies dans partie3_reseau.c. Elles y etaient
  *  statiques ; elles sont exposees ici pour que la partie 5 (comparaison
  *  Cloud / Edge) puisse monter le reseau sans dupliquer ce code.
  *
  *  A appeler depuis une tache FreeRTOS, apres tcpip_init().
  ******************************************************************************
  */
#ifndef RESEAU_H
#define RESEAU_H

/* Cree l'interface Ethernet, la demarre et lance le client DHCP. */
void reseau_init(void);

/* Attend une adresse DHCP, et bascule sur l'adresse de secours de main.h
   si aucun serveur ne repond dans le delai imparti. */
void reseau_attendre_adresse_ip(void);

/* Affiche adresse IP, masque et passerelle sur la console. */
void reseau_afficher_configuration_ip(void);

#endif /* RESEAU_H */
