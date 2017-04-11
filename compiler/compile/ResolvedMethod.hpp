/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/

#ifndef TR_RESOLVEDMETHODBASE_INCL
#define TR_RESOLVEDMETHODBASE_INCL

#include <stddef.h>                       // for NULL
#include <stdint.h>                       // for uint16_t, uint8_t, etc
#include "codegen/RecognizedMethods.hpp"  // for RecognizedMethod
#include "compile/Method.hpp"             // for TR_Method, etc
#include "env/TRMemory.hpp"               // for TR_Memory (ptr only), etc
#include "env/jittypes.h"                 // for intptrj_t, uintptrj_t
#include "il/DataTypes.hpp"               // for DataTypes, etc
#include "il/ILOpCodes.hpp"               // for ILOpCodes
#include "infra/Assert.hpp"               // for TR_ASSERT
#include "runtime/Runtime.hpp"            // for TR_RuntimeHelper

class TR_FrontEnd;
class TR_OpaqueClassBlock;
class TR_OpaqueMethodBlock;
class TR_PrexArgInfo;
class TR_ResolvedWCodeMethod;
class TR_TraceFragment;
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class IlGeneratorMethodDetails; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class SymbolReferenceTable; }

class TR_ResolvedMethod
   {
public:
   TR::RecognizedMethod getRecognizedMethod() { return convertToMethod()->getRecognizedMethod(); }
   virtual TR_Method *convertToMethod();

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
   virtual bool isDebugable();

   virtual bool isInterpreted();
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
   virtual intptrj_t getInvocationCount();
   virtual bool setInvocationCount(intptrj_t oldCount, intptrj_t newCount);
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
   virtual bool isUnresolvedString(int32_t cpIndex, bool optimizeForAOT = false);
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

   bool isDAAMethod();
   bool isDAAWrapperMethod();
   bool isDAAIntrinsicMethod();

   virtual void setMethodHandleLocation(uintptrj_t *location);
   virtual uintptrj_t *getMethodHandleLocation()
      {
      TR_ASSERT(convertToMethod()->isArchetypeSpecimen(), "All methods associated with a MethodHandle must be archetype specimens");
      return NULL;
      }

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

   virtual TR_OpaqueClassBlock *getResolvedInterfaceMethod(int32_t cpIndex, uintptrj_t * pITableIndex);

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

   TR_MethodParameterIterator* getParameterIterator(TR::Compilation& comp)
         { return convertToMethod()->getParameterIterator(comp, this); }

   bool isJ9() { return convertToMethod()->isJ9(); }
   bool isPython() { return convertToMethod()->isPython(); }
   bool isRuby() { return convertToMethod()->isRuby(); }

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
                                                          TR_PrexArgInfo  *argInfo = NULL)
      {
      return _genMethodILForPeeking (methodSymbol, comp, resetVisitCount, argInfo);
      };

   virtual TR::SymbolReferenceTable *_genMethodILForPeeking(TR::ResolvedMethodSymbol     *methodSymbol,
                                                                      TR::Compilation  *,
                                                                      bool resetVisitCount,
                                                                      TR_PrexArgInfo  *argInfo);

   // Make up a parameter list for the corresponding TR::ResolvedMethodSymbol
   virtual void makeParameterList(TR::ResolvedMethodSymbol *);

   virtual TR::ResolvedMethodSymbol *findOrCreateJittedMethodSymbol(TR::Compilation *comp);

   };

#endif
