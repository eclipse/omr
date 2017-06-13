###############################################################################
#
# (c) Copyright IBM Corp. 2015
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

GLOBAL_CPPFLAGS+=-I$(top_srcdir)/util/a2e/headers

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
  GLOBAL_FLAGS+=-Wc,g9
endif

# Enable Optimizations
ifeq ($(OMR_OPTIMIZE),1)
    COPTFLAGS=-O3 -Wc,TUNE\(10\) -Wc,inline\(auto,noreport,600,5000\)

    # -Wl,compat and -Wc,TARGET must agree with each other.
    #
    # Details for -Wl,compat can be found here: 
    # http://www-01.ibm.com/support/knowledgecenter/SSLTBW_2.1.0/com.ibm.zos.v2r1.ieab100/compat.htm
    COPTFLAGS+=-Wl,compat=$(OMR_ZOS_TARGET)
endif
ifneq ($(OMR_OPTIMIZE),1)
    COPTFLAGS=-0
endif
GLOBAL_CFLAGS+=$(COPTFLAGS)
GLOBAL_CXXFLAGS+=$(COPTFLAGS)

# Preprocessor Flags
GLOBAL_CPPFLAGS+=-DJ9ZOS390 -DLONGLONG -DJ9VM_TIERED_CODE_CACHE -D_ALL_SOURCE -D_XOPEN_SOURCE_EXTENDED -DIBM_ATOE -D_POSIX_SOURCE 

# Global Flags
# xplink   Link with the xplink calling convention
# convlit  Convert all string literals to a codepage
# rostring Place string literals in read only storage
# FLOAT    Use IEEE (instead of IBM Hex Format) style floats
# enum     Specifies how many bytes of storage enums occupy
# a,goff   Assemble into GOFF object files
# NOANSIALIAS Do not generate ALIAS binder control statements
# TARGET   Generate code for the target operating system
# list     Generate assembly listing
# offset   In assembly listing, show addresses as offsets of function entry points
GLOBAL_FLAGS+=-Wc,xplink,convlit\(ISO8859-1\),rostring,FLOAT\(IEEE,FOLD,AFP\),enum\(4\) -Wa,goff -Wc,NOANSIALIAS -Wc,TARGET\($(OMR_ZOS_TARGET)\) -W "c,list,offset"

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
    GLOBAL_SHARED_LIBS+=j9a2e
  endif
endif


# compilation for C files.
define COMPILE_C_COMMAND
$(CC) $(CPPFLAGS) $(MODULE_CPPFLAGS) $(GLOBAL_CPPFLAGS) $(GLOBAL_CFLAGS) $(MODULE_CFLAGS) $(CFLAGS) -c $< -o $@ > $*.asmlist
endef

# compilation for C++ files.
define COMPILE_CXX_COMMAND
$(CXX) $(CPPFLAGS) $(MODULE_CPPFLAGS) $(GLOBAL_CPPFLAGS) $(GLOBAL_CXXFLAGS) $(MODULE_CXXFLAGS) $(CXXFLAGS) -c $< -o $@ > $*.asmlist
endef

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
  $(OBJECTS) \
  $(LDFLAGS) $(MODULE_LDFLAGS) $(GLOBAL_LDFLAGS)
endef

# Do not create an export file
$(MODULE_NAME)_LINKER_EXPORT_SCRIPT:=

define LINK_C_SHARED_COMMAND
$(CCLINKSHARED) -o $($(MODULE_NAME)_shared) \
  $(OBJECTS) \
  $(LDFLAGS) $(MODULE_LDFLAGS) $(GLOBAL_LDFLAGS)
mv $(LIBPREFIX)$(MODULE_NAME).x $(lib_output_dir)
endef

define LINK_CXX_SHARED_COMMAND
$(CXXLINKSHARED) $(OMR_MK_CXXLINKFLAGS) -o $($(MODULE_NAME)_shared) \
  $(OBJECTS) \
  $(LDFLAGS) $(MODULE_LDFLAGS) $(GLOBAL_LDFLAGS)
mv $(LIBPREFIX)$(MODULE_NAME).x $(lib_output_dir)
endef

ifneq (,$(findstring shared,$(ARTIFACT_TYPE)))
CLEAN_FILES+=$(LIBPREFIX)$(MODULE_NAME).x
CLEAN_FILES+=$(lib_output_dir)/$(LIBPREFIX)$(MODULE_NAME).x
endif
define CLEAN_COMMAND
-$(RM) $(OBJECTS) $(OBJECTS:$(OBJEXT)=.d) $(CLEAN_FILES) 
endef
