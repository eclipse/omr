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
         J9ZOS390
         LONGLONG
         J9VM_TIERED_CODE_CACHE
         _ALL_SOURCE
         _XOPEN_SOURCE_EXTENDED
         IBM_ATOE
         _POSIX_SOURCE
         )

if(OMR_ENV_DATA64)
   list(APPEND OMR_OS_DEFINITIONS DJ9ZOS39064)
else()
   list(APPEND OMR_OS_DEFINITIONS _LARGE_FILES)
endif()

set(OMR_OS_COMPILE_OPTIONS)
