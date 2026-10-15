/* Schrift der selbst gezeichneten Menüleiste (src/menubar.c): Noto Sans, Teilmenge, siehe
 * third_party/noto-sans/SOURCE.txt. Pfad relativ zur Projektwurzel (dort wird gebaut). */
	.section .rodata
	.globl menubar_font
	.balign 4
menubar_font:
	.incbin "third_party/noto-sans/NotoSans-Regular-subset.ttf"
	.byte 0
	.section .note.GNU-stack,"",@progbits
