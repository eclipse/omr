***********************************************************************
* Copyright (c) 1991, 2012 IBM Corp. and others
* 
* This program and the accompanying materials are made available
*  under the terms of the Eclipse Public License 2.0 which 
* accompanies this distribution and is available at 
* https://www.eclipse.org/legal/epl-2.0/ or the 
* Apache License, Version 2.0 which accompanies this distribution
*  and is available at https://www.apache.org/licenses/LICENSE-2.0.
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

         TITLE 'GETPSW.S'

R2       EQU 2 *
R3       EQU 3 * output: the return code containing the PSW goes in register 3

* Jump to 64 bit prolog code if on z/OS 64, otherwise use 31
         AIF ('&SYSPARM' EQ 'BIT64').JMP1

* _GETPSW is the symbol that the caller uses
*        EDCXPRLG is the macro to generate
*          31 bit XPLINK assembler prolog code
_GETPSW  EDCXPRLG BASEREG=8

*        Jump over 64 bit prolog and extract the PSW
         AGO .JMP2

.JMP1    ANOP

* _GETPSW is the symbol that the caller uses
*        CELQPRLG is the macro to generate
*          64 bit XPLINK assembler prolog code
_GETPSW  CELQPRLG BASEREG=8
*        switch to 31 bit addressing mode
*         this truncate bits 0 through 32 which are unused in EPSW
         SAM31

.JMP2    ANOP
*        Extract PSW
*         See z/Architecture Principles of Operation 7-210
         EPSW R3,R2

*        Jump to 64 bit epilog code if on z/OS 64, otherwise use 31
         AIF ('&SYSPARM' EQ 'BIT64').JMP3

*        XPLINK 31 bit epilog macro
         EDCXEPLG
         AGO .JMP4

.JMP3    ANOP

*        switch back to 64 bit addressing mode
         SAM64
*        XPLINK 64 bit epilog macro
         CELQEPLG


.JMP4    ANOP

*
         END
