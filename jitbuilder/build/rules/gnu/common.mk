###############################################################################
# Copyright (c) 2016, 2019 IBM Corp. and others
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

CPP_API_OBJECTS=$(patsubst %,$(FIXED_OBJBASE)/%.o,$(basename $(CPP_GENERATED_API_SOURCES)))

# build the jitbuilder library with all the object files
CPP_JIT_PRODUCT_BACKEND_LIBRARY=$(FIXED_DLL_DIR)/cpp/$(LIBPREFIX)$(PRODUCT_NAME).a

# Figure out the name of the executable file
JIT_PRODUCT_SONAME=$(FIXED_DLL_DIR)/$(PRODUCT_NAME)

# Add build name to test
JIT_PRODUCT_BUILDNAME_SRC=$(FIXED_OBJBASE)/$(JIT_OMR_DIRTY_DIR)/env/TRBuildName.cpp
JIT_PRODUCT_BUILDNAME_OBJ=$(FIXED_OBJBASE)/$(JIT_OMR_DIRTY_DIR)/env/TRBuildName.o
JIT_PRODUCT_BACKEND_OBJECTS+=$(JIT_PRODUCT_BUILDNAME_OBJ)

$(CPP_JIT_PRODUCT_BACKEND_LIBRARY): $(CPP_API_OBJECTS)
jit: $(CPP_JIT_PRODUCT_BACKEND_LIBRARY)

$(CPP_JIT_PRODUCT_BACKEND_LIBRARY): $(JIT_PRODUCT_BACKEND_OBJECTS) $(CPP_API_OBJECTS)
	@mkdir -p $(dir $@)
	$(AR_CMD) rcsv $@ $(JIT_PRODUCT_BACKEND_OBJECTS) $(CPP_API_OBJECTS)

jit_clean::
	rm -f $(CPP_JIT_PRODUCT_BACKEND_LIBRARY)

jit_cleandll::
	rm -f $(JIT_PRODUCT_SONAME)

$(call RULE.cpp,$(JIT_PRODUCT_BUILDNAME_OBJ),$(JIT_PRODUCT_BUILDNAME_SRC))

$(JIT_PRODUCT_BUILDNAME_SRC):
	@mkdir -p $(dir $@)
	$(PERL_PATH) $(GENERATE_VERSION_SCRIPT) $(PRODUCT_RELEASE) $@

jit_clean::
	rm -f $(JIT_PRODUCT_BUILDNAME_SRC)

$(call RULE.cpp,$(GTEST_OBJ),$(GTEST_CC))

#
# This part generates the C++ client API
#
# Because the C++ API generator produces multiple outputs, some special handling
# is needed to prevent potential race conditions with parallel make. The strategy
# used follows some of the suggestions specified automake manual
# (https://www.gnu.org/savannah-checkouts/gnu/automake/manual/html_node/Multiple-Outputs.html).
#
# Essentially, the rules expands into the following sequence that ensures only
# one rule actually runs the generator:
#
# ```
# generated_file_1: generator api_description
#   run generator
#
# generated_file_2: generated_file_1
# generated_file_3: generated_file_1
# generated_file_4: generated_file_1
#   ...
# ```
#
CPP_API_FILES=$(addprefix $(FIXED_OBJBASE)/, $(CPP_GENERATED_API_SOURCES)) $(addprefix $(FIXED_SRCBASE)/, $(CPP_GENERATED_API_HEADERS))
CPP_API_SOURCE_DIR=$(FIXED_OBJBASE)/$(CPP_GENERATED_SOURCE_DIR)
CPP_API_HEADER_DIR=$(FIXED_SRCBASE)/$(CPP_GENERATED_HEADER_DIR)

$(firstword $(CPP_API_FILES)): $(FIXED_SRCBASE)/$(JITBUILDER_API_DESCRIPTION) $(FIXED_SRCBASE)/$(CPP_API_GENERATOR)
	@mkdir -p $(CPP_API_SOURCE_DIR)
	@mkdir -p $(CPP_API_HEADER_DIR)
	$(PYTHON_PATH) $(FIXED_SRCBASE)/$(CPP_API_GENERATOR) $(FIXED_SRCBASE)/$(JITBUILDER_API_DESCRIPTION) --sourcedir $(CPP_API_SOURCE_DIR) --headerdir $(CPP_API_HEADER_DIR)

$(wordlist 2, $(words $(CPP_API_FILES)), $(CPP_API_FILES)): $(firstword $(CPP_API_FILES))

# remove generated files
jit_clean::
	rm -f $(CPP_API_FILES)

#
# This part calls the "RULE.x" macros for each source file
#
$(foreach SRCFILE,$(JIT_PRODUCT_BACKEND_SOURCES),\
    $(call RULE$(suffix $(SRCFILE)),$(FIXED_OBJBASE)/$(basename $(SRCFILE))$(OBJSUFF),$(FIXED_SRCBASE)/$(SRCFILE)) \
 )

$(foreach SRCFILE,$(JIT_PRODUCT_SOURCE_FILES),\
    $(call RULE$(suffix $(SRCFILE)),$(FIXED_OBJBASE)/$(basename $(SRCFILE))$(OBJSUFF),$(FIXED_SRCBASE)/$(SRCFILE)) \
 )

$(foreach SRCFILE,$(CPP_GENERATED_API_SOURCES),\
    $(call RULE$(suffix $(SRCFILE)),$(FIXED_OBJBASE)/$(basename $(SRCFILE))$(OBJSUFF),$(FIXED_OBJBASE)/$(SRCFILE)) \
 )
