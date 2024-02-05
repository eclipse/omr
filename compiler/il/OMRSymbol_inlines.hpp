/*******************************************************************************
 * Copyright IBM Corp. and others 2017
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution
 * and is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following Secondary
 * Licenses when the conditions for such availability set forth in the
 * Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
 * version 2 with the GNU Classpath Exception [1] and GNU General Public
 * License, version 2 with the OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef OMR_SYMBOL_INLINES_INCL
#define OMR_SYMBOL_INLINES_INCL

#include "il/Symbol.hpp"

/**
 * Downcast to concrete instance
 *
 * This method is defined out-of-line because it requires a complete definition
 * of OMR::Symbol which is unavailable at the time the class OMR::Symbol is
 * being defined.
 *
 * This must be a static_cast or else the downcast will be unsafe.
 */
TR::Symbol *
OMR::Symbol::self()
   {
   return static_cast<TR::Symbol *>(this);
   }

/**
 * Cast to symbol type
 */
TR::RegisterMappedSymbol * OMR::Symbol::castToRegisterMappedSymbol()
   {
   TR_ASSERT(self()->isRegisterMappedSymbol(), "OMR::Symbol::castToRegisterMappedSymbol, symbol is not a register mapped symbol");
   return (TR::RegisterMappedSymbol *)this;
   }

TR::AutomaticSymbol * OMR::Symbol::castToAutoSymbol()
   {
   TR_ASSERT(self()->isAuto(), "OMR::Symbol::castToAutoSymbo, symbol is not an automatic symbol");
   return (TR::AutomaticSymbol *)this;
   }

TR::ParameterSymbol * OMR::Symbol::castToParmSymbol()
   {
   TR_ASSERT(self()->isParm(), "OMR::Symbol::castToParmSymbol, symbol is not a parameter symbol");
   return (TR::ParameterSymbol *)this;
   }

TR::AutomaticSymbol * OMR::Symbol::castToInternalPointerAutoSymbol()
   {
   TR_ASSERT(self()->isInternalPointerAuto(), "OMR::Symbol::castToInternalAutoSymbol, symbol is not an internal pointer automatic symbol");
   return (TR::AutomaticSymbol *)this;
   }

TR::AutomaticSymbol * OMR::Symbol::castToLocalObjectSymbol()
   {
   TR_ASSERT(self()->isLocalObject(), "OMR::Symbol::castToLocalObjectSymbol, symbol is not an internal pointer automatic symbol");
   return (TR::AutomaticSymbol *)this;
   }

TR::StaticSymbol * OMR::Symbol::castToStaticSymbol()
   {
   TR_ASSERT(self()->isStatic(), "OMR::Symbol::castToStaticSymbol, symbol is not a static symbol");
   return (TR::StaticSymbol *)this;
   }

TR::StaticSymbol * OMR::Symbol::castToNamedStaticSymbol()
   {
   TR_ASSERT(self()->isNamed() && self()->isStatic(), "OMR::Symbol::castToNamedStaticSymbol, symbol is not a named static symbol");
   return (TR::StaticSymbol *)this;
   }

TR::MethodSymbol * OMR::Symbol::castToMethodSymbol()
   {
   TR_ASSERT(self()->isMethod(), "OMR::Symbol::castToMethodSymbol, symbol[%p] is not a method symbol",
         this);
   return (TR::MethodSymbol *)this;
   }

TR::ResolvedMethodSymbol * OMR::Symbol::castToResolvedMethodSymbol()
   {
   TR_ASSERT(self()->isResolvedMethod(), "OMR::Symbol::castToResolvedMethodSymbol, symbol is not a resolved method symbol");
   return (TR::ResolvedMethodSymbol *)this;
   }

TR::Symbol * OMR::Symbol::castToShadowSymbol()
   {
   TR_ASSERT(self()->isShadow(), "OMR::Symbol::castToShadowSymbol, symbol is not a shadow symbol");
   return (TR::Symbol *)this;
   }

TR::RegisterMappedSymbol * OMR::Symbol::castToMethodMetaDataSymbol()
   {
   TR_ASSERT(self()->isMethodMetaData(), "OMR::Symbol::castToMethodMetaDataSymbol, symbol is not a meta data symbol");
   return (TR::RegisterMappedSymbol *)this;
   }

TR::LabelSymbol * OMR::Symbol::castToLabelSymbol()
   {
   TR_ASSERT(self()->isLabel(), "OMR::Symbol::castToLabelSymbol, symbol is not a label symbol");
   return (TR::LabelSymbol *)this;
   }

TR::ResolvedMethodSymbol * OMR::Symbol::castToJittedMethodSymbol()
   {
   TR_ASSERT(self()->isJittedMethod(), "OMR::Symbol::castToJittedMethodSymbol, symbol is not a resolved method symbol");
   return (TR::ResolvedMethodSymbol *)this;
   }

TR::StaticSymbol *OMR::Symbol::castToCallSiteTableEntrySymbol()
   {
   TR_ASSERT(self()->isCallSiteTableEntry(), "OMR::Symbol::castToCallSiteTableEntrySymbol expected a call site table entry symbol");
   return (TR::StaticSymbol*)this;
   }

TR::StaticSymbol *OMR::Symbol::castToMethodTypeTableEntrySymbol()
   {
   TR_ASSERT(self()->isMethodTypeTableEntry(), "OMR::Symbol::castToMethodTypeTableEntrySymbol expected a method type table entry symbol");
   return (TR::StaticSymbol*)this;
   }

TR::AutomaticSymbol * OMR::Symbol::castToVariableSizeSymbol()
   {
   TR_ASSERT(self()->isVariableSizeSymbol(), "OMR::Symbol::castToVariableSizeSymbol, symbol is not a VariableSizeSymbol symbol");
   return (TR::AutomaticSymbol *)this;
   }

/**
 * Flag functions
 */

bool
OMR::Symbol::isCollectedReference()
   {
   return (self()->getDataType()==TR::Address || self()->isLocalObject()) && !self()->isNotCollected();
   }

bool
OMR::Symbol::isInternalPointerAuto()
   {
   return (self()->isInternalPointer() && self()->isAuto());
   }

bool
OMR::Symbol::isNamed()
   {
   return self()->isStatic() && _flags.testAny(IsNamed);
   }

void
OMR::Symbol::setSpillTempAuto()
   {
   TR_ASSERT(self()->isAuto(), "assertion failure");
   _flags.set(SpillTemp);
   }

bool
OMR::Symbol::isSpillTempAuto()
   {
   return self()->isAuto() && _flags.testAny(SpillTemp);
   }

void
OMR::Symbol::setLocalObject()
   {
   TR_ASSERT(self()->isAuto(), "assertion failure");
   _flags.set(IsLocalObject);
   }

bool
OMR::Symbol::isLocalObject()
   {
   return self()->isAuto() && _flags.testAny(IsLocalObject);
   }

void
OMR::Symbol::setBehaveLikeNonTemp()
   {
   TR_ASSERT(self()->isAuto(), "assertion failure");
   _flags.set(BehaveLikeNonTemp);
   }

bool
OMR::Symbol::behaveLikeNonTemp()
   {
   return self()->isAuto() && _flags.testAny(BehaveLikeNonTemp);
   }

void
OMR::Symbol::setPinningArrayPointer()
   {
   TR_ASSERT(self()->isAuto(), "assertion failure");
   _flags.set(PinningArrayPointer);
   }

bool
OMR::Symbol::isPinningArrayPointer()
   {
   return self()->isAuto() && _flags.testAny(PinningArrayPointer);
   }

void
OMR::Symbol::setSpillTempLoaded()
   {
   if (self()->isAuto()) // For non auto spills (ie. to original location) we can't optimize removing spills
     _flags.set(SpillTempLoaded);
   }

bool
OMR::Symbol::isSpillTempLoaded()
   {
   return self()->isSpillTempAuto() && _flags.testAny(SpillTempLoaded);
   }

void
OMR::Symbol::setVariableSizeSymbol()
   {
   TR_ASSERT(self()->isAuto(), "assertion failure");
   _flags.set(VariableSizeSymbol);
   }

bool
OMR::Symbol::isVariableSizeSymbol()
   {
   return (self()->isAuto() && _flags.testAny(VariableSizeSymbol));
   }

void
OMR::Symbol::setThisTempForObjectCtor()
   {
   TR_ASSERT(self()->isAuto(), "assertion failure");
   _flags.set(ThisTempForObjectCtor);
   }

bool
OMR::Symbol::isThisTempForObjectCtor()
   {
   return self()->isAuto() && _flags.testAny(ThisTempForObjectCtor);
   }

void
OMR::Symbol::setParmHasToBeOnStack()
   {
   TR_ASSERT(self()->isParm(), "assertion failure");
   _flags.set(ParmHasToBeOnStack);
   }

bool
OMR::Symbol::isParmHasToBeOnStack()
   {
   return self()->isParm() && _flags.testAny(ParmHasToBeOnStack);
   }

void
OMR::Symbol::setReferencedParameter()
   {
   TR_ASSERT(self()->isParm(), "assertion failure");
   _flags.set(ReferencedParameter);
   }

void
OMR::Symbol::resetReferencedParameter()
   {
   TR_ASSERT(self()->isParm(), "assertion failure");
   _flags.reset(ReferencedParameter);
   }

bool
OMR::Symbol::isReferencedParameter()
   {
   return self()->isParm() && _flags.testAny(ReferencedParameter);
   }

void
OMR::Symbol::setReinstatedReceiver()
   {
   TR_ASSERT(self()->isParm(), "assertion failure");
   _flags.set(ReinstatedReceiver);
   }

bool
OMR::Symbol::isReinstatedReceiver()
   {
   return self()->isParm() && _flags.testAny(ReinstatedReceiver);
   }

void
OMR::Symbol::setConstString()
   {
   TR_ASSERT(self()->isStatic(), "assertion failure");
   _flags.set(ConstString);
   }

bool
OMR::Symbol::isConstString()
   {
   return self()->isStatic() && _flags.testAny(ConstString);
   }

void
OMR::Symbol::setConstantDynamic()
   {
   TR_ASSERT(self()->isStatic(), "assertion failure");
   _flags2.set(ConstantDynamic);
   }

bool
OMR::Symbol::isConstantDynamic()
   {
   return self()->isStatic() && _flags2.testAny(ConstantDynamic);
   }

void
OMR::Symbol::setAddressIsCPIndexOfStatic(bool b)
   {
   TR_ASSERT(self()->isStatic(), "assertion failure");
   _flags.set(AddressIsCPIndexOfStatic, b);
   }

bool
OMR::Symbol::addressIsCPIndexOfStatic()
   {
   return self()->isStatic() && _flags.testAny(AddressIsCPIndexOfStatic);
   }

bool
OMR::Symbol::isRecognizedStatic()
   {
   return self()->isStatic() && _flags.testAny(RecognizedStatic);
   }

void
OMR::Symbol::setCompiledMethod()
   {
   TR_ASSERT(self()->isStatic(), "assertion failure");
   _flags.set(CompiledMethod);
   }

bool
OMR::Symbol::isCompiledMethod()
   {
   return self()->isStatic() && _flags.testAny(CompiledMethod);
   }

void
OMR::Symbol::setStartPC()
   {
   TR_ASSERT(self()->isStatic(), "assertion failure");
   _flags.set(StartPC);
   }

bool
OMR::Symbol::isStartPC()
   {
   return self()->isStatic() && _flags.testAny(StartPC);
   }

void
OMR::Symbol::setCountForRecompile()
   {
   TR_ASSERT(self()->isStatic(), "assertion failure");
   _flags.set(CountForRecompile);
   }

bool
OMR::Symbol::isCountForRecompile()
   {
   return self()->isStatic() && _flags.testAny(CountForRecompile);
   }

void
OMR::Symbol::setRecompilationCounter()
   {
   TR_ASSERT(self()->isStatic(), "assertion failure");
   _flags.set(RecompilationCounter);
   }

bool
OMR::Symbol::isRecompilationCounter()
   {
   return self()->isStatic() && _flags.testAny(RecompilationCounter);
   }

void
OMR::Symbol::setGCRPatchPoint()
   {
   TR_ASSERT(self()->isStatic(), "assertion failure");
   _flags.set(GCRPatchPoint);
   }

bool
OMR::Symbol::isGCRPatchPoint()
   {
   return self()->isStatic() && _flags.testAny(GCRPatchPoint);
   }

bool
OMR::Symbol::isJittedMethod()
   {
   return self()->isResolvedMethod() && _flags.testAny(IsJittedMethod);
   }

void
OMR::Symbol::setArrayShadowSymbol()
   {
   TR_ASSERT(self()->isShadow(), "assertion failure");
   _flags.set(ArrayShadow);
   }

bool
OMR::Symbol::isArrayShadowSymbol()
   {
   return self()->isShadow() && _flags.testAny(ArrayShadow);
   }


void
OMR::Symbol::setContiguousArrayDataAddrFieldSymbol()
   {
   _flags.set(contiguousArrayDataAddrField);
   }

bool
OMR::Symbol::isContiguousArrayDataAddrFieldSymbol()
   {
   return _flags.testAny(contiguousArrayDataAddrField);
   }

bool
OMR::Symbol::isRecognizedShadow()
   {
   return self()->isShadow() && _flags.testAny(RecognizedShadow);
   }

bool
OMR::Symbol::isRecognizedKnownObjectShadow()
   {
   return self()->isRecognizedShadow() && _flags.testAny(RecognizedKnownObjectShadow);
   }

void
OMR::Symbol::setArrayletShadowSymbol()
   {
   TR_ASSERT(self()->isShadow(), "assertion failure");
   _flags.set(ArrayletShadow);
   }

bool
OMR::Symbol::isArrayletShadowSymbol()
   {
   return self()->isShadow() && _flags.testAny(ArrayletShadow);
   }

void
OMR::Symbol::setGlobalFragmentShadowSymbol()
   {
   TR_ASSERT(self()->isShadow(), "assertion failure");
   _flags.set(GlobalFragmentShadow);
   }

bool
OMR::Symbol::isGlobalFragmentShadowSymbol()
   {
   return self()->isShadow() && _flags.testAny(GlobalFragmentShadow);
   }

void
OMR::Symbol::setMemoryTypeShadowSymbol()
   {
   TR_ASSERT(self()->isShadow(), "assertion failure");
   _flags.set(MemoryTypeShadow);
   }

bool
OMR::Symbol::isMemoryTypeShadowSymbol()
   {
   return self()->isShadow() && _flags.testAny(MemoryTypeShadow);
   }

void
OMR::Symbol::setStartOfColdInstructionStream()
   {
   TR_ASSERT(self()->isLabel(), "assertion failure"); _flags.set(StartOfColdInstructionStream);
   }

bool
OMR::Symbol::isStartOfColdInstructionStream()
   {
   return self()->isLabel() && _flags.testAny(StartOfColdInstructionStream);
   }

void
OMR::Symbol::setStartInternalControlFlow()
   {
   TR_ASSERT(self()->isLabel(), "can only set start ICF on a label"); _flags.set(StartInternalControlFlow);
   }

bool
OMR::Symbol::isStartInternalControlFlow()
   {
   return self()->isLabel() && _flags.testAny(StartInternalControlFlow);
   }

void
OMR::Symbol::setEndInternalControlFlow()
   {
   TR_ASSERT(self()->isLabel(), "can only set end ICF on a label"); _flags.set(EndInternalControlFlow);
   }

bool
OMR::Symbol::isEndInternalControlFlow()
   {
   return self()->isLabel() && _flags.testAny(EndInternalControlFlow);
   }

void
OMR::Symbol::setInternalControlFlowMerge()
   {
   TR_ASSERT(self()->isLabel(), "assertion failure"); _flags.set(InternalControlFlowMerge);
   }

bool
OMR::Symbol::isInternalControlFlowMerge()
   {
   return self()->isLabel() && _flags.testAny(InternalControlFlowMerge);
   }

void
OMR::Symbol::setEndOfColdInstructionStream()
   {
   TR_ASSERT(self()->isLabel(), "assertion failure"); _flags.set(EndOfColdInstructionStream);
   }

bool
OMR::Symbol::isEndOfColdInstructionStream()
   {
   return self()->isLabel() && _flags.testAny(EndOfColdInstructionStream);
   }

void
OMR::Symbol::setNonLinear()
   {
   _flags.set(NonLinear);
   }

bool
OMR::Symbol::isNonLinear()
   {
   return (self()->isLabel() && _flags.testValue(OOLMask, (StartOfColdInstructionStream|NonLinear)));
   }

void
OMR::Symbol::setRelativeLabel()
   {
   TR_ASSERT(self()->isLabel(), "assertion failure"); _flags2.set(RelativeLabel);
   }

bool
OMR::Symbol::isRelativeLabel()
   {
   return self()->isLabel() && _flags2.testAny(RelativeLabel);
   }

void
OMR::Symbol::setConstMethodType()
   {
   TR_ASSERT(self()->isStatic(), "assertion failure");
   _flags2.set(ConstMethodType);
   }

bool
OMR::Symbol::isConstMethodType()
   {
   return self()->isStatic() && _flags2.testAny(ConstMethodType);
   }

void
OMR::Symbol::setConstMethodHandle()
   {
   TR_ASSERT(self()->isStatic(), "assertion failure");
   _flags2.set(ConstMethodHandle);
   }

bool
OMR::Symbol::isConstantPoolAddress()
   {
   return self()->isStatic() && _flags.testAny(ConstantPoolAddress);
   }

void
OMR::Symbol::setConstantPoolAddress()
   {
   TR_ASSERT(self()->isStatic(), "ConstantPoolAddress symbol has to be static");
   _flags.set(ConstantPoolAddress);
   }

bool
OMR::Symbol::isStaticAddressWithinMethodBounds()
   {
   return self()->isStatic() && _flags.testAny(StaticAddressWithinMethodBounds);
   }

void
OMR::Symbol::setStaticAddressWithinMethodBounds()
   {
   TR_ASSERT(self()->isStatic(), "Symbol must be static");
   _flags.set(StaticAddressWithinMethodBounds);
   }

bool
OMR::Symbol::isConstMethodHandle()
   {
   return self()->isStatic() && _flags2.testAny(ConstMethodHandle);
   }

void
OMR::Symbol::setNonSpecificConstObject()
   {
   TR_ASSERT(self()->isStatic(), "assert failure");
   _flags2.set(NonSpecificConstObject);
   }

bool
OMR::Symbol::isNonSpecificConstObject()
   {
   return self()->isStatic() && _flags2.testAny(NonSpecificConstObject);
   }

bool
OMR::Symbol::isConstObjectRef()
   {
   return self()->isStatic() && (_flags.testAny(ConstString) || _flags2.testAny(NonSpecificConstObject|ConstMethodType|ConstMethodHandle|ConstantDynamic));
   }

bool
OMR::Symbol::isStaticField()
   {
   return self()->isStatic() && !(self()->isConstObjectRef() || self()->isClassObject() || self()->isAddressOfClassObject() || self()->isConst());
   }

bool
OMR::Symbol::isFixedObjectRef()
   {
   return self()->isConstObjectRef() || self()->isCallSiteTableEntry() || self()->isMethodTypeTableEntry();
   }

void
OMR::Symbol::setCallSiteTableEntry()
   {
   TR_ASSERT(self()->isStatic(), "assertion failure");
   _flags2.set(CallSiteTableEntry);
   }

bool
OMR::Symbol::isCallSiteTableEntry()
   {
   return self()->isStatic() && _flags2.testAny(CallSiteTableEntry);
   }

void
OMR::Symbol::setMethodTypeTableEntry()
   {
   TR_ASSERT(self()->isStatic(), "assertion failure");
   _flags2.set(MethodTypeTableEntry);
   }

bool
OMR::Symbol::isMethodTypeTableEntry()
   {
   return self()->isStatic() && _flags2.testAny(MethodTypeTableEntry);
   }

void
OMR::Symbol::setNotDataAddress()
   {
   TR_ASSERT(self()->isStatic(), "assertion failure");
   _flags2.set(NotDataAddress);
   }

bool
OMR::Symbol::isNotDataAddress()
   {
   return self()->isStatic() && _flags2.testAny(NotDataAddress);
   }

void
OMR::Symbol::setUnsafeShadowSymbol()
   {
   TR_ASSERT(self()->isShadow(), "assertion failure"); _flags2.set(UnsafeShadow);
   }

bool
OMR::Symbol::isUnsafeShadowSymbol()
   {
   return self()->isShadow() && _flags2.testAny(UnsafeShadow);
   }

void
OMR::Symbol::setNamedShadowSymbol()
   {
   TR_ASSERT(self()->isShadow(), "assertion failure"); _flags2.set(NamedShadow);
   }

bool
OMR::Symbol::isNamedShadowSymbol()
   {
   return self()->isShadow() && _flags2.testAny(NamedShadow);
   }

TR::DataType
OMR::Symbol::getType()
   {
   return self()->getDataType();
   }

bool
OMR::Symbol::isMethod()
   {
   return self()->getKind() == IsMethod || self()->getKind() == IsResolvedMethod;
   }

bool
OMR::Symbol::isRegisterMappedSymbol()
   {
   return self()->getKind() <= LastRegisterMapped;
   }

bool
OMR::Symbol::isAutoOrParm()
   {
   return self()->getKind() <= IsParameter;
   }

bool
OMR::Symbol::isRegularShadow()
   {
   return self()->isShadow() && !self()->isAutoField() && !self()->isParmField();
   }

bool
OMR::Symbol::isSyncVolatile()
   {
   return self()->isVolatile();
   }

void
OMR::Symbol::setDummyResolvedMethod()
   {
   _flags2.set(DummyResolvedMethod);
   }

bool
OMR::Symbol::isDummyResolvedMethod()
   {
   return _flags2.testAny(DummyResolvedMethod);
   }

void
OMR::Symbol::setStaticDefaultValueInstance()
   {
   TR_ASSERT(self()->isStatic(), "Symbol must be static");
   _flags2.set(StaticDefaultValueInstance);
   }

bool
OMR::Symbol::isStaticDefaultValueInstance()
   {
   return self()->isStatic() && _flags2.testAny(StaticDefaultValueInstance);
   }

TR::RegisterMappedSymbol *
OMR::Symbol::getRegisterMappedSymbol()
   {
   return self()->isRegisterMappedSymbol() ? (TR::RegisterMappedSymbol *)this : 0;
   }

TR::AutomaticSymbol *
OMR::Symbol::getAutoSymbol()
   {
   return self()->isAuto() ? (TR::AutomaticSymbol *)this : 0;
   }

TR::ParameterSymbol *
OMR::Symbol::getParmSymbol()
   {
   return self()->isParm() ? (TR::ParameterSymbol *)this : 0;
   }

TR::AutomaticSymbol *
OMR::Symbol::getInternalPointerAutoSymbol()
   {
   return self()->isInternalPointerAuto() ? (TR::AutomaticSymbol *)this : 0;
   }

TR::AutomaticSymbol *
OMR::Symbol::getLocalObjectSymbol()
   {
   return self()->isLocalObject() ? (TR::AutomaticSymbol *)this : 0;
   }

TR::StaticSymbol *
OMR::Symbol::getStaticSymbol()
   {
   return self()->isStatic() ? (TR::StaticSymbol *)this : 0;
   }

TR::MethodSymbol *
OMR::Symbol::getMethodSymbol()
   {
   return self()->isMethod() ? (TR::MethodSymbol *)this : 0;
   }

TR::ResolvedMethodSymbol *
OMR::Symbol::getResolvedMethodSymbol()
   {
   return self()->isResolvedMethod() ? (TR::ResolvedMethodSymbol *)this : 0;
   }

TR::Symbol *
OMR::Symbol::getShadowSymbol()
   {
   return self()->isShadow() ? (TR::Symbol *)this : 0;
   }

TR::RegisterMappedSymbol *
OMR::Symbol::getMethodMetaDataSymbol()
   {
   return self()->isMethodMetaData() ? (TR::RegisterMappedSymbol *)this : 0;
   }

TR::LabelSymbol *
OMR::Symbol::getLabelSymbol()
   {
   return self()->isLabel() ? (TR::LabelSymbol *)this : 0;
   }

TR::ResolvedMethodSymbol *
OMR::Symbol::getJittedMethodSymbol()
   {
   return self()->isJittedMethod() ? (TR::ResolvedMethodSymbol *)this : 0;
   }

TR::StaticSymbol *
OMR::Symbol::getCallSiteTableEntrySymbol()
   {
   return self()->isCallSiteTableEntry()? self()->castToCallSiteTableEntrySymbol() : NULL;
   }

TR::StaticSymbol *
OMR::Symbol::getMethodTypeTableEntrySymbol()
   {
   return self()->isMethodTypeTableEntry()? self()->castToMethodTypeTableEntrySymbol() : NULL;
   }

TR::Symbol *
OMR::Symbol::getNamedShadowSymbol()
   {
   return self()->isNamedShadowSymbol() ? (TR::Symbol *)this : 0;
   }

TR::StaticSymbol *
OMR::Symbol::getRecognizedStaticSymbol()
   {
   return self()->isRecognizedStatic() ? (TR::StaticSymbol*)this : 0;
   }

TR::AutomaticSymbol *
OMR::Symbol::getVariableSizeSymbol()
   {
   return self()->isVariableSizeSymbol() ? (TR::AutomaticSymbol *)this : 0;
   }

#endif // OMR_SYMBOL_INLINES_INCL
