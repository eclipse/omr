/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef S390CONSTANTDATASNIPPET_INCL
#define S390CONSTANTDATASNIPPET_INCL

#include "codegen/Snippet.hpp"

#include <stddef.h>                 // for NULL
#include <stdint.h>                 // for uint32_t, uint8_t, int32_t, etc
#include "compile/Compilation.hpp"  // for comp, Compilation (ptr only)
#include "env/jittypes.h"           // for uintptrj_t
#include "infra/Assert.hpp"         // for TR_ASSERT
#include "OMRCodeGenerator.hpp"

class TR_Debug;
namespace TR { class CodeGenerator; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }
namespace TR { class Register; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }
namespace TR { class UnresolvedDataSnippet; }

namespace TR {

/**
 * ConstantDataSnippet is used to hold shared data
 */
class S390ConstantDataSnippet : public TR::Snippet
   {
   protected:
   union
      {
      uint8_t                       _value[1<<TR_DEFAULT_DATA_SNIPPET_EXPONENT];
      char *                        _string;
      };

   uint32_t                      _length;
   TR::UnresolvedDataSnippet *_unresolvedDataSnippet;
   TR::SymbolReference*           _symbolReference;
   uint32_t                      _reloType;

   uint32_t                      _extraInfo;

   bool                          _isString;

   public:

   S390ConstantDataSnippet(TR::CodeGenerator *cg, TR::Node *, void *c, uint16_t size);
   S390ConstantDataSnippet(TR::CodeGenerator *cg, TR::Node *, char *c);

   virtual Kind getKind() { return IsConstantData; }

   virtual uint32_t getConstantSize() { return _length; }
   virtual int32_t getDataAs1Byte()  { return *((int8_t *) &_value); }
   virtual int32_t getDataAs2Bytes() { return *((int16_t *) &_value); }
   virtual int32_t getDataAs4Bytes() { return *((int32_t *) &_value); }
   virtual int64_t getDataAs8Bytes() { return *((int64_t *) &_value); }
   virtual uint8_t * setRawData(uint8_t *value)
      {
      TR_ASSERT(false,"setRawData not implemented\n");
      return NULL;
      }

   virtual char * getDataAsString()
      {
      TR_ASSERT(_isString, "Data is not a string\n");
      return _string;
      }

   virtual uint8_t * getRawData()
      {
      return _value;
      }

   void addMetaDataForCodeAddress(uint8_t *cursor);
   virtual uint8_t *emitSnippetBody();
   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   virtual uint32_t setLength(uint32_t length) { return _length=length; }

   TR::UnresolvedDataSnippet* getUnresolvedDataSnippet() { return _unresolvedDataSnippet;}
   TR::UnresolvedDataSnippet* setUnresolvedDataSnippet(TR::UnresolvedDataSnippet* uds)
      {
      return _unresolvedDataSnippet = uds;
      }
   TR::SymbolReference* getSymbolReference() { return _symbolReference;}
   TR::SymbolReference* setSymbolReference(TR::SymbolReference* sr)
      {
      return _symbolReference = sr;
      }

   uint32_t            getReloType() { return _reloType;}
   uint32_t            setReloType(uint32_t rt)
      {
      return _reloType = rt;
      }
   uint32_t            getExtraInfo() {return _extraInfo;}
   void                setExtraInfo(uint32_t ei) {_extraInfo = ei;}
   bool                getValueIsInLitPool() { return false; }
   void                setValueIsInLitPool() { }

   bool                getValueIsString() { return _isString; }
   void                setValueIsString() { _isString = true; }

   TR::Compilation* comp() { return TR::comp(); }

   };

class S390ConstantInstructionSnippet : public TR::S390ConstantDataSnippet
   {
   TR::Instruction * _instruction;
   bool             _isRefed;

   public:

   S390ConstantInstructionSnippet(TR::CodeGenerator *cg, TR::Node *, TR::Instruction *);
   TR::Instruction * getInstruction() { return _instruction; }
   Kind getKind() { return IsConstantInstruction; }
   virtual uint32_t getLength(int32_t estimatedSnippetStart) { return 8; }
   uint32_t getConstantSize() { return 8; }
   virtual int64_t getDataAs8Bytes();
   uint8_t * emitSnippetBody();
   bool      isRefed() { return _isRefed; }
   void      setIsRefed(bool v) { _isRefed = v; }
   };

/**
 * Create constant data snippet with method ep and complete method signature
 */
class S390EyeCatcherDataSnippet : public TR::S390ConstantDataSnippet
   {
   uint8_t *  _value;

   public:

   S390EyeCatcherDataSnippet(TR::CodeGenerator *cg, TR::Node *);

   uint8_t *emitSnippetBody();
   Kind getKind() { return IsEyeCatcherData; }
   };

/**
 * WritableDataSnippet is used to hold patchable data
 */
class S390WritableDataSnippet : public TR::S390ConstantDataSnippet
   {
   public:

   S390WritableDataSnippet(TR::CodeGenerator *cg, TR::Node *, void *c, uint16_t size);

   virtual Kind getKind() { return IsWritableData; }
   };

/**
 * TargetAddress Snippet is used to hold address of snippet which exceed the branch limit
 */
class S390TargetAddressSnippet : public TR::Snippet
   {
   uintptrj_t   _targetaddress;
   TR::Snippet *_targetsnippet;
   TR::Symbol   *_targetsym;
   TR::LabelSymbol   *_targetlabel;

   public:

   S390TargetAddressSnippet(TR::CodeGenerator *cg, TR::Node *, TR::LabelSymbol *targetLabel);
   S390TargetAddressSnippet(TR::CodeGenerator *cg, TR::Node *, TR::Snippet *s);
   S390TargetAddressSnippet(TR::CodeGenerator *cg, TR::Node *, uintptrj_t addr);
   S390TargetAddressSnippet(TR::CodeGenerator *cg, TR::Node *, TR::Symbol *sym);

   TR::Snippet *getTargetSnippet()   { return _targetsnippet; }
   uintptrj_t  getTargetAddress()   { return _targetaddress; }
   TR::Symbol   *getTargetSym()       { return _targetsym; }
   TR::LabelSymbol   *getTargetLabel()   { return _targetlabel; }

   virtual Kind getKind() { return IsTargetAddress; }
   virtual uint8_t *emitSnippetBody();
   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };


class S390LookupSwitchSnippet : public TR::S390TargetAddressSnippet
   {
   public:

   S390LookupSwitchSnippet(TR::CodeGenerator *cg, TR::Node* switchNode);

   virtual Kind getKind() { return IsLookupSwitch; }
   virtual uint8_t *emitSnippetBody();
   virtual uint32_t getLength(int32_t estimatedSnippetStart);
   };


class S390InterfaceCallDataSnippet : public TR::S390ConstantDataSnippet
   {
   TR::Instruction * _firstCLFI;
   uint8_t _numInterfaceCallCacheSlots;
   uint8_t* _codeRA;
   uint8_t *thunkAddress;
   bool _useCLFIandBRCL;

   public:

  S390InterfaceCallDataSnippet(TR::CodeGenerator *,
                               TR::Node *,
                               uint8_t,
                               bool useCLFIandBRCL = false);

  S390InterfaceCallDataSnippet(TR::CodeGenerator *,
                               TR::Node *,
                               uint8_t,
                               uint8_t *,
                               bool useCLFIandBRCL = false);

   virtual Kind getKind() { return IsInterfaceCallData; }
   virtual uint8_t *emitSnippetBody();
   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   int8_t getNumInterfaceCallCacheSlots() {return _numInterfaceCallCacheSlots;}

   void setUseCLFIandBRCL(bool useCLFIandBRCL) {_useCLFIandBRCL = useCLFIandBRCL;}
   bool isUseCLFIandBRCL() {return _useCLFIandBRCL;}

   void setFirstCLFI(TR::Instruction* firstCLFI) { _firstCLFI = firstCLFI; }
   TR::Instruction* getFirstCLFI() { return _firstCLFI;}

   uint8_t* getCodeRA() { return _codeRA;}
   uint8_t* setCodeRA(uint8_t *codeRA)
      {
      return _codeRA = codeRA;
      }

   virtual uint32_t getCallReturnAddressOffset();
   virtual uint32_t getSingleDynamicSlotOffset();
   virtual uint32_t getLastCachedSlotFieldOffset();
   virtual uint32_t getFirstSlotFieldOffset();
   virtual uint32_t getLastSlotFieldOffset();
   virtual uint32_t getFirstSlotOffset();
   virtual uint32_t getLastSlotOffset();
  };


class S390JNICallDataSnippet : public TR::S390ConstantDataSnippet
   {
   /** Base register for this snippet */
   TR::Register *  _baseRegister;

   //for JNI Callout frame
   uintptrj_t _ramMethod;
   uintptrj_t _JNICallOutFrameFlags;
   TR::LabelSymbol * _returnFromJNICallLabel;  //for savedCP slot
   uintptrj_t _savedPC; // This is unused, and hence zero
   uintptrj_t _tagBits;

   // VMThread setup
   uintptrj_t _pc;
   uintptrj_t _literals;
   uintptrj_t _jitStackFrameFlags;

   //for releaseVMaccess
   uintptrj_t _constReleaseVMAccessMask;
   uintptrj_t _constReleaseVMAccessOutOfLineMask;

   /** For CallNativeFunction */
   uintptrj_t _targetAddress;


   public:

  S390JNICallDataSnippet(TR::CodeGenerator *,
                                  TR::Node *);

   virtual Kind getKind() { return IsJNICallData; }
   virtual uint8_t *emitSnippetBody();
   virtual void print(TR::FILE *, TR_Debug*);
   void setBaseRegister(TR::Register * aValue){ _baseRegister = aValue; }
   TR::Register * getBaseRegister() { return _baseRegister; }

   void setRAMMethod(uintptrj_t aValue){ _ramMethod = aValue; }
   void setJNICallOutFrameFlags(uintptrj_t aValue){ _JNICallOutFrameFlags = aValue; }
   void setReturnFromJNICall( TR::LabelSymbol * aValue){ _returnFromJNICallLabel = aValue; }
   void setSavedPC(uintptrj_t aValue){ _savedPC = aValue; }
   void setTagBits(uintptrj_t aValue){ _tagBits = aValue; }

   void setPC(uintptrj_t aValue){ _pc = aValue; }
   void setLiterals(uintptrj_t aValue){ _literals = aValue; }
   void setJitStackFrameFlags(uintptrj_t aValue){ _jitStackFrameFlags = aValue; }

   void setConstReleaseVMAccessMask(uintptrj_t aValue){ _constReleaseVMAccessMask = aValue; }
   void setConstReleaseVMAccessOutOfLineMask(uintptrj_t aValue){ _constReleaseVMAccessOutOfLineMask = aValue; }
   void setTargetAddress(uintptrj_t aValue){ _targetAddress = aValue; }

   uint32_t getJNICallOutFrameDataOffset(){ return 0; }
   uint32_t getRAMMethodOffset(){ return 0; }
   uint32_t getJNICallOutFrameFlagsOffset();
   uint32_t getReturnFromJNICallOffset();
   uint32_t getSavedPCOffset();
   uint32_t getTagBitsOffset();

   uint32_t getPCOffset();
   uint32_t getLiteralsOffset();
   uint32_t getJitStackFrameFlagsOffset();

   uint32_t getConstReleaseVMAccessMaskOffset();
   uint32_t getConstReleaseVMAccessOutOfLineMaskOffset();

   uint32_t getTargetAddressOffset();

   uint32_t getLength(int32_t estimatedSnippetStart);
  };

class S390LabelTableSnippet : public TR::S390ConstantDataSnippet
   {
   public:
   S390LabelTableSnippet(TR::CodeGenerator *cg, TR::Node *node, uint32_t size);
   virtual Kind getKind() { return IsLabelTable; }
   virtual uint8_t *emitSnippetBody();
   virtual uint32_t getLength(int32_t estimatedSnippetStart);

   int32_t getSize() { return _size; }
   TR::LabelSymbol *getLabel(int32_t index) { TR_ASSERT(index < _size, "out of range, index %d, size %d",index,_size); return _labelTable[index]; }
   TR::LabelSymbol *setLabel(int32_t index, TR::LabelSymbol *label) { TR_ASSERT(index < _size, "out of range, index %d, size %d",index,_size); return _labelTable[index] = label; }

   private:
   int32_t _size;
   TR::LabelSymbol **_labelTable;
   };

class S390DeclTrampSnippet : public TR::S390ConstantDataSnippet
   {
   public:
   S390DeclTrampSnippet(TR::CodeGenerator *cg, TR::LabelSymbol *label);
   virtual Kind getKind() { return IsDeclTramp; }
   virtual uint8_t *emitSnippetBody();
   TR::LabelSymbol *getLabel() { return _label; }
   TR::LabelSymbol *setLabel(TR::LabelSymbol *label) { return _label = label; }

   private:
   TR::LabelSymbol *_label;
   };

class S390SortJumpTrampSnippet : public TR::S390ConstantDataSnippet
   {
   public:
   S390SortJumpTrampSnippet(TR::CodeGenerator *cg, TR::LabelSymbol *label);
   virtual Kind getKind() { return IsSortJumpTramp; }
   virtual uint8_t *emitSnippetBody();
   TR::LabelSymbol *getLabel() { return _label; }
   TR::LabelSymbol *setLabel(TR::LabelSymbol *label) { return _label = label; }

   private:
   TR::LabelSymbol *_label;
   };

}

#endif
