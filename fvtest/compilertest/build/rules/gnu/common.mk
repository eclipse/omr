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

#
# Defines rules for compiling source files into object files
#
# Each of these rules will also add themselves to jit_cleanobjs and jit_cleandeps
# to clean build artifacts and dependecy files, respectively
#
include $(JIT_MAKE_DIR)/rules/gnu/filetypes.mk

# Convert the source file names to object file names
JIT_PRODUCT_BACKEND_OBJECTS=$(patsubst %,$(FIXED_OBJBASE)/%.o,$(basename $(JIT_PRODUCT_BACKEND_SOURCES)))
JIT_PRODUCT_OBJECTS=$(patsubst %,$(FIXED_OBJBASE)/%.o,$(basename $(JIT_PRODUCT_SOURCE_FILES)))

# With test, we need to build a base library of all the non-frontend stuff
JIT_PRODUCT_BACKEND_LIBRARY=$(FIXED_DLL_DIR)/lib$(PRODUCT_NAME)_base.a

# Figure out the name of the executable file
JIT_PRODUCT_SONAME=$(FIXED_DLL_DIR)/$(PRODUCT_NAME)

# Add build name to test
JIT_PRODUCT_BUILDNAME_SRC=$(FIXED_OBJBASE)/$(JIT_OMR_DIRTY_DIR)/env/TRBuildName.cpp
JIT_PRODUCT_BUILDNAME_OBJ=$(FIXED_OBJBASE)/$(JIT_OMR_DIRTY_DIR)/env/TRBuildName.o
JIT_PRODUCT_OBJECTS+=$(JIT_PRODUCT_BUILDNAME_OBJ)

# Add GTest
GTEST_CC=$(GTEST_PATH)/src/gtest-all.cc
GTEST_OBJ=$(FIXED_OBJBASE)/gtest.o
JIT_PRODUCT_OBJECTS+=$(GTEST_OBJ)

jit: $(JIT_PRODUCT_SONAME)

$(JIT_PRODUCT_SONAME): $(JIT_PRODUCT_OBJECTS) $(JIT_PRODUCT_BACKEND_LIBRARY) | jit_createdirs
	$(SOLINK_CMD) $(SOLINK_FLAGS) $(patsubst %,-L%,$(SOLINK_LIBPATH)) -o $@ $(SOLINK_PRE_OBJECTS) $(JIT_PRODUCT_OBJECTS) $(SOLINK_POST_OBJECTS) $(JIT_PRODUCT_BACKEND_LIBRARY) $(LINK_GROUP_START) $(patsubst %,-l%,$(SOLINK_SLINK)) $(LINK_GROUP_END) $(SOLINK_EXTRA_ARGS)

$(JIT_PRODUCT_BACKEND_LIBRARY): $(JIT_PRODUCT_BACKEND_OBJECTS) | jit_createdirs
	$(AR_CMD) rcsv $@ $(JIT_PRODUCT_BACKEND_OBJECTS)

JIT_DIR_LIST+=$(dir $(JIT_PRODUCT_SONAME))

JIT_DIR_LIST+=$(dir $(JIT_PRODUCT_BACKEND_LIBRARY))

jit_cleanobjs::
	rm -f $(JIT_PRODUCT_BACKEND_LIBRARY)

jit_cleandll::
	rm -f $(JIT_PRODUCT_SONAME)

$(call RULE.cpp,$(JIT_PRODUCT_BUILDNAME_OBJ),$(JIT_PRODUCT_BUILDNAME_SRC))    
    
.PHONY: $(JIT_PRODUCT_BUILDNAME_SRC)
$(JIT_PRODUCT_BUILDNAME_SRC): | jit_createdirs
	$(PERL_PATH) $(GENERATE_VERSION_SCRIPT) $(PRODUCT_RELEASE) > $@

JIT_DIR_LIST+=$(dir $(JIT_PRODUCT_BUILDNAME_SRC))

jit_cleanobjs::
	rm -f $(JIT_PRODUCT_BUILDNAME_SRC)

$(call RULE.cpp,$(GTEST_OBJ),$(GTEST_CC))

#
# This part calls the "RULE.x" macros for each source file
#
$(foreach SRCFILE,$(JIT_PRODUCT_BACKEND_SOURCES),\
    $(call RULE$(suffix $(SRCFILE)),$(FIXED_OBJBASE)/$(basename $(SRCFILE))$(OBJSUFF),$(FIXED_SRCBASE)/$(SRCFILE)) \
 )

$(foreach SRCFILE,$(JIT_PRODUCT_SOURCE_FILES),\
    $(call RULE$(suffix $(SRCFILE)),$(FIXED_OBJBASE)/$(basename $(SRCFILE))$(OBJSUFF),$(FIXED_SRCBASE)/$(SRCFILE)) \
 )
 
#
# Generate a rule that will create every directory before the build starts
#
$(foreach JIT_DIR,$(sort $(JIT_DIR_LIST)),$(eval jit_createdirs:: ; mkdir -p $(JIT_DIR)))
