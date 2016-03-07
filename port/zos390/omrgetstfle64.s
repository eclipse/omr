***********************************************************************
*
* (c) Copyright IBM Corp. 2013, 2016
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

* This is a zOS 64 helper that stores the STFLE bits
* into the passed memory reference.  Called as a C func
* from C++ code.
*
* The helper returns the number of valid double-words in the STFLE
* bits, minus 1.
*
* The stfle instruction is encoded as 0xb2b00000 in binary, leaving
* it in as such so that we can compile on any platform.

        TITLE 'omrgetstfle64.s'

CARG1 EQU 1
CRA EQU 7
RETURNOFFSET EQU 2
r0 EQU 0
r3 EQU 3

Z_GSTFLE  DS 0D
     ENTRY Z_GSTFLE
Z_GSTFLE  ALIAS C'getstfle'
Z_GSTFLE  XATTR SCOPE(X),LINKAGE(XPLINK)
Z_GSTFLE      AMODE 64

  lgr   r0,CARG1
  DC X'b2b02000'
  lgr   r3,r0
  b  RETURNOFFSET(CRA)
