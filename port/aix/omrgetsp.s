###############################################################################
# Copyright (c) 1991, 2006 IBM Corp. and others
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

# AIX runtime function: omrgetsp
# Input: None
# Output: current stack pointer

# GPR definitions
 .set r0,0; .set SP,1; .set r3,3; .set r4,4
 .set r5,5; .set r6,6; .set r7,7; .set r8,8; .set r9,9
 .set r10,10; .set r11,11; .set r12,12; .set r13,13; .set r14,14
 .set r15,15; .set r16,16; .set r17,17; .set r18,18; .set r19,19
 .set r20,20; .set r21,21; .set r22,22; .set r23,23; .set r24,24
 .set r25,25; .set r26,26; .set r27,27; .set r28,28; .set r29,29
 .set r30,30; .set r31,31

        .csect omrgetsp_TEXT{PR}
 .align 2
 .globl .__omrgetsp
.__omrgetsp:
 startproc.__omrgetsp:

	mr    r3, SP
	blr
