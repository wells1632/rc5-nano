; Copyright distributed.net 1997-2003 - All Rights Reserved
; For use in distributed.net projects only.
; Any other distribution or use of this source violates copyright.
;
; Author: Ianos Gnatiuc <ssianky@hotmail.com>
; based on r72-dg2 and r72ansi2 cores
; $Id: r72-ss2.asm,v 1.4 2009/01/05 22:45:21 mfeiri Exp $

%ifdef __OMF__ ; Borland and Watcom compilers/linkers
[SECTION _TEXT FLAT USE32 align=16 CLASS=CODE]
%else
[SECTION .text]
%endif

[GLOBAL _rc5_72_unit_func_ss_2]
[GLOBAL rc5_72_unit_func_ss_2]

;------------------------------------------------

%define RESULT_NOTHING  1
%define RESULT_FOUND    2
%define PIPES           2

%define P         0xB7E15163
%define Q         0x9E3779B9
%define S_not(N)  ((P+Q*(N)) & 0xFFFFFFFF)
%define S0_ROL3   0xBF0A8B1D

;------------------------------------------------

%define L2(N)       [esp +   0 + (N)*4] ;  12
%define L1(N)       [esp +  12 + (N)*4] ;  12
%define S1(N)       [esp +  24 + (N)*4] ; 104
%define work_iter    esp +   0 + 128    ;   4  +128
%define work_L0hi    esp +   4 + 128    ;   4
%define work_L0mid   esp +   8 + 128    ;   4
%define work_L0lo    esp +  12 + 128    ;   4
%define save_ebx     esp +  16 + 128    ;   4
%define save_esi     esp +  20 + 128    ;   4
%define save_edi     esp +  24 + 128    ;   4
%define save_ebp     esp +  28 + 128    ;   4
%define work_Clo     ebp -  32          ;   4
%define work_Chi     ebp -  28          ;   4
%define work_Plo     ebp -  24          ;   4
%define work_Phi     ebp -  20          ;   4
%define S1_ROL3      ebp -  16          ;   4
%define S2_ROL3      ebp -  12          ;   4
%define L0_ROL       ebp -   8          ;   4
%define L1_ROL       ebp -   4          ;   4
%define S2(N)       [ebp +   0 + (N)*4] ; 104  +(128+64)
%define S2_L1        ebp + 104          ;   4

%define work_size 300

%define RC5_72UnitWork  esp + work_size + 4
%define iterations      esp + work_size + 8

%define RC5_72UnitWork_plainhi  eax +  0
%define RC5_72UnitWork_plainlo  eax +  4
%define RC5_72UnitWork_cipherhi eax +  8
%define RC5_72UnitWork_cipherlo eax + 12
%define RC5_72UnitWork_L0hi     eax + 16
%define RC5_72UnitWork_L0mid    eax + 20
%define RC5_72UnitWork_L0lo     eax + 24
%define RC5_72UnitWork_CMCcount eax + 28
%define RC5_72UnitWork_CMChi    eax + 32
%define RC5_72UnitWork_CMCmid   eax + 36
%define RC5_72UnitWork_CMClo    eax + 40

;------------------------------------------------

%macro k7nop 1
    %if %1>3
        %error k7nop max 3
    %endif
    %if %1>2
        rep
    %endif
    %if %1>1
        rep
    %endif
    nop
%endmacro

; NASM needs help to generate optimized nop padding
%ifdef __NASM_VER__
%macro k7align 1
    %assign unal ($-startseg)&(%1-1)
    %if unal
        %assign fill %1 - unal
        %if fill<=9
            %rep 3
                %if fill>3
                    k7nop 3
                    %assign fill fill-3
                %else
                    k7nop fill
                    %exitrep
                %endif
            %endrep
        %else
            jmp short %%alend
            align %1
        %endif
    %endif
    %%alend:
%endmacro
%else
    %define k7align align
%endif

;------------------------------------------------

%macro KEYSETUP_BLOCK_CONSTANTS 2
;---
    add  esi, eax
    mov  ecx, esi
    lea  ebx, [ebx + edi + S_not(%1)]
    add  esi, L1(%2)

    rol  esi, cl
    rol  ebx, 3

    mov  L1(%2), esi
    mov  S2(%1), ebx
;---
    add  edi, ebx

    mov  ecx, edi
    lea  eax, [eax + esi + S_not(%1+1)]
    add  edi, L2(%2)

    rol  edi, cl
    rol  eax, 3

    mov  L2(%2), edi
    mov  S1(%1+1), eax
;---
%endmacro

;------------------------------------------------

%macro KEYSETUP_BLOCK 2
    %if %1==25
        %assign next 0
    %else
        %assign next %1 + 1
    %endif
;---
    add  esi, eax
    add  ebx, edi
    add  eax, S1(next)

    mov  ecx, esi
    add  esi, L1(%2)

    rol  esi, cl
    rol  ebx, 3

    mov  L1(%2), esi
    mov  S2(%1), ebx
;---
    add  edi, ebx
    add  eax, esi
    add  ebx, S2(next)

    mov  ecx, edi
    add  edi, L2(%2)

    rol  edi, cl
    rol  eax, 3

    mov  L2(%2), edi
    mov  S1(next), eax
;---
%endmacro

;------------------------------------------------

%macro ENCRYPTION_BLOCK 1
    mov  ecx, esi
    xor  eax, esi
    xor  ebx, edi

    rol  eax, cl
    mov  ecx, edi
    add  eax, S1(2*%1)

    rol  ebx, cl
    add  ebx, S2(2*%1)


    xor  esi, eax
    mov  ecx, eax
    xor  edi, ebx

    rol  esi, cl
    mov  ecx, ebx
    add  esi, S1(2*%1 + 1)

    rol  edi, cl
    add  edi, S2(2*%1 + 1)
%endmacro

;------------------------------------------------

align 16
startseg:
rc5_72_unit_func_ss_2:
_rc5_72_unit_func_ss_2:

    sub  esp, work_size

    mov  [save_ebp], ebp
    mov  [save_edi], edi
    mov  [save_esi], esi
    mov  [save_ebx], ebx
    lea  ebp, [esp+128+64]

    mov  eax, [RC5_72UnitWork]
    mov  ecx, [iterations]

    mov  ebx, [RC5_72UnitWork_plainhi]
    mov  edx, [RC5_72UnitWork_plainlo]
    mov  esi, [RC5_72UnitWork_cipherhi]
    mov  edi, [RC5_72UnitWork_cipherlo]
    mov  ecx, [ecx]

    mov  [work_Phi],  ebx
    mov  [work_Plo],  edx
    mov  [work_Chi],  esi
    mov  [work_Clo],  edi
    mov  [work_iter], ecx

    mov  ebx, [RC5_72UnitWork_L0lo]
    mov  esi, [RC5_72UnitWork_L0mid]
    mov  edx, [RC5_72UnitWork_L0hi]

    bswap ebx
    bswap esi

    mov  [work_L0lo],  ebx
    mov  [work_L0mid], esi


LOOP_LO_SS_2:

    ; L[0] = rc5_72unitwork->L0.lo;
    ; S[0] = S0_ROL3
    ; L[0] = ROTL(L[0] + S[0], S[0]);

    mov  eax, [work_L0lo]
    bswap eax

    add  eax, S0_ROL3
    rol  eax, 0x1D

    mov  [L0_ROL], eax


    k7align 16
LOOP_MID_SS_2:

    ; L[1] = rc5_72unitwork->L0.mid;
    ; S[1] = ROTL3(S_not(1) + S[0] + L[0]);
    ; L[1] = ROTL(L[1] + S[1] + L[0], S[1] + L[0]);

    mov  eax, [L0_ROL]
    mov  ebx, [work_L0mid]
    mov  ecx, eax
    bswap ebx

    add  eax, (S_not(1) + S0_ROL3) & 0xFFFFFFFF
    rol  eax, 3

    add  ecx, eax
    mov  [S1_ROL3], eax

    add  ebx, ecx
    rol  ebx, cl
    mov  [L1_ROL], ebx

    ; S[2] = ROTL3(S_not(2) + S[1] + L[1])

    lea  eax, [eax + ebx + S_not(2)]
    rol  eax, 3

    ; ECX = S[2] + L[1]

    add  ebx, eax

    mov  [S2_ROL3], eax
    mov  [S2_L1], ebx

    k7align 16
LOOP_HI_SS_2:

    ; S[0] = S0_ROL3
    ; S[1] = S1_ROL3
    ; S[2] = S2_ROL3

    ; L[0] = L0_ROL
    ; L[1] = L1_ROL

    ; L1[2] = rc5_72unitwork->L0.hi
    ; L2[2] = rc5_72unitwork->L0.hi + 1

    ; ECX = S2_L1

    ; L1[2] = ROTL(L1[2] + ECX, CL)
    ; L2[2] = ROTL(L2[2] + ECX, CL)

    ; ECX - shiftreg
    ; EAX - S1  ESI - L1
    ; EBX - S2  EDI - L2
    mov  esi, edx
    mov  eax, [L0_ROL]
    lea  edi, [edx+1]
    mov  ecx, [S2_L1]
    mov  ebx, [L1_ROL]

    mov  L1(0), eax
    mov  L2(0), eax
    add  esi, ecx
    add  edi, ecx

    mov  L1(1), ebx
    mov  L2(1), ebx
    rol  esi, cl
    rol  edi, cl

    mov  ebx, [S2_ROL3]
    mov  ecx, [S1_ROL3]

    mov  L2(2), edi
    mov  L1(2), esi
    lea  eax, [ebx + esi + S_not(3)]
    mov  S1(2), ebx
    mov  S2(2), ebx
    rol  eax, 3
    mov  S1(1), ecx
    mov  S2(1), ecx
    mov  S1(3), eax

    KEYSETUP_BLOCK_CONSTANTS  3, 0
    KEYSETUP_BLOCK_CONSTANTS  4, 1
    KEYSETUP_BLOCK_CONSTANTS  5, 2
    KEYSETUP_BLOCK_CONSTANTS  6, 0
    KEYSETUP_BLOCK_CONSTANTS  7, 1
    KEYSETUP_BLOCK_CONSTANTS  8, 2
    KEYSETUP_BLOCK_CONSTANTS  9, 0
    KEYSETUP_BLOCK_CONSTANTS 10, 1
    KEYSETUP_BLOCK_CONSTANTS 11, 2
    KEYSETUP_BLOCK_CONSTANTS 12, 0
    KEYSETUP_BLOCK_CONSTANTS 13, 1
    KEYSETUP_BLOCK_CONSTANTS 14, 2
    KEYSETUP_BLOCK_CONSTANTS 15, 0
    KEYSETUP_BLOCK_CONSTANTS 16, 1
    KEYSETUP_BLOCK_CONSTANTS 17, 2
    KEYSETUP_BLOCK_CONSTANTS 18, 0
    KEYSETUP_BLOCK_CONSTANTS 19, 1
    KEYSETUP_BLOCK_CONSTANTS 20, 2
    KEYSETUP_BLOCK_CONSTANTS 21, 0
    KEYSETUP_BLOCK_CONSTANTS 22, 1
    KEYSETUP_BLOCK_CONSTANTS 23, 2
    KEYSETUP_BLOCK_CONSTANTS 24, 0

;   KEYSETUP_BLOCK_CONSTANTS 25, 1
;---
    add  esi, eax
    mov  ecx, esi
    lea  ebx, [ebx + edi + S_not(25)]
    add  esi, L1(1)

    rol  esi, cl
    rol  ebx, 3

    mov  L1(1), esi
    mov  S2(25), ebx
;---
    add  edi, ebx
    lea  eax, [eax + esi + S0_ROL3]

    mov  ecx, edi
    add  edi, L2(1)

    rol  edi, cl
    rol  eax, 3

    add  ebx, S0_ROL3
    mov  L2(1), edi
    mov  S1(0), eax
;---

    KEYSETUP_BLOCK  0, 2
    KEYSETUP_BLOCK  1, 0
    KEYSETUP_BLOCK  2, 1
    KEYSETUP_BLOCK  3, 2
    KEYSETUP_BLOCK  4, 0
    KEYSETUP_BLOCK  5, 1
    KEYSETUP_BLOCK  6, 2
    KEYSETUP_BLOCK  7, 0
    KEYSETUP_BLOCK  8, 1
    KEYSETUP_BLOCK  9, 2
    KEYSETUP_BLOCK 10, 0
    KEYSETUP_BLOCK 11, 1
    KEYSETUP_BLOCK 12, 2
    KEYSETUP_BLOCK 13, 0
    KEYSETUP_BLOCK 14, 1
    KEYSETUP_BLOCK 15, 2
    KEYSETUP_BLOCK 16, 0
    KEYSETUP_BLOCK 17, 1
    KEYSETUP_BLOCK 18, 2
    KEYSETUP_BLOCK 19, 0
    KEYSETUP_BLOCK 20, 1
    KEYSETUP_BLOCK 21, 2
    KEYSETUP_BLOCK 22, 0
    KEYSETUP_BLOCK 23, 1
    KEYSETUP_BLOCK 24, 2
    KEYSETUP_BLOCK 25, 0

    KEYSETUP_BLOCK  0, 1
    KEYSETUP_BLOCK  1, 2
    KEYSETUP_BLOCK  2, 0
    KEYSETUP_BLOCK  3, 1
    KEYSETUP_BLOCK  4, 2
    KEYSETUP_BLOCK  5, 0
    KEYSETUP_BLOCK  6, 1
    KEYSETUP_BLOCK  7, 2
    KEYSETUP_BLOCK  8, 0
    KEYSETUP_BLOCK  9, 1
    KEYSETUP_BLOCK 10, 2
    KEYSETUP_BLOCK 11, 0
    KEYSETUP_BLOCK 12, 1
    KEYSETUP_BLOCK 13, 2
    KEYSETUP_BLOCK 14, 0
    KEYSETUP_BLOCK 15, 1
    KEYSETUP_BLOCK 16, 2
    KEYSETUP_BLOCK 17, 0
    KEYSETUP_BLOCK 18, 1
    KEYSETUP_BLOCK 19, 2
    KEYSETUP_BLOCK 20, 0
    KEYSETUP_BLOCK 21, 1
    KEYSETUP_BLOCK 22, 2
    KEYSETUP_BLOCK 23, 0
    KEYSETUP_BLOCK 24, 1

;   KEYSETUP_BLOCK 25, 2
;---

    mov  eax, [work_Plo]
    add  ebx, edi

    mov  esi, [work_Phi]
    rol  ebx, 3

    mov  edi, esi
    mov  S2(25), ebx

    mov  ebx, eax

    add  eax, S1(0)
    add  esi, S1(1)

    add  ebx, S2(0)
    add  edi, S2(1)

    ENCRYPTION_BLOCK 1
    ENCRYPTION_BLOCK 2
    ENCRYPTION_BLOCK 3
    ENCRYPTION_BLOCK 4
    ENCRYPTION_BLOCK 5
    ENCRYPTION_BLOCK 6
    ENCRYPTION_BLOCK 7
    ENCRYPTION_BLOCK 8
    ENCRYPTION_BLOCK 9
    ENCRYPTION_BLOCK 10
    ENCRYPTION_BLOCK 11
    ENCRYPTION_BLOCK 12


TEST_KEY_1_SS_2:
;---
    cmp  eax, [work_Clo]
    jne  short TEST_KEY_2_SS_2

    mov  eax, [RC5_72UnitWork]

    mov  ecx, [work_L0mid]
    inc  dword [RC5_72UnitWork_CMCcount]
    bswap ecx
    mov  [RC5_72UnitWork_CMCmid], ecx

    mov  ecx, [work_L0lo]
    mov  [RC5_72UnitWork_CMChi], edx
    bswap ecx
    mov  [RC5_72UnitWork_CMClo], ecx
;---
    cmp  esi, [work_Chi]
    jne  short TEST_KEY_2_SS_2

    mov  ecx, [iterations]
    mov  ebx, [work_iter]
    mov  esi, RESULT_FOUND
    sub  [ecx], ebx

    jmp  LOOP_EXIT_SS_2


    k7align 16
TEST_KEY_2_SS_2:
;---
    cmp  ebx, [work_Clo]
    jne  INC_KEY_HI_SS_2

    mov  eax, [RC5_72UnitWork]
    inc  dword [RC5_72UnitWork_CMCcount]

    inc  edx
    mov  ebx, [work_L0mid]
    mov  ecx, [work_L0lo]
    bswap ebx
    bswap ecx
    mov  [RC5_72UnitWork_CMChi], edx
    mov  [RC5_72UnitWork_CMCmid], ebx
    mov  [RC5_72UnitWork_CMClo], ecx
    dec edx
;---
    cmp  edi, [work_Chi]
    jne  short INC_KEY_HI_SS_2

    mov  ebx, [work_iter]
    mov  ecx, [iterations]
    dec  ebx
    mov  esi, RESULT_FOUND
    sub  [ecx], ebx

    jmp  short LOOP_EXIT_SS_2


    k7align 16
INC_KEY_HI_SS_2:

    add  dl, BYTE PIPES
    jc   INC_KEY_MID_SS_2

    sub  dword [work_iter], BYTE PIPES
    jnz  LOOP_HI_SS_2

    mov  esi, RESULT_NOTHING
    jmp  short LOOP_EXIT_SS_2


INC_KEY_MID_SS_2:

    adc  dword [work_L0mid], BYTE 0
    jc   INC_KEY_LO_SS_2

    sub  dword [work_iter], BYTE PIPES
    jnz  LOOP_MID_SS_2

    mov  esi, RESULT_NOTHING
    jmp  short LOOP_EXIT_SS_2


INC_KEY_LO_SS_2:

    adc  dword [work_L0lo], BYTE 0
;!  jc   LOOP_EXIT_NOTHING_SS_2

    sub  dword [work_iter], BYTE PIPES
    jnz  LOOP_LO_SS_2

    mov  esi, RESULT_NOTHING


LOOP_EXIT_SS_2:

    mov  eax, [RC5_72UnitWork]

    mov  ebx, [work_L0mid]
    mov  ecx, [work_L0lo]

    bswap ebx
    bswap ecx

    mov  [RC5_72UnitWork_L0hi],  edx
    mov  [RC5_72UnitWork_L0mid], ebx
    mov  [RC5_72UnitWork_L0lo],  ecx

    mov  eax, esi
    mov  edi, [save_edi]
    mov  esi, [save_esi]
    mov  ebx, [save_ebx]
    mov  ebp, [save_ebp]

    add  esp, work_size

    ret
