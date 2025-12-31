# Ordonnanceur Préemptif Ring 3

## Description
Ce projet met en œuvre un micro-noyau capable de gérer deux tâches utilisateur exécutées en Ring 3.  
Le système repose sur un ordonnanceur préemptif déclenché par l’horloge (IRQ0), une isolation mémoire via la pagination, une zone de mémoire partagée entre les tâches et un mécanisme d’appel système pour l’affichage du compteur.

## Instructions d'exécution
1. Détarer l'archive
2. Se placer dans le dossier `tp_exam` :
   ```bash
   cd tp_exam
3. Lancer la machine virtuelle avec QEMU
    ```bash
   make 
   make qemu

## Le code développé se trouve principalement dans les fichiers suivants: 
- tp.c : initialisation du système (segmentation, TSS, pagination), création des tâches et bascule initiale en Ring 3
- kernel/core/intr.c : gestion des interruptions, IRQ0 et ordonnancement préemptif
- task.h : structures de données associées aux tâches (contexte noyau, CR3)
- kernel/core/idt.s : Gestion du switch
- utils/linker.lds : configuration de notre mémoire physique.

## Cartographie mémoire
La cartographie mémoire complète (adresses physiques, virtuelles, segments GDT et privilèges)
est détaillée dans la documentation fournie avec le projet.

