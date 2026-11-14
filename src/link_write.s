@ void link_write(u16 value)
@
@ Writes one protocol value to the cart (0x08000000), then busy-waits so the cart's 8 MHz ATtiny
@ can read it.
@
@ Bus-faithful copy of the stock v1.1a ROM sequence (0x0800064E..0x0800066C): halfword store, then a
@ stack-byte counter from 0 to 50, executed in Thumb from ROM. The CPLD clears its "data ready" line
@ on the next sequential ROM read, so the ROM-fetch pattern after the write is part of the protocol.
@ Don't "optimise" this routine, move it to IWRAM or replace it with C.
@ Note 0x08000000 is built from immediates (movs+lsls) like the stock code, so there's no
@ literal-pool ROM read between the address setup and the store.

    .syntax unified
    .thumb
    .section .text.link_write, "ax", %progbits
    .global link_write
    .type   link_write, %function
    .thumb_func
link_write:
    push    {r4, lr}
    sub     sp, #8
    movs    r3, #0x80
    lsls    r3, r3, #20         @ r3 = 0x08000000
    adds    r4, r0, #0
    strh    r4, [r3]            @ the write the CPLD captures
    mov     r3, sp
    adds    r2, r3, #6
    movs    r3, #0
    strb    r3, [r2]            @ counter = 0
    ldrb    r3, [r2]
    cmp     r3, #0x31
    bhi     2f
1:  ldrb    r3, [r2]            @ while (counter <= 49) counter++   (volatile u8, as compiled by stock)
    adds    r3, #1
    lsls    r3, r3, #24
    lsrs    r3, r3, #24
    strb    r3, [r2]
    ldrb    r3, [r2]
    cmp     r3, #0x31
    bls     1b
2:  add     sp, #8
    pop     {r4}
    pop     {r0}
    bx      r0
    .size   link_write, . - link_write
