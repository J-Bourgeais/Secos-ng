/* GPLv2 (c) Airbus */
#include <debug.h>
#include <pagemem.h>
#include <cr.h>


/*Pour l'instant j'écris des idées, à compléter et corriger avec la correction*/


void tp() {
	// TODO
	//Q1
	printf("cr3 : %x",get_cr3()); //cf cr.h

	//Q2
	 //cf pagemem.h
	pde32_t* pgd = (pde32_t*)0x600000;
	set_cr3((uint32_t)pgd);

	//Q3
	uint32_t cr0 = get_cr0();//cf cr.h
	cr0 |= 0x80000000; //mettre le bit 31 à 1
	//set_cr0(cr0);
	//commenté parce que marche pas (c'est sensé pas marcher donc good)



	//marche pas car pas de mémoire virtuelle initialisée ? a tester, ca a l'air d'être ca car on doit initialiser Q5
/* Il faut activer CR0 seulement après avoir :
rempli PGD
rempli PTB
mappé PGD/PTB dans l’espace virtuel (sinon plus de moyen d'y accéder)
*/




	//Q4
	pte32_t* ptb = (pte32_t*)0x601000;

	//Q5
//Test
	//Préparer au moins une entrée dans le PGD pour la PTB.**
    //Préparer plusieurs entrées dans la PTB.
	pgd[0].p = 1; //présent
	pgd[0].rw = 1; //lecture/écriture
	pgd[0].addr = 0x601; //adresse de la PTB (0x601000 >> 12)
	for(int i = 0; i < 1024; i++) {
		ptb[i].p = 1; //présent
		ptb[i].rw = 1; //lecture/écriture
		ptb[i].addr = i; //adresse physique (i * 0x1000 >> 12)
	}
	//Une deuxième car les parties suivantes en auront besoin
	pgd[1].p = 1;
	pgd[1].rw = 1;
	pgd[1].addr = 0x602; // 0x602000 >> 12
	pte32_t* ptb2 = (pte32_t*)0x602000;
	for(int i = 0; i < 1024; i++) {
		ptb2[i].p = 1; //présent
		ptb2[i].rw = 1; //lecture/écriture
		ptb2[i].addr = i; //adresse physique (i * 0x1000 >> 12)
	}


	int ptb_index = (0x601000 >> 12) & 0x3FF; // = 1
	ptb[ptb_index].p = 1;
	ptb[ptb_index].rw = 1;
	ptb[ptb_index].addr = 0x601; // physiquement 0x601000
	//les adresses virtuelles doivent être identiques aux adresses physiques
	


	//Q7
	// Avant d'activer la pagination, on souhaiterait faire en sorte que l'adresse virtuelle `0xc0000000` permette de modifier votre PGD après activation de la pagination. Comment le réaliser ?
	//pgb de 0x300 amene a 0x600000 car le pgd est à 0x600000
	// PGD est à l'adresse physique 0x600000
	// On veut y accéder via l'adresse virtuelle 0xc0000000

	pgd[0x300].p = 1;         // 0xc0000000 >> 22 = 0x300
	pgd[0x300].rw = 1;
	pgd[0x300].addr = 0x600;  // 0x600000 >> 12




	//Q6
	printf("ptb[0] avant activation pagination : %x\n", ptb[0].rw);
	//Activer la pagination
	uint32_t cr0_paging = get_cr0();
	cr0_paging |= 0x80000000; //mettre le bit 31 à 1
	set_cr0(cr0_paging); //après mapping donc ok
	set_cr3((uint32_t)pgd); //recharger cr3
	//pte32_t* ptb_virtual = (pte32_t*)0x601000; // valable si identity mapping toujours actif
	printf("ptb[0] après activation pagination : %x\n", ptb[0].rw);
	//ca dis que ptb[0] est à 0; comme si on ne pouvais plus modifier

	
	printf("pgb[0x300] après activation pagination : %x\n ", pgd[0x300].rw);
	//Toujours à 0, bizarre


	//set_cr3(0xc0000000);
	//Car cr3 a l'air d'être le registre qui permet de modif
	//Pas sure du tout

	//Q8

	//Ces adresses virtuelles sont dans la plage [0x700000, 0x800000), soit dans la 2e Page Table.
	//Donc on dois utiliser une 2e entrée dans le PGD, et une nouvelle PTB.
	//--> Fait plus haut

	// Dans la nouvelle PTB
	ptb2[(0x700000 >> 12) & 0x3FF].p = 1;
	ptb2[(0x700000 >> 12) & 0x3FF].rw = 1;
	ptb2[(0x700000 >> 12) & 0x3FF].addr = 0x2; // 0x2000 >> 12

	ptb2[(0x7ff000 >> 12) & 0x3FF].p = 1;
	ptb2[(0x7ff000 >> 12) & 0x3FF].rw = 1;
	ptb2[(0x7ff000 >> 12) & 0x3FF].addr = 0x2;


	char* str1 = (char*)0x700000;
	char* str2 = (char*)0x7ff000;
	printf("str1 : %s\n", str1);
	printf("str2 : %s\n", str2);
	//A Tester

	//Q9 : Effacer la première entrée du PGD
	pgd[0].p = 0; //marquer comme non présent : vraiment effacé ?
	//Tester l'accès aux adresses 0x700000 et 0x7ff000
	printf("str1 après effacement pgd : %s\n", str1);
	printf("str2 après effacement pgd : %s\n", str2);
	//Devrait planter ??

/*
Rendu :

cr3 : 0ptb[0] avant activation pagination : 1
ptb[0] après activation pagination : 0
pgb[0x300] après activation pagination : 0
str1 : ��
tr2 : 
str1 après effacement pgd : ��
tr2 après effacement pgd : 
halted !
*/




}
