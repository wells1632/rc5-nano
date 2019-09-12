; 
; Copyright distributed.net 2003-2004 - All Rights Reserved
; For use in distributed.net projects only.
; Any other distribution or use of this source violates copyright.
;
; x86 Processor feature identification for distributed.net effort    
; 
; Written for distributed.net by Steven Nikkel, Nov 2003
; 
; $Id: x86htcount.asm,v 1.2 2007/10/22 16:48:32 jlawson Exp $
;
; return u32

%ifndef __OMF__
  %ifdef OS2
    %define __OMF__
  %endif
%endif
                      
%ifdef __OMF__   ; Watcom+OS/2 or Borland+Win32
[SECTION _DATA CLASS=DATA USE32 PUBLIC ALIGN=16]
[SECTION _TEXT CLASS=CODE USE32 PUBLIC ALIGN=16]
%define __DATASECT__ [SECTION _DATA]
%define __CODESECT__ [SECTION _TEXT]
%else
%define __DATASECT__ [SECTION .data]
%define __CODESECT__ [SECTION .text]
%endif

global          x86htcount,_x86htcount

%define CPU_F_HYPERTHREAD     00010000h

__CODESECT__
_x86htcount:            
x86htcount: 

  push ebx
  push ecx
  push edx
  push esi
  push edi

  mov esi, 0h

  ; See if CPUID instruction is supported ...
  ; ... Get copies of EFLAGS into eax and ecx
  pushf
  pop eax
  mov ecx, eax

  ; ... Toggle the ID bit in one copy and store
  ;     to the EFLAGS reg
  xor eax, 200000h
  push eax
  popf

  ; ... Get the (hopefully modified) EFLAGS
  pushf
  pop eax

  ; ... Compare and test result
  cmp ecx, eax
  je near NotSupported

Standard:
  mov eax, 1h
  cpuid
HT_test:
  test edx, 10000000h   ; Test for Hyper-Threading support
  jz Return
  and ebx, 00FF0000h
  shr ebx, 16
  mov esi, ebx

NotSupported:
Return:
  mov eax, esi

  pop edi
  pop esi
  pop edx
  pop ecx
  pop ebx

  ret
