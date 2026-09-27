; hello: cartridge header and boot stub, served at ROM4 ($FA0000).
; Copyright (C) 2026 Neil Rackett
; SPDX-License-Identifier: GPL-3.0-or-later
;
; At boot TOS runs pre_auto, which copies the rest to RAM and runs it
; there: print the firmware's greeting straight from ROM4, "ping" the
; firmware with a read of ROM3 ($FB1234), wait for its answer and print it.
; Offsets match firmware/hello.h. Build: make cart (needs stcmd).

ROM4		equ $FA0000
HELLO_TEXT	equ ROM4+$1000
HELLO_PONGS	equ ROM4+$1100
HELLO_PONG_TEXT	equ ROM4+$1102
HELLO_PING	equ $FB1234
WAIT_VBLS	equ 100
WORK_AREA	equ (-4096)		; below the screen

	section
	org ROM4

	dc.l $abcdef42			; cartridge magic
	dc.l 0				; CA_NEXT: no further programs
	dc.l $08000000+pre_auto		; CA_INIT: after GEMDOS init, before boot
	dc.l 0				; CA_RUN: none
	dc.w 0				; time
	dc.w 0				; date
	dc.l end_code-pre_auto		; size
	dc.b "HELLO",0
	even

pre_auto:
	move.w #2,-(sp)			; XBIOS Physbase
	trap #14
	addq.l #2,sp
	move.l d0,a2
	lea WORK_AREA(a2),a2
	move.l a2,a3
	lea start_code(pc),a1
	move.w #(end_code-start_code+3)/4-1,d6
.copy:
	move.l (a1)+,(a2)+
	dbf d6,.copy
	jmp (a3)

start_code:
	pea title(pc)
	bsr.s print
	pea HELLO_TEXT			; the greeting the firmware wrote
	bsr.s print
	tst.b HELLO_PING		; a ROM3 read: the ping
	move.w #WAIT_VBLS,d6
.wait:
	tst.w HELLO_PONGS
	bne.s .answered
	move.w #37,-(sp)		; XBIOS Vsync
	trap #14
	addq.l #2,sp
	dbf d6,.wait
	pea no_answer(pc)
	bra.s .done
.answered:
	pea HELLO_PONG_TEXT
.done:
	bsr.s print
	rts

print:					; print(string on the stack)
	move.l 4(sp),-(sp)
	move.w #9,-(sp)			; GEMDOS Cconws
	trap #1
	addq.l #6,sp
	move.l (sp)+,a0			; return address
	addq.l #4,sp			; drop the argument
	jmp (a0)

title:
	dc.b $d,$a,"md-emulator hello example",$d,$a,0
no_answer:
	dc.b "No answer from the firmware.",$d,$a,0
	even
	dc.w $ffff			; non-zero end, so trimming keeps it all
end_code:
