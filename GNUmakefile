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

top_srcdir:=.
include $(top_srcdir)/omrmakefiles/configure.mk

###
### Global targets
###

all: postbuild
.DEFAULT: all
clean:
.PHONY: all clean

help:
	@echo "help   Display this help message."
	@echo "all    Build OMR."
	@echo "clean  Clean OMR build artifacts."
	@echo "test   Run functional verification tests."
	@echo "lint   Run the OMR linter."
.PHONY: help

###
### Tracegen
###
ifeq (yes,$(ENABLE_TRACEGEN))
define TRACEGEN_COMMAND
cd $(exe_output_dir) && ./tracegen -treatWarningAsError -generatecfiles -threshold 1 -root $(top_srcdir)
endef

define TRACEMERGE_COMMAND
cd $(exe_output_dir) && ./tracemerge -majorversion 5 -minorversion 1 -root $(top_srcdir)
endef
else
define TRACEGEN_COMMAND
@echo Skipping tracegen
endef

define TRACEMERGE_COMMAND
@echo Skipping tracemerge
endef
endif


# ASCII To EBCDIC Targets
ifeq (zos,$(OMR_HOST_OS))
tool_targets += util/a2e
endif

tool_targets += tools/hookgen

HOOK_DEFINITION_FILES := $(abspath ./gc/base/omrmmprivate.hdf ./gc/include/omrmm.hdf ./fvtest/algotest/hooksample.hdf)
HOOK_DEFINITION_SENTINEL := $(patsubst %.hdf,%.sentinel, $(HOOK_DEFINITION_FILES))
define HOOKGEN_COMMAND
cd $(exe_output_dir) && ./hookgen $<
endef

# Trace Build Tools
ifeq (yes,$(ENABLE_TRACEGEN))
tool_targets += tools/tracegen
main_targets += tools/tracemerge
endif

# FVTest Helper Libraries
test_prereqs := third_party/pugixml-1.5 fvtest/util fvtest/omrGtestGlue
test_targets += $(test_prereqs)

# Utility Libraries
main_targets +=  util/omrutil util/pool util/avl util/hashtable util/hookable
test_targets += \
  fvtest/algotest \
  fvtest/utiltest

# Thread Targets
ifeq (1,$(OMR_THREAD))
main_targets += thread
test_targets += fvtest/threadtest
test_targets += fvtest/threadextendedtest
endif

# OMR GLue Target
main_targets += omr_glue_static_lib

# Garbage Collection Targets
ifeq (1,$(OMR_GC))
main_targets += \
  gc/base \
  gc/base/standard \
  gc/startup \
  gc/stats \
  gc/structs \
  gc/verbose \
  gc/verbose/handler_standard
test_targets += fvtest/gctest
test_targets += perftest/gctest
endif

# Omrsig Targets
ifeq (1,$(OMR_OMRSIG))
main_targets += omrsigcompat
test_targets += fvtest/sigtest
endif

# Portlibrary Targets
ifeq (1,$(OMR_PORT))
main_targets += port
test_targets += fvtest/porttest
test_targets += fvtest/porttest/sltestlib
ifeq (aix,$(OMR_HOST_OS))
test_targets += fvtest/porttest/aixbaddep
endif
endif

# RAS Libraries
main_targets += omrtrace

# OMR Startup
main_targets += omr omr/startup

# DDR tools
ifeq (yes,$(ENABLE_DDR))
postbuild_targets += tools/ddrgen
targets += tools/ddrgen
endif

# RAS Tests
test_targets += fvtest/rastest

# OMR Example Targets
ifeq (1,$(OMR_EXAMPLE))
test_targets += example
endif

# VM Tests
test_targets += fvtest/vmtest

# Test Compiler
ifeq (1,$(OMR_TEST_COMPILER))
main_targets += fvtest/compilertest
endif

# JitBuilder
ifeq (1,$(OMR_JITBUILDER))
main_targets += jitbuilder
test_targets += fvtest/jitbuildertest jitbuilder/release
endif

DO_TEST_TARGET := yes
# ENABLE_FVTEST_AGENT forces rastest to build, even if fvtests are disabled.
ifeq (no,$(ENABLE_FVTEST))
DO_TEST_TARGET := no
ifeq (yes,$(ENABLE_FVTEST_AGENT))
DO_TEST_TARGET := yes
test_targets := $(test_prereqs)
test_targets += fvtest/rastest fvtest/util
endif
endif
postbuild_targets += tests

# Remove duplicates
main_targets := $(sort $(main_targets))
test_targets := $(sort $(test_targets))

targets += $(tool_targets) $(prebuild_targets) $(main_targets) omr_static_lib $(test_targets)
targets_clean := $(addsuffix _clean,$(targets))
targets_ddrgen := $(addsuffix _ddrgen,$(filter-out omr_static_lib fvtest/% perftest/% third_party/% tools/%, $(targets)))


###
### Rules
###

postbuild: $(postbuild_targets)
	$(TRACEMERGE_COMMAND)

tests: staticlib
ifeq (yes,$(DO_TEST_TARGET))
	@$(MAKE) -f GNUmakefile $(test_targets)
endif

staticlib: mainbuild
	@$(MAKE) -f GNUmakefile omr_static_lib

mainbuild: prebuild
	@$(MAKE) -f GNUmakefile $(main_targets)

prebuild: tools
	@$(MAKE) -f GNUmakefile $(prebuild_targets) $(HOOK_DEFINITION_SENTINEL)
	$(TRACEGEN_COMMAND)

tools:
	@$(MAKE) -f GNUmakefile $(tool_targets)

.PHONY: tools prebuild mainbuild staticbuild tests postbuild

###
### Inter-target Dependencies
###

# These rules must be specified before $(targets)::

# If a prereq directory is not also defined in $(targets),
# then we won't execute 'make' in the prereq directory.

ifeq (zos,$(OMR_HOST_OS))
tools/tracegen:: util/a2e
tools/tracemerge:: util/a2e
tools/hookgen:: util/a2e
endif

tools/ddrgen:: staticlib

$(HOOK_DEFINITION_SENTINEL): $(exe_output_dir)/hookgen$(EXEEXT)
%.sentinel: %.hdf
	$(HOOKGEN_COMMAND)
	touch $@

hook_definition_sentinel_clean:
	rm -f $(HOOK_DEFINITION_SENTINEL)

omrsigcompat:: util/omrutil

example:: $(test_prereqs)

fvtest/algotest::$(test_prereqs)
fvtest/gctest:: $(test_prereqs)
fvtest/jitbuildertest:: $(test_prereqs)
fvtest/porttest:: $(test_prereqs)
fvtest/rastest:: $(test_prereqs)
fvtest/sigtest:: $(test_prereqs)
fvtest/threadextendedtest:: $(test_prereqs)
fvtest/threadtest:: $(test_prereqs)
fvtest/utiltest:: $(test_prereqs)
fvtest/vmtest:: $(test_prereqs)

perftest/gctest:: $(test_prereqs)

###
### Targets
###

$(targets)::
	$(MAKE) -C $@ all
.PHONY: $(targets)

%:
	@echo Error, No rule to build target $@
	@exit -1

clean: $(targets_clean) hook_definition_sentinel_clean
$(targets_clean)::
	$(MAKE) -C $(patsubst %_clean,%,$@) clean
.PHONY: $(targets_clean)

lint:
	$(MAKE) -C fvtest/compilertest -f linter.mk
.PHONY: lint

test:
ifeq (yes,$(ENABLE_FVTEST))
	$(MAKE) -f fvtest/omrtest.mk test
else
	@echo Functional verification tests are disabled.
	@echo Enable by configuring with --enable-fvtest
endif
.PHONY: test


# preprocess ddrgen-annotated source code
ddrgen:
	$(MAKE) -f GNUmakefile $(targets_ddrgen)

$(targets_ddrgen):
	$(MAKE) -C $(patsubst %_ddrgen,%,$@) ddrgen

.PHONY: ddrgen $(targets_ddrgen)


# Rerunning configure

configure.mk: config.status configure.mk.in
	config.status

config.status: $(top_srcdir)/configure
	$(top_srcdir)/configure -C

$(top_srcdir)/configure: $(top_srcdir)/configure.ac
	cd $(top_srcdir) && autoconf
