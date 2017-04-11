################################################################################
##
## (c) Copyright IBM Corp. 2016, 2016
##
##  This program and the accompanying materials are made available
##  under the terms of the Eclipse Public License v1.0 and
##  Apache License v2.0 which accompanies this distribution.
##
##      The Eclipse Public License is available at
##      http://www.eclipse.org/legal/epl-v10.html
##
##      The Apache License v2.0 is available at
##      http://www.opensource.org/licenses/apache2.0.php
##
## Contributors:
##    Multiple authors (IBM Corp.) - initial implementation and documentation
################################################################################


# Add our targets to the global targets
all: jit jitbuilder
clean: jit_clean
cleandeps: jit_cleandeps
cleandll: jit_cleandll

#
# Define our targets. "jit_clean" "jit_cleandeps" and "jit_cleandll" are double-colon so they can be appended to
# throughout the makefile.
#
.phony: jit jit_clean jit_cleandeps jit_cleandll
jit:
jit_clean::
jit_cleandeps::
jit_cleandll::

include $(JIT_MAKE_DIR)/rules/$(TOOLCHAIN)/common.mk

RELEASE_DIR=release
RELEASE_SRC=$(RELEASE_DIR)/src
RELEASE_INCLUDE=$(RELEASE_DIR)/include
JITBUILDER_TARBALL=jitbuilder.tgz

jitbuilder: $(JITBUILDER_TARBALL)

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/env:
	mkdir -p $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/env

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il:
	mkdir -p $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen:
	mkdir -p $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/env/jittypes.h: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/env/jittypes.h $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/env
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/env/defines.h: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/env/defines.h $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/env
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il/DataTypes.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/il/DataTypes.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il/DataTypesEnum.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/il/DataTypesEnum.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il/ILOpCodes.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/il/ILOpCodes.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il/OMRDataTypes.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/il/OMRDataTypes.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il/ILOpCodesEnum.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/il/ILOpCodesEnum.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il/OMRILOpCodesEnum.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/il/OMRILOpCodesEnum.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il/ILHelpers.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/il/ILHelpers.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/IlInjector.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/ilgen/IlInjector.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/IlBuilder.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/ilgen/IlBuilder.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/MethodBuilder.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/ilgen/MethodBuilder.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/ThunkBuilder.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/ilgen/ThunkBuilder.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/BytecodeBuilder.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/ilgen/BytecodeBuilder.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/TypeDictionary.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/ilgen/TypeDictionary.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/VirtualMachineState.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/ilgen/VirtualMachineState.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/VirtualMachineRegister.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/ilgen/VirtualMachineRegister.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/VirtualMachineRegisterInStruct.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/ilgen/VirtualMachineRegisterInStruct.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/VirtualMachineOperandStack.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/ilgen/VirtualMachineOperandStack.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen
	cp $< $@ || cp $< $@

$(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/IlGen.hpp: $(FIXED_SRCBASE)/$(JIT_OMR_DIRTY_DIR)/ilgen/IlGen.hpp $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen
	cp $< $@ || cp $< $@

JITBUILDER_FILES=$(RELEASE_DIR)/Makefile \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/env/defines.h \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/env/jittypes.h \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il/DataTypes.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il/DataTypesEnum.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il/ILHelpers.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il/ILOpCodes.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il/ILOpCodesEnum.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il/OMRDataTypes.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/il/OMRILOpCodesEnum.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/IlInjector.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/IlBuilder.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/MethodBuilder.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/ThunkBuilder.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/BytecodeBuilder.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/TypeDictionary.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/VirtualMachineState.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/VirtualMachineRegister.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/VirtualMachineRegisterInStruct.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/VirtualMachineOperandStack.hpp \
             $(RELEASE_INCLUDE)/$(JIT_OMR_DIRTY_DIR)/ilgen/IlGen.hpp \
             $(RELEASE_SRC)/Call.hpp \
             $(RELEASE_SRC)/Call.cpp \
             $(RELEASE_SRC)/DotProduct.hpp \
             $(RELEASE_SRC)/DotProduct.cpp \
             $(RELEASE_SRC)/IterativeFib.hpp \
             $(RELEASE_SRC)/IterativeFib.cpp \
             $(RELEASE_SRC)/LinkedList.hpp \
             $(RELEASE_SRC)/LinkedList.cpp \
             $(RELEASE_SRC)/Mandelbrot.hpp \
             $(RELEASE_SRC)/Mandelbrot.cpp \
             $(RELEASE_SRC)/NestedLoop.hpp \
             $(RELEASE_SRC)/NestedLoop.cpp \
             $(RELEASE_SRC)/Pointer.hpp \
             $(RELEASE_SRC)/Pointer.cpp \
             $(RELEASE_SRC)/RecursiveFib.hpp \
             $(RELEASE_SRC)/RecursiveFib.cpp \
             $(RELEASE_SRC)/Simple.hpp \
             $(RELEASE_SRC)/Simple.cpp \
             $(RELEASE_SRC)/Switch.hpp \
             $(RELEASE_SRC)/Switch.cpp \
             $(RELEASE_SRC)/Pow2.hpp \
             $(RELEASE_SRC)/Pow2.cpp \


$(JITBUILDER_TARBALL) : $(JITBUILDER_FILES) $(JIT_PRODUCT_BACKEND_LIBRARY)
	cd $(RELEASE_DIR) && tar cvzf $(JITBUILDER_TARBALL) libjitbuilder.a Makefile README.md LICENSE include/ src/
