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

_TEXT   segment para 'CODE'

public compareAndExchange1
public compareAndExchange2
public compareAndExchange4
public compareAndExchange8
public _patchingFence16

Align16 macro
; TODO:AMD64: Find a way not to get 16 bytes of padding when we are already aligned!
        align 16
endm

; Atomically Compare And Exchange the value contained data pointed to by ptr
; with oldValue, and if equal, replace by newValue
; Return zero on failure. Nonzero otherwise.
;
; prototype: int32_t compareAndExchange1(uint8_t *ptr, uint8_t oldValue, uint8_t newValue)
;

prepareCompareAndExchangeArguments macro
        ;; TODO: this generates Rex prefixes that are only needed for the 8-byte case
IFDEF WINDOWS
        ;; rcx=address, rdx=old, r8=new
        mov     rax, rdx
        mov     rdx, r8
ELSE
        ;; rdi=address, rsi=old, rdx=new
        mov     rcx, rdi
        mov     rax, rsi
ENDIF
        ;; rcx=address, rax=old, rdx=new
endm

        Align16
compareAndExchange1 PROC
        prepareCompareAndExchangeArguments
        lock cmpxchg byte ptr [rcx], dl
        jnz     failedCMPXCHG
passedCMPXCHG: ;; Placed here to enhance branch prediction
        mov     eax, 1
        ret
compareAndExchange1 ENDP

        Align16
compareAndExchange2 PROC
        prepareCompareAndExchangeArguments
        lock cmpxchg word ptr [rcx], dx
        jz      passedCMPXCHG
failedCMPXCHG: ;; Placed here to enhance branch prediction
        xor     eax, eax
        ret
compareAndExchange2 ENDP

        Align16
compareAndExchange4 PROC
        prepareCompareAndExchangeArguments
        lock cmpxchg dword ptr [rcx], edx
        jz      passedCMPXCHG
        jmp     failedCMPXCHG
compareAndExchange4 ENDP

        Align16
compareAndExchange8 PROC
        prepareCompareAndExchangeArguments
        lock cmpxchg qword ptr [rcx], rdx
        jz      passedCMPXCHG
        jmp     failedCMPXCHG
compareAndExchange8 ENDP

        Align16
_patchingFence16 PROC
        mfence
IFDEF WINDOWS
        clflush [rcx]
        clflush [rcx+8]
ELSE
        clflush [rdi]
        clflush [rdi+8]
ENDIF
        mfence
        ret
_patchingFence16 ENDP

_TEXT   ends
