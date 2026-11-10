OUTPUT_FORMAT("elf32-littlearm")
ENTRY(SmdkStart)
SECTIONS
{
    . = 0x40010000;
    .text : { KEEP(*(.vectors)) *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss (NOLOAD) : {
        . = ALIGN(16384);
        SmdkBssStart = .;
        *(.bss*) *(COMMON)
        . = ALIGN(16);
        . += 4096;
        SmdkAbortStack = .;
        . += 4096;
        SmdkIrqStack = .;
        . += 16384;
        SmdkStack = .;
        SmdkBssEnd = .;
    }
    /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment*) }
    ASSERT(SmdkBssEnd <= 0x40100000, "SMDKC210 test exceeds its identity mapping")
}
