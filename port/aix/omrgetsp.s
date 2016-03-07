###############################################################################
#
# (c) Copyright IBM Corp. 1991, 2006
#
#  This program and the accompanying materials are made available
#  under the terms of the Eclipse Public License v1.0 and
#  Apache License v2.0 which accompanies this distribution.
#
#      The Eclipse Public License is available at
#      http://www.eclipse.org/legal/epl-v10.html
#
#      The Apache License v2.0 is available at
#      http://www.opensource.org/licenses/apache2.0.php
#
# Contributors:
#    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
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
