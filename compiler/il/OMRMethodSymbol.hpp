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

#ifndef OMR_METHODSYMBOL_INCL
#define OMR_METHODSYMBOL_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_METHODSYMBOL_CONNECTOR
#define OMR_METHODSYMBOL_CONNECTOR
namespace OMR { class MethodSymbol; }
namespace OMR { typedef OMR::MethodSymbol MethodSymbolConnector; }
#endif

#include "il/Symbol.hpp"

#include "codegen/LinkageConventionsEnum.hpp"
#include "codegen/RecognizedMethods.hpp"
#include "compile/Method.hpp"
#include "il/DataTypes.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"

namespace TR { class MethodSymbol; }

namespace OMR
{

/**
 * Symbol for methods, along with information about the method
 */
class OMR_EXTENSIBLE MethodSymbol : public TR::Symbol
   {

protected:

   MethodSymbol(TR_LinkageConventions lc = TR_Private, TR::Method *m = 0);

public:

   template <typename AllocatorType>
   static TR::MethodSymbol * create(AllocatorType t, TR_LinkageConventions lc = TR_Private, TR::Method *m = 0);

   TR::MethodSymbol *self();

   bool firstArgumentIsReceiver();

   void * getMethodAddress()         { return _methodAddress; }
   void   setMethodAddress(void * a) { _methodAddress = a; }

   TR::RecognizedMethod getRecognizedMethod()          { return _method ? _method->getRecognizedMethod()          : TR::unknownMethod; }
   TR::RecognizedMethod getMandatoryRecognizedMethod() { return _method ? _method->getMandatoryRecognizedMethod() : TR::unknownMethod; }

   TR_LinkageConventions getLinkageConvention()               { return _linkageConvention; }
   void                  setLinkage(TR_LinkageConventions lc) { _linkageConvention = lc; }

   TR::Method *getMethod()              { return _method; }
   void        setMethod(TR::Method *m) { _method = m; }

   enum Kinds
      {
      Virtual                      = 0x00000001,
      Interface                    = 0x00000002,
      Static                       = 0x00000003,
      Special                      = 0x00000004,
      Helper                       = 0x00000005,
      ComputedStatic               = 0x00000006, // First child is the address of the method to call, no receiver
      ComputedVirtual              = 0x00000007, // Like ComputedStatic, plus first argument (second child) is receiver
      };

   enum Flags
      {
      MethodKindMask               = 0x00000007, // see enum Kinds

      Interpreted                  = 0x00000080,
      Synchronised                 = 0x00000100,
      EHAware                      = 0x00000200,
      NonReturning                 = 0x00000400,
      VMInternalNative             = 0x00000800, ///< Inl native, interpreter linkage
      JNI                          = 0x00001000,
      TreatAsAlwaysExpandBIF       = 0x00002000,
      PreservesAllRegisters        = 0x00004000,
      JITInternalNative            = 0x00008000, ///< Inl native, JIT linkage
      SystemLinkageDispatch        = 0x00010000,
      InlinedByCG                  = 0x00020000,
      MayHaveLongOps               = 0x00040000, ///< this group is only used by resolved method symbols
      MayHaveLoops                 = 0x00080000,
      MayHaveNestedLoops           = 0x00100000,
      // AVAILABLE                 = 0x00200000,
      MayHaveInlineableCall        = 0x00400000,
      IlGenSuccess                 = 0x00800000,
      MayContainMonitors           = 0x01000000,
      OnlySinglePrecision          = 0x02000000,
      SinglePrecisionMode          = 0x04000000,
      HasNews                      = 0x08000000,
      HasVeryRefinedAliasSets      = 0x10000000,
      MayHaveIndirectCalls         = 0x20000000,
      HasThisCalls                 = 0x40000000,
      dummyLastFlag
      };

   enum Flags2
      {
      HasMethodHandleInvokes        = 0x00000001,
      HasDememoizationOpportunities = 0x00000002,
      HasCheckCasts                 = 0x00000004,
      HasInstanceOfs                = 0x00000008,
      HasBranches                   = 0x00000010,
      dummyLastFlag2
      };

   bool isVirtual()                            { return _methodFlags.testValue(MethodKindMask, Virtual);}
   void setVirtual()                           { _methodFlags.setValue(MethodKindMask, Virtual);}

   void setInterface()                         { _methodFlags.setValue(MethodKindMask, Interface);}
   bool isInterface()                          { return _methodFlags.testValue(MethodKindMask, Interface);}

   bool isComputedStatic()                     { return _methodFlags.testValue(MethodKindMask, ComputedStatic);}
   bool isComputedVirtual()                    { return _methodFlags.testValue(MethodKindMask, ComputedVirtual);}
   bool isComputed();

   void setStatic()                            { _methodFlags.setValue(MethodKindMask, Static);}
   bool isStatic()                             { return _methodFlags.testValue(MethodKindMask, Static);}

   void setSpecial()                           { _methodFlags.setValue(MethodKindMask, Special);}
   bool isSpecial()                            { return _methodFlags.testValue(MethodKindMask, Special);}

   void setHelper()                            { _methodFlags.setValue(MethodKindMask, Helper);}
   bool isHelper()                             { return _methodFlags.testValue(MethodKindMask, Helper);}

   void setMethodKind(int32_t k)               { TR_ASSERT(!(k & ~MethodKindMask), "invalid Symbol Kind"); _methodFlags.setValue(MethodKindMask, k); }
   Kinds getMethodKind()                       { return (Kinds)_methodFlags.getValue(MethodKindMask); }

   bool preservesAllRegisters()                { return _methodFlags.testAny(PreservesAllRegisters);}
   void setPreservesAllRegisters()             { _methodFlags.set(PreservesAllRegisters);}

   bool isSystemLinkageDispatch()              { return _methodFlags.testAny(SystemLinkageDispatch);}
   void setSystemLinkageDispatch()             { _methodFlags.set(SystemLinkageDispatch);}

   bool isInterpreted()                        { return _methodFlags.testAny(Interpreted);}
   void setInterpreted(bool b=true)            { _methodFlags.set(Interpreted, b);}

   void setSynchronised()                      { _methodFlags.set(Synchronised);}
   void setUnsynchronised()                    { _methodFlags.reset(Synchronised);}
   bool isSynchronised()                       { return _methodFlags.testAny(Synchronised);}

   void setEHAware()                           { _methodFlags.set(EHAware);}
   bool isEHAware()                            { return _methodFlags.testAny(EHAware);}

   void setNonReturning()                      { _methodFlags.set(NonReturning);}
   bool isNonReturning()                       { return _methodFlags.testAny(NonReturning);}

   void setJNI()                               { _methodFlags.set(JNI);}
   void setVMInternalNative(bool f=true)       { _methodFlags.set(VMInternalNative, f);}
   void setJITInternalNative(bool f=true)      { _methodFlags.set(JITInternalNative, f);}
   bool isJNI()                                { return _methodFlags.testAny(JNI);}
   bool isVMInternalNative()                   { return _methodFlags.testAny(VMInternalNative);}
   bool isJITInternalNative()                  { return _methodFlags.testAny(JITInternalNative);}
   bool isNative()                             { return _methodFlags.testAny(JNI | VMInternalNative | JITInternalNative);}

   void setIsInlinedByCG()                     { _methodFlags.set(InlinedByCG); }
   bool isInlinedByCG()                        { return _methodFlags.testAny(InlinedByCG); }

   void setHasVeryRefinedAliasSets(bool b)     { _methodFlags.set(HasVeryRefinedAliasSets, b);}
   bool hasVeryRefinedAliasSets()              { return _methodFlags.testAny(HasVeryRefinedAliasSets);}

   void setTreatAsAlwaysExpandBIF(bool b=true) { _methodFlags.set(TreatAsAlwaysExpandBIF, b);}
   bool treatAsAlwaysExpandBIF()               { return _methodFlags.testAny(TreatAsAlwaysExpandBIF);}

   bool safeToSkipNullChecks() { return false; }
   bool safeToSkipBoundChecks() { return false; }
   bool safeToSkipDivChecks() { return false; }
   bool safeToSkipCheckCasts() { return false; }
   bool safeToSkipArrayStoreChecks() { return false; }
   bool safeToSkipZeroInitializationOnNewarrays() { return false; }
   bool safeToSkipChecksOnArrayCopies() { return false; }

   bool functionCallDoesNotYieldOSR() { return false; }
   bool isPureFunction() { return false; }

protected:

   void *                _methodAddress;

   TR::Method *          _method;

   flags32_t             _methodFlags;

   flags32_t             _methodFlags2;

   TR_LinkageConventions _linkageConvention;

   };

}

#endif
