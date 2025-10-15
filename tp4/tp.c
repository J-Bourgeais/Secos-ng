/* GPLv2 (c) Airbus */
#include <debug.h>
#include <pagemem.h>
#include <cr.h>


/*Pour l'instant j'écris des idées, à compléter et corriger avec la correction*/


void tp() {
	// TODO
	//Q1
	printf("cr3 :",get_cr3()); //cf cr.h

	//Q2
	 //cf pagemem.h
	pde32_t* pgd = (pde32_t*)0x600000;
	set_cr3((uint32_t)pgd);

	//Q3 : Tester
	uint32_t cr0 = get_cr0();//cf cr.h
	cr0 |= 0x80000000; //mettre le bit 31 à 1
	set_cr0(cr0);
	//marche pas car pas de mémoire virtuelle initialisée ? a tester, ca a l'air d'être ca car on doit initialiser Q5

	//Q4
	pte32_t* ptb = (pte32_t*)0x601000;

	//Q5
	//je sais vraiment pas quoi faire là ??
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
	//les adresses virtuelles doivent être identiques aux adresses physiques
	
	//Q6
	printf("ptb[0] avant activation pagination : %x\n", ptb[0].rw);
	//Activer la pagination
	uint32_t cr0_paging = get_cr0();
	cr0_paging |= 0x80000000; //mettre le bit 31 à 1
	set_cr0(cr0_paging);
	set_cr3((uint32_t)pgd); //recharger cr3
	printf("ptb[0] après activation pagination : %x\n", ptb[0].rw);
	//Je pense pas que ca soit bon

	//Q7
	// Avant d'activer la pagination, on souhaiterait faire en sorte que l'adresse virtuelle `0xc0000000` permette de modifier votre PGD après activation de la pagination. Comment le réaliser ?
	set_cr3(0xc0000000);
	//Car cr3 a l'air d'être le registre qui permet de modif
	//Pas sure du tout

	//Q8
	ptb[0x700000 >> 12].p = 1; //présent
	ptb[0x700000 >> 12].rw = 1; //lecture/écriture
	ptb[0x700000 >> 12].addr = 0x2; //adresse physique 0x2000 >> 12
	ptb[0x7ff000 >> 12].p = 1; //présent
	ptb[0x7ff000 >> 12].rw = 1; //lecture/écriture
	ptb[0x7ff000 >> 12].addr = 0x2; //adresse physique 0x2000 >> 12
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

}
