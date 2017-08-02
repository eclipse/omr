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

add_executable(omrthreadtest
	abortTest.cpp
	CEnterExit.cpp
	CMonitor.cpp
	createTest.cpp
	CThread.cpp
	joinTest.cpp
	keyDestructorTest.cpp
	lockedMonitorCountTest.cpp
	main.cpp
	ospriority.cpp
	priorityInterruptTest.cpp
	rwMutexTest.cpp
	sanityTest.cpp
	sanityTestHelper.cpp
	threadTestHelp.cpp
)
omr_set_target_flags(omrthreadtest)

#TODO Unported makefile fragment:
#ifeq (1,$(OMR_THR_FORK_SUPPORT))
  #OBJECTS += forkResetTest forkResetRWMutexTest
#endif

#OBJECTS := $(addsuffix $(OBJEXT),$(OBJECTS))

#MODULE_INCLUDES +=  $(top_srcdir)/fvtest/util
#MODULE_INCLUDES += $(OMR_GTEST_INCLUDES) $(top_srcdir)/thread
#MODULE_CXXFLAGS += $(OMR_GTEST_CXXFLAGS)

#vpath argmain.cpp $(top_srcdir)/fvtest/omrGtestGlue

#MODULE_STATIC_LIBS += \
  #omrGtest \
  #testutil \
  #omrstatic

#ifeq (linux,$(OMR_HOST_OS))
  #MODULE_SHARED_LIBS += rt pthread
#endif
#ifeq (osx,$(OMR_HOST_OS))
  #MODULE_SHARED_LIBS += iconv pthread
#endif
#ifeq (aix,$(OMR_HOST_OS))
  #MODULE_SHARED_LIBS += iconv perfstat
#endif
#ifeq (win,$(OMR_HOST_OS))
  #MODULE_SHARED_LIBS += ws2_32 shell32 Iphlpapi psapi pdh
#endif

#include $(top_srcdir)/omrmakefiles/rules.mk

target_link_libraries(omrthreadtest
    PRIVATE
	omrtestutil
	omrGtestGlue
	j9thrstatic
	j9hashtable
)
if(OMR_HOST_OS STREQUAL "win")
    target_link_libraries(omrthreadtest
        PRIVATE
        ws2_32
        shell32
        Iphlpapi
        psapi
        pdh
    )
endif()

if(OMR_HOST_OS STREQUAL "zos")
	target_link_libraries(omrthreadtest PRIVATE j9a2e)
endif()

add_test(NAME threadtest COMMAND omrthreadtest)
#TODO need to add other execution targets
#	./omrthreadtest --gtest_also_run_disabled_tests --gtest_filter=ThreadCreateTest.DISABLED_SetAttrThreadWeight
#ifneq (,$(findstring linux,$(SPEC)))
#	./omrthreadtest --gtest_filter=ThreadCreateTest.*:$(GTEST_FILTER) -realtime
#endif

