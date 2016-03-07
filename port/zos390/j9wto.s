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

         TITLE 'j9wto.s'

         AIF ('&SYSPARM' EQ 'BIT64').JMP1
_WTO     EDCXPRLG BASEREG=8
         LR    3,1
         AGO .JMP2
.JMP1    ANOP
_WTO     CELQPRLG BASEREG=8
         LR    3,1
         SAM31
.JMP2    ANOP
         WTO   MF=(E,(3))
         AIF ('&SYSPARM' EQ 'BIT64').JMP3
         EDCXEPLG
         AGO .JMP4
.JMP3    ANOP
         SAM64
         CELQEPLG
.JMP4    ANOP
*
         END
