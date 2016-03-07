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
