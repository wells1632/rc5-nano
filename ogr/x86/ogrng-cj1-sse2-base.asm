;
; Base assembly core for OGR-NG, SSE2 version. Based on MMX assembly core (ogrng-b-asm-rt.asm).
; $Id: ogrng-cj1-sse2-base.asm,v 1.1 2010/02/02 05:35:33 stream Exp $
;
; NOTE: Requires *ALL* DIST, LIST and COMP bitmaps to start on 16 byte boundaries.
; Designed for Pentium M and later 
;
; Created by Craig Johnston (craig.johnston@dolby.com)
;
; 2009-10-25: Added LZCNT, PEXTRD and PEXTRW support
;             Improved shifting code (From 64bit)
;
; 2009-04-23: Aligned stack to 16 bytes
;
; 2009-04-14: Changed some branches into conditional moves
;             Changed branches from signed to unsigned to allow macro uop fusion on Core2
;             Removed some redundant operations
;
; 2009-04-05: Rewrote lookup of first zero bit
;             Removed special case for 64th bit is the zero bit
;             Changed xorpd to pxor
;             Rewrote shift by 64 bits
;             Fixed potential bug where wrong values of list were saved after stop depth is reached
;             Resulted in 2% speedup
;
; 2009-03-30: Initial SSE2 version. Converted bitmap operations from MMX to SSE2
;             Optimised register usage to keep mark, depth and limit in general registers
;             Precomputed some of the memory offset calculations being done in the main loop
;             General optimisation of the math to reduce instructions
;
; Possible improvements:
; * Vary choose array cache size
; * Arrange the choose array for better cache hits, depth as most significant index perhaps?

	%define CHOOSE_DIST_BITS	16

	; Register renames
	%define mm_one		mm0
	%define mm_1		mm1
	%define mm_2		mm2
	%define mm_newbit	mm3
	%define mm_4		mm4
	%define mm_5		mm5
	%define mm_6		mm6
	%define mm_7		mm7

	%define xmm_temp_s	xmm0	; Shared by newbit
	%define xmm_temp_ss	xmm1
	%define xmm_list0	xmm2
	%define xmm_list2	xmm3
	%define xmm_comp0	xmm4
	%define xmm_comp2	xmm5
	%define xmm_temp_a	xmm6	; holds dist0 after PUSH_LEVEL_UPDATE_STATE
	%define xmm_temp_b	xmm7

	; REGISTER - globals
	; ebx = mark
	; edi = limit
	; edx = work depth
	; ebp = stack location

	%define regsavesize	10h	; 4 registers saved
	%define worksize	30h

	%define work_halfdepth				esp+00h
	%define work_halfdepth2				esp+04h
	%define work_nodes					esp+08h
	%define work_maxlen_m1				esp+0Ch
	%define work_half_depth_mark_addr	esp+10h
	%define work_maxdepth_m1			esp+14h
	%define work_stopdepth				esp+18h
	%define work_oState					esp+1Ch
	%define work_pnodes					esp+20h
	%define work_pchoose				esp+24h
	%define work_oldesp					esp+28h

	%define param_oState	esp+regsavesize+worksize+04h
	%define param_pnodes	esp+regsavesize+worksize+08h
	%define param_pchoose	esp+regsavesize+worksize+0Ch

	; State Offsets
	%define oState_max			00h
	%define oState_maxdepthm1	08h
	%define oState_half_depth	0Ch
	%define oState_half_depth2	10h
	%define oState_stopdepth	18h
	%define oState_depth		1Ch
	%define oState_Levels		20h

; It's possible to put ebp (current level) a little forward and reference
; elements as 'ebp-nn' and 'ebp+nn' (with signed byte offsets) to avoid
; long command 'ebp+nnnnnnnn' (dword offset).
	%define ebp_shift		128	; may be up to 128
	%define sizeof_level	112	; (32*3+8 + 8)
	%define level_list		00h
	%define level_dist		20h
	%define level_comp		40h
	%define level_mark		60h
	%define level_limit		64h

%define cur(el, index)   [ebp+level_ %+ el + ((index)*8) - ebp_shift]
%define next(el, index)  [ebp+sizeof_level+level_ %+ el + ((index)*8) - ebp_shift]

	; Macro defining the whole body of the function
	; Parameter 1 = The Name of this block
	; Parameter 2 = The Name of the block to jump to when pushing
	; Parameter 3 = The Name of the block to jump to when popping
%macro func 3

	align	16
do_loop_split_%1:
	movdqa	xmm_list0, cur(list, 0)
	movdqa	xmm_list2, cur(list, 2)

for_loop_%1:
	; REGISTER - end
	; eax = inverse shift amount (location of 0)
	; ecx = shift amount (ecx - eax)

%ifdef use_pextrd
	pextrd	eax, xmm_comp0, 1
%else
	pshuflw	xmm_temp_b, xmm_comp0, 11101110b
	movd	eax, xmm_temp_b
%endif

	;      if (comp0 == (SCALAR)~0) {

	xor	eax, 0FFFFFFFFh		; implies 'not eax'
	je	use_high_word_%1
%ifdef use_lzcnt
	lzcnt	ecx, eax
	mov	eax, 63
	sub	eax, ecx
	add	ecx, 1
%else
	bsr	eax, eax
	mov	ecx, 32
	sub	ecx, eax		; s = ecx-bsr
	add	eax, 32	; = ss
%endif
	jmp found_shift_%1

	align	16
use_high_word_%1:
	movd	eax, xmm_comp0
	xor	eax, 0FFFFFFFFh		; implies 'not eax'
	je	full_shift_%1
%ifdef use_lzcnt
	lzcnt	ecx, eax
	mov	eax, 64
	add	ecx, 33
	sub	eax, ecx
%else
	bsr	eax, eax
	mov	ecx, 64
	sub	ecx, eax		; s = ecx-bsr
%endif

found_shift_%1:
	; REGISTER - start
	; eax = inverse shift amount (location of 0)
	; ecx = shift amount (ecx - eax)

	;        if ((mark += s) > limit) {
	;          break;
	;        }
	add	ebx, ecx
	cmp	ebx, edi	; limit (==lev->limit)
	ja	break_for_%1

	;        COMP_LEFT_LIST_RIGHT(lev, s);
	; !!!

	movd	xmm_temp_s, ecx

	; newbit + list goes right and comp goes left

	; copy of list for shifting left
	movdqa	xmm_temp_a, xmm_list0
	psrlq	xmm_list0, xmm_temp_s
	movdqa	xmm_temp_b, xmm_list2

	movd	xmm_temp_ss, eax

	psrlq	xmm_list2, xmm_temp_s

	; WARNING xmm_temp_s now contains newbit
	movq2dq	xmm_temp_s, mm_newbit

	psllq	xmm_temp_a, xmm_temp_ss
	psllq	xmm_temp_b, xmm_temp_ss
	psllq	xmm_temp_s, xmm_temp_ss

	shufpd	xmm_temp_s, xmm_temp_a, 0	; select Low(a), Low(s)
	shufpd	xmm_temp_a, xmm_temp_b, 1	; select Low(b), High(a)

	por	xmm_list0, xmm_temp_s
	por	xmm_list2, xmm_temp_a

	; comp goes left

	; xmm_temp_s restored to normal purpose
	movd	xmm_temp_s, ecx
	movdqa	xmm_temp_a, xmm_comp0
	movdqa	xmm_temp_b, xmm_comp2

	psllq	xmm_comp0, xmm_temp_s
	psllq	xmm_comp2, xmm_temp_s
	pxor	xmm_temp_s, xmm_temp_s	;using xmm_temp_s as a source of 0's

	psrlq	xmm_temp_a, xmm_temp_ss
	psrlq	xmm_temp_b, xmm_temp_ss

	shufpd	xmm_temp_a, xmm_temp_b, 1	; select Low(b), High(a)
	psrldq	xmm_temp_b, 8

after_if_%1:
	; REGISTER - usage
	; eax = temp

	;      if (depth == oState->maxdepthm1) {
	;        goto exit;         /* Ruler found */
	;      }
	mov	eax, [work_maxdepth_m1]
	cmp	eax, edx
	je	ruler_found_%1

	por	xmm_comp0, xmm_temp_a
	por	xmm_comp2, xmm_temp_b

	;      PUSH_LEVEL_UPDATE_STATE(lev);
	; !!!
	; **   LIST[lev+1] = LIST[lev]
	; **   DIST[lev+1] = (DIST[lev] | LIST[lev+1])
	; **   COMP[lev+1] = (COMP[lev] | DIST[lev+1])
	; **   newbit = 1;

	; Save our loaded values
	movdqa	xmm_temp_a, cur(dist, 0)
	movdqa	xmm_temp_b, cur(dist, 2)
	movdqa	cur(list, 0), xmm_list0
	movdqa	cur(list, 2), xmm_list2

	; **   LIST[lev+1] = LIST[lev]	; No need as we keep list in registers
;	movdqa	next(list, 0), xmm_list0
;	movdqa	next(list, 2), xmm_list2

	; **   DIST[lev+1] = (DIST[lev] | LIST[lev+1])
	movdqa	cur(comp, 0), xmm_comp0
	por	xmm_temp_a, xmm_list0
	movdqa	cur(comp, 2), xmm_comp2
	por	xmm_temp_b, xmm_list2
	movdqa	next(dist, 0), xmm_temp_a
	por	xmm_comp0, xmm_temp_a
	movdqa	next(dist, 2), xmm_temp_b

	; **   COMP[lev+1] = (COMP[lev] | DIST[lev+1])
	por	xmm_comp2, xmm_temp_b
;	movdqa	next(comp, 0), xmm_comp0	; No need as we keep comp in registers
;	movdqa	next(comp, 2), xmm_comp2

%ifndef use_pextrw
	pshuflw	xmm_temp_a, xmm_temp_a, 11101110b	; Fill xmm_temp_a with dist0
%endif

;	!! delay init !!
;	newbit = 1

	;      lev->mark = mark;
	mov	[ebp+level_mark-ebp_shift], ebx
	mov	[ebp+level_limit-ebp_shift], edi

	;      lev++;
	add	ebp, sizeof_level

	;      depth++;
	add	edx, 1

	; /* Compute the maximum position for the next level */
	; #define choose(dist,seg) pchoose[(dist >> (SCALAR_BITS-CHOOSE_DIST_BITS)) * 32 + (seg)]
	; limit = choose(dist0, depth);

	; REGISTER - usage
	; eax = temp
	; esi = choose array mem location

	mov	esi, [work_pchoose]
%ifdef use_pextrw
	pextrw	eax, xmm_temp_a, 11b
%else
	movd	eax, xmm_temp_a ; dist 0
	shr	eax, 32 - CHOOSE_DIST_BITS
%endif
	shl	eax, 5
	add	eax, edx		; depth
	movzx	edi, word [esi+eax*2]

	;      if (depth > oState->half_depth && depth <= oState->half_depth2) {
	;;;      if (depth > halfdepth && depth <= halfdepth2) {

	cmp	edx, [work_halfdepth2]
	jbe	continue_if_depth_%1

skip_if_depth_%1:
	; REGISTER - usage
	; eax = temp

	sub	dword [work_nodes], 1
	movq	mm_newbit, mm_one		; newbit = 1 (delayed)

	;      if (--nodes <= 0) {
	jg	for_loop_%2

	;        goto exit;
	jmp	exit

	align	16
continue_if_depth_%1:
	; REGISTER - usage
	; ecx = dist0
	; esi = temp
	; eax = temp

	cmp	edx, [work_halfdepth]
	jbe	skip_if_depth_%1

;        int temp = maxlen_m1 - oState->Levels[oState->half_depth].mark;
;;        int temp = oState->max - 1 - oState->Levels[halfdepth].mark;

	mov	esi, [work_maxlen_m1]
	mov	eax, [work_half_depth_mark_addr]
	sub	esi, [eax]

;        if (depth < oState->half_depth2) {
	cmp	edx, [work_halfdepth2]
	jae	update_limit_temp_%1

;          temp -= LOOKUP_FIRSTBLANK(dist0); // "33" version
;;;        temp -= LOOKUP_FIRSTBLANK(dist0 & -((SCALAR)1 << 32));

%ifdef use_pextrw
	pshuflw	xmm_temp_a, xmm_temp_a, 11101110b	; Fill xmm_temp_a with dist0
%endif

	movd	ecx, xmm_temp_a		; move dist0 in ecx
	not	ecx
%ifdef use_lzcnt
	sub	esi, 1
	lzcnt	ecx, ecx
	sub	esi, ecx
%else
	mov	eax, -1
	bsr	ecx, ecx
	cmovz	ecx, eax
	add	esi, ecx
	sub	esi, 32
%endif

update_limit_temp_%1:
;        if (limit > temp) {
;          limit = temp;
;        }

	cmp	edi, esi
	cmovg	edi, esi
	jmp	skip_if_depth_%1

	align	16
full_shift_%1:
	; REGISTER - start
	; esi = comp0 (high)

	;      else {         /* s >= SCALAR_BITS */

	;        if ((mark += SCALAR_BITS) > limit) {
	;          break;
	;        }
	add	ebx, 64
	cmp	ebx, edi ; limit (==lev->limit)
	ja	break_for_%1

	;      COMP_LEFT_LIST_RIGHT_WORD(lev);
	;      continue;

	; COMP_LEFT_LIST_RIGHT_WORD(lev);
	; !!!

	pslldq	xmm_list2, 8
	movq2dq	xmm_temp_s, mm_newbit
	movhlps	xmm_list2, xmm_list0

	pslldq	xmm_list0, 8
	por	xmm_list0, xmm_temp_s

	psrldq	xmm_comp0, 8
	pxor	mm_newbit, mm_newbit	; Clear newbit
	punpcklqdq	xmm_comp0, xmm_comp2

	psrldq	xmm_comp2, 8

	jmp	for_loop_%1

	align	16
break_for_%1:

	;    lev--;
	sub	ebp, sizeof_level

	;    depth--;
	sub	edx, 1

	;    POP_LEVEL(lev);
	; !!!
	movdqa	xmm_comp0, cur(comp, 0)
	movdqa	xmm_comp2, cur(comp, 2)

	pxor	mm_newbit, mm_newbit	;      newbit = 0;

	;  } while (depth > oState->stopdepth);
	mov	eax, [work_stopdepth]

	; split loop header
	mov	ebx, [ebp+level_mark-ebp_shift]	; mark  = lev->mark;
	mov	edi, [ebp+level_limit-ebp_shift]

	cmp	eax, edx
	jb	do_loop_split_%3

	movdqa	xmm_list0, cur(list, 0)
	movdqa	xmm_list2, cur(list, 2)
	jmp	exit

ruler_found_%1:
	por	xmm_comp0, xmm_temp_a
	por	xmm_comp2, xmm_temp_b
	jmp	exit
%endmacro

%macro header 0
	push	ebx
	push	esi
	push	edi
	push	ebp			; note: regsavesize bytes pushed
	sub	esp, worksize

	; Grab all paramaters
	mov	ebx, [param_oState]
	mov	eax, [param_pnodes]
	mov	ecx, [param_pchoose]

	; Align stack to 16 bytes
	mov	edx, esp
	and	esp, 0xFFFFFFF0
	mov	[work_oldesp], edx

	; write the paramters in the aligned work space
	mov	[work_oState], ebx
	mov	[work_pnodes], eax
	mov	[work_pchoose], ecx

	mov	edx, [ebx+oState_depth]
	imul	eax, edx, sizeof_level
	lea	ebp, [eax+ebx+oState_Levels+ebp_shift]	; lev = &oState->Levels[oState->depth]
	mov	eax, [work_pnodes]
	mov	eax, [eax]
	mov	[work_nodes], eax	; nodes = *pnodes

	mov	eax, [ebx+oState_half_depth]
	mov	[work_halfdepth], eax	; halfdepth = oState->half_depth
	; get address of oState->Levels[oState->half_depth].mark
	; value of this var can be changed during crunching, but addr is const
	imul	eax, sizeof_level
	lea	eax, [eax+ebx+oState_Levels+level_mark]
	mov	[work_half_depth_mark_addr], eax

	mov	eax, [ebx+oState_half_depth2]
	mov	[work_halfdepth2], eax	; halfdepth2 = oState->half_depth2

	mov	eax, [ebx+oState_max]
	sub	eax, 1
	mov	[work_maxlen_m1], eax	; maxlen_m1 = oState->max - 1

	mov	eax, [work_oState]
	mov	eax, [eax+oState_maxdepthm1]
	mov	[work_maxdepth_m1], eax

	mov	eax, [work_oState]
	mov	eax, [eax+oState_stopdepth]
	mov	[work_stopdepth], eax

	; SETUP_TOP_STATE(lev);
	; !!!
	movdqa	xmm_comp0, cur(comp, 0)
	movdqa	xmm_comp2, cur(comp, 2)

	; int newbit = (depth < oState->maxdepthm1) ? 1 : 0;
	xor	eax, eax
	cmp	edx, [ebx+oState_maxdepthm1]
	setl	al
	movd	mm_newbit, eax

	mov	eax, 1
	movd	mm_one, eax

	; mm0..mm3 = comp
	; mm4 = newbit

	; split loop header
	mov	ebx, [ebp+level_mark-ebp_shift]	; mark  = lev->mark;
	mov	edi, [ebp+level_limit-ebp_shift]

%endmacro

%macro footer 0
exit:
	;  SAVE_FINAL_STATE(lev);
	; !!!

	movdqa	cur(list, 0), xmm_list0
	movdqa	cur(list, 2), xmm_list2
	movdqa	cur(comp, 0), xmm_comp0
	movdqa	cur(comp, 2), xmm_comp2

	;      lev->mark = mark;
	mov	[ebp+level_mark-ebp_shift], ebx
	mov	[ebp+level_limit-ebp_shift], edi

	mov	ebx, [work_pnodes]	; *pnodes -= nodes;
	mov	eax, [work_nodes]
	sub	[ebx], eax

	mov	eax, edx	; return depth;

	mov	esp, [work_oldesp]
	add	esp, worksize
	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
	emms
	ret
%endmacro

%macro body 1
	%assign max_id %1
	%assign id 1
	%rep %1
		%assign next_id id + 1
		%if next_id > max_id
			%assign next_id max_id
		%endif

		%assign prev_id id - 1
		%if prev_id < 1
			%assign prev_id 1
		%endif

		func id, next_id, prev_id
		%assign id id + 1
	%endrep
%endmacro
