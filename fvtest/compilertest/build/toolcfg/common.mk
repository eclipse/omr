###############################################################################
# Copyright IBM Corp. and others 2016
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
# [2] https://openjdk.org/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
###############################################################################


ifdef J9SRC
   OMR_DIR?=$(J9SRC)/omr
endif
OMR_DIR?=../..
top_srcdir=$(OMR_DIR)
include $(OMR_DIR)/omrmakefiles/configure.mk

GTEST_PATH=$(OMR_GTEST_DIR)

PRODUCT_INCLUDES=\
    $(OMR_DIR)/include_core \
    $(FIXED_SRCBASE)/$(JIT_PRODUCT_DIR)/$(TARGET_ARCH)/$(TARGET_SUBARCH) \
    $(FIXED_SRCBASE)/$(JIT_PRODUCT_DIR)/$(TARGET_ARCH) \
    $(FIXED_SRCBASE)/$(JIT_PRODUCT_DIR) \
    $(FIXED_SRCBASE)/fvtest \
    $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/$(TARGET_ARCH)/$(TARGET_SUBARCH) \
    $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/$(TARGET_ARCH) \
    $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR) \
    $(FIXED_SRCBASE) \
    $(FIXED_SRCBASE) \
    $(GTEST_PATH) \
    $(GTEST_PATH)/include

PRODUCT_DEFINES+=\
    BITVECTOR_BIT_NUMBERING_MSB \
    JITTEST \
    OMR_RELOCATION_RUNTIME \
    TEST_PROJECT_SPECIFIC

ifdef ASSUMES
    PRODUCT_DEFINES+=PROD_WITH_ASSUMES
endif

PRODUCT_RELEASE?=tr.open.test

PRODUCT_NAME?=testjit

PRODUCT_LIBPATH= $(top_srcdir)/lib
PRODUCT_SLINK= \
  j9prtstatic \
  j9thrstatic \
  j9hashtable \
  omrutil \
  j9pool \
  j9avl \
  j9hookstatic 
    
#
# Now we include the host and target tool config
# These don't really do much generally... They set a few defines but there really
# isn't a lot of stuff that's host/target dependent that isn't also dependent
# on what tools you're using
#
include $(JIT_MAKE_DIR)/toolcfg/host/$(HOST_ARCH).mk
include $(JIT_MAKE_DIR)/toolcfg/host/$(HOST_BITS).mk
include $(JIT_MAKE_DIR)/toolcfg/host/$(OS).mk
include $(JIT_MAKE_DIR)/toolcfg/target/$(TARGET_ARCH).mk
include $(JIT_MAKE_DIR)/toolcfg/target/$(TARGET_BITS).mk

#
# Now this is the big tool config file. This is where all the includes and defines
# get turned into flags, and where all the flags get setup for the different
# tools and file types
#
include $(JIT_MAKE_DIR)/toolcfg/$(TOOLCHAIN)/common.mk
