;/*******************************************************************************
; *  Based on CSR uEnergy SDK 2.2.2
; *
; * FILE
; *    pio_ctrlrcode.asm
; *
; *  DESCRIPTION
; *    This file contains the low level 8051 assembly code for the keyboard
; *    scanning.
; *
; ******************************************************************************/

.equ     locat, 0x0000

.equ WAKEUP       ,0x9E ; Interrupt/wakeup main XAP system with transition from 1 to 0.
.equ VALID_BASE   ,0x40 ; Pointer to valid dual port RAM with XAP
.equ KEYPTR_BASE  ,0x42 ; Pointer to shared dual port 0 RAM with XAP
.equ KEYPTR_BASE2 ,0x54 ; Pointer to shared dual port 1 RAM with XAP


START:
; set the stack up
      mov SP, #30H
;
; leave outputs as open collector so they don't short together
;
      mov    P0,#11111111B   ; set to all inputs and high outputs
      mov    P1,#11111111B   ; set to all inputs and high outputs
      mov    P2,#11111111B   ; set to all inputs and high outputs
      mov    P3,#11111111B   ; set to all inputs and high outputs
;
; use registers to speed up scan cycles
;
      mov      R0, #P3                  ; setup a register to point to PIO24 to 31
      mov      R3, #0                   ; Button state variable
      mov      VALID_BASE+1, #0         ; SET BUFFER 0 VALID
      
; There are 2 copies of the scanned keys in the shared RAM between the
; PIO Controller and the XAP. They flip between one buffer and the other so that
; the first set of scan keys are not over written by the scanning of the
; 2nd set of keys. The first word in the buffer defines the which buffer
; contains the latest scanned keys.

SCANLOOP:
; BUFFER 0 SCAN
; drive each output PIO low and then read in all 8 RX lines
;
;*******************************************************************************
;set output 0 low
      clr      P0.0                           ; 2 cycles
      mov      KEYPTR_BASE+00, @R0            ; 2 cycles Read in port
      setb     P0.0

;*******************************************************************************
;set output 1 low
      clr      P0.3                           ; 2 cycles
      mov      KEYPTR_BASE+01, @R0            ; 2 cycles Read in port 
      setb     P0.3

;*******************************************************************************
;set output 2 low
      clr      P0.4                           ; 2 cycles
      mov      KEYPTR_BASE+02, @R0            ; 2 cycles Read in port 
      setb     P0.4

;*******************************************************************************
;set output 3 low
      clr      P1.1                           ; 2 cycles
      mov      KEYPTR_BASE+03, @R0            ; 2 cycles Read in port 
      setb     P1.1

;*******************************************************************************
;set output 4 low
      clr      P1.2                           ; 2 cycles
      mov      KEYPTR_BASE+04, @R0            ; 2 cycles Read in port 
      setb     P1.2

;*******************************************************************************
;set output 5 low
      clr      P1.3                           ; 2 cycles
      mov      KEYPTR_BASE+05, @R0            ; 2 cycles Read in port 
      setb     P1.3

;*******************************************************************************
;set output 6 low
      clr      P1.4                           ; 2 cycles
      mov      KEYPTR_BASE+06, @R0            ; 2 cycles Read in port 
      setb     P1.4

;*******************************************************************************
;set output 7 low
      clr      P1.5                           ; 2 cycles
      mov      KEYPTR_BASE+07, @R0            ; 2 cycles Read in port 
      setb     P1.5

;*******************************************************************************
;set output 8 low
      clr      P1.6                           ; 2 cycles
      mov      KEYPTR_BASE+08, @R0            ; 2 cycles Read in port 
      setb     P1.6

;*******************************************************************************
;set output 9 low
      clr      P1.7                           ; 2 cycles
      mov      KEYPTR_BASE+09, @R0            ; 2 cycles Read in port 
      setb     P1.7

;*******************************************************************************
;set output 10 low
      clr      P2.0                           ; 2 cycles
      mov      KEYPTR_BASE+10, @R0            ; 2 cycles Read in port 
      setb     P2.0

;*******************************************************************************
;set output 11 low
      clr      P2.1
      mov      KEYPTR_BASE+11, @R0            ; 2 cycles Read in port 
      setb     P2.1

;*******************************************************************************
;set output 12 low
      clr      P2.2
      mov      KEYPTR_BASE+12, @R0            ; 2 cycles Read in port 
      setb     P2.2

;*******************************************************************************
;set output 13 low
      clr      P2.3
      mov      KEYPTR_BASE+13, @R0            ; 2 cycles Read in port 
      setb     P2.3

;*******************************************************************************
;set output 14 low
      clr      P2.4
      mov      KEYPTR_BASE+14, @R0            ; 2 cycles Read in port 
      setb     P2.4

;*******************************************************************************
;set output 15 low
      clr      P2.5
      mov      KEYPTR_BASE+15, @R0            ; 2 cycles Read in port 
      setb     P2.5

;*******************************************************************************
;set output 16 low
      clr      P2.6
      mov      KEYPTR_BASE+16, @R0            ; 2 cycles Read in port 
      setb     P2.6

;*******************************************************************************
;set output 17 low
      clr      P2.7
      mov      KEYPTR_BASE+17, @R0            ; 2 cycles Read in port 
      setb     P2.7

;*******************************************************************************
; look for a button being pressed low (NAND)
;Go through table ANDing together entries for port 1
       mov     A, KEYPTR_BASE+00
       anl     A, KEYPTR_BASE+01
       anl     A, KEYPTR_BASE+02
       anl     A, KEYPTR_BASE+03
       anl     A, KEYPTR_BASE+04
       anl     A, KEYPTR_BASE+05
       anl     A, KEYPTR_BASE+06
       anl     A, KEYPTR_BASE+07
       anl     A, KEYPTR_BASE+08
       anl     A, KEYPTR_BASE+09
       anl     A, KEYPTR_BASE+10
       anl     A, KEYPTR_BASE+11
       anl     A, KEYPTR_BASE+12
       anl     A, KEYPTR_BASE+13
       anl     A, KEYPTR_BASE+14
       anl     A, KEYPTR_BASE+15
       anl     A, KEYPTR_BASE+16
       anl     A, KEYPTR_BASE+17
       cpl     A                        ; invert from 0's to 1's
       mov     R2, A                    ; save  port 1 bits
       
       orl     A, R3                    ; OR in the last button state to check for key up

      jz    SKIPWAKE

;Wake up XAP with an interrupt on button press or button release
      mov      VALID_BASE, #0  ; SET BUFFER 0 VALID
      mov      A, #1
      mov      WAKEUP, A
      mov      A, #0
      mov      WAKEUP, A

SKIPWAKE:
; Update the previous value
          mov      A, R2
      mov      R3, A
;
;
; BUFFER 1 SCAN
; drive each output PIO low and then read in all 8 RX lines
;
;*******************************************************************************
;set output 0 low
      clr      P0.0                            ; 2 cycles
      mov      KEYPTR_BASE2+00, @R0            ; 2 cycles Read in port
      setb     P0.0

;*******************************************************************************
;set output 1 low
      clr      P0.3                            ; 2 cycles
      mov      KEYPTR_BASE2+01, @R0            ; 2 cycles Read in port 
      setb     P0.3

;*******************************************************************************
;set output 2 low
      clr      P0.4                            ; 2 cycles
      mov      KEYPTR_BASE2+02, @R0            ; 2 cycles Read in port 
      setb     P0.4

;*******************************************************************************
;set output 3 low
      clr      P1.1                            ; 2 cycles
      mov      KEYPTR_BASE2+03, @R0            ; 2 cycles Read in port 
      setb     P1.1

;*******************************************************************************
;set output 4 low
      clr      P1.2                            ; 2 cycles
      mov      KEYPTR_BASE2+04, @R0            ; 2 cycles Read in port 
      setb     P1.2

;*******************************************************************************
;set output 5 low
      clr      P1.3                            ; 2 cycles
      mov      KEYPTR_BASE2+05, @R0            ; 2 cycles Read in port 
      setb     P1.3

;*******************************************************************************
;set output 6 low
      clr      P1.4                            ; 2 cycles
      mov      KEYPTR_BASE2+06, @R0            ; 2 cycles Read in port 
      setb     P1.4

;*******************************************************************************
;set output 7 low
      clr      P1.5                            ; 2 cycles
      mov      KEYPTR_BASE2+07, @R0            ; 2 cycles Read in port 
      setb     P1.5

;*******************************************************************************
;set output 8 low
      clr      P1.6                            ; 2 cycles
      mov      KEYPTR_BASE2+08, @R0            ; 2 cycles Read in port 
      setb     P1.6

;*******************************************************************************
;set output 9 low
      clr      P1.7                            ; 2 cycles
      mov      KEYPTR_BASE2+09, @R0            ; 2 cycles Read in port 
      setb     P1.7

;*******************************************************************************
;set output 10 low
      clr      P2.0                            ; 2 cycles
      mov      KEYPTR_BASE2+10, @R0            ; 2 cycles Read in port 
      setb     P2.0

;*******************************************************************************
;set output 11 low
      clr      P2.1
      mov      KEYPTR_BASE2+11, @R0            ; 2 cycles Read in port 
      setb     P2.1

;*******************************************************************************
;set output 12 low
      clr      P2.2
      mov      KEYPTR_BASE2+12, @R0            ; 2 cycles Read in port 
      setb     P2.2

;*******************************************************************************
;set output 13 low
      clr      P2.3
      mov      KEYPTR_BASE2+13, @R0            ; 2 cycles Read in port 
      setb     P2.3

;*******************************************************************************
;set output 14 low
      clr      P2.4
      mov      KEYPTR_BASE2+14, @R0            ; 2 cycles Read in port 
      setb     P2.4

;*******************************************************************************
;set output 15 low
      clr      P2.5
      mov      KEYPTR_BASE2+15, @R0            ; 2 cycles Read in port 
      setb     P2.5

;*******************************************************************************
;set output 16 low
      clr      P2.6
      mov      KEYPTR_BASE2+16, @R0            ; 2 cycles Read in port 
      setb     P2.6

;*******************************************************************************
;set output 17 low
      clr      P2.7
      mov      KEYPTR_BASE2+17, @R0            ; 2 cycles Read in port 
      setb     P2.7

;*******************************************************************************
; look for a button being pressed low (NAND)
;Go through table ANDing together entries for port 1
       mov     A, KEYPTR_BASE2+00
       anl     A, KEYPTR_BASE2+01
       anl     A, KEYPTR_BASE2+02
       anl     A, KEYPTR_BASE2+03
       anl     A, KEYPTR_BASE2+04
       anl     A, KEYPTR_BASE2+05
       anl     A, KEYPTR_BASE2+06
       anl     A, KEYPTR_BASE2+07
       anl     A, KEYPTR_BASE2+08
       anl     A, KEYPTR_BASE2+09
       anl     A, KEYPTR_BASE2+10
       anl     A, KEYPTR_BASE2+11
       anl     A, KEYPTR_BASE2+12
       anl     A, KEYPTR_BASE2+13
       anl     A, KEYPTR_BASE2+14
       anl     A, KEYPTR_BASE2+15
       anl     A, KEYPTR_BASE2+16
       anl     A, KEYPTR_BASE2+17
       cpl     A                        ; invert from 0's to 1's
       mov     R2, A                    ; save  port 1 bits
       
       orl     A, R3                    ; OR in the last button state to check for key up

       jz    SKIPWAKE2
      
;Wake up XAP with an interrupt on button press or button release
       mov      VALID_BASE, #1  ; SET BUFFER 1 VALID
       mov      A, #1
       mov      WAKEUP, A
       mov      A, #0
       mov      WAKEUP, A

SKIPWAKE2:
; Update the previous value
       mov A, R2
       mov      R3, A
       ljmp     SCANLOOP  ; go back to scan buffer 0
;;     END

