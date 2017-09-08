###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
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

###
### Helpers
###

###
### Global Flags
###

GLOBAL_CPPFLAGS += -DLINUX -D_REENTRANT

# Compile without exceptions
ifeq (gcc,$(OMR_TOOLCHAIN))
	ifeq (1,$(OMR_RTTI))
		GLOBAL_CXXFLAGS+=-fno-exceptions -fno-threadsafe-statics
	else
		GLOBAL_CXXFLAGS+=-fno-exceptions -fno-rtti -fno-threadsafe-statics
	endif
endif

## Position Independent compile flag
ifeq (gcc,$(OMR_TOOLCHAIN))
	GLOBAL_CFLAGS+=-fPIC
	GLOBAL_CXXFLAGS+=-fPIC
endif

## ASFLAGS
# xlc on ppc does not actually understand this
# option, it is silently ignored.
GLOBAL_ASFLAGS+=-noexecstack

ifeq (s390,$(OMR_HOST_ARCH))
	ifeq (0,$(OMR_ENV_DATA64))
		GLOBAL_ASFLAGS+= -mzarch
	endif
	GLOBAL_ASFLAGS+= -march=z9-109 $(J9M31) -o $*.o
endif

###
### Platform Flags
###

#-- Add Platform flags		
ifeq (s390,$(OMR_HOST_ARCH))
	GLOBAL_CFLAGS+=$(J9M31) -fno-strict-aliasing
	GLOBAL_CXXFLAGS+=$(J9M31) -fno-strict-aliasing
	GLOBAL_CPPFLAGS+=-DS390 -D_LONG_LONG -DJ9VM_TIERED_CODE_CACHE
	ifeq (1,$(OMR_ENV_DATA64))
		GLOBAL_CPPFLAGS+=-DS39064
	endif
endif

ifneq (,$(findstring executable,$(ARTIFACT_TYPE)))
	## Default Libraries
	DEFAULT_LIBS:=-lCTIS -lCISO -lCLBM -lCTAL -lCFVS -lCTBX -lCTXO -lCJ00 -lCTDF -lCOMX -lCOMS -lCTHD -lCPP1 -lCTAD -lTPFSTUB -Wl,-z,origin,\$$ORIGIN,--disable-new-dtags,$(top_srcdir)
	GLOBAL_LDFLAGS+=$(DEFAULT_LIBS)
endif

###
### Shared Libraries
###

ifneq (,$(findstring shared,$(ARTIFACT_TYPE)))

## Export File
# All linux based toolchains use gcc style linker version scripts.  This
# includes xlc. The default rules create a gcc style version script.
$(MODULE_NAME)_LINKER_EXPORT_SCRIPT := $(MODULE_NAME).exp

# assuming a gcc environment

	GLOBAL_LDFLAGS+=-shared
	GLOBAL_LDFLAGS+=-Wl,-Map=$(MODULE_NAME).map
	GLOBAL_LDFLAGS+=-Wl,--version-script,$($(MODULE_NAME)_LINKER_EXPORT_SCRIPT)
	GLOBAL_LDFLAGS+=-Wl,-script=/ztpf/commit/base/util/tools/tpfscript
	GLOBAL_LDFLAGS+=-Wl,-soname=lib$(MODULE_NAME)$(SOLIBEXT)
	GLOBAL_LDFLAGS+=-Xlinker -z -Xlinker origin -Xlinker \$$ORIGIN -Xlinker --disable-new-dtags
	GLOBAL_LDFLAGS+=-Wl,-entry=0 

	ifeq (s390,$(OMR_HOST_ARCH))
		GLOBAL_LDFLAGS+=-Xlinker -rpath-link -Xlinker $(exe_output_dir)
	endif
	
	GLOBAL_LDFLAGS+=-lCTOE
	
	GLOBAL_LDFLAGS+=-lCISO
	GLOBAL_LDFLAGS+=-lCIV1 
	GLOBAL_LDFLAGS+=-lCLC1 
	GLOBAL_LDFLAGS+=-lCTIS 
	GLOBAL_LDFLAGS+=-lCLBM 
	GLOBAL_LDFLAGS+=-lCTAL 
	GLOBAL_LDFLAGS+=-lCFVS 
	GLOBAL_LDFLAGS+=-lCTBX 
	GLOBAL_LDFLAGS+=-lCTXO 
	GLOBAL_LDFLAGS+=-lCTDF 
	GLOBAL_LDFLAGS+=-lCOMX 
	GLOBAL_LDFLAGS+=-lCOMS 
	GLOBAL_LDFLAGS+=-lCTHD 
	GLOBAL_LDFLAGS+=-lCPP1
	GLOBAL_LDFLAGS+=-lCTAD 
	GLOBAL_LDFLAGS+=-lTPFSTUB
	
	GLOBAL_LDFLAGS+=-L/ztpf/commit/base/lib/
endif # ARTIFACT_TYPE contains "shared"


###
### Warning As Errors
###

ifeq ($(OMR_WARNINGS_AS_ERRORS),1)
	ifeq (ppc,$(OMR_HOST_ARCH))
		ifeq (gcc,$(OMR_TOOLCHAIN))
			GLOBAL_CFLAGS += -Wreturn-type -Werror
			GLOBAL_CXXFLAGS += -Wreturn-type -Werror
		else
			GLOBAL_CFLAGS += -qhalt=w
			GLOBAL_CXXFLAGS += -qhalt=w
		endif
	else
		GLOBAL_CFLAGS+=-Wimplicit -Wreturn-type -Werror
		GLOBAL_CXXFLAGS+=-Wreturn-type -Werror
	endif
endif


###
### Enhanced Warnings
###

ifeq ($(OMR_ENHANCED_WARNINGS),1)
	ifneq (ppc,$(OMR_HOST_ARCH))
		GLOBAL_CFLAGS+=-Wall
		GLOBAL_CXXFLAGS+=-Wall -Wno-non-virtual-dtor
	endif
endif

###
### Optimization Flags
###

ifeq ($(OMR_OPTIMIZE),1)
	ifeq (s390x,$(OMR_HOST_ARCH))
		OPTIMIZATION_FLAGS+=-O3 -march=z10 -mtune=z9-109 -mzarch
	else
		OPTIMIZATION_FLAGS+=-O
	endif		
else
	OPTIMIZATION_FLAGS+=-O0
endif

GLOBAL_CFLAGS+=$(OPTIMIZATION_FLAGS)
GLOBAL_CXXFLAGS+=$(OPTIMIZATION_FLAGS)

GLOBAL_CFLAGS+=-D_TPF_SOURCE -DOMRZTPF -DJ9ZTPF -DLINUX -DS390 -DS39064 -DFULL_ANSI -DMAXMOVE -DZTPF_POSIX_SOCKET -fPIC -fno-strict-aliasing -D_GNU_SOURCE -fexec-charset=ISO-8859-1 -fmessage-length=0 -funsigned-char -Wno-format-extra-args  -fverbose-asm -fno-builtin-abort -fno-builtin-exit -fno-builtin-sprintf -ffloat-store -DIBM_ATOE -Wno-unknown-pragmas -Wreturn-type -Wno-unused -Wno-uninitialized -Wno-parentheses -gdwarf-2 -Wno-unused-but-set-variable -Wno-unknown-pragmas -DZTPF -D_TPF_THREADS -mtpf-trace -I/ztpf/commit/base/a2e/headers -isystem /tpfjava/cleanbuild/jvmupgrade/base/include -isystem /tpfjava/cleanbuild/jvmupgrade/opensource/include -isystem /ztpf/commit/base/include -isystem /ztpf/commit/opensource/include46 -isystem /ztpf/commit/opensource/include46/g++ -isystem /ztpf/commit/opensource/include -isystem /ztpf/commit/noship/include -isystem /ztpf/commit
GLOBAL_CXXFLAGS+=-D_TPF_SOURCE -DOMRZTPF -DJ9ZTPF -DLINUX -DS390 -DS39064 -DFULL_ANSI -DMAXMOVE -DZTPF_POSIX_SOCKET -fPIC -fno-strict-aliasing -D_GNU_SOURCE -fexec-charset=ISO-8859-1 -fmessage-length=0 -funsigned-char -Wno-format-extra-args  -fverbose-asm -fno-builtin-abort -fno-builtin-exit -fno-builtin-sprintf -ffloat-store -DIBM_ATOE -Wno-unknown-pragmas -Wreturn-type -Wno-unused -Wno-uninitialized -Wno-parentheses -gdwarf-2 -Wno-unused-but-set-variable -Wno-unknown-pragmas -DZTPF -D_TPF_THREADS -mtpf-trace -I/ztpf/commit/base/a2e/headers -isystem /tpfjava/cleanbuild/jvmupgrade/base/include -isystem /tpfjava/cleanbuild/jvmupgrade/opensource/include -isystem /ztpf/commit/base/include -isystem /ztpf/commit/opensource/include46 -isystem /ztpf/commit/opensource/include46/g++ -isystem /ztpf/commit/opensource/include -isystem /ztpf/commit/noship/include -isystem /ztpf/commit

# Override the default recipe if we are using USE_GNU_DEBUG, so that we strip out the
# symbols and store them seperately.
ifneq (,$(findstring shared,$(ARTIFACT_TYPE)))
	ifeq (1,$(OMR_DEBUG))
		ifeq (1,$(USE_GNU_DEBUG))

			define LINK_C_SHARED_COMMAND
				$(CCLINKSHARED) -o $@ $(OBJECTS) $(LDFLAGS) $(MODULE_LDFLAGS) $(GLOBAL_LDFLAGS)
				cp $@ $@.dbg
				$(OBJCOPY) --strip-debug $@
				$(OBJCOPY) --add-gnu-debuglink=$@ $@.dbg
			endef

			define LINK_CXX_SHARED_COMMAND
				$(CXXLINKSHARED) -o $@ $(OBJECTS) $(LDFLAGS) $(MODULE_LDFLAGS) $(GLOBAL_LDFLAGS)
				cp $@ $@.dbg
				$(OBJCOPY) --strip-debug $@
				$(OBJCOPY) --add-gnu-debuglink=$@ $@.dbg
			endef

			## Files to clean
			CLEAN_FILES=$(OBJECTS) $(OBJECTS:$(OBJEXT)=.i) *.d
			CLEAN_FILES+=$($(MODULE_NAME)_shared).dbg $(MODULE_NAME).map
			define CLEAN_COMMAND
				-$(RM) $(CLEAN_FILES)
			endef

		endif # USE_GNU_DEBUG
	endif # OMR_DEBUG
endif # ARTIFACT_TYPE contains "shared"

###
###
###
# include *.d files generated by the compiler
DEPS := $(OBJECTS:$(OBJEXT)=.d)
show_deps:
	@echo "Dependencies are: $(DEPS)"

ifneq ($(DEPS),)
	-include $(DEPS)
endif

