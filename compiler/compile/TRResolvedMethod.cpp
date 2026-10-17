/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#include "compile/TRResolvedMethod.hpp"

#include <string.h>
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"
#include "compile/SymbolReferenceTable.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/jittypes.h"
#include "il/ParameterSymbol.hpp"
#include "il/ResolvedMethodSymbol.hpp"
#include "il/SymbolReference.hpp"
#include "infra/List.hpp"
#include "runtime/Runtime.hpp"

class TR_FrontEnd;
class TR_OpaqueMethodBlock;
class TR_PrexArgInfo;

TR::Method *TR_ResolvedMethod::convertToMethod()
{
    TR_UNIMPLEMENTED();
    return 0;
}

uint32_t TR_ResolvedMethod::numberOfParameters()
{
    TR_UNIMPLEMENTED();
    return 0;
}

uint32_t TR_ResolvedMethod::numberOfExplicitParameters()
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR::DataType TR_ResolvedMethod::parmType(uint32_t)
{
    TR_UNIMPLEMENTED();
    return TR::NoType;
}

char *TR_ResolvedMethod::getParameterTypeSignature(int32_t parmIndex)
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR::ILOpCodes TR_ResolvedMethod::directCallOpCode()
{
    TR_UNIMPLEMENTED();
    return TR::BadILOp;
}

TR::ILOpCodes TR_ResolvedMethod::indirectCallOpCode()
{
    TR_UNIMPLEMENTED();
    return TR::BadILOp;
}

TR::DataType TR_ResolvedMethod::returnType()
{
    TR_UNIMPLEMENTED();
    return TR::NoType;
}

uint32_t TR_ResolvedMethod::returnTypeWidth()
{
    TR_UNIMPLEMENTED();
    return 0;
}

bool TR_ResolvedMethod::returnTypeIsUnsigned()
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR::ILOpCodes TR_ResolvedMethod::returnOpCode()
{
    TR_UNIMPLEMENTED();
    return TR::BadILOp;
}

uint16_t TR_ResolvedMethod::classNameLength() { return static_cast<uint16_t>(strlen(classNameChars())); }

uint16_t TR_ResolvedMethod::nameLength() { return static_cast<uint16_t>(strlen(nameChars())); }

uint16_t TR_ResolvedMethod::signatureLength() { return static_cast<uint16_t>(strlen(signatureChars())); }

char *TR_ResolvedMethod::classNameChars()
{
    TR_UNIMPLEMENTED();
    return 0;
}

char *TR_ResolvedMethod::nameChars()
{
    TR_UNIMPLEMENTED();
    return 0;
}

char *TR_ResolvedMethod::signatureChars()
{
    TR_UNIMPLEMENTED();
    return 0;
}

bool TR_ResolvedMethod::isConstructor() { return false; }

bool TR_ResolvedMethod::isStatic() { return true; } // useful default

bool TR_ResolvedMethod::isAbstract() { return false; }

bool TR_ResolvedMethod::isCompilable(TR_Memory *) { return true; } // useful default

bool TR_ResolvedMethod::isInlineable(TR::Compilation *)
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::isNative() { return false; }

bool TR_ResolvedMethod::isSynchronized() { return false; }

bool TR_ResolvedMethod::isPrivate() { return false; }

bool TR_ResolvedMethod::isProtected() { return false; }

bool TR_ResolvedMethod::isPublic() { return true; } // useful default

bool TR_ResolvedMethod::isFinal() { return false; }

bool TR_ResolvedMethod::isInterpreted()
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::isInterpretedForHeuristics()
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::hasBackwardBranches() { return false; }

bool TR_ResolvedMethod::isObjectConstructor()
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::isNonEmptyObjectConstructor() { return false; }

bool TR_ResolvedMethod::isCold(TR::Compilation *, bool, TR::ResolvedMethodSymbol * /* = NULL */)
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::isSubjectToPhaseChange(TR::Compilation *) { return false; }

bool TR_ResolvedMethod::isSameMethod(TR_ResolvedMethod *other)
{
    return getPersistentIdentifier() == other->getPersistentIdentifier();
}

bool TR_ResolvedMethod::isNewInstanceImplThunk() { return false; }

bool TR_ResolvedMethod::isJNINative() { return false; }

bool TR_ResolvedMethod::isJITInternalNative() { return false; }

void *TR_ResolvedMethod::resolvedMethodAddress()
{
    TR_UNIMPLEMENTED();
    return 0;
}

void *TR_ResolvedMethod::startAddressForJittedMethod()
{
    TR_UNIMPLEMENTED();
    return 0;
}

void *TR_ResolvedMethod::startAddressForJNIMethod(TR::Compilation *)
{
    TR_UNIMPLEMENTED();
    return 0;
}

void *TR_ResolvedMethod::startAddressForJITInternalNativeMethod()
{
    TR_UNIMPLEMENTED();
    return 0;
}

void *TR_ResolvedMethod::startAddressForInterpreterOfJittedMethod()
{
    TR_UNIMPLEMENTED();
    return 0;
}

bool TR_ResolvedMethod::isWarmCallGraphTooBig(uint32_t bcIndex, TR::Compilation *)
{
    TR_UNIMPLEMENTED();
    return 0;
}

void TR_ResolvedMethod::setWarmCallGraphTooBig(uint32_t bcIndex, TR::Compilation *)
{
    TR_UNIMPLEMENTED();
    return;
}

TR_OpaqueClassBlock *TR_ResolvedMethod::getDeclaringClassFromFieldOrStatic(TR::Compilation *comp, int32_t cpIndex)
{
    TR_UNIMPLEMENTED();
    return 0;
}

int32_t TR_ResolvedMethod::classCPIndexOfFieldOrStatic(int32_t)
{
    TR_UNIMPLEMENTED();
    return 0;
}

const char *TR_ResolvedMethod::signature(TR_Memory *, TR_AllocationKind)
{
    TR_UNIMPLEMENTED();
    return 0;
}

const char *TR_ResolvedMethod::externalName(TR_Memory *, TR_AllocationKind)
{
    TR_UNIMPLEMENTED();
    return 0;
}

char *TR_ResolvedMethod::fieldName(int32_t, TR_Memory *, TR_AllocationKind kind) { return 0; }

char *TR_ResolvedMethod::staticName(int32_t, TR_Memory *, TR_AllocationKind kind) { return 0; }

char *TR_ResolvedMethod::localName(uint32_t, uint32_t, TR_Memory *) { return 0; }

char *TR_ResolvedMethod::fieldName(int32_t, int32_t &, TR_Memory *, TR_AllocationKind kind)
{
    TR_UNIMPLEMENTED();
    return 0;
}

char *TR_ResolvedMethod::staticName(int32_t, int32_t &, TR_Memory *, TR_AllocationKind kind)
{
    TR_UNIMPLEMENTED();
    return 0;
}

char *TR_ResolvedMethod::localName(uint32_t, uint32_t, int32_t &, TR_Memory *) { return 0; }

char *TR_ResolvedMethod::fieldNameChars(int32_t, int32_t &)
{
    TR_UNIMPLEMENTED();
    return 0;
}

char *TR_ResolvedMethod::fieldSignatureChars(int32_t, int32_t &)
{
    TR_UNIMPLEMENTED();
    return 0;
}

char *TR_ResolvedMethod::staticSignatureChars(int32_t, int32_t &)
{
    TR_UNIMPLEMENTED();
    return 0;
}

void *&TR_ResolvedMethod::addressOfClassOfMethod()
{
    TR_UNIMPLEMENTED();
    throw std::exception();
}

uint32_t TR_ResolvedMethod::vTableSlot()
{
    TR_UNIMPLEMENTED();
    return 0;
}

bool TR_ResolvedMethod::virtualMethodIsOverridden()
{
    TR_UNIMPLEMENTED();
    return false;
}

void TR_ResolvedMethod::setVirtualMethodIsOverridden() { TR_UNIMPLEMENTED(); }

void *TR_ResolvedMethod::addressContainingIsOverriddenBit()
{
    TR_UNIMPLEMENTED();
    return 0;
}

int32_t TR_ResolvedMethod::virtualCallSelector()
{
    TR_UNIMPLEMENTED();
    return 0;
}

uint32_t TR_ResolvedMethod::numberOfExceptionHandlers() { return 0; }

uint8_t *TR_ResolvedMethod::allocateException(uint32_t, TR::Compilation *)
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR_FrontEnd *TR_ResolvedMethod::fe()
{
    TR_UNIMPLEMENTED();
    return 0;
}

intptr_t TR_ResolvedMethod::getInvocationCount()
{
    TR_UNIMPLEMENTED();
    return 0;
}

bool TR_ResolvedMethod::setInvocationCount(intptr_t, intptr_t)
{
    TR_UNIMPLEMENTED();
    return false;
}

uint16_t TR_ResolvedMethod::numberOfParameterSlots()
{
    TR_UNIMPLEMENTED();
    return 0;
}

uint16_t TR_ResolvedMethod::archetypeArgPlaceholderSlot(TR_Memory *)
{
    TR_UNIMPLEMENTED();
    return 0;
}

uint16_t TR_ResolvedMethod::numberOfTemps()
{
    TR_UNIMPLEMENTED();
    return 0;
}

uint16_t TR_ResolvedMethod::numberOfPendingPushes()
{
    TR_UNIMPLEMENTED();
    return 0;
}

uint8_t *TR_ResolvedMethod::bytecodeStart()
{
    TR_UNIMPLEMENTED();
    return 0;
}

uint32_t TR_ResolvedMethod::maxBytecodeIndex()
{
    TR_UNIMPLEMENTED();
    return 0;
}

void *TR_ResolvedMethod::ramConstantPool()
{
    TR_UNIMPLEMENTED();
    return 0;
}

void *TR_ResolvedMethod::constantPool()
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR::DataType TR_ResolvedMethod::getLDCType(int32_t)
{
    TR_UNIMPLEMENTED();
    return TR::NoType;
}

bool TR_ResolvedMethod::isClassConstant(int32_t cpIndex)
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::isStringConstant(int32_t cpIndex)
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::isMethodTypeConstant(int32_t cpIndex)
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::isMethodHandleConstant(int32_t cpIndex)
{
    TR_UNIMPLEMENTED();
    return false;
}

uint32_t TR_ResolvedMethod::intConstant(int32_t)
{
    TR_UNIMPLEMENTED();
    return 0;
}

uint64_t TR_ResolvedMethod::longConstant(int32_t)
{
    TR_UNIMPLEMENTED();
    return 0;
}

float *TR_ResolvedMethod::floatConstant(int32_t)
{
    TR_UNIMPLEMENTED();
    return 0;
}

double *TR_ResolvedMethod::doubleConstant(int32_t, TR_Memory *)
{
    TR_UNIMPLEMENTED();
    return 0;
}

void *TR_ResolvedMethod::stringConstant(int32_t)
{
    TR_UNIMPLEMENTED();
    return 0;
}

bool TR_ResolvedMethod::isUnresolvedString(int32_t, bool optimizeForAOT)
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::isUnresolvedString(int32_t cpIndex, void **stringAddress)
{
    *stringAddress = stringConstant(cpIndex);
    return isUnresolvedString(cpIndex);
}

void *TR_ResolvedMethod::getConstantDynamicTypeFromCP(int32_t cpIndex)
{
    TR_UNIMPLEMENTED();
    return 0;
}

bool TR_ResolvedMethod::isConstantDynamic(int32_t cpIndex)
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::isUnresolvedConstantDynamic(int32_t cpIndex)
{
    TR_UNIMPLEMENTED();
    return false;
}

void *TR_ResolvedMethod::dynamicConstant(int32_t cpIndex, uintptr_t *obj)
{
    TR_UNIMPLEMENTED();
    return 0;
}

void *TR_ResolvedMethod::methodTypeConstant(int32_t)
{
    TR_UNIMPLEMENTED();
    return 0;
}

bool TR_ResolvedMethod::isUnresolvedMethodType(int32_t)
{
    TR_UNIMPLEMENTED();
    return false;
}

void *TR_ResolvedMethod::methodHandleConstant(int32_t)
{
    TR_UNIMPLEMENTED();
    return 0;
}

bool TR_ResolvedMethod::isUnresolvedMethodHandle(int32_t)
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::isUnresolvedCallSiteTableEntry(int32_t callSiteIndex)
{
    TR_UNIMPLEMENTED();
    return false;
}

void *TR_ResolvedMethod::callSiteTableEntryAddress(int32_t callSiteIndex)
{
    TR_UNIMPLEMENTED();
    return 0;
}

bool TR_ResolvedMethod::isUnresolvedMethodTypeTableEntry(int32_t cpIndex)
{
    TR_UNIMPLEMENTED();
    return false;
}

void *TR_ResolvedMethod::methodTypeTableEntryAddress(int32_t cpIndex)
{
    TR_UNIMPLEMENTED();
    return 0;
}

bool TR_ResolvedMethod::isStable(int32_t cpIndex, TR::Compilation *comp)
{
    TR_UNIMPLEMENTED();
    return false;
}

uint32_t TR_ResolvedMethod::romMethodArgCountAtCallSiteIndex(int32_t callSiteIndex)
{
    TR_UNIMPLEMENTED();
    return 0;
}

uint32_t TR_ResolvedMethod::romMethodArgCountAtCPIndex(int32_t cpIndex)
{
    TR_UNIMPLEMENTED();
    return 0;
}

int32_t TR_ResolvedMethod::exceptionData(int32_t, int32_t *, int32_t *, int32_t *)
{
    TR_UNIMPLEMENTED();
    return 0;
}

char *TR_ResolvedMethod::getClassNameFromConstantPool(uint32_t, uint32_t &)
{
    TR_UNIMPLEMENTED();
    return 0;
}

bool TR_ResolvedMethod::fieldsAreSame(int32_t, TR_ResolvedMethod *, int32_t, bool &sigSame)
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::staticsAreSame(int32_t, TR_ResolvedMethod *, int32_t, bool &sigSame)
{
    TR_UNIMPLEMENTED();
    return false;
}

char *TR_ResolvedMethod::classNameOfFieldOrStatic(int32_t, int32_t &)
{
    TR_UNIMPLEMENTED();
    return 0;
}

char *TR_ResolvedMethod::classSignatureOfFieldOrStatic(int32_t, int32_t &)
{
    TR_UNIMPLEMENTED();
    return 0;
}

char *TR_ResolvedMethod::staticNameChars(int32_t, int32_t &)
{
    TR_UNIMPLEMENTED();
    return 0;
}

const char *TR_ResolvedMethod::newInstancePrototypeSignature(TR_Memory *, TR_AllocationKind)
{
    TR_UNIMPLEMENTED();
    return 0;
}

bool TR_ResolvedMethod::isJ9() { return convertToMethod()->isJ9(); }

TR::RecognizedMethod TR_ResolvedMethod::getRecognizedMethod() { return convertToMethod()->getRecognizedMethod(); }

TR_MethodParameterIterator *TR_ResolvedMethod::getParameterIterator(TR::Compilation &comp)
{
    return convertToMethod()->getParameterIterator(comp, this);
}

uintptr_t *TR_ResolvedMethod::getMethodHandleLocation()
{
    TR_ASSERT(convertToMethod()->isArchetypeSpecimen(),
        "All methods associated with a MethodHandle must be archetype specimens");
    return NULL;
}

bool TR_ResolvedMethod::getUnresolvedFieldInCP(int32_t)
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::getUnresolvedStaticMethodInCP(int32_t)
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::getUnresolvedSpecialMethodInCP(int32_t)
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::getUnresolvedVirtualMethodInCP(int32_t)
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::fieldAttributes(TR::Compilation *, int32_t, uint32_t *, TR::DataType *, bool *, bool *, bool *,
    bool, bool *, bool)
{
    TR_UNIMPLEMENTED();
    return false;
}

bool TR_ResolvedMethod::staticAttributes(TR::Compilation *, int32_t, void **, TR::DataType *, bool *, bool *, bool *,
    bool, bool *, bool)
{
    TR_UNIMPLEMENTED();
    return false;
}

TR_OpaqueClassBlock *TR_ResolvedMethod::containingClass()
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR_OpaqueClassBlock *TR_ResolvedMethod::getClassFromConstantPool(TR::Compilation *, uint32_t, bool)
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR_OpaqueClassBlock *TR_ResolvedMethod::classOfStatic(int32_t, bool)
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR_OpaqueClassBlock *TR_ResolvedMethod::classOfMethod()
{
    TR_UNIMPLEMENTED();
    return 0;
}

uint32_t TR_ResolvedMethod::classCPIndexOfMethod(uint32_t)
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR_OpaqueMethodBlock *TR_ResolvedMethod::getNonPersistentIdentifier()
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR_OpaqueMethodBlock *TR_ResolvedMethod::getPersistentIdentifier()
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR_OpaqueClassBlock *TR_ResolvedMethod::getResolvedInterfaceMethod(int32_t, uintptr_t *)
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR_ResolvedMethod *TR_ResolvedMethod::owningMethod()
{
    TR_UNIMPLEMENTED();
    return 0;
}

void TR_ResolvedMethod::setOwningMethod(TR_ResolvedMethod *) { TR_UNIMPLEMENTED(); }

TR_ResolvedMethod *TR_ResolvedMethod::getResolvedStaticMethod(TR::Compilation *, int32_t, bool *)
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR_ResolvedMethod *TR_ResolvedMethod::getResolvedSpecialMethod(TR::Compilation *, int32_t, bool *)
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR_ResolvedMethod *TR_ResolvedMethod::getResolvedVirtualMethod(TR::Compilation *, int32_t, bool, bool *)
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR_ResolvedMethod *TR_ResolvedMethod::getResolvedDynamicMethod(TR::Compilation *, int32_t, bool *, bool *)
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR_ResolvedMethod *TR_ResolvedMethod::getResolvedHandleMethod(TR::Compilation *, int32_t, bool *, bool *)
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR_ResolvedMethod *TR_ResolvedMethod::getResolvedHandleMethodWithSignature(TR::Compilation *, int32_t, char *)
{
    TR_UNIMPLEMENTED();
    return 0;
}

uint32_t TR_ResolvedMethod::getResolvedInterfaceMethodOffset(TR_OpaqueClassBlock *, int32_t)
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR_ResolvedMethod *TR_ResolvedMethod::getResolvedInterfaceMethod(TR::Compilation *, TR_OpaqueClassBlock *, int32_t)
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR_ResolvedMethod *TR_ResolvedMethod::getResolvedVirtualMethod(TR::Compilation *, TR_OpaqueClassBlock *, int32_t, bool)
{
    TR_UNIMPLEMENTED();
    return 0;
}

void TR_ResolvedMethod::setMethodHandleLocation(uintptr_t *location) { TR_UNIMPLEMENTED(); }

TR::IlGeneratorMethodDetails *TR_ResolvedMethod::getIlGeneratorMethodDetails()
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR::SymbolReferenceTable *TR_ResolvedMethod::_genMethodILForPeeking(TR::ResolvedMethodSymbol *, TR::Compilation *,
    bool resetVisitCount, TR_PrexArgInfo *argInfo)
{
    TR_UNIMPLEMENTED();
    return 0;
}

TR::ResolvedMethodSymbol *TR_ResolvedMethod::findOrCreateJittedMethodSymbol(TR::Compilation *comp)
{
    return TR::ResolvedMethodSymbol::createJittedMethodSymbol(comp->trHeapMemory(), this, comp);
}

void TR_ResolvedMethod::makeParameterList(TR::ResolvedMethodSymbol *methodSym)
{
    ListAppender<TR::ParameterSymbol> la(&methodSym->getParameterList());
    TR::ParameterSymbol *parmSymbol;
    int32_t slot = 0;
    int32_t ordinal = 0;

    uint32_t parmSlots = numberOfParameterSlots();
    for (auto parmIndex = 0U; parmIndex < parmSlots; ++parmIndex) {
        parmSymbol = methodSym->comp()->getSymRefTab()->createParameterSymbol(methodSym, slot, parmType(parmIndex));
        parmSymbol->setOrdinal(ordinal++);

        char *s = getParameterTypeSignature(static_cast<int32_t>(parmIndex));
        uint32_t len = static_cast<uint32_t>(strlen(s));
        parmSymbol->setTypeSignature(s, len);

        la.add(parmSymbol);

        ++slot;
    }

    int32_t lastInterpreterSlot = slot + numberOfTemps();
    methodSym->setTempIndex(lastInterpreterSlot, methodSym->comp()->fe());
    methodSym->setFirstJitTempIndex(methodSym->getTempIndex());
}

TR::SymbolReferenceTable *TR_ResolvedMethod::genMethodILForPeeking(TR::ResolvedMethodSymbol *methodSymbol,
    TR::Compilation *comp, bool resetVisitCount, TR_PrexArgInfo *argInfo)
{
    if (comp->getOption(TR_EnableHCR))
        return NULL;

    return _genMethodILForPeeking(methodSymbol, comp, resetVisitCount, argInfo);
}
