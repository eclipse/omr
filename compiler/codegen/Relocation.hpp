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

#ifndef RELOCATION_INCL
#define RELOCATION_INCL

#include <stddef.h>
#include <stdint.h>
#include "env/TRMemory.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"
#include "infra/Link.hpp"
#include "runtime/Runtime.hpp"
#include "ras/DebugCounter.hpp"

namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class Node; }

typedef enum
   {
   TR_UnusedGlobalValue             = 0,
   // start with 1, as there is a check in PPC preventing 0 valued immediates from being relocated.
   TR_CountForRecompile             = 1,
   TR_HeapBase                      = 2,
   TR_HeapTop                       = 3,
   TR_HeapBaseForBarrierRange0      = 4,
   TR_ActiveCardTableBase           = 5,
   TR_HeapSizeForBarrierRange0      = 6,
   TR_MethodEnterHookEnabledAddress = 7,
   TR_MethodExitHookEnabledAddress  = 8,
   TR_NumGlobalValueItems           = 9
   } TR_GlobalValueItem;

namespace TR {

/** encapsulates debug information about a relocation record */
class RelocationDebugInfo
   {
   public:

   /** the file name of the code that generated the associated RR*/
   const char* file;
   /** the line number in the file file that created the associated RR*/
   uintptr_t line;
   /** the node in the IL that triggered the creation of the associated RR */
   TR::Node* node;
   public:
   TR_ALLOC(TR_Memory::RelocationDebugInfo);
   };

class Relocation
   {
   uint8_t *_updateLocation;
   TR::RelocationDebugInfo* _genData;

   public:
   TR_ALLOC(TR_Memory::Relocation)

   Relocation() : _updateLocation(NULL) {}
   Relocation(uint8_t *p) : _updateLocation(p) {}

   virtual uint8_t *getUpdateLocation()           {return _updateLocation;}
   uint8_t *setUpdateLocation(uint8_t *p) {return (_updateLocation = p);}

   virtual bool isExternalRelocation() { return false; }

   TR::RelocationDebugInfo* getDebugInfo();

   void setDebugInfo(TR::RelocationDebugInfo* info);

   /**dumps a trace of the internals - override as required */
   virtual void trace(TR::Compilation* comp);

   virtual void addExternalRelocation(TR::CodeGenerator *cg) {}

   virtual void apply(TR::CodeGenerator *cg);
   };

class LabelRelocation : public TR::Relocation
   {
   TR::LabelSymbol *_label;
   public:
   LabelRelocation() : TR::Relocation(), _label(NULL) {}
   LabelRelocation(uint8_t *p, TR::LabelSymbol *l) : TR::Relocation(p),
                                                     _label(l) {}

   TR::LabelSymbol *getLabel()                  {return _label;}
   TR::LabelSymbol *setLabel(TR::LabelSymbol *l) {return (_label = l);}

   protected:
   void assertLabelDefined();
   };

class LabelRelative8BitRelocation : public TR::LabelRelocation
   {
   public:
   LabelRelative8BitRelocation() : TR::LabelRelocation() {}
   LabelRelative8BitRelocation(uint8_t *p, TR::LabelSymbol *l)
      : TR::LabelRelocation(p, l) {}
   virtual void apply(TR::CodeGenerator *cg);
   };

class LabelRelative12BitRelocation : public TR::LabelRelocation
   {
   bool _isCheckDisp;
   public:
   LabelRelative12BitRelocation() : TR::LabelRelocation() {}
   LabelRelative12BitRelocation(uint8_t *p, TR::LabelSymbol *l, bool isCheckDisp = true)
      : TR::LabelRelocation(p, l), _isCheckDisp(isCheckDisp) {}
   bool isCheckDisp() {return _isCheckDisp;}
   virtual void apply(TR::CodeGenerator *cg);
   };


class LabelRelative16BitRelocation : public TR::LabelRelocation
   {
   // field _instructionOffser is used for 16-bit relocation when we need to specify where relocation starts relative to the first byte of the instruction.
   //Eg.: BranchPreload instruction BPP (48-bit instruction), where 16-bit relocation is at 32-47 bits, TR::LabelRelative16BitRelocation(*p,*l, 4, true)
   //
   // The regular TR::LabelRelative16BitRelocation(uint8_t *p, TR::LabelSymbol *l) only supports instructions with 16-bit relocations,
   // where the relocation is 2 bytes (hardcoded value) form the start of the instruction
   bool _instructionOffset;
   int8_t _addressDifferenceDivisor;
   public:
   LabelRelative16BitRelocation()
      : TR::LabelRelocation(), _addressDifferenceDivisor(1) {}
   LabelRelative16BitRelocation(uint8_t *p, TR::LabelSymbol *l)
      : TR::LabelRelocation(p, l), _addressDifferenceDivisor(1) {}
   LabelRelative16BitRelocation(uint8_t *p, TR::LabelSymbol *l, int8_t divisor, bool isInstOffset = false)
      : TR::LabelRelocation(p, l), _addressDifferenceDivisor(divisor), _instructionOffset(isInstOffset) {}

   bool isInstructionOffset()  {return _instructionOffset;}
   bool setInstructionOffset(bool b) {return (_instructionOffset = b);}
   int8_t getAddressDifferenceDivisor()  {return _addressDifferenceDivisor;}
   int8_t setAddressDifferenceDivisor(int8_t d) {return (_addressDifferenceDivisor = d);}

   virtual void apply(TR::CodeGenerator *cg);
   };

class LabelRelative24BitRelocation : public TR::LabelRelocation
   {
   public:
   LabelRelative24BitRelocation() : TR::LabelRelocation() {}
   LabelRelative24BitRelocation(uint8_t *p, TR::LabelSymbol *l)
      : TR::LabelRelocation(p, l) {}
   virtual void apply(TR::CodeGenerator *cg);
   };

class LabelRelative32BitRelocation : public TR::LabelRelocation
   {
   public:
   LabelRelative32BitRelocation() : TR::LabelRelocation() {}
   LabelRelative32BitRelocation(uint8_t *p, TR::LabelSymbol *l)
      : TR::LabelRelocation(p, l) {}
   virtual void apply(TR::CodeGenerator *cg);
   };

/** \brief
 *
 *  Represents a 16-bit relocation from an offset into the binary encoding location of an instruction to a label by a
 *  specified width (bytes, half-words, words, double-words, etc.).
 */
class InstructionLabelRelative16BitRelocation : public TR::LabelRelocation
   {
   public:

   /** \brief
    *     Initializes the InstructionLabelRelative16BitRelocation relocation using a \c NULL base target pointer.
    *
    *  \param cursor
    *     The instruction whose binary encoding location will be used for the relocation.
    *
    *  \param offset
    *     The offset from the binary encoding of \p cursor where the relocation will be encoded
    *
    *  \param l
    *     The label whose distance from \p cursor will be encoded
    *
    *  \param divisor
    *     The width of each unit of the encoding. For example if \p divisor is 2 then the value which will be encoded
    *     at \p cursor + \p offset will be the number of half-words between \p cursor and \p l
    */
   InstructionLabelRelative16BitRelocation(TR::Instruction* cursor, int32_t offset, TR::LabelSymbol* l, int32_t divisor);

   virtual uint8_t* getUpdateLocation();
   virtual void apply(TR::CodeGenerator* cg);

   private:

   TR::Instruction* _cursor;
   int32_t _offset;
   int32_t _divisor;
   };

/** \brief
 *
 *  Represents a 32-bit relocation from an offset into the binary encoding location of an instruction to a label by a
 *  specified width (bytes, half-words, words, double-words, etc.).
 */
class InstructionLabelRelative32BitRelocation : public TR::LabelRelocation
   {
   public:

   /** \brief
    *     Initializes the InstructionLabelRelative16BitRelocation relocation using a \c NULL base target pointer.
    *
    *  \param cursor
    *     The instruction whose binary encoding location will be used for the relocation.
    *
    *  \param offset
    *     The offset from the binary encoding of \p cursor where the relocation will be encoded
    *
    *  \param l
    *     The label whose distance from \p cursor will be encoded
    *
    *  \param divisor
    *     The width of each unit of the encoding. For example if \p divisor is 2 then the value which will be encoded
    *     at \p cursor + \p offset will be the number of half-words between \p cursor and \p l
    */
   InstructionLabelRelative32BitRelocation(TR::Instruction* cursor, int32_t offset, TR::LabelSymbol* l, int32_t divisor);

   virtual uint8_t* getUpdateLocation();
   virtual void apply(TR::CodeGenerator* cg);

   private:

   TR::Instruction* _cursor;
   int32_t _offset;
   int32_t _divisor;
   };

class LabelAbsoluteRelocation : public TR::LabelRelocation
   {
   public:
   LabelAbsoluteRelocation() : TR::LabelRelocation() {}
   LabelAbsoluteRelocation(uint8_t *p, TR::LabelSymbol *l)
      : TR::LabelRelocation(p, l) {}
   virtual void apply(TR::CodeGenerator *cg);
   };


#define MAX_SIZE_RELOCATION_DATA ((uint16_t)0xffff)
#define MIN_SHORT_OFFSET         -32768
#define MAX_SHORT_OFFSET         32767

class IteratedExternalRelocation : public TR_Link<TR::IteratedExternalRelocation>
   {
   public:
   IteratedExternalRelocation() : TR_Link<TR::IteratedExternalRelocation>(),
                                  _numberOfRelocationSites(0),
                                  _targetAddress(NULL),
                                  _targetAddress2(NULL),
                                  _relocationData(NULL),
                                  _relocationDataCursor(NULL),
                                  _sizeOfRelocationData(0),
                                  _recordModifier(0),
                                  _full(false),
                                  _kind(TR_ConstantPool) {}

   IteratedExternalRelocation(uint8_t *target, TR_ExternalRelocationTargetKind k, flags8_t modifier, TR::CodeGenerator *cg);
   IteratedExternalRelocation(uint8_t *target, uint8_t *target2, TR_ExternalRelocationTargetKind k, flags8_t modifier, TR::CodeGenerator *cg);

   uint32_t getNumberOfRelocationSites() {return _numberOfRelocationSites;}
   uint32_t setNumberOfRelocationSites(uint32_t s)
      {return (_numberOfRelocationSites = s);}

   uint8_t *getTargetAddress()           {return _targetAddress;}
   uint8_t *setTargetAddress(uint8_t *p) {return (_targetAddress = p);}

   uint8_t *getTargetAddress2()          {return _targetAddress2;}

   uint8_t *getRelocationData()           {return _relocationData;}
   uint8_t *setRelocationData(uint8_t *p) {return (_relocationData = p);}

   uint8_t *getRelocationDataCursor()           {return _relocationDataCursor;}
   uint8_t *setRelocationDataCursor(uint8_t *p) {return (_relocationDataCursor = p);}

   void initializeRelocation(TR::CodeGenerator * cg);
   void addRelocationEntry(uint32_t locationOffset);

   uint16_t getSizeOfRelocationData()             {return _sizeOfRelocationData;}
   uint16_t setSizeOfRelocationData(uint16_t s)   {return (_sizeOfRelocationData = s);}
   uint16_t addToSizeOfRelocationData(uint16_t s) {return (_sizeOfRelocationData += s);}

   bool needsWideOffsets()
      {return _recordModifier.testAny(RELOCATION_TYPE_WIDE_OFFSET);}
   void setNeedsWideOffsets() {_recordModifier.set(RELOCATION_TYPE_WIDE_OFFSET);}

   bool isOrderedPair() {return _recordModifier.testAny(ITERATED_RELOCATION_TYPE_ORDERED_PAIR);}
   void setOrderedPair() {_recordModifier.set(ITERATED_RELOCATION_TYPE_ORDERED_PAIR);}

   uint8_t getModifierValue() {return _recordModifier.getValue();}
   void    setModifierValue(uint8_t v) {_recordModifier.setValue(0x00, v);}

   bool full()    {return _full;}
   void setFull() {_full = true;}

   TR_ExternalRelocationTargetKind getTargetKind()                                  {return _kind;}
   TR_ExternalRelocationTargetKind setTargetKind(TR_ExternalRelocationTargetKind k) {return (_kind = k);}

   private:
   uint32_t                        _numberOfRelocationSites;
   uint8_t                        *_targetAddress;
   uint8_t                        *_targetAddress2;
   uint8_t                        *_relocationData;
   uint8_t                        *_relocationDataCursor;
   uint16_t                        _sizeOfRelocationData;
   flags8_t                        _recordModifier;
   bool                            _full;
   TR_ExternalRelocationTargetKind _kind;
   };

class ExternalRelocation : public TR::Relocation
   {
   public:

   /**
    * @brief Factory method for ExternalRelocation
    *
    * @param[in] codeAddress : the code cache address this relocation applies to
    * @param[in] targetAddress : the external address for this relocation
    * @param[in] kind : external relocation kind
    * @param[in] cg : \c TR::CodeGenerator object
    *
    * @return Allocated \c ExternalRelocation object
    */
   static ExternalRelocation *create(
      uint8_t *codeAddress,
      uint8_t *targetAddress,
      TR_ExternalRelocationTargetKind kind,
      TR::CodeGenerator *cg);

   /**
    * @brief Factory method for ExternalRelocation
    *
    * @param[in] codeAddress : the code cache address this relocation applies to
    * @param[in] targetAddress : the external address for this relocation
    * @param[in] targetAddress2 : the second external address for this relocation
    * @param[in] kind : external relocation kind
    * @param[in] cg : \c TR::CodeGenerator object
    *
    * @return Allocated \c ExternalRelocation object
    */
   static ExternalRelocation *create(
      uint8_t *codeAddress,
      uint8_t *targetAddress,
      uint8_t *targetAddress2,
      TR_ExternalRelocationTargetKind kind,
      TR::CodeGenerator *cg);

   ExternalRelocation()
      : TR::Relocation(),
        _targetAddress(NULL),
        _targetAddress2(NULL),
        _kind(TR_ConstantPool),
        _relocationRecord(NULL)
        {}

   ExternalRelocation(uint8_t                        *p,
                      uint8_t                        *target,
                      TR_ExternalRelocationTargetKind kind,
                      TR::CodeGenerator *cg)
      : TR::Relocation(p),
        _targetAddress(target),
        _targetAddress2(NULL),
        _kind(kind),
        _relocationRecord(NULL)
        {}

   ExternalRelocation(uint8_t                        *p,
                      uint8_t                        *target,
                      uint8_t                        *target2,
                      TR_ExternalRelocationTargetKind  kind,
                      TR::CodeGenerator *cg)
      : TR::Relocation(p),
        _targetAddress(target),
        _targetAddress2(target2),
        _kind(kind),
        _relocationRecord(NULL)
        {}

   uint8_t *getTargetAddress()            { return _targetAddress;        }
   uint8_t *setTargetAddress(uint8_t *p)  { return (_targetAddress = p);  }

   uint8_t *getTargetAddress2()           { return _targetAddress2;       }
   uint8_t *setTargetAddress2(uint8_t *p) { return (_targetAddress2 = p); }

   TR_ExternalRelocationTargetKind getTargetKind()                                  { return _kind;       }
   TR_ExternalRelocationTargetKind setTargetKind(TR_ExternalRelocationTargetKind k) { return (_kind = k); }

   TR::IteratedExternalRelocation *getRelocationRecord()
      {return _relocationRecord;}

   void trace(TR::Compilation* comp);

   void addExternalRelocation(TR::CodeGenerator *cg);
   virtual uint8_t collectModifier();
   virtual uint32_t getNarrowSize() {return 2;}
   virtual uint32_t getWideSize() {return 4;}

   virtual void apply(TR::CodeGenerator *cg);

   virtual bool isExternalRelocation() { return true; }

   static const char *getName(TR_ExternalRelocationTargetKind k) {return _externalRelocationTargetKindNames[k];}
   static uintptr_t   getGlobalValue(uint32_t g)
         {
         TR_ASSERT(g >= 0 && g < TR_NumGlobalValueItems, "invalid index for global item");
         return _globalValueList[g];
         }
   static void        setGlobalValue(uint32_t g, uintptr_t v)
         {
         TR_ASSERT(g >= 0 && g < TR_NumGlobalValueItems, "invalid index for global item");
         _globalValueList[g] = v;
         }
   static const char *nameOfGlobal(uint32_t g)
         {
         TR_ASSERT(g >= 0 && g < TR_NumGlobalValueItems, "invalid index for global item");
         return _globalValueNames[g];
         }

   private:
   uint8_t                         *_targetAddress;
   uint8_t                         *_targetAddress2;
   TR::IteratedExternalRelocation  *_relocationRecord;
   TR_ExternalRelocationTargetKind  _kind;
   static const char               *_externalRelocationTargetKindNames[TR_NumExternalRelocationKinds];
   static uintptr_t                 _globalValueList[TR_NumGlobalValueItems];
   static uint8_t                   _globalValueSizeList[TR_NumGlobalValueItems];
   static const char                *_globalValueNames[TR_NumGlobalValueItems];
   };

class ExternalOrderedPair32BitRelocation: public TR::ExternalRelocation
   {
   public:
   ExternalOrderedPair32BitRelocation(uint8_t *location1,
                                      uint8_t *location2,
                                      uint8_t *target,
                                      TR_ExternalRelocationTargetKind  k,
                                      TR::CodeGenerator *cg);

   ExternalOrderedPair32BitRelocation(uint8_t *location1,
                                      uint8_t *location2,
                                      uint8_t *target,
                                      uint8_t *target2,
                                      TR_ExternalRelocationTargetKind  k,
                                      TR::CodeGenerator *cg);

   uint8_t *getLocation2() {return _update2Location;}
   void setLocation2(uint8_t *l) {_update2Location = l;}

   virtual uint8_t collectModifier();
   virtual uint32_t getNarrowSize() {return 4;}
   virtual uint32_t getWideSize() {return 8;}
   virtual void apply(TR::CodeGenerator *cg);

   private:
   uint8_t                         *_update2Location;
   };

class BeforeBinaryEncodingExternalRelocation : public TR::ExternalRelocation
   {
   private:
   TR::Instruction  *_instruction;

   public:
   BeforeBinaryEncodingExternalRelocation()
      : TR::ExternalRelocation()
        {}

   BeforeBinaryEncodingExternalRelocation(TR::Instruction *instr,
                                          uint8_t *target,
                                          TR_ExternalRelocationTargetKind kind,
                                          TR::CodeGenerator *cg)
      : TR::ExternalRelocation(NULL, target, kind, cg),
        _instruction(instr)
         {}

   BeforeBinaryEncodingExternalRelocation(TR::Instruction *instr,
                                          uint8_t *target,
                                          uint8_t *target2,
                                          TR_ExternalRelocationTargetKind kind,
                                          TR::CodeGenerator *cg)
      : TR::ExternalRelocation(NULL, target, target2, kind, cg),
        _instruction(instr)
         {}

   virtual uint8_t *getUpdateLocation();

   TR::Instruction* getUpdateInstruction() { return _instruction; }

   };

}

#endif
