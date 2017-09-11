###############################################################################
# Copyright (c) 2015, 2017 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
###############################################################################

# This makefile fragment defines logic that is common to building both shared and static libraries.

# The user of this makefile must include omrmakefiles/configure.mk, to define the environment
# variables and the buildflags.

# The location of the source code is parameterized so that this makefile fragment can be used
# to build object files in a different directory.
HOOKABLE_SRCDIR ?= ./

OBJECTS := hookable$(OBJEXT)
OBJECTS += ut_j9hook$(OBJEXT)

vpath %.cpp $(HOOKABLE_SRCDIR)
vpath %.c $(HOOKABLE_SRCDIR)

MODULE_INCLUDES += $(HOOKABLE_SRCDIR)
