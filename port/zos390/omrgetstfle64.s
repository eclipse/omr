***********************************************************************
* Copyright (c) 2013, 2016 IBM Corp. and others
* 
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License 2.0 which accompanies this
* distribution and is available at 
* https://www.eclipse.org/legal/epl-1.0/ or the Apache License, 
* Version 2.0 which accompanies this distribution and is available at  
* https://www.apache.org/licenses/LICENSE-2.0.
* 
* This Source Code is also Distributed under one or more Secondary 
* Licenses, as those terms are defined by the Eclipse Public License, 
*  v. 2.0: GNU General Public License, version 2 with the GNU 
* Classpath Exception [1] and GNU General Public License, version 2 
* with the OpenJDK Assembly Exception [2].
* 
* [1] https://www.gnu.org/software/classpath/license.html
* [2] http://openjdk.java.net/legal/assembly-exception.html
*
* Contributors:
*   Multiple authors (IBM Corp.) - initial API and implementation 
*   and/or initial documentation
* 
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
