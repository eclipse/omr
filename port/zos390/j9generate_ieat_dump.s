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

         TITLE 'j9generate_ieat_dump.s'

         AIF ('&SYSPARM' EQ 'BIT64').JMP1
_TDUMP   EDCXPRLG BASEREG=8
*  IEATDUMP macro toast R1, so keep it in R3
         LR    3,1
         USING IOPARMS,3
         CALL  BPX1ENV,                                                +
               (=A(ENV_TOGGLE_SEC),                                    +
               =A(0),                                                  +
               =A(0),                                                  +
               =A(0),                                                  +
               =A(0),                                                  +
               RETVAL,                                                 +
               RETCODE,                                                +
               RSNCODE),                                               +
               VL,MF=(E,PLIST)
         AGO .JMP2
.JMP1    ANOP
_TDUMP    CELQPRLG BASEREG=8
*  IEATDUMP macro toast R1, so keep it in R3
         LR    3,1
         USING IOPARMS,3
         CALL  BPX4ENV,                                                +
               (=A(ENV_TOGGLE_SEC),                                    +
               =AD(0),                                                 +
               =A(0),                                                  +
               =AD(0),                                                 +
               =A(0),                                                  +
               RETVAL,                                                 +
               RETVAL,                                                 +
               RETVAL),                                                +
               MF=(E,PLIST)
         SAM31
.JMP2    ANOP

* Note: the SDATA options below are echoed in the IEATDUMP progress 
* message in tdump() in omrosdump.c. That message needs to be kept 
* consistent with the actual options specified here.
         IEATDUMP DSN=(2),HDR=DUMPTITL,RETCODE=RETCODE,RSNCODE=RSNCODE,+
               SDATA=(LPA,GRSQ,LSQA,NUC,PSA,RGN,SQA,SUM,SWA,TRT),      +
               MF=(E,PLIST)

         AIF ('&SYSPARM' EQ 'BIT64').JMP3
         CALL  BPX1ENV,                                                +
               (=A(ENV_TOGGLE_SEC),                                    +
               =A(0),                                                  +
               =A(0),                                                  +
               =A(0),                                                  +
               =A(0),                                                  +
               RETVAL,                                                 +
               RETCODE,                                                +
               RSNCODE),                                               +
               VL,MF=(E,PLIST)
         EDCXEPLG
         AGO .JMP4
.JMP3    ANOP
         SAM64
         CALL  BPX4ENV,                                                +
               (=A(ENV_TOGGLE_SEC),                                    +
               =AD(0),                                                 +
               =A(0),                                                  +
               =AD(0),                                                 +
               =A(0),                                                  +
               RETVAL,                                                 +
               RETVAL,                                                 +
               RETVAL),                                                +
               MF=(E,PLIST)
         CELQEPLG
.JMP4    ANOP

*
DUMPTITL DC    AL1(DMPTLLEN)
         DC    C'TRANSACTION DUMP TO AN OPEN DCB'
DMPTLLEN EQU   *-DUMPTITL-1

ENV_TOGGLE_SEC       EQU   4    Toggle btw task/process security

IOPARMS  DSECT
PLIST    DS    256AD                 Calling parameter list
RETCODE  DS    A                     Return code
RSNCODE  DS    A                     Reason code
RETVAL   DS    A                     Return value - next PROCTOKEN

         END
