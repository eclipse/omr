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
* SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR
* GPL-2.0 WITH Classpath-exception-2.0 OR
* LicenseRef-GPL-2.0 WITH Assembly-exception
***********************************************************************

         TITLE 'omrgetthent.s'
*

         AIF ('&SYSPARM' EQ 'BIT64').JMP4

         ENTRY GETTHENT
GETTHENT ALIAS C'getthent'
GETTHENT  EDCPRLG BASEREG=8

* load the argument values into registers R8 through R14
         LM    8,11,2124(4)
* marshall the parameter list (the address of the parameters) and execute __getthent
         CALL  BPX1GTH,(1,2,3,8,9,10,11),VL

         EDCEPIL

.JMP4    ANOP
* no 64bit implementation yet so we jump straight here
         END


        AIF ('&SYSPARM' EQ 'BIT64').JMP1

         ENTRY GETTHENT
GETTHENT EDCPRLG BASEREG=8, PARMWRDS=7
         LM    8,11,2124(4)
         CALL  BPX1GTH,(1,2,3,8,9,10,11),VL
         EDCEPIL
         DROP  8

.JMP1    ANOP
* No code yet for 64-bit
.JMP2    ANOP

         END
