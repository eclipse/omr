###############################################################################
# Copyright (c) 2015, 2015 IBM Corp. and others
# 
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
# 
# This Source Code is also Distributed under one or more Secondary Licenses,
# as those terms are defined by the Eclipse Public License, v. 2.0: GNU
# General Public License, version 2 with the GNU Classpath Exception [1]
# and GNU General Public License, version 2 with the OpenJDK Assembly
# Exception [2].
# 
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
# 
# Contributors:
#   Multiple authors (IBM Corp.) - initial implementation and documentation
###############################################################################

top_srcdir := ../..
include $(top_srcdir)/tools/toolconfigure.mk

MODULE_NAME := tracemerge
ARTIFACT_TYPE := cxx_executable
OBJECTS := ArgParser FileReader DATMerge FileUtils StringUtils Port main
OBJECTS := $(addsuffix $(OBJEXT),$(OBJECTS))

vpath %.c $(top_srcdir)/tools/tracegen
vpath %.cpp $(top_srcdir)/tools/tracegen
MODULE_INCLUDES := $(top_srcdir)/tools/tracegen

include $(top_srcdir)/omrmakefiles/rules.mk
