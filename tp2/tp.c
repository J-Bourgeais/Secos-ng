/* GPLv2 (c) Airbus */
#include <debug.h>
#include <intr.h>

void bp_handler() {
   // Q2
   // debug("#BP handling\n");
   // end Q2

   // Q5
    // L'implem à la façon Q2 se termine par un RET 
    // Cela dépile la pile sous forme de "frame" de fonction
    // Alors que la fonction a été appelée à l'arrivée d'une int
    // ce qui n'est pas cohérent...
    // end Q5
    //il faudrait peut etre faire un iret plutot qu'un ret - J

   // Q7
   // uint32_t val;
   // asm volatile ("mov 4(%%ebp), %0":"=r"(val));
    // debug("EIP = %p\n", val);
    // cette valeur est l'adresse dans bp_trigger
    // qui suit l'instruction int3.
   // end Q7

   // Q8
   // asm volatile ("pusha");
   // debug("#BP handling\n");
   // uint32_t eip;
   // asm volatile ("mov 4(%%ebp), %0":"=r"(eip));
   // debug("EIP = %p\n", eip);
    // end Q8

   // Q9: doit se terminer par l'instruction IRET :
   asm volatile ("pusha");
   debug("#BP handling\n");
   uint32_t eip;
   asm volatile ("mov 4(%%ebp), %0":"=r"(eip));
   debug("EIP = %x\n", (unsigned int) eip);
   asm volatile ("popa");
   asm volatile ("leave; iret");
    // end Q9

    // Q11
   // Dev en C rajoute les frames de fonction non désirées...
    // end Q11
}

void bp_trigger() {
    // Q4
    // asm volatile ("int3");
    // end Q4

    // Q10
    asm volatile ("int3");
    debug("after bp triggered\n");
    // end Q10
}

void tp() {
   // Q1
   idt_reg_t idtr;
   //dans intr.h
   get_idtr(idtr);
   debug("IDT @ 0x%x\n", (unsigned int) idtr.addr);
   // end Q1
   // Q3
   //Modif direct ici, le but était de ne pas modifier intr_handler directement - J
   int_desc_t *bp_dsc = &idtr.desc[3];
   bp_dsc->offset_1 = (uint16_t)((uint32_t)bp_handler);
   bp_dsc->offset_2 = (uint16_t)(((uint32_t)bp_handler)>>16);
   debug("bp_handler in intr_handler");
   // end Q3
   // Q4
   bp_trigger();
   // end Q4
}