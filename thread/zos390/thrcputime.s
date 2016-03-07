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
         TITLE 'thrcputime.s'

         AIF ('&SYSPARM' EQ 'BIT64').JMP1
_CPUTIME EDCXPRLG BASEREG=8
         LR    3,1
         TIMEUSED ECT=COND,LINKAGE=SYSTEM,STORADR=(3)
         EDCXEPLG
         AGO .JMP2
.JMP1    ANOP
_CPUTIME CELQPRLG BASEREG=8
         LGR    2,1
         SYSSTATE AMODE64=YES
         TIMEUSED ECT=COND,LINKAGE=SYSTEM,STORADR=(2)
         CELQEPLG
.JMP2    ANOP
*
         IHAPSA
         CVT DSECT=YES
         IHAECVT
         END
