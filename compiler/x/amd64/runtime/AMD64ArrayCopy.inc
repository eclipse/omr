;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Copyright (c) 2000, 2016 IBM Corp. and others
;;
;; This program and the accompanying materials are made available under
;; the terms of the Eclipse Public License 2.0 which accompanies this
;; distribution and is available at http://eclipse.org/legal/epl-2.0
;; or the Apache License, Version 2.0 which accompanies this distribution
;; and is available at https://www.apache.org/licenses/LICENSE-2.0.
;;
;; This Source Code may also be made available under the following Secondary
;; Licenses when the conditions for such availability set forth in the
;; Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
;; version 2 with the GNU Classpath Exception [1] and GNU General Public
;; License, version 2 with the OpenJDK Assembly Exception [2].
;;
;; [1] https://www.gnu.org/software/classpath/license.html
;; [2] http://openjdk.java.net/legal/assembly-exception.html
;;
;; SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

MOV_THRESHOLD equ           64  ;; Use movq or movdqu if greater than or equal than this size

_DATA   segment para 'DATA'

JumpTableStart memmoveTable
                dq      offset  memmove0
                dq      offset  memmove1
                dq      offset  memmove2
                dq      offset  memmove3
                dq      offset  memmove4
                dq      offset  memmove5
                dq      offset  memmove6
                dq      offset  memmove7
                dq      offset  memmove8
                dq      offset  memmove9
                dq      offset  memmove10
                dq      offset  memmove11
                dq      offset  memmove12
                dq      offset  memmove13
                dq      offset  memmove14
                dq      offset  memmove15
                dq      offset  memmove16
JumpTableEnd memmoveTable


                ; forward direction halfword arraycopy table
JumpTableStart fwdHalfWordCopyTable
                dq      offset  fwdCopy0
                dq      0
                dq      offset  fwdCopy2
                dq      0
                dq      offset  fwdCopy4
                dq      0
                dq      offset  fwdCopy6
                dq      0
                dq      offset  fwdCopy8
                dq      0
                dq      offset  fwdCopy10
                dq      0
                dq      offset  fwdCopy12
                dq      0
                dq      offset  fwdCopy14
                dq      0
                dq      offset  fwdCopy16
                dq      0
                dq      offset  fwdCopy18
                dq      0
                dq      offset  fwdCopy20
                dq      0
                dq      offset  fwdCopy22
                dq      0
                dq      offset  fwdCopy24
                dq      0
                dq      offset  fwdCopy26
                dq      0
                dq      offset  fwdCopy28
                dq      0
                dq      offset  fwdCopy30
                dq      0
                dq      offset  fwdCopy32
                dq      0
                dq      offset  fwdCopy34
                dq      0
                dq      offset  fwdCopy36
                dq      0
                dq      offset  fwdCopy38
                dq      0
                dq      offset  fwdCopy40
                dq      0
                dq      offset  fwdCopy42
                dq      0
                dq      offset  fwdCopy44
                dq      0
                dq      offset  fwdCopy46
                dq      0
                dq      offset  fwdCopy48
JumpTableEnd fwdHalfWordCopyTable
_DATA   ends


_TEXT           segment para 'CODE'
ifdef DEBUGSTATS
                ExternHelper _numSSECopies:dword
                ExternHelper _numSSECopiesAlignFailed:dword
                ExternHelper _numStringMoves:dword
endif

                public _arrayCopy
                public _halfWordArrayCopy
                public _wordArrayCopy
                public _longArrayCopy
                public _forwardArrayCopy
                public _forwardArrayCopyAMDOpteron
                public _forwardHalfWordArrayCopy
                public _forwardWordArrayCopy
                public _forwardLongArrayCopy
                public fwdHalfWordCopyTable
                public  _arrayCopyAggressive
                public  _wordArrayCopyAggressive
                public  _halfWordArrayCopyAggressive
                public  _SSEforwardArrayCopyAggressive

ifdef DEBUGSTATS
                align 16
_arrayMoveStats proc
                inc     dword ptr [rcx * 4 + _copyStats + statMoveShort]
                cmp     rcx, 8
                jb      moveStatSmall
                test    rdi, 7
                je      moveStatEnd
                test    rsi, 7
                je      moveStatSemiAligned
moveStatUnaligned:
                inc     dword ptr [_copyStats + statUnaligned]
                ret
moveStatSemiAligned:
                inc     dword ptr [_copyStats + statSemialigned]
                ret
moveStatSmall:
                test    rdi, 1
                je      moveStatEnd
                test    rsi, 1
                je      moveStatSemiAligned
                jmp     moveStatUnaligned
moveStatEnd:
                ret
_arrayMoveStats endp
endif

; _arrayCopy
; _BC_arrayCopy      ;Block Concurrent Array Copy
; _BC_arrayCopy_acrossCacheLine
; _halfWordArrayCopy
; _wordArrayCopy
; _longArrayCopy
;
; Basically a C-style memmove(). While _arrayCopy makes no assumptions on
; the element size, the other variants assume that the element size (and
; alignment) is known to be 2, 4 or 8 bytes, respectively. Copies of 0 to
; 16 bytes (aligned or otherwise) are optimized by hand; if we are to copy
; more than 16 bytes of data, we perform the copies using string moves.
;
; Parameters:
;       rcx = length of copy in bytes (0...8*MAX_INT)
;       rsi = source address
;       rdi = destination address
;
                align 16
_arrayCopy      proc
LarrayCopy:
                cmp     rcx, 16
                ja      memmoveLoop1
ifdef DEBUGSTATS
                CallHelper arrayMoveStats
endif
                push    rax
                JumpTableHelper rax,rcx,memmoveTable
_arrayCopy      endp

                align 16
_halfWordArrayCopy proc
LhalfWordArrayCopy:
                cmp     rcx, 16
                ja      memmoveLoop2
ifdef DEBUGSTATS
                CallHelper arrayMoveStats
endif
                push    rax
                JumpTableHelper rax,rcx,memmoveTable
_halfWordArrayCopy endp

                align 16
_wordArrayCopy proc
LwordArrayCopy:
                cmp     rcx, 16
                ja      memmoveLoop4
ifdef DEBUGSTATS
                CallHelper arrayMoveStats
endif
                push    rax
                JumpTableHelper rax,rcx,memmoveTable
_wordArrayCopy endp

                align 16
_longArrayCopy proc
LlongArrayCopy:
                cmp     rcx, 16
                ja      memmoveLoop8
ifdef DEBUGSTATS
                CallHelper arrayMoveStats
endif
                push    rax
                JumpTableHelper rax,rcx,memmoveTable

memmove4:
                mov     ecx, dword ptr [rsi]
                mov     dword ptr [rdi], ecx
memmove0:
                pop     rax
                ret
                align   16
memmove1:
                mov     cl, byte ptr [rsi]
                pop     rax
                mov     byte ptr [rdi], cl
                ret
                align   16
memmove2:
                mov     cx, word ptr [rsi]
                pop     rax
                mov     word ptr [rdi], cx
                ret
                align   16
memmove3:
                mov     cl, byte ptr [rsi + 2]
                mov     si, word ptr [rsi]
                pop     rax
                mov     word ptr [rdi], si
                mov     byte ptr [rdi + 2], cl
                ret
                align   16
memmove5:
                mov     cl, byte ptr [rsi + 4]
                mov     esi, dword ptr [rsi]
                pop     rax
                mov     dword ptr [rdi], esi
                mov     byte ptr [rdi + 4], cl
                ret
                align   16
memmove6:
                mov     ecx, dword ptr [rsi]
                mov     si, word ptr [rsi + 4]
                pop     rax
                mov     dword ptr [rdi], ecx
                mov     word ptr [rdi + 4], si
                ret
                align   16
memmove7:
                mov     ecx, dword ptr [rsi]
                mov     esi, dword ptr [rsi + 3]
                pop     rax
                mov     dword ptr [rdi], ecx
                mov     dword ptr [rdi + 3], esi
                ret
                align   16
memmove8:
                mov     rcx, qword ptr [rsi]
                pop     rax
                mov     qword ptr [rdi], rcx
                ret
                align   16
memmove9:
                mov     cl, byte ptr [rsi]
                mov     rsi, qword ptr [rsi + 1]
                pop     rax
                mov     byte ptr [rdi], cl
                mov     qword ptr [rdi + 1], rsi
                ret
                align   16
memmove10:
                mov     rcx, qword ptr [rsi]
                mov     si, word ptr [rsi + 8]
                pop     rax
                mov     qword ptr [rdi], rcx
                mov     word ptr [rdi + 8], si
                ret
                align   16
memmove11:
                mov     rcx, qword ptr [rsi]
                mov     esi, dword ptr [rsi + 7]
                pop     rax
                mov     qword ptr [rdi], rcx
                mov     dword ptr [rdi + 7], esi
                ret
                align   16
memmove12:
                mov     rcx, qword ptr [rsi]
                mov     esi, dword ptr [rsi + 8]
                pop     rax
                mov     qword ptr [rdi], rcx
                mov     dword ptr [rdi + 8], esi
                ret
                align   16
memmove13:
                mov     rcx, qword ptr [rsi]
                mov     eax, dword ptr [rsi + 8]
                mov     sil, byte ptr [rsi + 12]
                mov     qword ptr [rdi], rcx
                mov     dword ptr [rdi + 8], eax
                pop     rax
                mov     byte ptr [rdi + 12], sil
                ret
                align   16
memmove14:
                mov     rcx, qword ptr [rsi]
                mov     eax, dword ptr [rsi + 8]
                mov     si, word ptr [rsi + 12]
                mov     qword ptr [rdi], rcx
                mov     dword ptr [rdi + 8], eax
                pop     rax
                mov     word ptr [rdi + 12], si
                ret
                align   16
memmove15:
                mov     rcx, qword ptr [rsi]
                mov     rsi, qword ptr [rsi + 7]
                pop     rax
                mov     qword ptr [rdi], rcx
                mov     qword ptr [rdi + 7], rsi
                ret
                align   16
memmove16:
                mov     rcx, qword ptr [rsi]
                mov     rsi, qword ptr [rsi + 8]
                pop     rax
                mov     qword ptr [rdi], rcx
                mov     qword ptr [rdi + 8], rsi
                ret

; memmoveLoop1
;
; Copies possibly unaligned data, using REP MOVSQ for most of the aligned quad
; words, and REP MOVSB for residues. The direction of copying is reversed if
; the beginning of the destination overlaps the end of the source.
;
                align   16
memmoveLoop1:
                push    rax
                mov     rax, rdi
                sub     rax, rsi
                cmp     rax, rcx
                jb      memmoveReverse1
ifdef DEBUGSTATS
                inc     dword ptr [_copyStats + statMoveFwd]
endif
                test    rdi, 7
                je      short memmove1Aligned8
                test    rdi, 3
                je      short memmove1Aligned4
                test    rdi, 1
                je      short memmove1Aligned2
                movsb
                dec     rcx
                test    rdi, 2
                je      short memmove1Aligned4
memmove1Aligned2:
                movsw
                sub     rcx, 2
memmove1Aligned4:
                movsd
                sub     rcx, 4
memmove1Aligned8:
                mov     rax, rcx
                shr     rcx, 3
                rep movsq
                test    rax, 7
                je      memmove1Done

                mov     rcx, rax
                test    rcx, 4
                je      short memmoveResidue2
                mov     eax, dword ptr [rsi]    ; if our count is >=4 do 4 byte move
                mov     dword ptr [rdi], eax
                add     rsi, 4
                add     rdi, 4

memmoveResidue2:
                test    rcx, 2
                je      short memmoveResidue1
                mov     ax, word ptr [rsi]      ; if our count is >=2 do 2 byte move
                mov     word ptr [rdi], ax
                add     rsi, 2
                add     rdi, 2

memmoveResidue1:
                test    rcx, 1
                je      memmove1Done
                mov     al, byte ptr [rsi]      ; if our count is >=1 do 1 byte move
                mov     byte ptr [rdi], al

                ;old code slow and incorrect for 64bit compressed
                ;mov     rcx, rax
                ;and     rcx, 7
                ;rep movsb
                jmp     memmove1Done
memmoveReverse1:
ifdef DEBUGSTATS
                inc     dword ptr [_copyStats + statMoveBack]
endif
                add     rdi, rcx
                add     rsi, rcx
                std
                test    rdi, 1
                je      short memmoveReverse1Aligned2
                dec     rsi
                dec     rdi
                mov     al, byte ptr [rsi]
                dec     rcx
                mov     byte ptr [rdi], al
memmoveReverse1Aligned2:
                test    rdi, 2
                je      short memmoveReverse1Aligned4
                sub     rsi, 2
                sub     rdi, 2
                mov     ax, word ptr [rsi]
                sub     rcx, 2
                mov     word ptr [rdi], ax
memmoveReverse1Aligned4:
                test    rdi, 4
                je      short memmoveReverse1Aligned8
                sub     rsi, 4
                sub     rdi, 4
                mov     eax, dword ptr [rsi]
                sub     rcx, 4
                mov     dword ptr [rdi], eax
memmoveReverse1Aligned8:
                mov     rax, rcx
                sub     rdi, 8
                sub     rsi, 8
                shr     rcx, 3
                rep movsq
                test    rax, 7
                je      memmove1Done      			; no residue just end the sequence

                add     rsi, 8							    ; we have residue to copy, adjust the pointers
                add     rdi, 8                  ; so that we point right before where the last copy was
                mov     rcx, rax
                test    rcx, 4
                je      short memmoveReverseResidue2
                sub     rsi, 4
                sub     rdi, 4
                mov     eax, dword ptr [rsi]    ; if our count is >=4 do 4 byte move
                mov     dword ptr [rdi], eax

memmoveReverseResidue2:
                test    rcx, 2
                je      short memmoveReverseResidue1
                sub     rsi, 2
                sub     rdi, 2
                mov     ax, word ptr [rsi]      ; if our count is >=2 do 2 byte move
                mov     word ptr [rdi], ax

memmoveReverseResidue1:
                test    rcx, 1
                je      short memmove1Done
                dec     rsi
                dec     rdi
                mov     al, byte ptr [rsi]      ; if our count is >=1 do 1 byte move
                mov     byte ptr [rdi], al

                ;oldCode:
                ;mov     rcx, rax        ; copy at most 7 residual bytes
                ;add     rsi, 7          ; this is slow but more over it is invalid for compressed
                ;add     rdi, 7          ; references refrerence copy
                ;and     rcx, 7
                ;rep movsb
memmove1Done:
                cld
                pop     rax
                ret

; memmoveLoop2
;
; Copies 2 byte-aligned data. The direction of copying is reversed if the
; beginning of the destination overlaps the end of the source.
;
                align   16
memmoveLoop2:
                push    rax
                mov     rax, rdi
                sub     rax, rsi
                cmp     rax, rcx
                jb      short memmoveReverse2
ifdef DEBUGSTATS
                inc     dword ptr [_copyStats + statMoveFwd]
endif
                test    rdi, 6
                je      short memmove2Aligned8
                test    rdi, 2
                je      short memmove2Aligned4
                movsw
                sub     rcx, 2
memmove2Aligned4:
                movsd
                sub     rcx, 4
memmove2Aligned8:
                mov     rax, rcx
                shr     rcx, 3
                rep movsq
                test    rax, 6
                je      memmove2Done
                mov     rcx, rax
                and     rcx, 6
                shr     rcx, 1
                rep movsw
                jmp     memmove2Done
memmoveReverse2:
ifdef DEBUGSTATS
                inc     dword ptr [_copyStats + statMoveBack]
endif
                add     rdi, rcx
                add     rsi, rcx
                std
                test    rdi, 2
                je      short memmoveReverse2Aligned4
                sub     rsi, 2
                sub     rdi, 2
                mov     ax, word ptr [rsi]
                sub     rcx, 2
                mov     word ptr [rdi], ax
memmoveReverse2Aligned4:
                test    rdi, 4
                je      short memmoveReverse2Aligned8
                sub     rsi, 4
                sub     rdi, 4
                mov     eax, dword ptr [rsi]
                sub     rcx, 4
                mov     dword ptr [rdi], eax
memmoveReverse2Aligned8:
                mov     rax, rcx
                sub     rsi, 8
                sub     rdi, 8
                shr     rcx, 3
                rep movsq
                test    rax, 6
                je      short memmove2Done
                mov     rcx, rax        ; copy at most 3 residual half words
                add     rsi, 6
                and     rcx, 6
                add     rdi, 6
                shr     rcx, 1
                rep     movsw
memmove2Done:
                cld
                pop     rax
                ret

; memmoveLoop4
;
; Copies 4 byte-aligned data. The direction of copying is reversed if the
; beginning of the destination overlaps the end of the source.
;
                align   16
memmoveLoop4:
                push    rax
                mov     rax, rdi
                sub     rax, rsi
                cmp     rax, rcx
                jb      short memmoveReverse4
ifdef DEBUGSTATS
                inc     dword ptr [_copyStats + statMoveFwd]
endif
                test    rdi, 4
                je      short memmove4Aligned8
                movsd
                sub     rcx, 4
memmove4Aligned8:
                mov     rax, rcx
                shr     rcx, 3
                rep movsq
                test    rax, 4
                je      short memmove4Done
                movsd
                jmp     short memmove4Done
memmoveReverse4:
ifdef DEBUGSTATS
                inc     dword ptr [_copyStats + statMoveBack]
endif
                add     rdi, rcx
                add     rsi, rcx
                std
                test    rdi, 4
                je      short memmoveReverse4Aligned8
                sub     rsi, 4
                sub     rdi, 4
                mov     eax, dword ptr [rsi]
                sub     rcx, 4
                mov     dword ptr [rdi], eax
memmoveReverse4Aligned8:
                mov     rax, rcx
                sub     rdi, 8
                sub     rsi, 8
                shr     rcx, 3
                rep movsq
                test    rax, 4
                je      short memmove4Done
                mov     eax, dword ptr [rsi + 4]        ; copy at most 1 residual word
                mov     dword ptr [rdi + 4], eax
memmove4Done:
                cld
                pop     rax
                ret

; memmoveLoop8
;
; Copies 8 byte-aligned data. The direction of copying is reversed if the
; beginning of the destination overlaps the end of the source.
;
                align   16
memmoveLoop8:
                push    rax
                mov     rax, rdi
                sub     rax, rsi
                cmp     rax, rcx
                jb      short memmoveReverse8
ifdef DEBUGSTATS
                inc     dword ptr [_copyStats + statMoveFwd]
endif
                shr     rcx, 3
                rep movsq
                pop     rax
                ret
memmoveReverse8:
ifdef DEBUGSTATS
                inc     dword ptr [_copyStats + statMoveBack]
endif
                lea     rdi, [rdi + rcx - 8]
                lea     rsi, [rsi + rcx - 8]
                shr     rcx, 3
                rep movsq
                cld
                pop     rax
                ret
_longArrayCopy endp
_TEXT           ends

_DATA   segment para 'DATA'
JumpTableStart memcpyTable
                dq      offset  memcpy0
                dq      offset  memcpy1
                dq      offset  memcpy2
                dq      offset  memcpy3
                dq      offset  memcpy4
                dq      offset  memcpy5
                dq      offset  memcpy6
                dq      offset  memcpy7
                dq      offset  memcpy8
                dq      offset  memcpy9
                dq      offset  memcpy10
                dq      offset  memcpy11
                dq      offset  memcpy12
                dq      offset  memcpy13
                dq      offset  memcpy14
                dq      offset  memcpy15
                dq      offset  memcpy16
                dq      offset  memcpy17
                dq      offset  memcpy18
                dq      offset  memcpy19
                dq      offset  memcpy20
                dq      offset  memcpy21
                dq      offset  memcpy22
                dq      offset  memcpy23
                dq      offset  memcpy24
JumpTableEnd memcpyTable
_DATA   ends

_TEXT           segment para 'CODE'
ifdef DEBUGSTATS
                align 16
_arrayCopyStats proc
                inc     dword ptr [rcx*4 + _copyStats + statCopyShort]
                cmp     rcx, 8
                jb      copyStatSmall
                test    rdi, 7
                je      copyStatEnd
                test    rsi, 7
                je      copyStatSemiAligned
copyStatUnaligned:
                inc     dword ptr [_copyStats + statUnaligned]
                ret
copyStatSemiAligned:
                inc     dword ptr [_copyStats + statSemiAligned]
                ret
copyStatSmall:
                test    rdi, 1
                je      copyStatEnd
                test    rsi, 1
                je      copyStatSemiAligned
                jmp     copyStatUnaligned
copyStatEnd:
                ret
_arrayCopyStats endp
endif

; _forwardArrayCopy
; _forwardArrayCopyAMDOpteron
; _forwardHalfWordArrayCopy
; _forwardWordArrayCopy
; _forwardLongArrayCopy
;
; Basically a C-style memcpy(). While _forwardArrayCopy makes no assumptions
; on the element size, the other variants assume that the element size (and
; alignment) is known to be 2, 4 or 8 bytes, respectively. Copies of 0 to 24
; bytes (aligned or otherwise) are optimized by hand; if we are to copy more
; than 24 bytes of data, we perform the copies using string moves.
;
; Parameters:
;       rcx = length of copy in bytes (0...8*MAX_INT)
;       rsi = source address
;       rdi = destination address
;
                align 16
_forwardArrayCopy proc
LforwardArrayCopy:
                cmp     rcx, 24
                ja      memcpyLoop1
ifdef DEBUGSTATS
                CallHelper arrayCopyStats
endif
LforwardArrayCopySmallSize:
                push    rax
                JumpTableHelper rax,rcx,memcpyTable
_forwardArrayCopy endp

                align 16
_SSEforwardArrayCopyAggressive PROC
LSSEfwdCopyAggressiveStart:

LSSEfwdCopyAggressive:
                ;cmp     rcx, 24
                ;jbe     LforwardArrayCopySmallSize;
                ; make some temp registers

                test    rcx, rcx
                je      LSSEfwdCopyEndWithoutRestore

                push    rax

                ; use single index register to avoid one extra add in loops
                sub     rdi, rsi

                ; for small copies just go down to copy, 1, 2 and 4 bytes
                cmp     rcx, 8
                jb      LSSEfwdCopyLessThanEight


                ; align the source to 8 bytes
                ; we rely on the compare at the top to handle lengths smaller than 8 bytes
                ; we can safely copy at least 24 bytes

                test    rsi, 1
                jz      LSSEfwdCopySourceAligned2
                mov     al, byte ptr [rsi]
                mov     byte ptr [rdi + rsi], al
                add     rsi, 1
                sub     rcx, 1

LSSEfwdCopySourceAligned2:
                test    rsi, 2
                jz      LSSEfwdCopySourceAligned4
                mov     ax, word ptr [rsi]
                mov     word ptr [rdi + rsi], ax
                add     rsi, 2
                sub     rcx, 2

LSSEfwdCopySourceAligned4:
                test    rsi, 4
                jz      LSSEfwdCopySourceAligned
                mov     eax, dword ptr [rsi]
                mov     dword ptr [rdi + rsi], eax
                add     rsi, 4
                sub     rcx, 4

LSSEfwdCopySourceAligned:

		        cmp rcx,256
        		jae LSSEfwdCopySourceAlignedLarge

                ; make x 8 counter in eax
                ; we want to move at least chunks of 8
                mov     rax, rcx
                shr     rax, 3
                ; see if you can do at least one step of the unroll
                sub     rax, 8
                jge     LSSEfwdCopyUnrolled
                ; if not add back the counter and go 8 bytes at a time
                add     rax, 8
                jnz     LSSEfwdCopyEightAtATime
                jmp     LSSEfwdCopyLessThanEight

                align   16
LSSEfwdCopyUnrolled:
                ; do unrolled copy of 64 bytes at a time
                movq    xmm0, qword ptr [rsi]
                movq    xmm1, qword ptr [rsi + 8]
                movq    xmm2, qword ptr [rsi + 16]
                movq    qword ptr [rdi + rsi], xmm0
                movq    xmm3, qword ptr [rsi + 24]
                movq    qword ptr [rdi + rsi + 8], xmm1
                movq    xmm0, qword ptr [rsi + 32]
                movq    qword ptr [rdi + rsi + 16], xmm2
                movq    xmm1, qword ptr [rsi + 40]
                movq    qword ptr [rdi + rsi + 24], xmm3
                movq    xmm2, qword ptr [rsi + 48]
                movq    qword ptr [rdi + rsi + 32], xmm0
                movq    xmm3, qword ptr [rsi + 56]
                movq    qword ptr [rdi + rsi + 40], xmm1
                movq    qword ptr [rdi + rsi + 48], xmm2
                movq    qword ptr [rdi + rsi + 56], xmm3

                add     rsi, 64
LSSEfwdCopyCheckForUnroll:
                sub     rax, 8
                jge     LSSEfwdCopyUnrolled
                add     rax, 8
                jz      LSSEfwdCopyLessThanEight

                align   16
LSSEfwdCopyEightAtATime:
                movq    xmm0, qword ptr [rsi]
                movq    qword ptr [rdi + rsi], xmm0
                add     rsi, 8
                dec     rax
                jg      LSSEfwdCopyEightAtATime

LSSEfwdCopyLessThanEight:
                ; now we check the original counter for residue
                ; below 8 bytes

                test    rcx, 4
                jz      LSSEfwdCopyResidue2
                mov     eax, dword ptr [rsi]
                mov     dword ptr [rdi + rsi], eax
                add     rsi, 4

LSSEfwdCopyResidue2:
                test    rcx, 2
                jz      LSSEfwdCopyResidue1
                mov     ax, word ptr [rsi]
                mov     word ptr [rdi + rsi], ax
                add     rsi, 2

LSSEfwdCopyResidue1:
                test    ecx, 1
                jz      LSSEfwdCopyEnd
                mov     al, byte ptr [rsi]
                mov     byte ptr [rdi + rsi], al
LSSEfwdCopyEnd:
                ; restore temp registers
                pop     rax

LSSEfwdCopyEndWithoutRestore:
                ret


LSSEfwdCopySourceAlignedLarge:
                ; make x 16 counter in eax
                ; we want to move at least chunks of 16
                mov     rax, rcx
                shr     rax, 4
                ; see if you can do at least one step of the unroll
                sub     rax, 8
                jge     LSSEfwdCopyUnrolledLarge
                ; if not add back the counter and go 16 bytes at a time
                add     rax, 8
                jnz     LSSEfwdCopySixteenAtATime
                jmp     LSSEfwdCopyLessThanSixteen

LSSEfwdCopyUnrolledLarge:
                ; do unrolled copy of 64 bytes at a time
                movdqu  xmm0, oword ptr [rsi]
                movdqu  xmm1, oword ptr [rsi + 16]
                movdqu  xmm2, oword ptr [rsi + 32]
                movdqu  oword ptr [rdi + rsi], xmm0
                movdqu  xmm3, oword ptr [rsi + 48]
                movdqu  oword ptr [rdi + rsi + 16], xmm1
                movdqu  xmm0, oword ptr [rsi + 64]
                movdqu  oword ptr [rdi + rsi + 32], xmm2
                movdqu  xmm1, oword ptr [rsi + 80]
                movdqu  oword ptr [rdi + rsi + 48], xmm3
                movdqu  xmm2, oword ptr [rsi + 96]
                movdqu  oword ptr [rdi + rsi + 64], xmm0
                movdqu  xmm3, oword ptr [rsi + 112]
                movdqu  oword ptr [rdi + rsi + 80], xmm1
                movdqu  oword ptr [rdi + rsi + 96], xmm2
                movdqu  oword ptr [rdi + rsi + 112], xmm3

                add     rsi, 128

LSSEfwdCopyCheckForUnrollLarge:
                sub     rax, 8
                jge     LSSEfwdCopyUnrolledLarge
                add     rax, 8
                jz      LSSEfwdCopyLessThanSixteen

                align   16
LSSEfwdCopySixteenAtATime:
                movq    xmm0, qword ptr [rsi]
                movq    qword ptr [rdi + rsi], xmm0
                movq    xmm1, qword ptr [rsi+8]
                movq    qword ptr [rdi + rsi+8], xmm1

                add     rsi, 16
                dec     rax
                jg      LSSEfwdCopySixteenAtATime

				align 16
LSSEfwdCopyLessThanSixteen:
                ; now we check the original counter for residue
                ; below 16 bytes

                test    rcx, 8
                jz      LSSEfwdCopyLessThanEight
                movq    xmm0, qword ptr [rsi]
                movq    qword ptr [rdi + rsi], xmm0
                add     rsi, 8
                jmp     LSSEfwdCopyLessThanEight

_SSEforwardArrayCopyAggressive endp


; aggressive general direction copies

                align   16
_arrayCopyAggressive PROC
                sub     rdi, rsi
                cmp     rdi, rcx       ; Determine copy direction required
                lea     rdi, [rdi+rsi]
                jb      LarrayCopy
                jmp     LSSEfwdCopyAggressiveStart
_arrayCopyAggressive ENDP

                align   16
_wordArrayCopyAggressive PROC
                sub     rdi, rsi
                cmp     rdi, rcx       ; Determine copy direction required
                lea     rdi, [rdi+rsi]
                jb      LwordArrayCopy
                jmp     LSSEfwdCopyAggressiveStart
_wordArrayCopyAggressive ENDP


                align   16
_halfWordArrayCopyAggressive PROC
                sub     rdi, rsi
                cmp     rdi, rcx       ; Determine copy direction required
                lea     rdi, [rdi+rsi]
                jb      LhalfWordArrayCopy
                jmp     LSSEfwdCopyAggressiveStart
_halfWordArrayCopyAggressive ENDP

; end aggressive

                align 16
_forwardArrayCopyAMDOpteron proc
                cmp     rcx, 24
                ja      memcpyLoop1AMDOpteron
ifdef DEBUGSTATS
                CallHelper arrayCopyStats
endif
                push    rax
                JumpTableHelper rax,rcx,memcpyTable
_forwardArrayCopyAMDOpteron endp

                align 16
_forwardHalfWordArrayCopy proc
                cmp     rcx, 24
                ja      memcpyLoop2
ifdef DEBUGSTATS
                CallHelper arrayCopyStats
endif
                push    rax
                JumpTableHelper rax,rcx,memcpyTable
_forwardHalfWordArrayCopy endp

                align   16
_forwardWordArrayCopy proc
                cmp     rcx, 24
                ja      memcpyLoop4
ifdef DEBUGSTATS
                CallHelper arrayCopyStats
endif
                push    rax
                JumpTableHelper rax,rcx,memcpyTable
_forwardWordArrayCopy endp

                align   16
_forwardLongArrayCopy proc
                cmp     rcx, 24
                ja      memcpyLoop8
ifdef DEBUGSTATS
                CallHelper arrayCopyStats
endif
                push    rax
                JumpTableHelper rax,rcx,memcpyTable

memcpy4:
                mov     ecx, dword ptr [rsi]
                mov     dword ptr [rdi], ecx
memcpy0:
                pop     rax
                ret
                align   16
memcpy1:
                mov     cl, byte ptr [rsi]
                pop     rax
                mov     byte ptr [rdi], cl
                ret
                align   16
memcpy2:
                mov     cx, word ptr [rsi]
                pop     rax
                mov     word ptr [rdi], cx
                ret
                align   16
memcpy3:
                mov     cl, byte ptr [rsi + 2]
                mov     si, word ptr [rsi]
                pop     rax
                mov     word ptr [rdi], si
                mov     byte ptr [rdi + 2], cl
                ret
                align   16
memcpy5:
                mov     cl, byte ptr [rsi + 4]
                mov     esi, dword ptr [rsi]
                pop     rax
                mov     dword ptr [rdi], esi
                mov     byte ptr [rdi + 4], cl
                ret
                align   16
memcpy6:
                mov     ecx, dword ptr [rsi]
                mov     si, word ptr [rsi + 4]
                pop     rax
                mov     dword ptr [rdi], ecx
                mov     word ptr [rdi + 4], si
                ret
                align   16
memcpy7:
                mov     ecx, dword ptr [rsi]
                mov     esi, dword ptr [rsi + 3]
                pop     rax
                mov     dword ptr [rdi], ecx
                mov     dword ptr [rdi + 3], esi
                ret
                align   16
memcpy8:
                mov     rcx, qword ptr [rsi]
                pop     rax
                mov     qword ptr [rdi], rcx
                ret
                align   16
memcpy9:
                mov     cl, byte ptr [rsi]
                mov     rsi, qword ptr [rsi + 1]
                pop     rax
                mov     byte ptr [rdi], cl
                mov     qword ptr [rdi + 1], rsi
                ret
                align   16
memcpy10:
                mov     rcx, qword ptr [rsi]
                mov     si, word ptr [rsi + 8]
                pop     rax
                mov     qword ptr [rdi], rcx
                mov     word ptr [rdi + 8], si
                ret
                align   16
memcpy11:
                mov     rcx, qword ptr [rsi]
                mov     esi, dword ptr [rsi + 7]
                pop     rax
                mov     qword ptr [rdi], rcx
                mov     dword ptr [rdi + 7], esi
                ret
                align   16
memcpy12:
                mov     rcx, qword ptr [rsi]
                mov     esi, dword ptr [rsi + 8]
                pop     rax
                mov     qword ptr [rdi], rcx
                mov     dword ptr [rdi + 8], esi
                ret
                align   16
memcpy13:
                mov     rcx, qword ptr [rsi]
                mov     eax, dword ptr [rsi + 8]
                mov     sil, byte ptr [rsi + 12]
                mov     qword ptr [rdi], rcx
                mov     dword ptr [rdi + 8], eax
                pop     rax
                mov     byte ptr [rdi + 12], sil
                ret
                align   16
memcpy14:
                mov     rcx, qword ptr [rsi]
                mov     eax, dword ptr [rsi + 8]
                mov     si, word ptr [rsi + 12]
                mov     qword ptr [rdi], rcx
                mov     dword ptr [rdi + 8], eax
                pop     rax
                mov     word ptr [rdi + 12], si
                ret
                align   16
memcpy15:
                mov     rcx, qword ptr [rsi]
                mov     rsi, qword ptr [rsi + 7]
                pop     rax
                mov     qword ptr [rdi], rcx
                mov     qword ptr [rdi + 7], rsi
                ret
                align   16
memcpy16:
                mov     rcx, qword ptr [rsi]
                mov     rsi, qword ptr [rsi + 8]
                pop     rax
                mov     qword ptr [rdi], rcx
                mov     qword ptr [rdi + 8], rsi
                ret

                align   16
memcpy17:
                mov     rcx, qword ptr [rsi]
                mov     rax, qword ptr [rsi + 8]
                mov     sil, byte ptr [rsi + 16]
                mov     qword ptr [rdi], rcx
                mov     qword ptr [rdi + 8], rax
                pop     rax
                mov     byte ptr [rdi + 16], sil
                ret
                align   16
memcpy18:
                mov     rcx, qword ptr [rsi]
                mov     rax, qword ptr [rsi + 8]
                mov     si, word ptr [rsi + 16]
                mov     qword ptr [rdi], rcx
                mov     qword ptr [rdi + 8], rax
                pop     rax
                mov     word ptr [rdi + 16], si
                ret
                align   16
memcpy19:
                mov     rcx, qword ptr [rsi]
                mov     rax, qword ptr [rsi + 8]
                mov     esi, dword ptr [rsi + 15]
                mov     qword ptr [rdi], rcx
                mov     qword ptr [rdi + 8], rax
                pop     rax
                mov     dword ptr [rdi + 15], esi
                ret
                align   16
memcpy20:
                mov     rcx, qword ptr [rsi]
                mov     rax, qword ptr [rsi + 8]
                mov     esi, dword ptr [rsi + 16]
                mov     qword ptr [rdi], rcx
                mov     qword ptr [rdi + 8], rax
                pop     rax
                mov     dword ptr [rdi + 16], esi
                ret
                align   16
memcpy21:
                mov     rcx, qword ptr [rsi]
                mov     rax, qword ptr [rsi + 8]
                mov     qword ptr [rdi], rcx
                mov     qword ptr [rdi + 8], rax
                mov     ecx, dword ptr [rsi + 16]
                mov     sil, byte ptr [rsi + 20]
                pop     rax
                mov     dword ptr [rdi + 16], ecx
                mov     byte ptr [rdi + 20], sil
                ret
                align   16
memcpy22:
                mov     rcx, qword ptr [rsi]
                mov     rax, qword ptr [rsi + 8]
                mov     qword ptr [rdi], rcx
                mov     qword ptr [rdi + 8], rax
                mov     ecx, dword ptr [rsi + 16]
                mov     si, word ptr [rsi + 20]
                pop     rax
                mov     dword ptr [rdi + 16], ecx
                mov     word ptr [rdi + 20], si
                ret
                align   16
memcpy23:
                mov     rcx, qword ptr [rsi]
                mov     rax, qword ptr [rsi + 8]
                mov     rsi, qword ptr [rsi + 15]
                mov     qword ptr [rdi], rcx
                mov     qword ptr [rdi + 8], rax
                pop     rax
                mov     qword ptr [rdi + 15], rsi
                ret
                align   16
memcpy24:
                mov     rcx, qword ptr [rsi]
                mov     rax, qword ptr [rsi + 8]
                mov     rsi, qword ptr [rsi + 16]
                mov     qword ptr [rdi], rcx
                mov     qword ptr [rdi + 8], rax
                pop     rax
                mov     qword ptr [rdi + 16], rsi
                ret

; memcpyLoop1
;
; Copies possibly unaligned data, using REP MOVSQ for most of the aligned quad
; words, and REP MOVSB for residues. The destination is assumed not to overlap
; the source.
;
                align   16
memcpyLoop1:
                cmp     ecx, MOV_THRESHOLD
                jae     fclMovQ   ; Use unsigned compare

forwardCopyOverlapped:
                push    rax
ifdef DEBUGSTATS
                inc     dword ptr [_copyStats + statCopyLong]
endif
                test    rdi, 7
                je      short memcpy1Aligned8
                test    rdi, 3
                je      short memcpy1Aligned4
                test    rdi, 1
                je      short memcpy1Aligned2
                movsb
                dec     rcx
                test    rdi, 2
                je      short memcpy1Aligned4
memcpy1Aligned2:
                movsw
                sub     rcx, 2
memcpy1Aligned4:
                movsd
                sub     rcx, 4
memcpy1Aligned8:
                mov     rax, rcx
                shr     rcx, 3
                rep movsq
                test    rax, 7
                je      short memcpy1Done

                mov     rcx, rax
                test    rcx, 4
                je      short memcpyResidue2
                mov     eax, dword ptr [rsi]    ; if our count is >=4 do 4 byte move
                mov     dword ptr [rdi], eax
                add     rsi, 4
                add     rdi, 4

memcpyResidue2:
                test    rcx, 2
                je      short memcpyResidue1
                mov     ax, word ptr [rsi]      ; if our count is >=2 do 2 byte move
                mov     word ptr [rdi], ax
                add     rsi, 2
                add     rdi, 2

memcpyResidue1:
                test    rcx, 1
                je      memcpy1Done
                mov     al, byte ptr [rsi]      ; if our count is >=1 do 1 byte move
                mov     byte ptr [rdi], al

                ;old code slow and incorrect for 64bit compressed
                ;mov     rcx, rax
                ;and     rcx, 7
                ;rep movsb
memcpy1Done:
                pop     rax
                ret
fclMovQ:
                push rax
                mov rax, rsi
                sub rax, rdi
                and rax, 31
                jz fclNoOverLap
                pop rax
                jmp forwardCopyOverlapped
fclNoOverLap:
                ; save xmm registers used for copy
                sub rsp, 64
                movdqu oword ptr [rsp], xmm0
                movdqu oword ptr [rsp + 16], xmm1
                movdqu oword ptr [rsp + 32], xmm2
                movdqu oword ptr [rsp + 48], xmm3
                ;
                mov rax, rcx
                sub rcx, rdi
                sub rcx, rax
                and rcx, 31
                sub rax, rcx
                jle fclMovQLEndBytes
                add rdi, rcx
                add rsi, rcx
                neg rcx
                movq xmm0, qword ptr [rsi+rcx]
                movq xmm1, qword ptr [rsi+rcx+8]
                movq xmm2, qword ptr [rsi+rcx+16]
                movq xmm3, qword ptr [rsi+rcx+24]
                movq qword ptr [rdi+rcx], xmm0
                movq qword ptr [rdi+rcx+8], xmm1
                movq qword ptr [rdi+rcx+16], xmm2
                movq qword ptr [rdi+rcx+24], xmm3
                mov rcx, rax
                and rax, 31
                shr rcx, 5
                jz fclMovQLEndBytes
                sub rdi, rsi
fclMovQLoop:
                movq xmm0, qword ptr [rsi]
                movq xmm1, qword ptr [rsi+8]
                movq xmm2, qword ptr [rsi+16]
                movq xmm3, qword ptr [rsi+24]
                movq qword ptr [rdi+rsi], xmm0
                movq qword ptr [rdi+rsi+8], xmm1
                movq qword ptr [rdi+rsi+16], xmm2
                movq qword ptr [rdi+rsi+24], xmm3
                add rsi, 32
                dec rcx
                jnz fclMovQLoop
                add rdi, rsi
fclMovQLEndBytes:
                add rcx, rax
                sub rcx, 32
                movq xmm0, qword ptr [rsi+rcx]
                movq xmm1, qword ptr [rsi+rcx+8]
                movq xmm2, qword ptr [rsi+rcx+16]
                movq xmm3, qword ptr [rsi+rcx+24]
                movq qword ptr [rdi+rcx], xmm0
                movq qword ptr [rdi+rcx+8], xmm1
                movq qword ptr [rdi+rcx+16], xmm2
                movq qword ptr [rdi+rcx+24], xmm3

                ; restore temp xmm registers
                movdqu xmm0, oword ptr [rsp]
                movdqu xmm1, oword ptr [rsp + 16]
                movdqu xmm2, oword ptr [rsp + 32]
                movdqu xmm3, oword ptr [rsp + 48]
                add rsp, 64
                ;
                pop rax
                ret

; memcpyLoop1AMDOpteron
;
; Copies possibly unaligned data, using REP MOVSQ for most of the aligned quad
; words, and REP MOVSB for residues. The destination is assumed not to overlap
; the source.
;
                align   16
memcpyLoop1AMDOpteron:
                cmp     ecx, MOV_THRESHOLD
                jae     short fclMovDQU   ; Use unsigned compare

                jmp     forwardCopyOverlapped;

; you can not call this function if your arraycopy length is less than 32
                align 16
fclMovDQU:
                ; save temp regs
                sub rsp, 32
                movdqu oword ptr [rsp], xmm0
                movdqu oword ptr [rsp + 16], xmm1
                push rax

                mov rax, rsi
                sub rax, rdi
                and rax, 31
                jz fclMovDQAMDOpteronNoOverlap
                pop rax
                add rsp, 32
                jmp forwardCopyOverlapped;

fclMovDQAMDOpteronNoOverlap:
                ; align destination and adjust size
                mov rax, rcx;
                sub rcx, rdi;
                sub rcx, rax;
                and rcx, 31;
                sub rax, rcx;
                jle fclMovDQUEndBytes;
                add rdi, rcx;
                add rsi, rcx;
                neg rcx
                movdqu xmm0, oword ptr [rsi+rcx];
                movdqu xmm1, oword ptr [rsi+rcx+16];
                movdqu oword ptr [rdi+rcx], xmm0;
                movdqu oword ptr [rdi+rcx+16], xmm1;
                mov rcx, rax;
                and rax, 31;
                shr rcx, 5;
                jz fclMovDQUEndBytes;
                sub rdi, rsi;
fclMovDQULoop:
                movdqu xmm0, oword ptr [rsi];
                movdqu oword ptr [rdi+rsi], xmm0;
                movdqu xmm1, oword ptr [rsi+16];
                movdqu oword ptr [rdi+rsi+16], xmm1;
                add rsi, 32;
                dec rcx;
                jnz fclMovDQULoop;
                add rdi, rsi;
fclMovDQUEndBytes:
                add rcx, rax;
                sub rcx, 32
                movdqu xmm0, oword ptr [rsi+rcx];
                movdqu xmm1, oword ptr [rsi+rcx+16];
                movdqu oword ptr [rdi+rcx], xmm0;
                movdqu oword ptr [rdi+rcx+16], xmm1;

                ; restore temp regs
                pop rax
                movdqu xmm0, oword ptr [rsp]
                movdqu xmm1, oword ptr [rsp + 16]
                add rsp, 32
                ret

; memcpyLoop2
;
; Copies 2 byte-aligned data. The destination is assumed not to overlap the source.
;
                align   16
memcpyLoop2:
                push    rax
ifdef DEBUGSTATS
                inc     dword ptr [_copyStats + statCopyLong]
endif
                test    rdi, 6
                je      short memcpy2Aligned8
                test    rdi, 2
                je      short memcpy2Aligned4
                movsw
                sub     rcx, 2
memcpy2Aligned4:
                movsd
                sub     rcx, 4
memcpy2Aligned8:
                mov     rax, rcx
                shr     rcx, 3
                rep movsq
                test    rax, 6
                je      short memcpy2Done
                mov     rcx, rax
                and     rcx, 6
                shr     rcx, 1
                rep movsw
memcpy2Done:
                pop     rax
                ret

; memcpyLoop4
;
; Copies 4 byte-aligned data. The destination is assumed not to overlap the source.
;
                align   16
memcpyLoop4:
                push    rax
ifdef DEBUGSTATS
                inc     dword ptr [_copyStats + statCopyLong]
endif
                test    rdi, 4
                je      short memcpy4Aligned8
                movsd
                sub     rcx, 4
memcpy4Aligned8:
                mov     rax, rcx
                shr     rcx, 3
                rep movsq
                test    rax, 4
                je      short memcpy4Done
                movsd
memcpy4Done:
                pop     rax
                ret

; memcpyLoop8
;
; Copies 8 byte-aligned data. The destination is assumed not to overlap the source.
;
                align   16
memcpyLoop8:
ifdef DEBUGSTATS
                inc     dword ptr [_copyStats + statCopyLong]
endif
                shr     rcx, 3
                rep movsq
                ret
_forwardLongArrayCopy ENDP



fwdHalfWordCopy proc

fwdCopy48:      mov      rcx, qword ptr [rsi-48]
                mov      qword ptr [rdi-48], rcx
fwdCopy40:      mov      rcx, qword ptr [rsi-40]
                mov      qword ptr [rdi-40], rcx
fwdCopy32:      mov      rcx, qword ptr [rsi-32]
                mov      qword ptr [rdi-32], rcx
fwdCopy24:      mov      rcx, qword ptr [rsi-24]
                mov      qword ptr [rdi-24], rcx
fwdCopy16:      mov      rcx, qword ptr [rsi-16]
                mov      qword ptr [rdi-16], rcx
fwdCopy8:       mov      rcx, qword ptr [rsi-8]
                mov      qword ptr [rdi-8], rcx
                ret

fwdCopy46:      mov      rcx, qword ptr [rsi-46]
                mov      qword ptr [rdi-46], rcx
fwdCopy38:      mov      rcx, qword ptr [rsi-38]
                mov      qword ptr [rdi-38], rcx
fwdCopy30:      mov      rcx, qword ptr [rsi-30]
                mov      qword ptr [rdi-30], rcx
fwdCopy22:      mov      rcx, qword ptr [rsi-22]
                mov      qword ptr [rdi-22], rcx
fwdCopy14:      mov      rcx, qword ptr [rsi-14]
                mov      qword ptr [rdi-14], rcx
fwdCopy6:       mov      ecx, dword ptr [rsi-6]
                mov      dword ptr [rdi-6], ecx
                mov      cx, word ptr [rsi-2]
                mov      word ptr [rdi-2], cx
                ret

fwdCopy44:      mov      rcx, qword ptr [rsi-44]
                mov      qword ptr [rdi-44], rcx
fwdCopy36:      mov      rcx, qword ptr [rsi-36]
                mov      qword ptr [rdi-36], rcx
fwdCopy28:      mov      rcx, qword ptr [rsi-28]
                mov      qword ptr [rdi-28], rcx
fwdCopy20:      mov      rcx, qword ptr [rsi-20]
                mov      qword ptr [rdi-20], rcx
fwdCopy12:      mov      rcx, qword ptr [rsi-12]
                mov      qword ptr [rdi-12], rcx
fwdCopy4:       mov      ecx, dword ptr [rsi-4]
                mov      dword ptr [rdi-4], ecx
                ret

fwdCopy42:      mov      rcx, qword ptr [rsi-42]
                mov      qword ptr [rdi-42], rcx
fwdCopy34:      mov      rcx, qword ptr [rsi-34]
                mov      qword ptr [rdi-34], rcx
fwdCopy26:      mov      rcx, qword ptr [rsi-26]
                mov      qword ptr [rdi-26], rcx
fwdCopy18:      mov      rcx, qword ptr [rsi-18]
                mov      qword ptr [rdi-18], rcx
fwdCopy10:      mov      rcx, qword ptr [rsi-10]
                mov      qword ptr [rdi-10], rcx
fwdCopy2:       mov      cx, word ptr [rsi-2]
                mov      word ptr [rdi-2], cx
fwdCopy0:
                ret

fwdHalfWordCopy ENDP


_TEXT           ends

ifdef NOT_SUPPORTED
; _shortArrayCopy
;
; Simple memcpy() for short arrays, using discrete moves.
;
_TEXT           segment para public 'CODE'
                public _shortArrayCopy
                public _forwardSSEArrayCopy
                public _forwardArrayCopyAMDOpteron
                public _forwardSSEArrayCopyNoSizeCheck
                public _forwardSSEArrayCopyNoAlignCheck
                public _forwardArrayCopy2
                align   16
_shortArrayCopy proc
LshortArrayCopy:
                test    rcx, 3                  ; multiple of 4?
                jnz     mod2                    ; no
                test    rcx, rcx                ; 0?
                jz      short copyZero
                lea     rsi, [rsi + rcx]
                lea     rdi, [rdi + rcx]
                neg     rcx
mod4loop:
                mov     eax, dword ptr [rsi + rcx]
                mov     dword ptr [rdi + rcx], eax
                add     rcx, 4
                jnz     short mod4loop
copyZero:
                ret

mod2:
                test    rcx, 1                  ; multiple of 2?
                jnz     oddCopy                 ; no
                and     rcx, 0fffffffch
                jz      short copyTwo                 ; 2?
                lea     rsi, [rsi + rcx]
                lea     rdi, [rdi + rcx]
                neg     rcx
mod2loop:
                mov     eax, dword ptr [rsi + rcx]
                mov     dword ptr [rdi + rcx], eax
                add     rcx, 4
                jnz     short mod2loop
copyTwo:
                mov     ax, word ptr [rsi + rcx]
                mov     word ptr [rdi + rcx], ax
                ret

oddCopy:
                test    rcx, 0fffffffdh         ; 1 or 3?
                jnz     oneOrThree
                push    rcx
                and     rcx, 0fffffffch
                lea     rsi, [rsi + rcx]
                lea     rdi, [rdi + rcx]
                neg     rcx
oddLoop:
                mov     eax, dword ptr [rsi + rcx]
                mov     dword ptr [rdi + rcx], eax
                add     rcx, 4
                jnz     short oddLoop
                pop     rcx
oneOrThree:
                test    rcx, 2
                jz      short one
                mov     ax, word ptr [rsi]
                mov     word ptr [rdi], ax
                mov     cl, byte ptr [rsi + 2]
                mov     byte ptr [rdi + 2], cl
                ret
one:
                mov     al, byte ptr [rsi]
                mov     byte ptr [rdi], al
                ret
_shortArrayCopy endp


                align 16
_forwardSSEArrayCopy proc
; Don't use SSE or string moves for short buffers.
;
                cmp     rcx, 28
                jle     LshortArrayCopy

_forwardSSEArrayCopyNoSizeCheck:
; Test if source and destination buffers are mutually aligned.
;
                mov     rax, rsi
                xor     rax, rdi
                and     rax, 0fh
                jnz     LforwardArrayCopy2              ; not mutually aligned, use string moves

_forwardSSEArrayCopyNoAlignCheck:
ifdef DEBUGSTATS
                inc     dword ptr [_numSSECopies]
endif

; Check how many pre-aligned moves are required before
; starting the aligned 16-byte copies.
;
                test    rsi, 0fh                        ; 16 aligned?
                jnz     short doPreAlignment

; Fast path the aligned, no residue copy.
;
                test    rcx, 0fh                        ; any residue?
                jnz     short copyWithResidue

                lea     rsi, [rsi + rcx]
                lea     rdi, [rdi + rcx]
                neg     rcx
loopWithoutResidue:
                movaps  xmm0, oword ptr [rsi + rcx]
                movaps  oword ptr [rdi + rcx], xmm0
                add     rcx, 16
                jnz     short loopWithoutResidue
                ret

copyWithResidue:
                mov     rax, rcx                        ; save post-aligned bytes to copy for residue
                and     rax, 0fffffff0h                 ; 16-byte chunks
                lea     rsi, [rsi + rax]
                lea     rdi, [rdi + rax]
                neg     rax
loopWithResidue:
                movaps  xmm0, oword ptr [rsi + rax]
                movaps  oword ptr [rdi + rax], xmm0
                add     rax, 16
                jnz     short loopWithResidue
                and     rcx, 0fh                        ; length mod 16
                jmp     LshortArrayCopy

                align   16
doPreAlignment:
                test    esi, 03h
                jnz     preAlignStringCopy              ; not 4, 8, or 12 aligned.  Do string copy.
                mov     eax, dword ptr [rsi]            ; always need at least 4 bytes
                mov     dword ptr [rdi], eax
                test    esi, 0ch                        ; check if 12-aligned
                jpo     short align4or8
                lea     esi, [rsi +4]
                sub     ecx, 4
                lea     edi, [rdi +4]
                test    ecx, 0fh                        ; residue?
                jnz     short copyWithResidue
                lea     esi, [rsi + rcx]
                lea     edi, [rdi + rcx]
                neg     ecx
                jmp     short loopWithoutResidue

                align   16
align4or8:
                mov     eax, dword ptr [rsi +4]
                mov     dword ptr [rdi +4], eax
                test    esi, 08h                        ; check if 8-aligned
                jz      short align4
                lea     esi, [rsi +8]
                lea     edi, [rdi +8]
                sub     ecx, 8
                test    ecx, 0fh                        ; residue?
                jnz     copyWithResidue
                lea     esi, [rsi + rcx]
                lea     edi, [rdi + rcx]
                neg     ecx
                jmp     loopWithoutResidue

                align   16
align4:
                mov     eax, dword ptr [rsi +8]
                mov     dword ptr [rdi +8], eax
                lea     esi, [rsi +12]
                lea     edi, [rdi +12]
                sub     ecx, 12
                test    ecx, 0fh                        ; residue?
                jnz     copyWithResidue
                lea     esi, [rsi + rcx]
                lea     edi, [rdi + rcx]
                neg     ecx
                jmp     loopWithoutResidue
_forwardSSEArrayCopy   endp


                align   16
_forwardArrayCopy2 proc
LforwardArrayCopy2:
ifdef DEBUGSTATS
                inc     dword ptr [_numStringMoves]
endif

                test    edi, 03h                ; destination 4-aligned?
                jnz     short preAlignStringCopy

                mov     eax, ecx
                shr     ecx, 2                  ; number of dwords to copy
                rep movsd

                ; Add residual copies (at most 3 bytes)
                ;
                and     eax, 03h                ; length mod 4
                jnz     short doStringCopyResidue
                ret

doStringCopyResidue:
                mov     ecx, eax
                jmp     LshortArrayCopy

preAlignStringCopy:
                test    edi, 1
                jz      short alignedTo2

        ; Alignment must be either 1 or 3 mod 4
        ;
                mov     al, byte ptr [rsi]
                inc     esi
                mov     byte ptr [rdi], al
                inc     edi
                dec     ecx
                test    edi, 2
                jz      short alignedTo4
alignedTo2:
                mov     ax, word ptr [rsi]
                add     esi, 2
                mov     word ptr [rdi], ax
                add     edi, 2
                sub     ecx, 2
alignedTo4:
                mov     eax, ecx
                shr     ecx, 2                  ; number of dwords to copy
                rep movsd

                ; Add residual copies (at most 3 bytes)
                ;
                and     eax, 03h                ; length mod 4
                jnz     short doStringCopyResidue
                ret
_forwardArrayCopy2 endp

_TEXT           ends
endif
