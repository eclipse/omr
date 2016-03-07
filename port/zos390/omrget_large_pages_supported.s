***********************************************************************
*
* (c) Copyright IBM Corp. 1991, 2016
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

         TITLE 'Get_Large_Pages_Supported'

**********************************************************************
* 20070611               gfr Base for omrget_large_pages_supported.s
* 20031027 63548 rgf Initial port attempt to 64 bit zOS.             *
*                    This is new                                     *
* 20031208 66624 rgf Correct assignment to LH from L for CSDCPUOL.   *
**********************************************************************
R0       EQU   0
R1       EQU   1      Input: nothing
R2       EQU   2
R3       EQU   3      Output: 1 if large_pages supported, 0 otherwise
R6       EQU   6      Base register
*
* SYSPARM is set in makefile,  CFLAGS+= -Wa,SYSPARM\(BIT64\)
* to get 64 bit bit prolog and epilog.  Delete SYSPARM from makefile
* to get 31 bit version of xplink enabled prolog and epilog.
         AIF  ('&SYSPARM' EQ 'BIT64').JMP1
GETLPSUP EDCXPRLG BASEREG=6,DSASIZE=0
         AGO  .JMP2
.JMP1    ANOP
GETLPSUP CELQPRLG BASEREG=6,DSASIZE=0
.JMP2    ANOP
*
         LA    R1,0           Clear working register
         LA    R2,0           Clear working register
         LA    R3,0           Clear returned value register
*
         USING PSA,R0         Map PSA
         L     R1,FLCCVT      Get CVT
         USING CVT,R1         Map CVT
         L     R2,CVTCSD      Load CSD addr from CVT
         USING CSD,R2         Map CSD
* Check for OS level "X'40'" z/OS V1R7 (in CVTOSLV5)
* Test to see if CVTZOS_V1R7 bit is on (0x40)
*         TM    CVTOSLV5,CVTZOS_V1R7
*         BZ    EXIT
* Test for CVTZOS_V1R7 using immediate
*         TM    CVTOSLV5,X'40'
*         BZ    EXIT
* Test for CVTZOS_V1R9 using immediate as this is not defined on v1.7
         TM    CVTOSLV5,X'10'    
         BZ    EXIT
* Test to see if EDAT bit is on
         TM    CVTFLAG2,X'01'
         BZ    EXIT
* Load return value 0x1 into R3 to indicate success
         LA    R3,1
         B     EXIT           Without this branch fails at runtime
EXIT     DS    0F
         DROP  R2
         DROP  R1
         DROP  R0

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
         IHACSD
         CVT DSECT=YES
*
         END   ,
