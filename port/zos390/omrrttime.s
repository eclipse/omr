***********************************************************************
* Copyright (c) 1991, 2016 IBM Corp. and others
* 
* This program and the accompanying materials are made available 
* under the terms of the Eclipse Public License 2.0 which accompanies 
* this distribution and is available at  
* https://www.eclipse.org/legal/epl-2.0/ or the Apache License, 
* Version 2.0 which accompanies this distribution and
* is available at https://www.apache.org/licenses/LICENSE-2.0.
* 
* This Source Code may also be made available under the following
* Secondary Licenses when the conditions for such availability set
* forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
* General Public License, version 2 with the GNU Classpath 
* Exception [1] and GNU General Public License, version 2 with the
* OpenJDK Assembly Exception [2].
* 
* [1] https://www.gnu.org/software/classpath/license.html
* [2] http://openjdk.java.net/legal/assembly-exception.html
* 
* SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
***********************************************************************

         TITLE 'omrrttime.s'

r0       EQU      0
r1       EQU      1
r2       EQU      2
r3       EQU      3
r4       EQU      4

CLOCK31  EQU 2112
CLOCK64  EQU 2176
NOBORROW EQU B'0011'

         AIF ('&SYSPARM' EQ 'BIT64').JMP1
maxprec  EDCXPRLG BASEREG=8
         stckf CLOCK31(r4)
         lm  r2,r3,CLOCK31(r4)
         sl  r3,LC0+4
         brc NOBORROW,LCarry0
         ahi r2,-1
LCarry0  s   r2,LC0
         using PSA,r0
         l   r1,FLCCVT
         using CVT,r1
         l   r1,CVTEXT2
         using CVTXTNT2,r1
         sl  r3,CVTLSO+4
         brc NOBORROW,LCarry1
         ahi r2,-1
LCarry1  s   r2,CVTLSO
         drop r0,r1
         srdl r2,1
         EDCXEPLG
         AGO .JMP2

.JMP1    ANOP
maxprec  CELQPRLG BASEREG=8
         stckf CLOCK64(r4)
         lg  r3,CLOCK64(r4)
         sg  r3,LC0
         using PSA,r0
         llgt  r1,FLCCVT
         using CVT,r1
         llgt  r1,CVTEXT2
         using CVTXTNT2,r1
         slg r3,CVTLSO
         drop r0,r1
         srlg r3,r3,1
         CELQEPLG

.JMP2    ANOP

*
ALIGN8   DS  D
LC0      DC  XL8'7D91048BCA000000'
*
         LTORG
         IHAPSA
         CVT DSECT=YES

         END
