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

# This makefile fragment defines logic that is common to building both shared and static libraries.

# The user of this makefile must include omrmakefiles/configure.mk, to define the environment
# variables and the buildflags.

# The location of the source code is parameterized so that this makefile fragment can be used
# to build object files in a different directory.
HOOKABLE_SRCDIR ?= ./

OBJECTS := hookable$(OBJEXT)

vpath %.cpp $(HOOKABLE_SRCDIR)
vpath %.c $(HOOKABLE_SRCDIR)

MODULE_INCLUDES += $(HOOKABLE_SRCDIR)
