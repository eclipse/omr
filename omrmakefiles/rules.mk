###############################################################################
# Copyright (c) 2015, 2019 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
###############################################################################

###
### rules.mk
###
# This file should be included by each subdirectory makefile.
# It specifies rules for building object files, executables and libraries.
#
# To use this file, specify the following variables:
#
# MODULE_NAME = <module name>
#
# ARTIFACT_TYPE = <type>
#      cxx_executable - executable, linked with the C++ linker
#      c_executable   - executable, linked with the C linker
#      cxx_shared     - shared library, linked with the C++ linker
#      c_shared       - shared library, linked with the C linker
#      archive        - archive library
#
# OBJECTS = <list of objects>
#      The list of object files to include in the artifact.
#

###
### Default Targets
###
# Define these before any targets that might be defined in the
# platform-specific rules.

all:

clean:
	$(CLEAN_COMMAND)
.PHONY: all clean

show-objects:
	@echo $(OBJECTS)
.PHONY: show-objects

ddrgen: $(OBJECTS:$(OBJEXT)=.i)
.PHONY: ddrgen

##
## Helpers
##
# In this section, we define defaults which assume a gcc and linux like
# interface.  We override these afterwords for other compilers and platforms.

# Include Path Flag and Library Search Path Flag
buildCPPIncludeFlags = $(foreach path,$(1),-I$(path))
buildASIncludeFlags = $(foreach path,$(1),-I$(path))
buildLibPathFlags = $(foreach path,$(1),-L$(path))

ifeq (msvc,$(OMR_TOOLCHAIN))
  buildCPPIncludeFlags = $(foreach path,$(1),-I$(path))
  buildASIncludeFlags = $(foreach path,$(1),/I$(path))
  buildLibPathFlags = $(foreach path,$(1),/LIBPATH:$(path))
endif

# Library Linking and Link Groups
buildStaticLibLinkFlags = $(foreach lib,$(1),-l$(lib))
buildSharedLibLinkFlags = $(foreach lib,$(1),-l$(lib))

ifneq (osx,$(OMR_HOST_OS))
LINK_GROUP_START:=-Xlinker --start-group
LINK_GROUP_END:=-Xlinker --end-group
endif
ifeq (xlc,$(OMR_TOOLCHAIN))
  LINK_GROUP_START:=-Wl,--start-group
  LINK_GROUP_END:=-Wl,--end-group
endif
buildLinkGroup = $(if $(1),$(LINK_GROUP_START) $(1) $(LINK_GROUP_END),)

ifeq (aix,$(OMR_HOST_OS))
  buildLinkGroup = $(1)
endif
ifeq (msvc,$(OMR_TOOLCHAIN))
  # .lib is the Windows import library
  buildSharedLibLinkFlags = $(foreach lib,$(1),$(LIBPREFIX)$(lib).lib)
  buildStaticLibLinkFlags = $(foreach lib,$(1),$(lib)$(ARLIBEXT))
  buildLinkGroup = $(1)
endif
ifeq (zos,$(OMR_HOST_OS))
  # .x is the z/os export library
  buildSharedLibLinkFlags = $(foreach lib,$(1),$(lib_output_dir)/$(LIBPREFIX)$(lib).x)
  # z/OS linker does not support link groups.  We use this feature on other
  # platforms to help satisfy (sometimes circular) link dependencies. On z/OS
  # we instead repeat static libraries 4 times.  3 was not enough.
  buildLinkGroup = $(if $(1),$(1) $(1) $(1) $(1),)
endif

# Build filenames for artifacts
ifeq (msvc,$(OMR_TOOLCHAIN))
  buildStaticLibFilename = $(foreach lib,$(1),$(lib_output_dir)/$(lib).lib)
  buildExeFilename = $(foreach exe,$(1),$(exe_output_dir)/$(exe).exe)
else
  buildStaticLibFilename = $(foreach lib,$(1),$(lib_output_dir)/$(LIBPREFIX)$(lib)$(ARLIBEXT))
  buildExeFilename = $(foreach exe,$(1),$(exe_output_dir)/$(exe))
endif

###
### Build platform-specific options for include paths, library paths, static
### libraries and shared libraries
###

# Includes
MODULE_CPPFLAGS += $(call buildCPPIncludeFlags,$(MODULE_INCLUDES))
GLOBAL_CPPFLAGS += $(call buildCPPIncludeFlags,$(GLOBAL_INCLUDES))
MODULE_ASFLAGS += $(call buildASIncludeFlags,$(MODULE_AS_INCLUDES))
GLOBAL_ASFLAGS += $(call buildASIncludeFlags,$(GLOBAL_AS_INCLUDES))

# LibPath
MODULE_LDFLAGS += $(call buildLibPathFlags,$(MODULE_LIBPATH))
GLOBAL_LDFLAGS += $(call buildLibPathFlags,$(GLOBAL_LIBPATH))

# Static Libraries
ifeq (zos,$(OMR_HOST_OS))
  # zOS has really strange dependencies on the order of options.  Create separate variables so they
  # can be added at the correct place on the link lines.
  LD_STATIC_LIBS = $(call buildLinkGroup,$(call buildStaticLibLinkFlags,$(MODULE_STATIC_LIBS)))
  LD_STATIC_LIBS += $(call buildLinkGroup,$(call buildStaticLibLinkFlags,$(GLOBAL_STATIC_LIBS)))
else
  MODULE_LDFLAGS += $(call buildLinkGroup,$(call buildStaticLibLinkFlags,$(MODULE_STATIC_LIBS)))
  GLOBAL_LDFLAGS += $(call buildLinkGroup,$(call buildStaticLibLinkFlags,$(GLOBAL_STATIC_LIBS)))
endif

# Shared Libraries. Must be after static libraries.
ifeq (zos,$(OMR_HOST_OS))
  # zOS has really strange dependencies on the order of options.  Create separate variables so they
  # can be added at the correct place on the link lines.
  LD_SHARED_LIBS = $(call buildSharedLibLinkFlags,$(MODULE_SHARED_LIBS))
  LD_SHARED_LIBS += $(call buildSharedLibLinkFlags,$(GLOBAL_SHARED_LIBS))
else
  MODULE_LDFLAGS += $(call buildSharedLibLinkFlags,$(MODULE_SHARED_LIBS))
  GLOBAL_LDFLAGS += $(call buildSharedLibLinkFlags,$(GLOBAL_SHARED_LIBS))
endif

###
### Default build commands
###

define COMPILE_C_COMMAND
$(CC) $(CPPFLAGS) $(MODULE_CPPFLAGS) $(GLOBAL_CPPFLAGS) -c $(GLOBAL_CFLAGS) $(MODULE_CFLAGS) $(CFLAGS) -o $@ $<
endef

define COMPILE_CXX_COMMAND
$(CXX) $(CPPFLAGS) $(MODULE_CPPFLAGS) $(GLOBAL_CPPFLAGS) -c $(GLOBAL_CXXFLAGS) $(MODULE_CXXFLAGS) $(CXXFLAGS) -o $@ $<
endef

define AS_COMMAND
$(AS) -o $@ $(GLOBAL_ASFLAGS) $(MODULE_ASFLAGS) $(ASFLAGS) $<
endef

# For the gcc toolchain, the order of link artifacts matters.
define LINK_C_EXE_COMMAND
$(CCLINKEXE) -o $@ $(OBJECTS) $(LDFLAGS) $(MODULE_LDFLAGS) $(GLOBAL_LDFLAGS)
endef

define LINK_CXX_EXE_COMMAND
$(CXXLINKEXE) -o $@ $(OBJECTS) $(LDFLAGS) $(MODULE_LDFLAGS) $(GLOBAL_LDFLAGS)
endef

define GENERATE_EXPORT_SCRIPT_COMMAND
sh $(top_srcdir)/omrmakefiles/generate-exports.sh gcc $(MODULE_NAME) $(EXPORT_FUNCTIONS_FILE) $($(MODULE_NAME)_LINKER_EXPORT_SCRIPT)
endef

define LINK_C_SHARED_COMMAND
$(CCLINKSHARED) -o $@ $(OBJECTS) $(LDFLAGS) $(MODULE_LDFLAGS) $(GLOBAL_LDFLAGS)
endef

define LINK_CXX_SHARED_COMMAND
$(CXXLINKSHARED) -o $@ $(OBJECTS) $(LDFLAGS) $(MODULE_LDFLAGS) $(GLOBAL_LDFLAGS)
endef

define AR_COMMAND
$(AR) $(ARFLAGS) $(MODULE_ARFLAGS) $(GLOBAL_ARFLAGS) rcv $@ $(OBJECTS)
endef

ifeq (linux_ztpf,$(OMR_HOST_OS))
define CLEAN_COMMAND
-$(RM) $(OBJECTS) $(OBJECTS:$(OBJEXT)=.i) $(OBJECTS:$(OBJEXT)=.lst) *.d
endef
else
define CLEAN_COMMAND
-$(RM) $(OBJECTS) $(OBJECTS:$(OBJEXT)=.i) *.d
endef
endif

DDR_SED_COMMAND := \
    sed -n -e '/^DDRFILE_BEGIN /,/^DDRFILE_END /s/^/@/' -e '/^@./p'

define DDR_C_COMMAND
$(CC) $(CPPFLAGS) $(MODULE_CPPFLAGS) $(GLOBAL_CPPFLAGS) -E $< | $(DDR_SED_COMMAND) > $@
endef

define DDR_CPP_COMMAND
$(CXX) $(CPPFLAGS) $(MODULE_CPPFLAGS) $(GLOBAL_CPPFLAGS) -E $< | $(DDR_SED_COMMAND) > $@
endef

###
### Platform-Specific Options
###

ifeq (aix,$(OMR_HOST_OS))
  include $(top_srcdir)/omrmakefiles/rules.aix.mk
endif
ifeq (linux,$(OMR_HOST_OS))
  include $(top_srcdir)/omrmakefiles/rules.linux.mk
endif
ifeq (osx,$(OMR_HOST_OS))
  include $(top_srcdir)/omrmakefiles/rules.osx.mk
endif
ifeq (win,$(OMR_HOST_OS))
  include $(top_srcdir)/omrmakefiles/rules.win.mk
endif
ifeq (zos,$(OMR_HOST_OS))
  include $(top_srcdir)/omrmakefiles/rules.zos.mk
endif
ifeq (linux_ztpf,$(OMR_HOST_OS))
  include $(top_srcdir)/omrmakefiles/rules.ztpf.mk
endif

###
### Executable Rules
###

ifeq ($(ARTIFACT_TYPE),c_executable)

$(MODULE_NAME)_executable := $(exe_output_dir)/$(MODULE_NAME)$(EXEEXT)
all: $($(MODULE_NAME)_executable)
$($(MODULE_NAME)_executable): $(OBJECTS) $(DEPENDENCIES)
	$(LINK_C_EXE_COMMAND)

# Clean executable when `make clean`
$(MODULE_NAME)_cleanexecutable:
	-$(RM) $($(MODULE_NAME)_executable)
ifeq (win,$(OMR_HOST_OS))
	-$(RM) $($(MODULE_NAME)_executable:%$(EXEEXT)=%.pdb)
endif
clean: $(MODULE_NAME)_cleanexecutable
.PHONY: $(MODULE_NAME)_cleanexecutable

endif

ifeq ($(ARTIFACT_TYPE),cxx_executable)

$(MODULE_NAME)_executable := $(exe_output_dir)/$(MODULE_NAME)$(EXEEXT)
all: $($(MODULE_NAME)_executable)
$($(MODULE_NAME)_executable): $(OBJECTS) $(DEPENDENCIES)
	$(LINK_CXX_EXE_COMMAND)

# Clean executable when `make clean`
$(MODULE_NAME)_cleanexecutable:
	-$(RM) $($(MODULE_NAME)_executable)
ifeq (win,$(OMR_HOST_OS))
	-$(RM) $($(MODULE_NAME)_executable:%$(EXEEXT)=%.pdb)
endif
clean: $(MODULE_NAME)_cleanexecutable
.PHONY: $(MODULE_NAME)_cleanexecutable

endif

###
### Shared Library Rules
###

## C Shared Library
ifeq ($(ARTIFACT_TYPE),c_shared)

$(MODULE_NAME)_shared := $(exe_output_dir)/$(LIBPREFIX)$(MODULE_NAME)$(SOLIBEXT)
all: $($(MODULE_NAME)_shared)
$($(MODULE_NAME)_shared): $(OBJECTS) $(DEPENDENCIES)
	$(LINK_C_SHARED_COMMAND)

# clean the library when `make clean`
$(MODULE_NAME)_cleanlib:
	-$(RM) $($(MODULE_NAME)_shared)
ifeq (win,$(OMR_HOST_OS))
	-$(RM) $($(MODULE_NAME)_shared:%$(SOLIBEXT)=%.pdb)
endif
clean: $(MODULE_NAME)_cleanlib
.PHONY: $(MODULE_NAME)_cleanlib

endif

## C++ Shared Library
ifeq ($(ARTIFACT_TYPE),cxx_shared)

$(MODULE_NAME)_shared := $(exe_output_dir)/$(LIBPREFIX)$(MODULE_NAME)$(SOLIBEXT)
all: $($(MODULE_NAME)_shared)
$($(MODULE_NAME)_shared): $(OBJECTS) $(DEPENDENCIES)
	$(LINK_CXX_SHARED_COMMAND)

# clean the library when `make clean`
$(MODULE_NAME)_cleanlib:
	-$(RM) $($(MODULE_NAME)_shared)
ifeq (win,$(OMR_HOST_OS))
	-$(RM) $($(MODULE_NAME)_shared:%$(SOLIBEXT)=%.pdb)
endif
clean: $(MODULE_NAME)_cleanlib
.PHONY: $(MODULE_NAME)_cleanlib

endif

## Exported Function Rules
# Some platforms may not need an exportfile, and will not have defined a value.
# Skip this rule if it does not need to be created.
ifneq (,$(findstring shared,$(ARTIFACT_TYPE)))
ifneq (,$($(MODULE_NAME)_LINKER_EXPORT_SCRIPT))
$($(MODULE_NAME)_shared): $($(MODULE_NAME)_LINKER_EXPORT_SCRIPT)
$($(MODULE_NAME)_LINKER_EXPORT_SCRIPT): $(EXPORT_FUNCTIONS_FILE)
	$(GENERATE_EXPORT_SCRIPT_COMMAND)

clean: $(MODULE_NAME)_cleanexports
$(MODULE_NAME)_cleanexports:
	-$(RM) $($(MODULE_NAME)_LINKER_EXPORT_SCRIPT)
.PHONY: $(MODULE_NAME)_cleanexports
endif
endif

###
### Archive Library Rules
###

ifeq ($(ARTIFACT_TYPE),archive)

$(MODULE_NAME)_static := $(lib_output_dir)/$(LIBPREFIX)$(MODULE_NAME)$(ARLIBEXT)
all: $($(MODULE_NAME)_static)
$($(MODULE_NAME)_static): $(OBJECTS) $(DEPENDENCIES)
	$(AR_COMMAND)

# clean the library when `make clean`
$(MODULE_NAME)_cleanlib:
	-$(RM) $($(MODULE_NAME)_static)
ifeq (win,$(OMR_HOST_OS))
	-$(RM) $($(MODULE_NAME)_static:%$(ARLIBEXT)=%.pdb)
endif
clean: $(MODULE_NAME)_cleanlib
.PHONY: $(MODULE_NAME)_cleanlib

endif

###
### Object File Rules
###

# Disable built-in rules, enable the rules we support
.SUFFIXES:

# C
%$(OBJEXT): %.c
	$(COMPILE_C_COMMAND)
ifeq (linux_ztpf,$(OMR_HOST_OS))
	tpfobjpp -O ONotApplicable -g gNotApplicable $@
endif

# C++
%$(OBJEXT): %.cpp
	$(COMPILE_CXX_COMMAND)
ifeq (linux_ztpf,$(OMR_HOST_OS))
	tpfobjpp -O ONotApplicable -g gNotApplicable $@
endif

# Assembly
%$(OBJEXT): %.s
	$(AS_COMMAND)

# Windows assembly
%$(OBJEXT): %.asm
	$(AS_COMMAND)

###
### DDR Rules
###
%.i: %.c
	$(DDR_C_COMMAND)

%.i: %.cpp
	$(DDR_CPP_COMMAND)

# just create empty output files for other types of source files
$(foreach E, asm mc s, $(eval %.i : %.$E ; touch $$@))
