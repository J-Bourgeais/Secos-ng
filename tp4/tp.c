/* GPLv2 (c) Airbus */
#include <debug.h>
#include <pagemem.h>


/*Pour l'instant j'écris des idées, à compléter et corriger avec la correction*/


void tp() {
	// TODO
	//Q1
	printf("cr3 :",get_cr3());

	//Q2
	 //cf pagemem.h
	pde32_t* pgd = (pde32_t*)0x600000;
	set_cr3((uint32_t)pgd);

	//Q3 : Tester

	//Q4
	pte32_t* ptb = (pte32_t*)0x601000;

	//Q5
	//I don't know


}
