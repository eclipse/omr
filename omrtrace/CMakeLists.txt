###############################################################################
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

add_library(omrtrace STATIC
	omrtraceapi.cpp
	omrtracecomponent.cpp
	omrtraceformatter.cpp
	omrtracelog.cpp
	omrtracemain.cpp
	omrtracemisc.cpp
	omrtraceoptions.cpp
	omrtracepublish.cpp
	omrtracewrappers.cpp
)
omr_set_target_flags(omrtrace)

target_link_libraries(omrtrace
	PUBLIC
	omrutil
)
#TODO: check if following makefile fragment still required:
#ifeq (gcc,$(OMR_TOOLCHAIN))
## disable builtin strncpy buffer length check for optimized builds (_FORTIFY_SOURCE=1 requires -O3)
  #ifeq ($(OMR_OPTIMIZE),1)
    #MODULE_CXXFLAGS+=-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=1
  #endif
#endif
