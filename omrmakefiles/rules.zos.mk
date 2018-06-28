###############################################################################
# Copyright (c) 2015, 2015 IBM Corp. and others
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

# if enhanced ascii support is not enabled
# override the TARGET_ENHANCED_ASCII flag
ifneq ($(OMR_ENHANCED_ASCII),1)
  TARGET_ENHANCED_ASCII := 0
endif

ifneq ($(TARGET_ENHANCED_ASCII),1)
  GLOBAL_CPPFLAGS+=-I$(top_srcdir)/util/a2e/headers
endif

# Specify the minimum arch for 64-bit programs
GLOBAL_CFLAGS+=-Wc,ARCH\(7\)
GLOBAL_CXXFLAGS+=-Wc,ARCH\(7\)

# Enable Warnings as Errors
ifeq ($(OMR_WARNINGS_AS_ERRORS),1)
endif

# Enable more warnings
ifeq ($(OMR_ENHANCED_WARNINGS),1)
endif

# Enable Debugging Symbols
ifeq ($(OMR_DEBUG),1)
endif




# Enable Optimizations
ifeq ($(OMR_OPTIMIZE),1)
    COPTFLAGS=-O3 -Wc,TUNE\(10\) -Wc,inline\(auto,noreport,600,5000\)

    # OMRTODO: The COMPAT=ZOSV1R13 option does not appear to be related to
    # optimizations.  This linker option is supplied only on the compile line,
    # and never when we link. It might be relevant to note that this was added
    # at the same time as the compilation flag "-Wc,target=ZOSV1R10", which
    # would give a performance boost.
    # option means: "COMPAT=ZOSV1R13 is the minimum level that supports conditional sequential RLDs"
    # http://www-01.ibm.com/support/knowledgecenter/SSLTBW_2.1.0/com.ibm.zos.v2r1.ieab100/compat.htm
    COPTFLAGS+=-Wl,compat=ZOSV1R13
endif
ifneq ($(OMR_OPTIMIZE),1)
    COPTFLAGS=-0
endif
GLOBAL_CFLAGS+=$(COPTFLAGS)
GLOBAL_CXXFLAGS+=$(COPTFLAGS)

# Preprocessor Flags
GLOBAL_CPPFLAGS+=-DJ9ZOS390 -DLONGLONG -DJ9VM_TIERED_CODE_CACHE -D_ALL_SOURCE -D_XOPEN_SOURCE_EXTENDED -D_POSIX_SOURCE

# Global Flags
# xplink   Link with the xplink calling convention
# convlit  Convert all string literals to a codepage
# rostring Place string literals in read only storage
# FLOAT    Use IEEE (instead of IBM Hex Format) style floats
# enum     Specifies how many bytes of storage enums occupy
# a,goff   Assemble into GOFF object files
# NOANSIALIAS Do not generate ALIAS binder control statements
# TARGET   Generate code for the target operating system
GLOBAL_FLAGS+=-Wc,xplink,convlit\(ISO8859-1\),rostring,FLOAT\(IEEE,FOLD,AFP\),enum\(4\) -Wa,goff -Wc,NOANSIALIAS -Wc,TARGET\(zOSV1R13\)

ifeq ($(TARGET_ENHANCED_ASCII),1)
  GLOBAL_FLAGS+=-Wc,ascii
else
  GLOBAL_CPPFLAGS+=-DIBM_ATOE
endif

ifeq (1,$(OMR_ENV_DATA64))
  GLOBAL_CPPFLAGS+=-DJ9ZOS39064
  GLOBAL_FLAGS+=-Wc,lp64 -Wa,SYSPARM\(BIT64\)
else
  GLOBAL_CPPFLAGS+=-D_LARGE_FILES
endif

GLOBAL_CFLAGS+=-Wc,"langlvl(extc99)" $(GLOBAL_FLAGS)
GLOBAL_CXXFLAGS+=-Wc,"langlvl(extended)" -+ $(GLOBAL_FLAGS)

ifneq (,$(findstring archive,$(ARTIFACT_TYPE)))
  DO_LINK:=0
else
  DO_LINK:=1
endif
ifeq (1,$(DO_LINK))
  ifneq (,$(findstring shared,$(ARTIFACT_TYPE)))
    GLOBAL_CPPFLAGS+=-Wc,DLL,EXPORTALL
  endif

  # This is the first option applied to the C++ linking command.
  # It is not applied to the C linking command.
  OMR_MK_CXXLINKFLAGS=-Wc,"langlvl(extended)" -+ 

  ifneq (,$(findstring shared,$(ARTIFACT_TYPE)))
    GLOBAL_LDFLAGS+=-Wl,xplink,dll
  else
    # Assume we're linking an executable
    GLOBAL_LDFLAGS+=-Wl,xplink
  endif
  ifeq (1,$(OMR_ENV_DATA64))
    OMR_MK_CXXLINKFLAGS+=-Wc,lp64
    GLOBAL_LDFLAGS+=-Wl,lp64
  endif

  # always link a2e last, unless we are creating the a2e library
  ifneq (j9a2e,$(MODULE_NAME))
    ifneq (1,$(TARGET_ENHANCED_ASCII))
      GLOBAL_SHARED_LIBS+=j9a2e
    endif
  endif
endif

# compilation for metal-C files.
ifeq (1,$(OMR_ENV_DATA64))
  MCFLAGS=-q64
endif
%$(OBJEXT): %.mc
	cp $< $*.c
	xlc $(MCFLAGS) -qmetal -qlongname -S -o $*.s $*.c > $*.asmlist
	rm -f $*.c
	as -mgoff -I CBC.SCCNSAM $*.s
	rm -f $*.s

# compilation for .s files
define AS_COMMAND
$(CC) $(CPPFLAGS) $(MODULE_CPPFLAGS) $(GLOBAL_CPPFLAGS) $(GLOBAL_CFLAGS) $(MODULE_CFLAGS) $(CFLAGS) -c $<
endef

define LINK_CXX_EXE_COMMAND
$(CXXLINKEXE) $(OMR_MK_CXXLINKFLAGS) -o $@ \
  $(LDFLAGS) $(MODULE_LDFLAGS) $(GLOBAL_LDFLAGS) \
  $(LD_SHARED_LIBS) $(OBJECTS) $(LD_STATIC_LIBS)
endef

# Do not create an export file
$(MODULE_NAME)_LINKER_EXPORT_SCRIPT:=

define LINK_C_SHARED_COMMAND
$(CCLINKSHARED) -o $($(MODULE_NAME)_shared) \
  $(LDFLAGS) $(MODULE_LDFLAGS) $(GLOBAL_LDFLAGS) \
  $(LD_SHARED_LIBS) $(OBJECTS) $(LD_STATIC_LIBS)
mv $(LIBPREFIX)$(MODULE_NAME).x $(lib_output_dir)
endef

define LINK_CXX_SHARED_COMMAND
$(CXXLINKSHARED) $(OMR_MK_CXXLINKFLAGS) -o $($(MODULE_NAME)_shared) \
  $(LDFLAGS) $(MODULE_LDFLAGS) $(GLOBAL_LDFLAGS) \
  $(LD_SHARED_LIBS) $(OBJECTS) $(LD_STATIC_LIBS)
mv $(LIBPREFIX)$(MODULE_NAME).x $(lib_output_dir)
endef

ifneq (,$(findstring shared,$(ARTIFACT_TYPE)))
CLEAN_FILES+=$(LIBPREFIX)$(MODULE_NAME).x
CLEAN_FILES+=$(lib_output_dir)/$(LIBPREFIX)$(MODULE_NAME).x
endif
define CLEAN_COMMAND
-$(RM) $(OBJECTS) $(OBJECTS:$(OBJEXT)=.d) $(CLEAN_FILES) 
endef
