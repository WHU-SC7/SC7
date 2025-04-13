OUTPUT_ARCH( "loongarch" )
ENTRY( init_main )

SECTIONS
{
    . = 0x00000000;

    .text : {
    *(.text.user.init .text.user.init.*)
    *(.text.syscall_function .text.syscall_function.*)
    *(.text .text.*)
}

  .rodata : {
    . = ALIGN(16);
    *(.srodata .srodata.*) /* do not need to distinguish this from .rodata */
    . = ALIGN(16);
    *(.rodata .rodata.*)
  }

  .data : {
    . = ALIGN(16);
    *(.sdata .sdata.*) /* do not need to distinguish this from .data */
    . = ALIGN(16);
    *(.data .data.*)
  }

  .bss : {
    . = ALIGN(16);
    *(.sbss .sbss.*) /* do not need to distinguish this from .bss */
    . = ALIGN(16);
    *(.bss .bss.*)
  }
}