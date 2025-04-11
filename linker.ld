SECTIONS
{
    . = 0x7e00;
    .text : {*(.text)}
    .data : {*(.data)}
    .bss : {
        PROVIDE(__bss_start = .); 
        *(.bss)
        PROVIDE(__bss_end = .); 
    }
    PROVIDE(__kernel_end = .);
}
