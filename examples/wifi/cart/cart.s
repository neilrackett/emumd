; wifi: cartridge header and boot stub, served at ROM4 ($FA0000).
; Copyright (C) 2026 Neil Rackett
; SPDX-License-Identifier: GPL-3.0-or-later
;
; At boot TOS runs pre_auto, which copies the rest to RAM and runs it
; there: wait for the firmware to join the network and print how that
; went, then ask it for a web page with a read of ROM3 ($FB1234), wait
; for the answer and print it, and leave it there for a few seconds.
; Offsets match firmware/wifi_demo.h.
; Build: make cart (needs stcmd).

ROM4		equ $FA0000
DEMO_STATUS	equ ROM4+$1000
DEMO_STATUS_TEXT equ ROM4+$1002
DEMO_ANSWERS	equ ROM4+$1100
DEMO_ANSWER_TEXT equ ROM4+$1102
DEMO_FETCH	equ $FB1234
WAIT_VBLS	equ 1500		; 30 seconds, for each
PAUSE_VBLS	equ 250			; 5 seconds, before TOS goes on
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
	dc.b "WIFI",0
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
	bsr print
	pea joining(pc)
	bsr print
	move.w #WAIT_VBLS,d6
.join:
	tst.w DEMO_STATUS
	bne.s .joined
	bsr vsync
	dbf d6,.join
	pea no_answer(pc)
	bra.s .last
.joined:
	pea DEMO_STATUS_TEXT		; how joining went
	bsr print
	cmp.w #1,DEMO_STATUS
	bne.s .done
	pea fetching(pc)
	bsr print
	tst.b DEMO_FETCH		; a ROM3 read: fetch the page
	move.w #WAIT_VBLS,d6
.wait:
	tst.w DEMO_ANSWERS
	bne.s .answered
	bsr vsync
	dbf d6,.wait
	pea no_answer(pc)
	bra.s .last
.answered:
	pea DEMO_ANSWER_TEXT
.last:
	bsr print
.done:
	move.w #PAUSE_VBLS,d6		; time to read it
.pause:
	bsr vsync
	dbf d6,.pause
	rts

vsync:
	move.w #37,-(sp)		; XBIOS Vsync
	trap #14
	addq.l #2,sp
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
	dc.b $d,$a,"EmuMD Wi-Fi example",$d,$a,0
joining:
	dc.b "Joining the network...",$d,$a,0
fetching:
	dc.b "Fetching a page...",$d,$a,0
no_answer:
	dc.b "No answer from the firmware.",$d,$a,0
	even
	dc.w $ffff			; non-zero end, so trimming keeps it all
end_code:
