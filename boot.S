	// simple GBA fastmem bootloader
	// GreaseMonkey, 2014-2015 - Public Domain
	.arm
	.section .text
	.global run_exe
	.extern init
	b _start_asm
	.incbin "nintendo.bin"
	.ascii "multibooter "
	.ascii "AAAE00"
	.byte 0x96, 0x00, 0x00
	.byte 0,0,0,0,0,0,0
	.byte 0x00
	.byte 0x13 // compliment check. here's a compliment: NINTENDO SUCKS.
	.hword 0x00
	.word s3m_beg
	b run_exe
_start_asm:
	ldr r0, =init_beg
	ldr r1, =init_end
run_exe:
	/* Copy to WRAM */
	ldr r2, =0x03000000
	mov r3, r2
	_lp_copy_prog:
		ldmia r0!, {r4-r11}
		stmia r2!, {r4-r11}
		cmp r1, r0
		bhi _lp_copy_prog

	/* Jump */
	bx r3
	.pool

	.align 5
init_beg:
	.incbin "player_gba.bin"
	.align 5
init_end:

s3m_beg:
	// Pick a module here
	.incbin "pod.s3m"
	.align 5
s3m_end:

