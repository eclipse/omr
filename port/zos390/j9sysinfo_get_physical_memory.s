***********************************************************************
*
* (c) Copyright IBM Corp. 1991, 2014
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

         TITLE 'Get_physical_memory'

R0       EQU   0
R1       EQU   1      Input: nothing
R2       EQU   2
R3       EQU   3      Output: Amount of physical memory, a long integer
R4       EQU   4
R5       EQU   5
R6       EQU   6      Base register
R7       EQU   7
R8       EQU   8
R9       EQU   9
R10      EQU   10
R11      EQU   11
R12      EQU   12
R13      EQU   13
R14      EQU   14
R15      EQU   15
*
* SYSPARM is set in makefile,  CFLAGS+= -Wa,SYSPARM\(BIT64\)
* to get 64 bit bit prolog and epilog.  Delete SYSPARM from makefile
* to get 31 bit version of xplink enabled prolog and epilog.
         AIF  ('&SYSPARM' EQ 'BIT64').JMP1
GETPHYM  EDCXPRLG BASEREG=6,DSASIZE=0
         AGO  .JMP2
.JMP1    ANOP
GETPHYM  CELQPRLG BASEREG=6,DSASIZE=0
.JMP2    ANOP
*
         LA    R1,0           Clear working register
         LA    R3,0           Clear returned value register
*
         USING PSA,R0         Map PSA
         L     R1,FLCCVT      Get CVT
         USING CVT,R1         Map CVT
         L     R3,CVTRLSTG    Load CVTLLSTG addr from CVT
* Return amount of physical memory (loaded into R3).
*
         AIF  ('&SYSPARM' EQ 'BIT64').JMP3
         EDCXEPLG
         AGO  .JMP4
.JMP3    ANOP
         CELQEPLG
.JMP4    ANOP
*
         LTORG ,
*
         IHAPSA
         CVT DSECT=YES
*
         END   ,
