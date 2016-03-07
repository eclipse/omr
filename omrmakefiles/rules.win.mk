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

# Windows-specific tools
RC ?= rc
MT ?= mt
IMPLIB ?= lib

# Assembler flags and Preprocessor
ifeq (1,$(OMR_ENV_DATA64))
  GLOBAL_ASFLAGS+=/c /Cp /nologo -DWIN32 -DWIN64 -DJ9HAMMER
  GLOBAL_CPPFLAGS+=-D_AMD64_=1 -DWIN64 -D_WIN64 -DWIN32 -D_WIN32 -DJ9HAMMER
else
  # /c Assemble without linking
  # /Cp Preserve case of user identifiers
  # /nologo Suppress copyright message
  # /coff generate COFF format object file
  # /Gd Use C calls (i.e. prepend underscored to symbols)
  # /Zm Enable MASM 5.10 compatibility
  GLOBAL_ASFLAGS+=/c /Cp /nologo /safeseh /coff /Gd -DWIN32 /Zm
  GLOBAL_CPPFLAGS+=-D_X86_=1 -DWIN32 -D_WIN32
endif

# We don't currently want CRT security warnings
GLOBAL_CPPFLAGS+=-D_CRT_SECURE_NO_WARNINGS
GLOBAL_CPPFLAGS+=-DCRTAPI1=_cdecl -DCRTAPI2=_cdecl -nologo
GLOBAL_CPPFLAGS+=-D_WIN95 -D_WIN32_WINDOWS=0x0500 /D_WIN32_DCOM
GLOBAL_CPPFLAGS+=-D_MT

# Defining _WINSOCKAPI_ prevents <winsock.h> from being included by <windows.h>.
# We want to use <winsock2.h>.
# This protects us in case <windows.h> is inadvertently included before <winsock2.h>.
GLOBAL_CPPFLAGS+=-D_WINSOCKAPI_ 

# Set minimum required system to Win XP
OMR_MK_WINVER=0x0501
GLOBAL_CPPFLAGS+=-D_WIN32_IE=0x0500 -DWINVER=$(OMR_MK_WINVER)
GLOBAL_CPPFLAGS+=-D_WIN32_WINNT=$(OMR_MK_WINVER)

# Specify precompiled header memory allocation limit
# -Zm400 max memory is 400% the default maxiumum (~300 mb)
GLOBAL_CFLAGS+=-Zm400
GLOBAL_CXXFLAGS+=-Zm400

# -MD use the multithread-specific and DLL-specific version of the CRT
GLOBAL_CXXFLAGS+=-MD
GLOBAL_CFLAGS+=-MD
GLOBAL_CPPFLAGS+=-D_DLL

# Enable Optimizations
ifeq ($(OMR_OPTIMIZE),1)
  COPTFLAGS=/Ox
endif
COPTFLAGS+=/GS- # /GS-: disable buffer security check (that is on by default)
GLOBAL_CFLAGS+=$(COPTFLAGS)
GLOBAL_CXXFLAGS+=$(COPTFLAGS)

# Enable Warnings as Errors
ifeq ($(OMR_WARNINGS_AS_ERRORS),1)
    GLOBAL_CFLAGS+=-WX
    GLOBAL_CXXFLAGS+=-WX
endif

# Enable more warnings
ifeq ($(OMR_ENHANCED_WARNINGS),1)
    GLOBAL_CFLAGS+=-W3
    GLOBAL_CXXFLAGS+=-W3
    GLOBAL_ASFLAGS+=-W3
endif

# Enable Debugging Symbols
# To support parallel make we write a PDB file for each source file
GLOBAL_CFLAGS+=-Fd$*.pdb
GLOBAL_CXXFLAGS+=-Fd$*.pdb
ifeq ($(OMR_DEBUG),1)
    # /Zd Add line number info
    # /Zi add debug symbols
    GLOBAL_LDFLAGS+=/debug
    GLOBAL_ASFLAGS+=/Zi /Zd
    GLOBAL_CFLAGS+=/Zi
    GLOBAL_CXXFLAGS+=/Zi
endif

ifneq (,$(findstring archive,$(ARTIFACT_TYPE)))
  DO_LINK:=0
else
  DO_LINK:=1
endif

ifeq (1,$(DO_LINK))
  GLOBAL_LDFLAGS+=/opt:icf /opt:ref
  ifeq (0,$(OMR_ENV_DATA64))
    GLOBAL_LDFLAGS+=/SAFESEH
  endif

  ifeq (0,$(OMR_ENV_DATA64))
    WIN_TARGET_MACHINE:=i386
  else
    WIN_TARGET_MACHINE:=AMD64
  endif

## ifdef UMA_NO_CONSOLE
## WIN_SUBSYSTEM_TYPE=windows
## else
  WIN_SUBSYSTEM_TYPE=console
## endif

  GLOBAL_LDFLAGS+=-subsystem:$(WIN_SUBSYSTEM_TYPE) -machine:$(WIN_TARGET_MACHINE)

  ## EXE-specific options
  OMR_MK_EXEFLAGS+=/INCREMENTAL:NO /NOLOGO /LARGEADDRESSAWARE

  # basic subsystem specific libraries, less the C Run-Time
  OMR_MK_EXELIBS+=kernel32.lib wsock32.lib advapi32.lib
## ifdef UMA_NO_CONSOLE
## OMR_MK_EXELIBS+=user32.lib gdi32.lib comdlg32.lib winspool.lib
## endif

## ifeq ($(UMA_NO_CRT),1)
## UMA_EXE_LINK_FLAGS+=/NODEFAULTLIB:MSVCRT
## #OMR_MK_EXELIBS+=libcmt.lib
## else
## UMA_EXE_LINK_FLAGS+=wsetargv.obj
# Microsoft calls this a "link option"
  OMR_MK_EXEFLAGS+=wsetargv.obj
## endif # ($(UMA_NO_CRT),1)

  ifeq ($(OMR_ENV_DATA64),1)
    OMR_MK_EXEFLAGS+=/NODEFAULTLIB:MSVCRTD
  endif

  ## DLL-specific options
  OMR_MK_DLLFLAGS+=/INCREMENTAL:NO /NOLOGO 
  ifeq (1,$(OMR_ENV_DATA64))
    OMR_MK_DLLFLAGS+=-entry:_DllMainCRTStartup
  else
    OMR_MK_DLLFLAGS+=-entry:_DllMainCRTStartup@12
  endif
  OMR_MK_DLLFLAGS+=-dll
  OMR_MK_DLLLIBS+=kernel32.lib ws2_32.lib advapi32.lib user32.lib gdi32.lib comdlg32.lib winspool.lib
  
  # Delay Load Libraries
  # all windows system libraries are delayload
  ifneq (,$(MODULE_DELAYLOAD_LIBS))
    MODULE_LDFLAGS+=Delayimp.lib
    MODULE_LDFLAGS+=$(call buildSharedLibLinkFlags,$(MODULE_DELAYLOAD_LIBS))
    OMR_DELAYLOAD_INSTRUCTIONS+=$(foreach lib,$(MODULE_DELAYLOAD_LIBS),-delayload:$(lib).dll)
  endif

  # These libraries are passed to the LIB command for building DLL import libraries.
  IMPORTLIB_LIBS=$(call buildStaticLibLinkFlags,$(MODULE_STATIC_LIBS) $(GLOBAL_STATIC_LIBS) $(MODULE_SHARED_LIBS) $(GLOBAL_SHARED_LIBS) $(MODULE_DELAYLOAD_LIBS))
  IMPORTLIB_LIBPATH=$(call buildLibPathFlags,$(MODULE_LIBPATH) $(GLOBAL_LIBPATH))
endif

define COMPILE_C_COMMAND
$(CC) $(CPPFLAGS) $(MODULE_CPPFLAGS) $(GLOBAL_CPPFLAGS) $(GLOBAL_CFLAGS) $(MODULE_CFLAGS) $(CFLAGS) -c $< /Fo$(dir $@)
endef

define COMPILE_CXX_COMMAND
$(CXX) $(CPPFLAGS) $(MODULE_CPPFLAGS) $(GLOBAL_CPPFLAGS) $(GLOBAL_CXXFLAGS) $(MODULE_CXXFLAGS) $(CXXFLAGS) -c $< /Fo$(dir $@)
endef

define AS_COMMAND
$(AS) $(GLOBAL_ASFLAGS) $(MODULE_ASFLAGS) $(ASFLAGS) $<
endef

RC_INCLUDES=$(call buildCPPIncludeFlags,$(MODULE_INCLUDES) $(GLOBAL_INCLUDES))

# compilation rule for message text files
%.res: %.mc
	mc $<
	$(RC) $(RC_INCLUDES) $*.rc
	$(RM) $*.rc

define AR_COMMAND
$(AR) -out:$@ $(OBJECTS)
endef

define LINK_C_EXE_COMMAND
$(CCLINKEXE) $(LDFLAGS) $(MODULE_LDFLAGS) $(OMR_MK_EXEFLAGS) -out:$@ \
  $(GLOBAL_LDFLAGS) $(OBJECTS) $(OMR_MK_EXELIBS)
$(MT) -manifest $(top_srcdir)/omrmakefiles/winexe.manifest -outputresource:$@;#1
endef

define LINK_CXX_EXE_COMMAND
$(CXXLINKEXE) $(LDFLAGS) $(MODULE_LDFLAGS) $(OMR_MK_EXEFLAGS) -out:$@ \
  $(GLOBAL_LDFLAGS) $(OBJECTS) $(OMR_MK_EXELIBS)
$(MT) -manifest $(top_srcdir)/omrmakefiles/winexe.manifest -outputresource:$@;#1
endef

## Shared Library Build Rules

# Module-definition file
$(MODULE_NAME)_LINKER_EXPORT_SCRIPT:=$(MODULE_NAME).def

# Import library
$(MODULE_NAME)_importlib:=$(lib_output_dir)/$(LIBPREFIX)$(MODULE_NAME).lib

define GENERATE_EXPORT_SCRIPT_COMMAND
sh $(top_srcdir)/omrmakefiles/generate-exports.sh msvc $(MODULE_NAME) $(EXPORT_FUNCTIONS_FILE) $($(MODULE_NAME)_LINKER_EXPORT_SCRIPT)
endef

# Create an import library first, and then create a DLL from the import library.
# The import library is used both to build the DLL and for executables to link
# against when they want to use the DLL.

define LINK_C_SHARED_COMMAND
$(IMPLIB) -subsystem:$(WIN_SUBSYSTEM_TYPE) -out:$($(MODULE_NAME)_importlib) -def:$($(MODULE_NAME)_LINKER_EXPORT_SCRIPT) -machine:$(WIN_TARGET_MACHINE) $(OBJECTS) $(IMPORTLIB_LIBS) $(IMPORTLIB_LIBPATH)
$(CCLINKSHARED) $(OBJECTS) \
  $(OMR_DELAYLOAD_INSTRUCTIONS) $(LDFLAGS) $(MODULE_LDFLAGS) $(OMR_MK_DLLFLAGS) $(GLOBAL_LDFLAGS) \
  -out:$@ -map:$(MODULE_NAME).map \
  $(OMR_MK_DLLLIBS) \
  $(MODULE_NAME).exp
endef

define LINK_CXX_SHARED_COMMAND
$(IMPLIB) -subsystem:$(WIN_SUBSYSTEM_TYPE) -out:$($(MODULE_NAME)_importlib) -def:$($(MODULE_NAME)_LINKER_EXPORT_SCRIPT) -machine:$(WIN_TARGET_MACHINE) $(OBJECTS) $(IMPORTLIB_LIBS) $(IMPORTLIB_LIBPATH)
$(CXXLINKSHARED) $(OBJECTS) \
  $(OMR_DELAYLOAD_INSTRUCTIONS) $(LDFLAGS) $(MODULE_LDFLAGS) $(OMR_MK_DLLFLAGS) $(GLOBAL_LDFLAGS) \
  -out:$@ -map:$(MODULE_NAME).map \
  $(OMR_MK_DLLLIBS) \
  $(MODULE_NAME).exp
endef

## Files to clean
CLEAN_FILES=$(OBJECTS) *.d *.pdb
ifneq (,$(findstring shared,$(ARTIFACT_TYPE)))
CLEAN_FILES+=$($(MODULE_NAME)_LINKER_EXPORT_SCRIPT) $($(MODULE_NAME)_importlib) $(MODULE_NAME).exp $(MODULE_NAME).map
endif

define CLEAN_COMMAND
-$(RM) $(CLEAN_FILES)
endef

## Dependencies
# include *.d files generated by the compiler
DEPS:=$(filter-out %.res,$(OBJECTS))
DEPS:=$(DEPS:$(OBJEXT)=.d)
-include $(DEPS)
show_deps:
	@echo "Dependencies are: $(DEPS)"
.PHONY: show_deps

