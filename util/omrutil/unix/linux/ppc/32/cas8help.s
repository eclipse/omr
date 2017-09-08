###############################################################################
# Copyright (c) 1991, 2015 IBM Corp. and others
# 
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#      
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#    
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#       
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

	.file "cas8help.s"
 	.set r0,0
 	.set r1,1
 	.set r3,3
 	.set r4,4
 	.set r5,5
 	.set r6,6
 	.set r7,7
 	.set r8,8
 	.set r9,9
 	.set r10,10
 	.set r11,11
 	.set r12,12
 	.set r13,13
 	.set r14,14
 	.set r15,15
 	.set r16,16
 	.set r17,17
 	.set r18,18
 	.set r19,19
 	.set r20,20
 	.set r21,21
 	.set r22,22
 	.set r23,23
 	.set r24,24
 	.set r25,25
 	.set r26,26
 	.set r27,27
 	.set r28,28
 	.set r29,29
 	.set r30,30
 	.set r31,31
	.section ".rodata"
	.global J9CAS8Helper
	.type J9CAS8Helper@function
	.section ".text"
	.align 2
J9CAS8Helper:
# in:
#
# r3 = the address of the 8-aligned memory address
# r4 = low  part of compare value
# r5 = high part of compare value
# r6 = low  part of swap value
# r7 = high part of swap value
#
# out:
#
# r3 = high part of read value
# r4 = low  part of read value
	ori r12, r3, 0
	ori r8, r4, 0
loop:
	ldarx r9, 0, r12
	srdi r3, r9, 32
	ori r4, r9, 0
	ori r10, r8, 0
	ori r11, r6, 0
	rldimi r10, r5, 32, 0
	rldimi r11, r7, 32, 0
	cmpl cr0, 1, r9, r10
	bne fail
	stdcx. r11, 0, r12
	bne loop
	blr
fail:
	stdcx. r9, 0, r12
	bne loop
	blr
