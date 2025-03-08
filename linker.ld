SECTIONS
{
    . = 0x7c60;
    .text : {*(.text)}
    .data : {*(.data)}
    .bss : {*(.bss)}
}
