/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at http://eclipse.org/legal/epl-2.0
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
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef TR_RESOLVEDMETHODBASE_INCL
#define TR_RESOLVEDMETHODBASE_INCL

#include <stddef.h>
#include <stdint.h>
#include "codegen/RecognizedMethods.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "infra/Assert.hpp"
#include "runtime/Runtime.hpp"

class TR_FrontEnd;
class TR_MethodParameterIterator;
class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;
class TR_PrexArgInfo;
class TR_ResolvedWCodeMethod;
class TR_TraceFragment;
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class LabelSymbol; }
namespace TR { class Method; }
namespace TR { class Node; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReferenceTable; }

class TR_ResolvedMethod
   {
public:
   TR::RecognizedMethod getRecognizedMethod();
   virtual TR::Method *convertToMethod();

   virtual uint32_t numberOfParameters();
   virtual uint32_t numberOfExplicitParameters(); // excludes receiver if any
   virtual TR::DataType parmType(uint32_t parmNumber); // returns the type of the parmNumber'th parameter (0-based)
   virtual TR::ILOpCodes directCallOpCode();
   virtual TR::ILOpCodes indirectCallOpCode();
   virtual TR::DataType returnType();
   virtual uint32_t returnTypeWidth();
   virtual bool returnTypeIsUnsigned();
   virtual TR::ILOpCodes returnOpCode();
   virtual const char *signature(TR_Memory *, TR_AllocationKind = heapAlloc);
   virtual const char *externalName(TR_Memory *, TR_AllocationKind = heapAlloc);
   virtual uint16_t classNameLength();
   virtual uint16_t nameLength();
   virtual uint16_t signatureLength();
   virtual char *classNameChars(); // returns the utf8 of the class that this method is in.
   virtual char *nameChars(); // returns the utf8 of the method name
   virtual char *signatureChars(); // returns the utf8 of the signature

   virtual bool isConstructor(); // returns true if this method is object constructor.
   virtual bool isStatic();
   virtual bool isAbstract();
   virtual bool isCompilable(TR_Memory *);
   virtual bool isInlineable(TR::Compilation *);
   virtual bool isNative();
   virtual bool isSynchronized();
   virtual bool isPrivate();
   virtual bool isProtected();
   virtual bool isPublic();
   virtual bool isFinal();

   virtual bool isInterpreted();
   virtual bool isInterpretedForHeuristics();
   virtual bool hasBackwardBranches();
   virtual bool isObjectConstructor();
   virtual bool isNonEmptyObjectConstructor();
   virtual bool isCold(TR::Compilation *, bool, TR::ResolvedMethodSymbol * sym = NULL);

   virtual void *resolvedMethodAddress();
   virtual void *startAddressForJittedMethod();
   virtual void *startAddressForInterpreterOfJittedMethod();

   virtual uint16_t numberOfParameterSlots();
   virtual TR_FrontEnd *fe();
   virtual uint32_t maxBytecodeIndex();
   virtual bool isSameMethod(TR_ResolvedMethod *);
   virtual uint16_t numberOfTemps();
   virtual uint16_t numberOfPendingPushes();

   // --------------------------------------------------------------------------
   // J9
   virtual bool isStrictFP();
   virtual void *startAddressForJNIMethod(TR::Compilation *);
   virtual void *startAddressForJITInternalNativeMethod();
   virtual bool isSubjectToPhaseChange(TR::Compilation *);
   virtual uint16_t archetypeArgPlaceholderSlot(TR_Memory *);
   virtual intptr_t getInvocationCount();
   virtual bool setInvocationCount(intptr_t oldCount, intptr_t newCount);
   virtual bool isWarmCallGraphTooBig(uint32_t bcIndex, TR::Compilation *);
   virtual void setWarmCallGraphTooBig(uint32_t bcIndex, TR::Compilation *);
   virtual uint8_t *bytecodeStart();
   virtual TR_OpaqueClassBlock *containingClass();
   virtual TR_ResolvedMethod * owningMethod();
   virtual void setOwningMethod(TR_ResolvedMethod *);
   virtual bool owningMethodDoesntMatter(){ return false; }
   virtual void *ramConstantPool();
   virtual void *constantPool();
   virtual TR_OpaqueClassBlock * getClassFromConstantPool(TR::Compilation *, uint32_t cpIndex, bool returnClassForAot = false);
   virtual bool canAlwaysShareSymbolDespiteOwningMethod(TR_ResolvedMethod *other) { return false; }
   virtual char *getClassNameFromConstantPool(uint32_t cpIndex, uint32_t &length);
   virtual TR::DataType getLDCType(int32_t cpIndex);
   virtual bool isClassConstant(int32_t cpIndex);
   virtual bool isStringConstant(int32_t cpIndex);
   virtual bool isMethodTypeConstant(int32_t cpIndex);
   virtual bool isMethodHandleConstant(int32_t cpIndex);
   virtual uint32_t intConstant(int32_t cpIndex);
   virtual uint64_t longConstant(int32_t cpIndex);
   virtual float *floatConstant(int32_t cpIndex);
   virtual double *doubleConstant(int32_t cpIndex, TR_Memory *);
   virtual void *stringConstant(int32_t cpIndex);
   virtual void *getConstantDynamicTypeFromCP(int32_t cpIndex);
   virtual bool isUnresolvedString(int32_t cpIndex, bool optimizeForAOT = false);
   virtual bool isConstantDynamic(int32_t cpIndex);
   virtual bool isUnresolvedConstantDynamic(int32_t cpIndex);
   virtual void *dynamicConstant(int32_t cpIndex, uintptr_t *obj);
   virtual void *methodTypeConstant(int32_t cpIndex);
   virtual bool isUnresolvedMethodType(int32_t cpIndex);
   virtual void *methodHandleConstant(int32_t cpIndex);
   virtual bool isUnresolvedMethodHandle(int32_t cpIndex);

   virtual bool isNewInstanceImplThunk();
   virtual bool isJNINative();
   virtual bool isUnresolvedCallSiteTableEntry(int32_t callSiteIndex);
   virtual void *callSiteTableEntryAddress(int32_t callSiteIndex);
   virtual bool isUnresolvedMethodTypeTableEntry(int32_t cpIndex);
   virtual void *methodTypeTableEntryAddress(int32_t cpIndex);

   virtual bool fieldsAreSame(int32_t, TR_ResolvedMethod *, int32_t, bool &sigSame);
   virtual bool staticsAreSame(int32_t, TR_ResolvedMethod *, int32_t, bool &sigSame);
   virtual bool getUnresolvedFieldInCP(int32_t);
   virtual bool getUnresolvedStaticMethodInCP(int32_t);
   virtual bool getUnresolvedSpecialMethodInCP(int32_t);
   virtual bool getUnresolvedVirtualMethodInCP(int32_t);

   bool isDAAWrapperMethod();
   bool isDAAMarshallingWrapperMethod();
   bool isDAAPackedDecimalWrapperMethod();

   bool isDAAIntrinsicMethod();
   bool isDAAMarshallingIntrinsicMethod();
   bool isDAAPackedDecimalIntrinsicMethod();

   virtual void setMethodHandleLocation(uintptr_t *location);
   virtual uintptr_t *getMethodHandleLocation();

   virtual const char *newInstancePrototypeSignature(TR_Memory *, TR_AllocationKind = heapAlloc);

   // --------------------------------------------------------------------------

   virtual bool fieldAttributes (TR::Compilation *, int32_t cpIndex, uint32_t * fieldOffset, TR::DataType * type, bool * volatileP, bool * isFinal, bool *isPrivate, bool isStore, bool * unresolvedInCP = 0, bool needAOTValidation=true);
   virtual bool staticAttributes(TR::Compilation *, int32_t cpIndex, void * *, TR::DataType * type, bool * volatileP, bool * isFinal, bool *isPrivate, bool isStore, bool * unresolvedInCP = 0, bool needAOTValidation=true);

   virtual char *classNameOfFieldOrStatic(int32_t cpIndex, int32_t & len);
   virtual char *classSignatureOfFieldOrStatic(int32_t cpIndex, int32_t & len);
   virtual int32_t classCPIndexOfFieldOrStatic(int32_t cpIndex);
   TR_OpaqueClassBlock *getClassFromFieldOrStatic(TR::Compilation *comp, int32_t cpIndex, bool returnClassForAOT = false) { return getClassFromConstantPool(comp, classCPIndexOfFieldOrStatic(cpIndex), returnClassForAOT); }
   virtual TR_OpaqueClassBlock *getDeclaringClassFromFieldOrStatic(TR::Compilation *comp, int32_t cpIndex);

   // These functions actually return a complete description of the field/static
   virtual char *fieldName (int32_t cpIndex, TR_Memory *, TR_AllocationKind kind=heapAlloc);
   virtual char *staticName(int32_t cpIndex, TR_Memory *, TR_AllocationKind kind=heapAlloc);
   virtual char *localName (uint32_t slotNumber, uint32_t bcIndex, TR_Memory *);
   virtual char *fieldName (int32_t cpIndex, int32_t & len, TR_Memory *, TR_AllocationKind kind=heapAlloc);
   virtual char *staticName(int32_t cpIndex, int32_t & len, TR_Memory *, TR_AllocationKind kind=heapAlloc);
   virtual char *localName (uint32_t slotNumber, uint32_t bcIndex, int32_t &len, TR_Memory *);

   virtual char *fieldNameChars(int32_t cpIndex, int32_t & len);
   virtual char *staticNameChars(int32_t cpIndex, int32_t & len);
   virtual char *fieldSignatureChars(int32_t cpIndex, int32_t & len);
   virtual char *staticSignatureChars(int32_t cpIndex, int32_t & len);

   virtual TR_OpaqueClassBlock *classOfStatic(int32_t cpIndex, bool returnClassForAOT = false);
   virtual TR_OpaqueClassBlock *classOfMethod();
   virtual uint32_t classCPIndexOfMethod(uint32_t);
   virtual void * & addressOfClassOfMethod();

   virtual uint32_t vTableSlot(uint32_t);

   virtual TR_OpaqueClassBlock *getResolvedInterfaceMethod(int32_t cpIndex, uintptr_t * pITableIndex);

   virtual TR_ResolvedMethod *getResolvedStaticMethod (TR::Compilation *, int32_t cpIndex, bool * unresolvedInCP = 0);
   virtual TR_ResolvedMethod *getResolvedSpecialMethod(TR::Compilation *, int32_t cpIndex, bool * unresolvedInCP = 0);
   virtual TR_ResolvedMethod *getResolvedDynamicMethod(TR::Compilation *, int32_t cpIndex, bool * unresolvedInCP = 0);
   virtual TR_ResolvedMethod *getResolvedHandleMethod(TR::Compilation *, int32_t cpIndex, bool * unresolvedInCP = 0);
   virtual TR_ResolvedMethod *getResolvedHandleMethodWithSignature(TR::Compilation *, int32_t cpIndex, char *signature);
   virtual TR_ResolvedMethod *getResolvedVirtualMethod(TR::Compilation *, int32_t cpIndex, bool ignoreReResolve = true, bool * unresolvedInCP = 0);

   virtual uint32_t getResolvedInterfaceMethodOffset(TR_OpaqueClassBlock * classObject, int32_t cpIndex);
   virtual TR_ResolvedMethod *getResolvedInterfaceMethod(TR::Compilation *, TR_OpaqueClassBlock * classObject, int32_t cpIndex);
   virtual TR_ResolvedMethod *getResolvedVirtualMethod(TR::Compilation *, TR_OpaqueClassBlock * classObject, int32_t cpIndex,bool ignoreReResolve = true);

   virtual bool virtualMethodIsOverridden();
   virtual void setVirtualMethodIsOverridden();
   virtual void *addressContainingIsOverriddenBit();
   virtual int32_t virtualCallSelector(uint32_t cpIndex);

   virtual int32_t exceptionData(int32_t exceptionNumber, int32_t * startIndex, int32_t * endIndex, int32_t * catchType);
   virtual uint32_t numberOfExceptionHandlers();

   virtual bool isJITInternalNative();

   virtual TR_OpaqueMethodBlock *getNonPersistentIdentifier();
   virtual TR_OpaqueMethodBlock *getPersistentIdentifier();
   virtual uint8_t *allocateException(uint32_t, TR::Compilation*);

   TR_MethodParameterIterator* getParameterIterator(TR::Compilation& comp);

   bool isJ9();

   virtual TR::IlGeneratorMethodDetails *getIlGeneratorMethodDetails();

   //genMethodILForPeeking and _genMethodILForPeeking is an implementation of a C++ pattern.
   //Namely, we would like to have the same default arg for every override of genMethodILForPeeking.
   //Unfortunately, Consistently setting the default arg for every override is error-prone,
   //so what we do instead is to create a non-virtual genMethodILForPeeking with a default arg
   //that calls a virtual counterpart.
   //This approach allows us to mantain the def arg invariant across all the overrides.
   TR::SymbolReferenceTable *genMethodILForPeeking(TR::ResolvedMethodSymbol *methodSymbol,
                                                          TR::Compilation  *comp,
                                                          bool resetVisitCount = false,
                                                          TR_PrexArgInfo  *argInfo = NULL);

   /** \brief
    *     Generate IL for a method for peeking even under method redefinition mode. To be called by optimizations that have protections
    *     against method redefinition, e.g. inlining with TR_HCRGuard.
    *
    *  \param methodSymbol
    *     The symbol for the method whose IL is generated.
    *
    *  \param comp
    *     The compilation object.
    *
    *  \param resetVisitCount
    *     Boolean to indicate whether to reset the visit count.
    *
    *  \param argInfo
    *     The TR_PrexArgInfo for the method to peek.
    *
    *  \return
    *     The SymbolReferenceTable for the method to peek if peeking succeeds; 0 otherwise.
    */
   TR::SymbolReferenceTable *genMethodILForPeekingEvenUnderMethodRedefinition(TR::ResolvedMethodSymbol *methodSymbol,
                                                          TR::Compilation  *comp,
                                                          bool resetVisitCount = false,
                                                          TR_PrexArgInfo  *argInfo = NULL)
      {
      return _genMethodILForPeeking(methodSymbol, comp, resetVisitCount, argInfo);
      }

   virtual TR::SymbolReferenceTable *_genMethodILForPeeking(TR::ResolvedMethodSymbol     *methodSymbol,
                                                                      TR::Compilation  *,
                                                                      bool resetVisitCount,
                                                                      TR_PrexArgInfo  *argInfo);

   /**
    * @brief Retrieve the type signature name for the given parameter index.
    *
    * @param[in] parmIndex : The parameter index
    *
    * @return The char * type signature name.
    */
   virtual char *getParameterTypeSignature(int32_t parmIndex);

   /**
    * @brief Create TR::ParameterSymbols from the signature of a method, and add them
    *        to the ParameterList on the ResolvedMethodSymbol.
    *
    * @param[in] methodSym : the ResolvedMethodSymbol to create the parameter list for
    */
   virtual void makeParameterList(TR::ResolvedMethodSymbol *);

   virtual TR::ResolvedMethodSymbol *findOrCreateJittedMethodSymbol(TR::Compilation *comp);

   };

#endif
