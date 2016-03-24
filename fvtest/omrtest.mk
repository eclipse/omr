###############################################################################
#
# (c) Copyright IBM Corp. 2015, 2016
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

top_srcdir := .
include $(top_srcdir)/omrmakefiles/configure.mk

######################J9 Lab specific settings#################################
ifneq (,$(findstring linux_ppc,$(SPEC)))
	EXPORT_LIB=true
else
ifneq (,$(findstring linux_390,$(SPEC)))
	EXPORT_LIB=true
endif
endif

ifeq ($(EXPORT_LIB),true)
LIB_PATH:=$(SITE_PATH_STREAMROOT)/../../javatest/lib/googletest/lib/$(SPEC):$(LD_LIBRARY_PATH)
export LD_LIBRARY_PATH=$(LIB_PATH)
$(warning In $(SPEC), set LD_LIBRARY_PATH=$(LIB_PATH))
endif

######################J9 Lab specific settings#################################

all: test

omr_algotest:
	./omralgotest -avltest:fvtest/algotest/avltest.lst
	
omr_gctest:
	./omrgctest -configListFile=fvtest/gctest/configuration/fvConfigListFile.txt
	
omr_porttest:
	./omrporttest
ifneq (,$(findstring cuda,$(SPEC)))
	./omrporttest --gtest_filter="Cuda*" -earlyExit
endif
	@echo ALL $@ PASSED

omr_rastest:
	./omrrastest
	./omrsubscribertest --gtest_filter=-RASSubscriberForkTest.*
	./omrtraceoptiontest
	@echo ALL $@ PASSED
	
omr_subscriberforktest:
	./omrsubscribertest --gtest_filter=RASSubscriberForkTest.*
	
omr_sigtest:
	./omrsigtest

omr_threadextendedtest:
	./omrthreadextendedtest

omr_threadtest:
	./omrthreadtest
	./omrthreadtest --gtest_also_run_disabled_tests --gtest_filter=ThreadCreateTest.DISABLED_SetAttrThreadWeight
ifneq (,$(findstring linux,$(SPEC)))
	./omrthreadtest --gtest_filter=ThreadCreateTest.* -realtime
endif
	@echo ALL $@ PASSED

omr_utiltest:
	./omrutiltest
	
omr_vmtest:
	./omrvmtest

.NOTPARALLEL:
test: omr_algotest omr_utiltest
ifeq (1,$(OMR_EXAMPLE))
test: omr_vmtest omr_gctest omr_rastest omr_subscriberforktest
endif
ifeq (1,$(OMR_PORT))
test: omr_porttest
endif
ifeq (1,$(OMR_THREAD))
test: omr_threadtest omr_threadextendedtest
endif
ifeq (1,$(OMR_OMRSIG))
test: omr_sigtest
endif

.PHONY: all test omr_algotest omr_gctest omr_porttest omr_rastest omr_subscriberforktest omr_sigtest omr_threadextendedtest omr_threadtest omr_utiltest omr_vmtest 
