;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;; (c) Copyright IBM Corp. 2000, 2016
;;
;;  This program and the accompanying materials are made available
;;  under the terms of the Eclipse Public License v1.0 and
;;  Apache License v2.0 which accompanies this distribution.
;;
;;      The Eclipse Public License is available at
;;      http://www.eclipse.org/legal/epl-v10.html
;;
;;      The Apache License v2.0 is available at
;;      http://www.opensource.org/licenses/apache2.0.php
;;
;; Contributors:
;;    Multiple authors (IBM Corp.) - initial implementation and documentation
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	.486p
	assume cs:flat,ds:flat,ss:flat

; hardcoded STMXCSR instruction (32-bit result stored in [esp])
stmxcsr macro
	db	00Fh
	db	0AEh
	db	01Ch
	db	024h
endm

	_TEXT SEGMENT PARA USE32 PUBLIC 'CODE'
	public jitGetMXCSR

; Prototype: U_32 jitGetMXCSR(void);
; - throws an Illegal Instruction exception if the OS does not support SSE instructions
; - otherwise, returns the content of the MXCSR.
	align	16
jitGetMXCSR	proc near
	sub	esp, 4
	stmxcsr
	pop	eax
	ret
jitGetMXCSR	endp

	_TEXT	ENDS

