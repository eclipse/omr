/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef METHOD_INCL
#define METHOD_INCL

#include "compile/InlineBlock.hpp"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include "codegen/RecognizedMethods.hpp"
#include "env/TRMemory.hpp"
#include "il/DataTypes.hpp"
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "infra/Assert.hpp"

class TR_BitVector;
class TR_CompactLocals;
class TR_GlobalRegisterAllocator;
class TR_InlinerBase;
class TR_OpaqueClassBlock;
class TR_ResolvedMethod;
namespace TR { class S390SystemLinkage; }
class TR_Value;
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class Node; }
namespace TR { class Snippet; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
template <class T> class TR_ScratchList;

// Method indexes
//
class mcount_t {

private:

   uint32_t _value;
   mcount_t(uint32_t value) : _value(value) {}

public:

   mcount_t() : _value(0) {}
   mcount_t(const mcount_t &other) : _value(other._value) {}

   static mcount_t valueOf(uint16_t value){ return mcount_t((uint32_t)value); }
   static mcount_t valueOf(uint32_t value){ return mcount_t((uint32_t)value); }
   static mcount_t valueOf(int32_t value) { return mcount_t((uint32_t)value); }

   bool operator >= (mcount_t other) const { return _value >= other._value; }
   bool operator >  (mcount_t other) const { return _value >  other._value; }
   bool operator <= (mcount_t other) const { return _value <= other._value; }
   bool operator == (mcount_t other) const { return _value == other._value; }
   bool operator != (mcount_t other) const { return _value != other._value; }

   mcount_t operator + (int32_t increment) const { return mcount_t(_value + increment); }

   uint32_t value() const { return _value; }
};

#define JITTED_METHOD_INDEX (mcount_t::valueOf((uint32_t)0)) // Index of the top-level method being compiled
#define MAX_CALLER_INDEX (mcount_t::valueOf((uint32_t)INT_MAX)) // Could be UINT_MAX, in theory, but let's avoid corner cases until that day comes when we need 3 billion caller indexes

enum NonUserMethod
   {
   unknownNonUserMethod,
   nonUser_java_util_HashMap_rehash,
   nonUser_java_util_HashMap_analyzeMap,
   nonUser_java_util_HashMap_calculateCapacity,
   nonUser_java_util_HashMap_findNullKeyEntry,

   numNonUserMethods
   };

class TR_MethodParameterIterator
   {
public:
   TR_ALLOC(TR_Memory::Method)
   virtual TR::DataType getDataType() = 0;
   virtual TR_OpaqueClassBlock * getOpaqueClass() = 0; //  if getDataType() == TR::Aggregate
   virtual bool isArray() = 0; // refines getOpaqueClass
   virtual bool isClass() = 0; // refines getOpaqueClass
   virtual char * getUnresolvedJavaClassSignature(uint32_t&)=0;
   virtual bool atEnd() = 0;
   virtual void advanceCursor() = 0;

protected:
   TR_MethodParameterIterator(TR::Compilation& comp) : _comp(comp) { }
   TR::Compilation &                 _comp;
   };

class TR_Method
   {
   public:

   TR_ALLOC(TR_Memory::Method);

   enum Type {J9, Test, JitBuilder};


   virtual TR::DataType parmType(uint32_t parmNumber); // returns the type of the parmNumber'th parameter (0-based)
   virtual bool isConstructor(); // returns true if this method is object constructor.
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

   bool isJ9()     { return _typeOfMethod == J9;     }
   Type methodType() { return _typeOfMethod; }

   TR_Method(Type t = J9) : _typeOfMethod(t) { _recognizedMethod = _mandatoryRecognizedMethod = TR::unknownMethod; }

   // --------------------------------------------------------------------------
   // J9
   virtual uint32_t numberOfExplicitParameters();
   virtual bool isArchetypeSpecimen(){ return false; }
   virtual void setArchetypeSpecimen(bool b = true);
   virtual bool isUnsafeWithObjectArg(TR::Compilation * = NULL);
   virtual bool isUnsafeCAS(TR::Compilation * = NULL);
   virtual bool isBigDecimalMethod (TR::Compilation * = NULL);
   virtual bool isBigDecimalConvertersMethod (TR::Compilation * = NULL);
   virtual bool isFinalInObject();


   virtual TR_MethodParameterIterator *getParameterIterator(TR::Compilation&, TR_ResolvedMethod * = NULL);

   // ---------------------------------------------------------------------------

   // Use this for optional logic that takes advantage of known information about a particular method
   //
   TR::RecognizedMethod getRecognizedMethod() { return _recognizedMethod; }

   // Use this for logic where failing to recognize a method leads to bugs
   //
   TR::RecognizedMethod getMandatoryRecognizedMethod() { return _mandatoryRecognizedMethod; }

   void setRecognizedMethod(TR::RecognizedMethod rm) { _recognizedMethod = rm; }
   void setMandatoryRecognizedMethod(TR::RecognizedMethod rm) { _mandatoryRecognizedMethod = rm; }

   protected:


   private:

   TR::RecognizedMethod _recognizedMethod;
   TR::RecognizedMethod _mandatoryRecognizedMethod;
   Type  _typeOfMethod;

   };


typedef struct TR_AOTMethodInfo
   {
   TR_ResolvedMethod *resolvedMethod;
   int32_t cpIndex;
   } TR_AOTMethodInfo;

#endif
