; Parts were taken from https://sites.google.com/site/pinczakko/low-cost-embedded-x86-teaching-tool

	cpu	186
	bits	16

rom_size equ	16  ; length of option rom in 512b increments = 8k

EXTERN _puts
EXTERN _kernel_main
EXTERN _kernel_init

;----------------------------------------------------------
; _main Is a misnomer, but it makes the compiler happy
;----------------------------------------------------------
GLOBAL _main
_main:
	db	0x55
	db	0xAA   ; signature
	
rom_size_p db	rom_size
	jmp	rom_init

	; XXX: Fix this once we have an actual PCI device!
 	Times   0x18-($-$$) db	0	; zero fill in between
	dw	0;PCI_DATA_STRUC	; Pointer to PCI HDR structure (at 18h)

	Times 	0x1A-($-$$) db	0	; zero fill in between
	dw	PnP_Header		; PnP Expansion Header Pointer (at 1Ah)

;----------------------------
; PCI data structure
;----------------------------
;PCI_DATA_STRUC:
;	db	'PCIR'		; PCI Header Sign
;
;	dw	0x9004		; Vendor ID
;	dw	0x8178		; Device ID
;
;	dw	0x00		; VPD
;	dw	0x18		; PCI data struc length (byte)
;	db	0x00		; PCI Data struct Rev
;	db	0x02		; Base class code, 02h == Network Controller
;	db	0x00		; Sub class code = 00h and interface = 00h -->Ethernet Controller
;	db	0x00		; Interface code, see PCI Rev2.2 Spec Appendix D
;	dw	rom_size	; Image length in mul of 512 byte, little endian format
;	dw	0x00		; Rev level
;	db	0x00		; Code type = x86
;	db	0x80		; Last image indicator
;	dw	0x00		; Reserved

;-----------------------------
; PnP ROM Bios Header
;-----------------------------
; https://www.scs.stanford.edu/nyu/04fa/lab/specsbbs101.pdf
; https://stuff.mit.edu/afs/sipb/contrib/doc/specs/protocol/pnp/PNPBIOS.rtf
; See: http://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/devids.txt
PnP_Header:
	db	'$PnP'			; PnP Rom header sign
	db	0x01			; Structure Revision
	db	0x02			; Header structure Length in mul of 16 bytes
	dw	0x00			; Offset to next header (00 if none)
	db	0x00			; Reserved
	db	0x7A			; Checksum
	dd	0x00			; PnP Device ID
	dw	manufacturer_str	; Pointer to manufacturer string
	dw	product_str		; Pointer to product string

	db	0x01,0x80,0x00		; Type codes
	db	0x04			; Device indicator: IPL device
	dw	bcv			; Boot Connection Vector (BCV)
	dw	0x00			; Disconnect Vector, 00h = disabled
	dw	0x00			; Bootstrap Entry Vector (BEV), 00h = disabled
	dw	0x00			; Reserved
	dw	0x00			; Static resource Information vector (0000h if unused)

;----------------------------------------------------------
; Identifier strings
;----------------------------------------------------------
manufacturer_str	db	'Tom Wimmenhove electronics',00h
product_str		db	'UEFI ROM Loader',00h

;----------------------------------------------------------
; ROM initializatin vector
;----------------------------------------------------------
rom_init:
	push	ds
	push	es
	pushf
	pusha
;	cli

	mov	 ax, cs
	mov	 ds, ax
	mov	 es, ax
	call	_kernel_init

	popa
	popf
	pop	es
	pop	ds

	mov	bx, rom_size_p
;	mov	al, 0
;	mov	[bx], al	; Clear 3rd byte (image size)
	xor	ax, ax
	mov	[bx], ax	; Clear 3rd byte (image size)

	mov	ax, 0x0120	; Tell BIOS we hook INT 13h (bit 8) and are an IPL
				; devive (bit 5:4).
				; See AX bits in section 3.3 of PNPBIOS.rtf

	retf			; return far to system BIOS

;----------------------------------------------------------
; Boot Connection Vector entry
;----------------------------------------------------------
bcv:
	push	ds
	push	es
	pushf
	pusha
;	cli

	mov	ax,cs      
	mov	ds,ax
	mov	es,ax
	call	_kernel_main
	  
	popa
	popf
	pop	es
	pop	ds
	retf

;----------------------------------------------------------
; Get code segment
;----------------------------------------------------------
GLOBAL _getcodeseg
_getcodeseg:
	mov	ax, cs
	ret

;----------------------------------------------------------
; Get stack segment
;----------------------------------------------------------
GLOBAL _getstackseg
_getstackseg:
	mov	ax, ss
	ret

;----------------------------------------------------------
; Put a char (first arg, on stack) onto the screen
;----------------------------------------------------------
GLOBAL _putchar
_putchar:
	mov	ah, 0x0e
	mov	bx, sp
	mov	al, ss:[bx+2]
	int	0x10
	ret

;----------------------------------------------------------
; Simple segment:offset memcopy
;----------------------------------------------------------
GLOBAL _copy
_copy:
	push	bp
	mov	bp, sp
	push	ds
	push	si
	push	es
	push	di
	push	cx

	mov	cx, [ bp + 12 ]    ; Set the loop counter to N
	mov	di, [ bp + 10 ]    ; Load the destination offset into DI
	mov	ax, [ bp + 8 ]
	mov	es, ax             ; Load the destination segment into ES
	mov	si, [ bp + 6 ]     ; Load the source offset into SI
	mov	ax, [ bp + 4 ]
	mov	ds, ax             ; Load the source segment into DS

.copy_loop:
	mov	al, [ds:si]
	mov	[es:di], al
	inc	si
	inc	di
	dec	cx
	jnz	.copy_loop

	pop	cx
	pop	di
	pop	es
	pop	si
	pop	ds
	pop	bp

	ret

EXTERN _ser_put
_ser_put:
	push	bp
	mov	bp, sp
	push	ax
	mov	dx, 0
	mov	al, [ bp + 4 ]
	mov	ah, 1
	int	0x14
	pop	ax
	pop	bp
	ret

EXTERN _ser_get
_ser_get:
	mov	dx, 0
	mov	ah, 2
	int	0x14
	test    ah, 0x80
	jnz	_ser_get
	ret

;----------------------------------------------------------
; Read byte from I/O port
;----------------------------------------------------------
EXTERN _inb
_inb:
	push	dx
	mov	dx, 0x3F8
	in	al, dx
	pop	dx
	ret

	push	bp
	mov	bp, sp
	push	dx
	mov	ah, 0
	mov	dx, [ bp + 4 ]
	in	al, dx
	pop	dx
	pop	bp
	ret

;----------------------------------------------------------
; Write byte to I/O port
;----------------------------------------------------------
EXTERN _outb
_outb:
	push	bp
	mov	bp, sp
	push	dx
	push	ax
	mov	ah, 0
	mov	dx, [ bp + 4 ]
	mov	ax, [ bp + 6 ]
	out	dx, al
	pop	ax
	pop	dx
	pop	bp
	ret

;----------------------------------------------------------
; Call the 'old' INT13h
;----------------------------------------------------------
EXTERN _call_old_int13
_call_old_int13:
	; push return pointer and flags for old interrupt's iret
	pushf
	push	cs
	push	.ret

	; write the return address onto the stack
	sub	sp, 4
	push	bp
	mov	bp, sp
	push	bx
	push	ds
	mov	bx, cs
	mov	ds, bx
	mov	bx, word [old_int13 + 2]
	mov	word [bp + 2 + 2], bx
	mov	bx, word [old_int13]
	mov	word [bp + 2], bx
	pop	ds
	pop	bx
	pop	bp

	; Call old interrupt
	retf

.ret:
	ret

EXTERN _printhex
GLOBAL _test
_test:
	push	bp
	mov	bp, sp
	push	ds
	mov	ax, [ bp + 4 ]
	mov	ds, ax
	mov	si, [ bp + 6 ]
	mov	ah, 0x48
	mov	dl, 0x81
	call	_call_old_int13
	pop	ds
	pop	bp

	ret
	mov	ah, 0x41
	mov	bx, 0x55aa
	call	_call_old_int13

	push	ax
	call	_printhex
	pop	ax

	push	bx
	call	_printhex
	pop	bx

	push	cx
	call	_printhex
	pop	cx

	ret

;----------------------------------------------------------
; Read data hack
;----------------------------------------------------------
EXTERN _read
_read:
	push	bp
	mov	bp, sp
	push	ds

	mov	bx, [ bp + 12 ]
	mov	ax, [ bp + 10 ]
	mov	es, ax
	mov	dx, [ bp + 8 ]
	mov	cx, [ bp + 6 ]
	mov	ax, [ bp + 16 ]
	mov	si, ax
	mov	ax, [ bp + 14 ]
	mov	ds, ax
	mov	ax, [ bp + 4 ]

	call	_call_old_int13

	pop	ds
	pop	bp
	ret

;----------------------------------------------------------
; Our chained INT13h interrupt vector
;----------------------------------------------------------
EXTERN _int13handler
ISR_int13:
	; write the return address onto the stack
	sub	sp, 4
	push	bp
	mov	bp, sp
	push	bx
	push	ds
	mov	bx, cs
	mov	ds, bx
	mov	bx, word [old_int13 + 2]
	mov	word [bp + 2 + 2], bx
	mov	bx, word [old_int13]
	mov	word [bp + 2], bx
	pop	ds
	pop	bx
	pop	bp

	push	ds
	push	es

        ; Set es and ds equal to cs
        push    cs
        pop     es
        sub     sp, 2
        pop     ds

	push	ax
	push	cx
	push	dx
	push	bx
	push	bp
	push	si
	push	di
	pushf
	call	_int13handler
	popf
	pop	di
	pop	si
	pop	bp
	pop	bx
	pop	dx
	pop	cx
	pop	ax

	pop	es
	pop	ds

	jc	.chain
	add	sp, 4  ; Discard far return address
	iret

.chain:
	retf ; Jump to old_int13

;----------------------------------------------------------
; Hook our own INT13h ISR
;----------------------------------------------------------
GLOBAL _hookint13
_hookint13:
	push	es
	push	ds

	mov	ax,0
	mov	es,ax

	cli
	mov	ax, es:[0x13 * 4]
	mov	[old_int13], ax
	mov	ax, es:[0x13 * 4 + 2]
	mov	[old_int13 + 2], ax

	mov	ax, ISR_int13
	mov	es:[0x13 * 4], ax
	mov	es:[0x13 * 4 + 2], cs
	sti

	pop	ds
	pop	es

	ret

;----------------------------------------------------------
; Old INT13h vector is stored here
;----------------------------------------------------------
old_int13:
	dw	0, 0
