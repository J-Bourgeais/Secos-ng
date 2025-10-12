/* GPLv2 (c) Airbus */
#include <debug.h>

void bp_handler() {
   
   uint32_t val;
   asm volatile ("mov 4(%%ebp), %0":"=r"(val));
   //Ce que le CPU ne sauvegarde pas automatiquement et qu’il faut sauvegarder manuellement = les registres généraux et de segment.
   __asm__ volatile (
        "pushal\n\t"        // Sauvegarde tous les registres généraux (EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP)
        "push %ds\n\t"      // Sauvegarde des registres de segment
        "push %es\n\t"
        "push %fs\n\t"
        "push %gs\n\t"
    );
	printf("Breakpoint exception handled!\n");

	__asm__ volatile (
        "pop %gs\n\t"       // Restauration dans l’ordre inverse
        "pop %fs\n\t"
        "pop %es\n\t"
        "pop %ds\n\t"
        "popal\n\t"         // Restaure les registres généraux
        "add $4, %esp\n\t"  // Nettoie le code d’erreur (si présent, ici #BP n’en a pas)
        "iret\n\t"          // Retour d’interruption
    );
}

void bp_trigger() {
	// TODO
	printf("Déclenchement du breakpoint\n");
    __asm__ volatile("int3");  // Déclenche #BP
}

void tp() {
	//Q1
	// TODO print idtr
	//#define get_ldtr(aLocation)       \
   	//asm volatile ("sldt %0"::"m"(aLocation):"memory")
	printf(get_ldtr());
	//Q3 : 
	//Je ne sais pas ce qu'il faut modifier. intr.h ligne 50 ??
	// TODO call bp_trigger
   bp_trigger();
}
