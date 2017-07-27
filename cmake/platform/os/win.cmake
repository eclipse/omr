##############################################################################
#
# (c) Copyright IBM Corp. 2017
#
#  This program and the accompanying materials are made available
#  under the terms of the Eclipse Public License v1.0 and
#  Apache License v2.0 which accompanies this distribution.
#
#      The Eclipse Public License is available at
#      http://www.eclipse.org/legal/epl-v10.html
#
#      The Apache License v2.0 is available at
#      http://www.opensource.org/licenses/apache2.0.php
#
# Contributors:
#    Multiple authors (IBM Corp.) - initial implementation and documentation
###############################################################################


set(OMR_OS_DEFINITIONS 
   WIN32
   _CRT_SECURE_NO_WARNINGS
   CRTAPI1=_cdecl
   CRTAPI2=_cdecl
   _WIN_95
   _WIN32_WINDOWS=0x0500
   _WIN32_DCOM
   _MT
   _WINSOCKAPI_
   _WIN32_WINVER=${OMR_WINVER}
   _WIN32_WINNT=${OMR_WINVER}
   _DLL
   _HAS_EXCEPTIONS=0
   )

if(OMR_ENV_DATA64)
   list(APPEND OMR_OS_DEFINITIONS 
      WIN64
      _AMD64_=1)
else()
   list(APPEND OMR_OS_DEFINITIONS 
      _X86_)
endif()


