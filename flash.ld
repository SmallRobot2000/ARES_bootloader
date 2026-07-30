ENTRY(_start)

MEMORY
{
	flash(rx)       : ORIGIN = 0x80000000, LENGTH = 1M
	ram(rwx)	: ORIGIN = 0x90000000, LENGTH = 256M
}

SECTIONS
{

    .text : {
	. = ALIGN(4);
        *(.text*)
    } > flash

    .rodata : {
	. = ALIGN(4);
        *(.rodata*)
    } > flash

    .data : {
	. = ALIGN(4);
	__data_start = .;
        *(.data*)
	__data_end = .;
    } > ram AT > flash

    __data_load = LOADADDR(.data);

    .bss : {
	. = ALIGN(4);
	__bss_start = .;
        *(.bss*)
	*(.sbss*)
	*(COMMON)
	__bss_end = .;
    } > ram

    .stack : {
	. = ALIGN(8);
	. += 16384;
	_sp = . + 16;
    } > ram

    . = ALIGN(4);
    _end = .;
}
