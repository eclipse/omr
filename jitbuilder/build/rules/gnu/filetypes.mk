###############################################################################
# Copyright (c) 2016, 2016 IBM Corp. and others
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


# These are the rules to compile files of type ".x" into object files
# as well as to generate clean and cleandeps rules
#
# I apologize that this is horribly ugly. Don't get too mad or attached to it
# because I already have several ideas of how to do this in a more readable way
#
# In the meantime though, it works and is very reliable and flexible... 
# ...just ugly
#

#
# Compile .c file into .o file
#
define DEF_RULE.c

$(1): $(2) $(1)$(DEPSUFF)
	@mkdir -p $$(dir $$@)
	$$(C_CMD) $$(C_FLAGS) $$(patsubst %,-D%,$$(C_DEFINES)) $$(patsubst %,-I'%',$$(C_INCLUDES)) -MMD -MF $$@$$(DEPSUFF) -MT $$@ -MP -o $$@ -c $$<

$(1)$(DEPSUFF): ;

-include $(1)$(DEPSUFF)

jit_clean::
	rm -f $(1)

jit_cleandeps::
	rm -f $(1)$(DEPSUFF)

endef # DEF_RULE.c

RULE.c=$(eval $(DEF_RULE.c))

#
# Compile .cpp file into .o file
#
define DEF_RULE.cpp
$(1): $(2) $(1)$(DEPSUFF)
	@mkdir -p $$(dir $$@)
	$$(CXX_CMD) $$(CXX_FLAGS) $$(patsubst %,-D%,$$(CXX_DEFINES)) $$(patsubst %,-I'%',$$(CXX_INCLUDES)) -MMD -MF $$@$$(DEPSUFF) -MT $$@ -MP -o $$@ -c $$<

$(1)$(DEPSUFF): ;

-include $(1)$(DEPSUFF)

jit_clean::
	rm -f $(1)

jit_cleandeps::
	rm -f $(1)$(DEPSUFF)

endef # DEF_RULE.cpp

RULE.cc=$(eval $(DEF_RULE.cpp))
RULE.cpp=$(eval $(DEF_RULE.cpp))

#
# Compile .s file into .o file
#
ifeq ($(LLVM_ASSEMBLER),1)

define DEF_RULE.s
$(1): $(2)
	@mkdir -p $$(dir $$@)
	$$(S_CMD) $$(S_FLAGS) $$(patsubst %,-D%=1,$$(S_DEFINES)) $$(patsubst %,-I'%',$$(S_INCLUDES)) -o $$@ $$<

jit_clean::
	rm -f $(1)
endef

else

define DEF_RULE.s
$(1): $(2)
	@mkdir -p $$(dir $$@)
	$$(S_CMD) $$(S_FLAGS) $$(patsubst %,--defsym %=1,$$(S_DEFINES)) $$(patsubst %,-I'%',$$(S_INCLUDES)) -o $$@ $$<

jit_clean::
	rm -f $(1)
endef

endif


RULE.s=$(eval $(DEF_RULE.s))

##### START X SPECIFIC RULES #####
ifeq ($(HOST_ARCH),x)

# TODO - Fix MASM2GAS to have a little more sane output file behavior
#
# Compile .asm file into .o file
# (By changing it to a .s file and then assembling it)
#
define DEF_RULE.asm
$(1).s: $(2)
	@mkdir -p $$(dir $$@)
	$$(PERL_PATH) $$(ASM_SCRIPT) $$(ASM_FLAGS) $$(patsubst %,-I'%',$$(ASM_INCLUDES)) -o$$(dir $$@) $$<
	-mv $$(dir $$@)$$(notdir $$(basename $$<).s) $$@

jit_clean::
	rm -f $(1).s

$(call RULE.s,$(1),$(1).s)
endef # DEF_RULE.asm

RULE.asm=$(eval $(DEF_RULE.asm))

#
# Compile .pasm file into .o file
#
define DEF_RULE.pasm
$(1).asm: $(2)
	@mkdir -p $$(dir $$@)
	$$(PASM_CMD) $$(PASM_FLAGS) $$(patsubst %,-I'%',$$(PASM_INCLUDES)) -o $$@ -x assembler-with-cpp -E -P $$<

jit_clean::
	rm -f $(1).asm

$(call RULE.asm,$(1),$(1).asm)
endef # DEF_RULE.pasm

RULE.pasm=$(eval $(DEF_RULE.pasm))

endif # ($(HOST_ARCH),x)
##### END X SPECIFIC RULES #####

##### START PPC SPECIFIC RULES #####
ifeq ($(HOST_ARCH),p) 

#
# Compile .ipp file into .o file
#
define DEF_RULE.ipp
$(1).s: $(2)
	@mkdir -p $$(dir $$@)
	$$(IPP_CMD) $$(IPP_FLAGS) 's/\!/\#/g' $$< > $$@

jit_clean::
	rm -f $(1).s

$(call RULE.s,$(1),$(1).s)
endef # DEF_RULE.ipp

RULE.ipp=$(eval $(DEF_RULE.ipp))

#
# Compile .spp file into .o file
#
define DEF_RULE.spp
$(1).ipp: $(2)
	@mkdir -p $$(dir $$@)
	$$(SPP_CMD) $$(SPP_FLAGS) $$(patsubst %,-D%,$$(SPP_DEFINES)) $$(patsubst %,-I'%',$$(SPP_INCLUDES)) -o $$@ -x assembler-with-cpp -E -P $$<

jit_clean::
	rm -f $(1).ipp

$(call RULE.ipp,$(1),$(1).ipp)
endef # DEF_RULE.spp

RULE.spp=$(eval $(DEF_RULE.spp))

endif # ($(HOST_ARCH),p) 
##### END PPC SPECIFIC RULES #####

##### START Z SPECIFIC RULES #####
ifeq ($(HOST_ARCH),z)

#
# Compile .m4 file into .o file
#
define DEF_RULE.m4
$(1).s: $(2)
	@mkdir -p $$(dir $$@)
	$$(PERL_PATH) $$(ZASM_SCRIPT) $$<
	$$(M4_CMD) $$(M4_FLAGS) $$(patsubst %,-I'%',$$(M4_INCLUDES)) $$(patsubst %,-D%,$$(M4_DEFINES)) $$< > $$@
	$$(PERL_PATH) $$(ZASM_SCRIPT) $$@

jit_clean::
	rm -f $(1).s

$(call RULE.s,$(1),$(1).s)
endef # DEF_RULE.m4

RULE.m4=$(eval $(DEF_RULE.m4))

endif # ($(HOST_ARCH),z)
##### END Z SPECIFIC RULES #####

##### START ARM SPECIFIC RULES #####
ifeq ($(HOST_ARCH),arm)

#
# Preprocess .armasm file into .spp
#
define DEF_RULE.armasm
$(1).spp: $(2)
	@mkdir -p $$(dir $$@)
	$$(ARMASM_CMD) -f $$(ARMASM_SCRIPT) $$< > $$@

jit_clean::
	rm -f $(1).spp

$(call RULE.spp,$(1),$(1).spp)
endef

RULE.armasm=$(eval $(DEF_RULE.armasm))

#
# Preprocess .spp file into .s
#
define DEF_RULE.spp
$(1).s: $(2)
	@mkdir -p $$(dir $$@)
	$$(SPP_CMD) $$(SPP_FLAGS) $$(patsubst %,-D%,$$(SPP_DEFINES)) $$(patsubst %,-I'%',$$(SPP_INCLUDES)) -o $$@ -x assembler-with-cpp -E -P $$<

jit_clean::
	rm -f $(1).s

$(call RULE.s,$(1),$(1).s)
endef

RULE.spp=$(eval $(DEF_RULE.spp))

endif # ($(HOST_ARCH),arm)
##### END ARM SPECIFIC RULES #####
