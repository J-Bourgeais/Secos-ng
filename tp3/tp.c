/* GPLv2 (c) Airbus */
#include <debug.h>

void userland() {
   asm volatile ("mov %eax, %cr0");
}

void tp() {
   // TODO
   //Q1  En théorie, rien a faire, la gdt est deja active non ?
   //Q2
   asm volatile (
        /* placer SS (16-bit) dans eax low, zero-extend, puis push 32-bit */
        "xor %%eax, %%eax\n\t"    /* eax = 0 */
        "movw %%ss, %%ax\n\t"     /* ax = ss (16-bit) ) */
        "push %%eax\n\t"          /* push 32-bit SS (zero-extend) */

        /* push ESP (la valeur courante d'ESP) */
        "push %%esp\n\t"

        /* push EFLAGS */
        "pushf\n\t"

        /* push CS (sélecteur user) et EIP (adresse userland) */
        "push %[cs_sel]\n\t"
        "push %[eip_addr]\n\t"
        "iret\n\t"
        :
        : [cs_sel]"i"(0x1B),            /* exemple : sélecteur code ring3 */
          [eip_addr]"r"(userland)       /* adresse de début userland */
        : "eax", "memory"
    );
}
