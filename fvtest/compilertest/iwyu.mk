###############################################################################
# Copyright (c) 2016, 2017 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at http://eclipse.org/legal/epl-2.0
# or the Apache License, Version 2.0 which accompanies this distribution
# and is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following Secondary
# Licenses when the conditions for such availability set forth in the
# Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
# version 2 with the GNU Classpath Exception [1] and GNU General Public
# License, version 2 with the OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

# include-what-you-use, or iwyu, is a open source tool based on clang
# that is used to analyse C++ source paths. It will check files that it's
# given for uses of objects, and suggest including the necessary
# forward declaration or header file. It will also suggest removing any
# unneccessary headers which do not contain anything used by the file.
#
# A few notes on using iwyu and this makefile:
#
#  - iwyu is hosted on gitlab and has precompiled releases available
#  - Before using this makefile, make sure iwyu is somewhere in your $PATH
#  - iwyu will use the exact include path for whatever definition is
#    needed. That means if a member of the OMR layer of an extensible
#    class is needed, iwyu will suggest the OMR version of the file. To
#    solve this, a mapping file is used to force the inclusion of the
#    most derived class. Some glibc libraries and gtest also
#    run into the same problem; iwyu has provided some mapping files for
#    such cases.
#  - iwyu will also scan the first included file of the input file if
#    it's the relevant header.
#  - iwyu (and by extension this makefile) only scans the file; it does
#    not make the changes for you.
#  - If you want to automatically make the changes, pipe the output of
#    iwyu into the fix_includes.py tool from the iwyu devs. Note that this
#    may fail to do the correct thing if run on files with ifdef-ed
#    includes.
#  - BE CAREFUL WHEN REMOVING INCLUDES FROM HEADERS. If another file
#    includes the modified header and was relying on the header to include
#    a certain file, then removing the include will cause the other file
#    to fail to compile.
#
#  - Incremental builds are unfortunately broken. When they are fixed, the
#    suggested workflow will be building files one by one and making the
#    changes as you go.
#  - iwyu doesn't return 0 even when includes are correct; this makefile
#    will not stop upon failing to compile a file.
#  - The latest precompiled version of iwyu (git:f09b88a) fails to scan
#    OMRCodeGenerator.cpp: running it results in a segfault.
#
# Using this makefile:
#
#   To run iwyu on all files:
#
# PLATFORM=foo-bar-gcc make -f iwyu.mk
#
#   To run iwyu on a specific file:
#
# PLATFORM=foo-bar-gcc make -f iwyu.mk JIT_CPP_FILES='$file'
#
# Paths are relative to the source root, e.g. compiler/il/OMRNode.cpp
#

.PHONY: iwyu
iwyu::


# default paths, unless overriden
export CC_PATH?=include-what-you-use
export CXX_PATH?=include-what-you-use

#
# First setup some important paths
# Personally, I feel it's best to default to out-of-tree build but who knows, there may be
# differing opinions on that.
#
JIT_SRCBASE?=../..
JIT_OBJBASE?=../../objs/trj9_$(BUILD_CONFIG)
JIT_DLL_DIR?=$(JIT_OBJBASE)

#
# Windows users will likely use backslashes, but Make tends to not like that so much
#
FIXED_SRCBASE=$(subst \,/,$(JIT_SRCBASE))
FIXED_OBJBASE=$(subst \,/,$(JIT_OBJBASE))
FIXED_DLL_DIR=$(subst \,/,$(JIT_DLL_DIR))

# TODO - "debug" as default?
BUILD_CONFIG?=prod

#
# This is where we setup our component dirs
# Note these are all relative to JIT_SRCBASE and JIT_OBJBASE
# It just makes sense since source and build dirs may be in different places
# in the filesystem :)
#
JIT_OMR_DIRTY_DIR?=compiler
JIT_PRODUCT_DIR?=fvtest/compilertest

#
# Dirs used internally by the makefiles
#
JIT_MAKE_DIR?=$(FIXED_SRCBASE)/fvtest/compilertest/build
JIT_SCRIPT_DIR?=$(FIXED_SRCBASE)/tools/compiler/scripts

#
# First we set a bunch of tokens about the platform that the rest of the
# makefile will use as conditionals
#
include $(JIT_MAKE_DIR)/platform/common.mk

#
# Now we include the names of all the files that will go into building the JIT
# Will automatically include files needed from HOST and TARGET platform
#
include $(JIT_MAKE_DIR)/files/common.mk

#
# Now we configure all the tooling we will use to build the files
#
# There is quite a bit of shared stuff, but the overwhelming majority of this
# is toolchain-dependent.
#
# That makes sense - You can't expect XLC and GCC to take the same arguments
#
include $(JIT_MAKE_DIR)/toolcfg/common.mk


# The iwyu invocation magic line.
EXTRA_FLAGS=-std=c++0x -Wno-writable-strings
LINTER_FLAGS=-Xiwyu --mapping_file=$(JIT_MAKE_DIR)/IWYU_Mappings.imp $(EXTRA_FLAGS)

define DEF_RULE.iwyu
.PHONY: $(1).iwyu

$(1).iwyu: $(1)
	- $$(CXX_CMD) $(LINTER_FLAGS) $$(patsubst %,-D%,$$(CXX_DEFINES)) $$(patsubst %,-I'%',$$(CXX_INCLUDES)) $$<

iwyu:: $(1).iwyu

endef # DEF_RULE.iwyu

RULE.iwyu=$(eval $(DEF_RULE.iwyu))

# The list of sources.
JIT_CPP_FILES=$(filter %.cpp,$(JIT_PRODUCT_SOURCE_FILES) $(JIT_PRODUCT_BACKEND_SOURCES))

# Construct lint dependencies.
$(foreach SRCFILE,$(JIT_CPP_FILES),\
   $(call RULE.iwyu,$(FIXED_SRCBASE)/$(SRCFILE)))
