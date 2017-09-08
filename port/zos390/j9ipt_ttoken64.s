***********************************************************************
* Copyright (c) 2015, 2016 IBM Corp. and others
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

         TITLE 'j9ipt_ttoken64.s'

R1       EQU   1
R2       EQU   2
R3       EQU   3
R8       EQU   8
R9       EQU   9
R10      EQU   10
R15      EQU   15
*
BUF_SZ   EQU   16
*
*=====================================================================
* int32_t omrget_ipt_ttoken(char * buf, uint32_t len);
*
* @brief Retrieve the IPT ttoken for an LE process or thread (64-bit
* only).
* @param[in] buf Buffer where ttoken should be written 
* @param[in] len Length of buf. It should be at least BUF_SZ.
* @return -1 in case of error. Otherwise, 0
*=====================================================================

         AIF   ('&SYSPARM' EQ 'BIT64').JMP1
         AGO .JMP2
.JMP1    ANOP
J9IPTTKN CELQPRLG PARMWRDS=2,BASEREG=R8,EXPORT=YES
         SYSSTATE ARCHLVL=2
J9IPTTKN ALIAS C'omrget_ipt_ttoken'
*
         LGHI      R3,-1                  Default ret code is -1 
*
* if (NULL == buf) || (BUF_SZ > len) : goto DONE
         CGHI     R1,0
         JE       DONE
         CHI      R2,BUF_SZ
         JL       DONE
*
         LLGT     R10,PSALAA-PSA(,0)      Get current LAA
         USING    CEELAA,R10
         LLGT     R8,CEELAA_IPTLAA        Get LAA for IPT
         DROP     R10
*
         USING    CEELAA,R8               Switch to IPT LAA
         LLGT     R9,CEELAA_STCB          Get STCB for IPT
         USING    STCB,R9
*
         MVC      0(BUF_SZ,R1),STCBTTKN
         DROP     R8,R9
         XGR      R3,R3
*
DONE     DS       0H
*
         CELQEPLG
*
         LTORG ,
*
         IHAPSA
         IHASTCB
         CEELAA
*
         AGO .JMP3
.JMP2    ANOP
* Code is only meant to be used on the 64-bit build
.JMP3    ANOP
         END
