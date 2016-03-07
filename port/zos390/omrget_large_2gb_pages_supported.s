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

         TITLE 'Get_Large_2GB_Pages_Supported'

R0       EQU   0
R1       EQU   1      Input: nothing
R2       EQU   2
R3       EQU   3
R6       EQU   6      Base register
*
* SYSPARM is set in makefile,  CFLAGS+= -Wa,SYSPARM\(BIT64\)
* to get 64 bit bit prolog and epilog.  Delete SYSPARM from makefile
* to get 31 bit version of xplink enabled prolog and epilog.
         AIF  ('&SYSPARM' EQ 'BIT64').JMP1
GL2GBPS  EDCXPRLG BASEREG=6,DSASIZE=0
         AGO  .JMP2
.JMP1    ANOP
GL2GBPS  CELQPRLG BASEREG=6,DSASIZE=0
.JMP2    ANOP
*
         LA    R1,0           Clear working register
         LA    R2,0           Clear working register
         LA    R3,0           Clear returned value register
*
         USING PSA,R0         Map PSA
         L     R1,FLCCVT      Get CVT
         USING CVT,R1         Map CVT
* Test for CVTEDAT2 bit in CVTFLAG4 field of CVT data area
* This is required to check EDAT2 support is active on the system
         TM    CVTFLAG4,X'01'
         BZ    EXIT
         L     R2,CVTRCEP     Load RCE addr from CVT
         USING RCE,R2         Map RCE
* Test for RCEFEAT5ENAB bit in RCEFLAGS3 field of RCE data area
* This is required to check RSM supports 2G pages
         TM    RCEFLAGS3,X'08'
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
         IARRCE
         CVT DSECT=YES
*
         END   ,
