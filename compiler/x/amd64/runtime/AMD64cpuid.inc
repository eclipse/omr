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

OPTION NOSCOPED

ifndef WINDOWS

; hardcoded STMXCSR instruction (64-bit result stored in [esp])
stmxcsr macro mustbeesp
	db	00Fh
	db	0AEh
	db	01Ch
	db	024h
endm
endif

	_TEXT SEGMENT PARA 'CODE'
	public jitGetMXCSR

; Prototype: U_32 jitGetMXCSR(void);
; - throws an Illegal Instruction exception if the OS does not support SSE instructions
; - otherwise, returns the content of the MXCSR.
	align	16
jitGetMXCSR	proc
	sub	rsp, 8
	stmxcsr [rsp]
	pop	rax
	ret
jitGetMXCSR	endp

	_TEXT	ENDS
