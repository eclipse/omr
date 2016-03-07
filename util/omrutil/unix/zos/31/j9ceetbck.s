***********************************************************************
*
* (c) Copyright IBM Corp. 1991, 2010
*
*  This program and the accompanying materials are made available
*  under the terms of the Eclipse Public License v1.0 and
*  Apache License v2.0 which accompanies this distribution.
*
*      The Eclipse Public License is available at
*      http://www.eclipse.org/legal/epl-v10.html
*
*      The Apache License v2.0 is available at
*      http://www.opensource.org/licenses/apache2.0.php
*
* Contributors:
*    Multiple authors (IBM Corp.) - initial API and implementation
*    and/or initial documentation
***********************************************************************

         TITLE 'j9ceetbck.s'

r0       EQU      0
fp0      EQU      0
r1       EQU      1
fp1      EQU      1
r2       EQU      2
fp2      EQU      2
r3       EQU      3
fp3      EQU      3
r4       EQU      4
fp4      EQU      4
r5       EQU      5
fp5      EQU      5
r6       EQU      6
fp6      EQU      6
r7       EQU      7
fp7      EQU      7
r8       EQU      8
fp8      EQU      8
r9       EQU      9
fp9      EQU      9
r10      EQU      10
fp10     EQU      10
r11      EQU      11
fp11     EQU      11
r12      EQU      12
fp12     EQU      12
r13      EQU      13
fp13     EQU      13
r14      EQU      14
fp14     EQU      14
r15      EQU      15
fp15     EQU      15


         AIF ('&SYSPARM' EQ 'BIT64').JMP1
DUMMY   CSECT
DUMMY   AMODE 31
DUMMY   RMODE ANY

         ENTRY CEETBCK
CEETBCK  EDCPRLG BASEREG=R11
         L     R15,CEECAALEOV-CEECAA(,R12) vector table via CAA
         L     R15,304(,R15)               address of CEETBCK
         BALR  R14,R15
         EDCEPIL
         DROP  R11
         CEECAA

.JMP1    ANOP
* No code yet for 64-bit
.JMP2    ANOP

         END
