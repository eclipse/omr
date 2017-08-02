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

add_library(omrsig SHARED
	omrsig.cpp
)
omr_set_target_flags(omrsig)

#TODO need more comprehensive symbols
set(exportedSyms
	omrsig_primary_signal
	omrsig_handler
	signal
)

#for now only export symbols on windows
if(OMR_HOST_OS STREQUAL "win")
	file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/omrsig.exportlist" "EXPORTS\n" )
	foreach(exportSymbol ${exportedSyms})
		file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/omrsig.exportlist" "${exportSymbol}\n" )
	endforeach()
	set_target_properties(omrsig
		PROPERTIES LINK_FLAGS "-def:${CMAKE_CURRENT_BINARY_DIR}/omrsig.exportlist")
	set(CRT_DLL "MSVC100.dll")
	#TODO: below assumes we are using debug builds
	if(MSVC12)
		set(CRT_DLL "MSVC120d.dll")
	elseif(MSVC14)
		set(CRT_DLL "ucrtbased.dll")
	endif()
	target_compile_definitions(omrsig PRIVATE -DMSVC_RUNTIME_DLL="${CRT_DLL}")
endif()

#Symbols exported from source makefile:
#EXPORT_FUNCTIONS_FILE := omrsig.exportlist
#$(EXPORT_FUNCTIONS_FILE):
	#@# all
	#@echo omrsig_primary_signal >>$@
	#@echo omrsig_handler >>$@
	#@echo signal >>$@
#ifeq (,$(findstring win,$(OMR_HOST_OS)))
	#@# !windows
	#@echo omrsig_primary_sigaction >>$@
	#@echo sigaction >>$@
	#@echo sigset >>$@
	#@echo sigignore >>$@
	#@echo bsd_signal >>$@
	#@echo sysv_signal >>$@
#endif
#ifneq (,$(findstring linux,$(OMR_HOST_OS)))
	#@# linux
	#@echo __sysv_signal >>$@
	#@echo ssignal >>$@
#endif
#ifneq (,$(findstring osx,$(OMR_HOST_OS)))
	#@# osx
	#@echo __sysv_signal >>$@
	#@echo ssignal >>$@
#endif
#ifneq (,$(findstring zos,$(OMR_HOST_OS)))
	#@# z/os
	#@echo __sigactionset >>$@
#endif
#ifeq (,$(findstring win,$(OMR_HOST_OS)))
#ifeq (,$(findstring zos,$(OMR_HOST_OS)))
	#@# !win && !zos
	#@echo sigvec >>$@
#endif
#endif

target_compile_options(omrsig PRIVATE ${OMR_EXPORT_ALL})
target_link_libraries(omrsig  PRIVATE omrutil)

#TODO ensure we are linking in the same way as source makefile:
#MODULE_STATIC_LIBS += omrutil
#ifneq (,$(findstring linux,$(OMR_HOST_OS)))
    #MODULE_SHARED_LIBS += pthread
#endif
#ifneq (,$(findstring osx,$(OMR_HOST_OS)))
    #MODULE_SHARED_LIBS += pthread
#endif

