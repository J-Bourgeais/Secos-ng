/* GPLv2 (c) Airbus */
OUTPUT_FORMAT("elf32-i386","elf32-i386","elf32-i386");
OUTPUT_ARCH("i386")

ENTRY(entry)

PHDRS
{
   phboot  PT_LOAD FLAGS (7);
   phstack PT_LOAD FLAGS (6);
   phsetup PT_LOAD FLAGS (7);
}

SECTIONS
{
   . = 0x300000;
   .mbh      : { KEEP(*(.mbh)) . = ALIGN(4);     } : phboot
   .stack    : { KEEP(*(.stack))                 } : phstack

   __kernel_start__ = .;

   .idt_jmp  : { KEEP(*(.idt_jmp))               } : phsetup
   .text     : { *(.text)                        } : phsetup
   .rodata   : { *(.rodata)                      } : phsetup
   .data     : { *(.data)                        } : phsetup
   .bss      : { *(.bss COMMON)                  } : phsetup
   /DISCARD/ : { *(.note* .indent .comment)      } : phsetup

   __kernel_end__ = .;

   /* Alignement des pages pour faciliter le mapping */

   /* Page de code utilisateurs (code user 1 & code user 2 dans la meme page de 4 MB) */

   . = ALIGN(0x400000);
   __user_start__ = .;

   .user : {
      KEEP(*(.user))
      KEEP(*(.user.*))
   } : phsetup

   __user_end__ = .;


   /* Pile user 1 de 0x0080 0000 MB a 0x00800FFF */

   . = 0x00800000;

   __user1_stack_base__ = .;
   .user1_stack (NOLOAD) : { KEEP(*(.user1_stack)) } : phsetup
   __user1_stack_end__ = .;


   /* Pile user 2 de 0x00801000 MB a 0x00801FFF */

   . = 0x00801000;

   __user2_stack_base__ = .;
   .user2_stack (NOLOAD) : { KEEP(*(.user2_stack)) } : phsetup
   __user2_stack_end__ = .;

   /* Page partagee de 0x00802000 MB a 0x00802FFF */

   . = 0x00802000;
   __shared_base__ = .;
   .shared (NOLOAD) : { KEEP(*(.shared)) } : phsetup
   __shared_end__ = .;


   /* 2 piles noyau de 4KB dans le meme bloc de 4 MB (meme entree PDE) + la pile noyeau ring 0 */
   
   . = 0x00c00000;
   __kernel_stack_user1_base__ = .;
   .kernel_stack_user1 (NOLOAD) : { KEEP(*(.kernel_stack_user1)) } : phsetup
   __kernel_stack_user1_end__ = .;

   . = 0x00c01000;
   __kernel_stack_user2_base__ = .;
   .kernel_stack_user2 (NOLOAD) : { KEEP(*(.kernel_stack_user2)) } : phsetup
   __kernel_stack_user2_end__ = .;

   . = 0x00c02000;
   __kernel_stack_base__ = .;
   .kernel_stack (NOLOAD) : { KEEP(*(.kernel_stack)) } : phsetup
   __kernel_stack_end__ = .;

}