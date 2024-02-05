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

/**
 * \page symbolcreation Symbol Creation
 *
 * For historical reasons, there are a number of relatively special case
 * properties on some symbols.
 *
 * In order to create symbols with those properties, without creating
 * dozens of subclasses, we have chosen to create [special-case symbol factories.][design]
 *
 * For consistency's sake, we therefore create all symbols through
 * factory methods.
 *
 */

#ifndef OMR_SYMBOL_INCL
#define OMR_SYMBOL_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_SYMBOL_CONNECTOR
#define OMR_SYMBOL_CONNECTOR
namespace OMR { class Symbol; }
namespace OMR { typedef OMR::Symbol SymbolConnector; }
#endif

#include <stddef.h>
#include <stdint.h>
#include "infra/Annotations.hpp"
#include "env/TRMemory.hpp"
#include "env/jittypes.h"
#include "il/DataTypes.hpp"
#include "infra/Assert.hpp"
#include "infra/Flags.hpp"

class TR_FrontEnd;
class TR_ResolvedMethod;
namespace TR { class AutomaticSymbol; }
namespace TR { class Compilation; }
namespace TR { class LabelSymbol; }
namespace TR { class MethodSymbol; }
namespace TR { class ParameterSymbol; }
namespace TR { class RegisterMappedSymbol; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class StaticSymbol; }
namespace TR { class Symbol; }

namespace OMR {

/**
 * Symbol
 *
 * A Symbol object contains data type, size and attribute
 * information for a symbol.
 */
class OMR_EXTENSIBLE Symbol
   {

public:
   TR_ALLOC(TR_Memory::Symbol)

   /**
    * \brief Downcast to concrete instance
    */
   inline TR::Symbol * self();

   template <typename AllocatorType>
   static TR::Symbol * create(AllocatorType);

   template <typename AllocatorType>
   static TR::Symbol * create(AllocatorType, TR::DataType);

   template <typename AllocatorType>
   static TR::Symbol * create(AllocatorType, TR::DataType, uint32_t);

protected:

   /**
    * @brief Generic constructor for the creation of a new symbol.  The
    *        size is set to 0.
    */
   Symbol() :
      _size(0),
      _name(0),
      _declaredClass(0),
      _flags(0),
      _flags2(0),
      _localIndex(0)
   { }

   /**
    * @brief Create symbol of specified data type, inferring the size
    *        from the type.
    * @param[in] d : TR::DataType of the new symbol
    */
   Symbol(TR::DataType d);

   /**
    * @brief Create symbol of specified data type and specified size.
    *
    * @param[in] d : TR::DataType of the new symbol
    * @param[in] size : size of new symbol
    */
   Symbol(TR::DataType d, uint32_t size);

public:
   /**
    * This destructor must be virtual since we rarely use pointers directly to
    * derived classes and often free through a pointer to this class.
    */
   virtual ~Symbol() {}

   /**
    * If the symbol is of the correct type the get method downcasts the
    * symbol to the correct type otherwise it returns 0.
    */
   inline TR::RegisterMappedSymbol                   *getRegisterMappedSymbol();
   inline TR::AutomaticSymbol                        *getAutoSymbol();
   inline TR::ParameterSymbol                        *getParmSymbol();
   inline TR::AutomaticSymbol                        *getInternalPointerAutoSymbol();
   inline TR::AutomaticSymbol                        *getLocalObjectSymbol();
   inline TR::StaticSymbol                           *getStaticSymbol();
   inline TR::ResolvedMethodSymbol                   *getResolvedMethodSymbol();
   inline TR::MethodSymbol                           *getMethodSymbol();
   inline TR::Symbol                                 *getShadowSymbol();
   inline TR::Symbol                                 *getNamedShadowSymbol();
   inline TR::RegisterMappedSymbol                   *getMethodMetaDataSymbol();
   inline TR::LabelSymbol                            *getLabelSymbol();
   inline TR::ResolvedMethodSymbol                   *getJittedMethodSymbol();
   inline TR::StaticSymbol                           *getRecognizedStaticSymbol();
   inline TR::AutomaticSymbol                        *getVariableSizeSymbol();
   inline TR::StaticSymbol                           *getCallSiteTableEntrySymbol();
   inline TR::StaticSymbol                           *getMethodTypeTableEntrySymbol();

   // These methods perform an explicit (debug) assume that the symbol is a correct type
   // and then return the symbol explicitly cast to the type.
   //
   inline TR::RegisterMappedSymbol            *castToRegisterMappedSymbol();
   inline TR::AutomaticSymbol                 *castToAutoSymbol();
   inline TR::AutomaticSymbol                 *castToVariableSizeSymbol();
   inline TR::ParameterSymbol                 *castToParmSymbol();
   inline TR::AutomaticSymbol                 *castToInternalPointerAutoSymbol();
   inline TR::AutomaticSymbol                 *castToLocalObjectSymbol();
   inline TR::ResolvedMethodSymbol            *castToResolvedMethodSymbol();
   inline TR::MethodSymbol                    *castToMethodSymbol();
   inline TR::Symbol                          *castToShadowSymbol();
   inline TR::RegisterMappedSymbol            *castToMethodMetaDataSymbol();
   inline TR::LabelSymbol                     *castToLabelSymbol();
   inline TR::ResolvedMethodSymbol            *castToJittedMethodSymbol();

   inline TR::StaticSymbol                    *castToStaticSymbol();
   inline TR::StaticSymbol                    *castToNamedStaticSymbol();
   inline TR::StaticSymbol                    *castToCallSiteTableEntrySymbol();
   inline TR::StaticSymbol                    *castToMethodTypeTableEntrySymbol();

   int32_t getOffset();

   uint32_t getNumberOfSlots();

   bool dontEliminateStores(TR::Compilation *comp, bool isForLocalDeadStore = false);

   bool isReferenced();

   static uint32_t convertTypeToSize(TR::DataType dt) { return TR::DataType::getSize(dt); }

   static uint32_t convertTypeToNumberOfSlots(TR::DataType dt) { return (dt == TR::Int64 || dt == TR::Double)? 2 : 1; }

   static TR::DataType convertSigCharToType(char sigChar);

   /**
    * Field functions
    */

   size_t   getSize()                       { return _size; }
   void     setSize(size_t s)               { _size = s; }
   uint32_t getRoundedSize();

   const char * getName()                   { return _name; }
   void         setName(const char * name)  { _name = name; }

   // When DataType is TR::Address, the declared class of the field, so that it
   // can be found even without a signature. When present, this type info is
   // exactly as reliable as the type signature.
   TR_OpaqueClassBlock *getDeclaredClass() { return _declaredClass; }
   void setDeclaredClass(TR_OpaqueClassBlock* klass) { _declaredClass = klass; }

   uint32_t getFlags()                      { return _flags.getValue(); }
   uint32_t getFlags2()                     { return _flags2.getValue(); }
   void setFlagValue(uint32_t v, bool b)    { _flags.setValue(v, b); }

   uint16_t getLocalIndex()                 { return _localIndex; }
   uint16_t setLocalIndex(uint16_t li)      { return (_localIndex = li); }

   /**
    * Flag functions
    */

   void          setDataType(TR::DataType dt);
   TR::DataType  getDataType() { return (TR::DataTypes)_flags.getValue(DataTypeMask);}
   inline TR::DataType  getType();

   int32_t getKind()             { return _flags.getValue(KindMask);}

   bool isAuto()                 { return _flags.testValue(KindMask, IsAutomatic); }
   bool isParm()                 { return _flags.testValue(KindMask, IsParameter); }
   bool isMethodMetaData()       { return _flags.testValue(KindMask, IsMethodMetaData); }
   bool isResolvedMethod()       { return _flags.testValue(KindMask, IsResolvedMethod); }
   inline bool isMethod();
   bool isStatic()               { return _flags.testValue(KindMask, IsStatic); }
   bool isShadow()               { return _flags.testValue(KindMask, IsShadow); }
   bool isLabel()                { return _flags.testValue(KindMask, IsLabel); }
   void setIsLabel()             { _flags.setValue(KindMask, IsLabel);}

   inline bool isRegisterMappedSymbol();

   inline bool isAutoOrParm();
   bool isAutoField()            { return false; }
   bool isParmField()            { return false; }
   bool isWeakSymbol()           { return false; }
   inline bool isRegularShadow();

   void setIsInGlobalRegister(bool b)       { _flags.set(IsInGlobalRegister, b); }
   bool isInGlobalRegister()                { return _flags.testAny(IsInGlobalRegister); }

   void setConst()                          { _flags.set(Const); }
   bool isConst()                           { return _flags.testAny(Const); }

   void setVolatile()                       { _flags.set(Volatile); }
   void resetVolatile()                     { _flags.reset(Volatile); }
   bool isVolatile()                        { return _flags.testAny(Volatile); }
   inline bool isSyncVolatile();

   void setInitializedReference()           { _flags.set(InitializedReference); }
   void setUninitializedReference()         { _flags.reset(InitializedReference); }
   bool isInitializedReference()            { return _flags.testAny(InitializedReference); }

   void setClassObject()                    { _flags.set(ClassObject); }
   bool isClassObject()                     { return _flags.testAny(ClassObject); }

   void setNotCollected()                   { _flags.set(NotCollected); }
   bool isNotCollected()                    { return _flags.testAny(NotCollected); }
   inline bool isCollectedReference();

   void setFinal()                          { _flags.set(Final); }
   bool isFinal()                           { return _flags.testAny(Final); }

   void setInternalPointer()                { _flags.set(InternalPointer); }
   bool isInternalPointer()                 { return _flags.testAny(InternalPointer); }
   inline bool isInternalPointerAuto();

   void setPrivate()                        { _flags.set(Private); }
   bool isPrivate()                         { return _flags.testAny(Private); }

   void setAddressOfClassObject()           { _flags.set(AddressOfClassObject); }
   bool isAddressOfClassObject()            { return _flags.testAny(AddressOfClassObject); }

   void setSlotSharedByRefAndNonRef(bool b) { _flags.set(SlotSharedByRefAndNonRef, b); }
   bool isSlotSharedByRefAndNonRef()        { return _flags.testAny(SlotSharedByRefAndNonRef); }

   void setHoldsMonitoredObject()           { _flags.set(HoldsMonitoredObject); }
   bool holdsMonitoredObject()              { return _flags.testAny(HoldsMonitoredObject); }

   void setIsDebugCounter()                 { _flags2.set(DebugCounter); }
   bool isDebugCounter()                    { return _flags2.testAny(DebugCounter); }

   void setIsBlockFrequency()               { _flags2.set(BlockFrequency); }
   bool isBlockFrequency()                  { return _flags2.testAny(BlockFrequency); }

   void setIsRecompQueuedFlag()             { _flags2.set(RecompQueuedFlag); }
   bool isRecompQueuedFlag()                { return _flags2.testAny(RecompQueuedFlag); }

   void setIsCatchBlockCounter()            { _flags2.set(CatchBlockCounter); }
   bool isCatchBlockCounter()               { return _flags2.testAny(CatchBlockCounter); }

   void setIsEnterEventHookAddress()        { _flags2.set(EnterEventHookAddress); }
   bool isEnterEventHookAddress()           { return _flags2.testAny(EnterEventHookAddress); }

   void setIsExitEventHookAddress()         { _flags2.set(ExitEventHookAddress); }
   bool isExitEventHookAddress()            { return _flags2.testAny(ExitEventHookAddress); }

   inline bool isNamed();

   // flag methods specific to Autos
   //
   inline void setSpillTempAuto();
   inline bool isSpillTempAuto();

   inline void setLocalObject();
   inline bool isLocalObject();

   inline void setBehaveLikeNonTemp();
   inline bool behaveLikeNonTemp();

   inline void setPinningArrayPointer();
   inline bool isPinningArrayPointer();

   inline void setAutoAddressTaken();
   inline bool isAutoAddressTaken();

   inline void setSpillTempLoaded();
   inline bool isSpillTempLoaded();

   inline void setVariableSizeSymbol();
   inline bool isVariableSizeSymbol();

   inline void setThisTempForObjectCtor();
   inline bool isThisTempForObjectCtor();

   // flag methods specific to Parms
   //
   inline void setParmHasToBeOnStack();
   inline bool isParmHasToBeOnStack();

   inline void setReferencedParameter();
   inline void resetReferencedParameter();
   inline bool isReferencedParameter();

   inline void setReinstatedReceiver();
   inline bool isReinstatedReceiver();

   // flag methods specific to statics
   //
   inline void setConstString();
   inline bool isConstString();

   inline void setConstantDynamic();
   inline bool isConstantDynamic();

   inline void setAddressIsCPIndexOfStatic(bool b);
   inline bool addressIsCPIndexOfStatic();

   inline bool isRecognizedStatic();

   inline void setCompiledMethod();
   inline bool isCompiledMethod();

   inline void setStartPC();
   inline bool isStartPC();

   inline void setCountForRecompile();
   inline bool isCountForRecompile();

   inline void setRecompilationCounter();
   inline bool isRecompilationCounter();

   inline void setGCRPatchPoint();
   inline bool isGCRPatchPoint();

   inline void setConstantPoolAddress();
   inline bool isConstantPoolAddress();

   inline void setStaticAddressWithinMethodBounds();
   inline bool isStaticAddressWithinMethodBounds();

   // flag methods specific to resolved
   //
   inline bool isJittedMethod();

   // flag methods specific to shadows
   //
   inline void setArrayShadowSymbol();
   inline bool isArrayShadowSymbol();

   inline void setContiguousArrayDataAddrFieldSymbol();
   inline bool isContiguousArrayDataAddrFieldSymbol();

   inline bool isRecognizedShadow();

   inline bool isRecognizedKnownObjectShadow();

   inline void setArrayletShadowSymbol();
   inline bool isArrayletShadowSymbol();

   inline void setGlobalFragmentShadowSymbol();
   inline bool isGlobalFragmentShadowSymbol();

   inline void setMemoryTypeShadowSymbol();
   inline bool isMemoryTypeShadowSymbol();

   void setOrdered() { _flags.set(Ordered); }
   bool isOrdered()  { return _flags.testAny(Ordered); }

   // flag methods specific to labels
   //
   inline void setStartOfColdInstructionStream();
   inline bool isStartOfColdInstructionStream();

   inline void setStartInternalControlFlow();
   inline bool isStartInternalControlFlow();

   inline void setEndInternalControlFlow();
   inline bool isEndInternalControlFlow();

   inline void setInternalControlFlowMerge();
   inline bool isInternalControlFlowMerge();

   inline void setEndOfColdInstructionStream();
   inline bool isEndOfColdInstructionStream();

   inline bool isNonLinear();
   inline void setNonLinear();

   inline void setRelativeLabel();
   inline bool isRelativeLabel();

   inline void setConstMethodType();
   inline bool isConstMethodType();

   inline void setConstMethodHandle();
   inline bool isConstMethodHandle();

   inline void setNonSpecificConstObject();
   inline bool isNonSpecificConstObject();

   inline bool isConstObjectRef();
   inline bool isStaticField();
   inline bool isFixedObjectRef();

   inline void setCallSiteTableEntry();
   inline bool isCallSiteTableEntry();

   void setHasAddrTaken() {  _flags2.set(HasAddrTaken); }
   bool isHasAddrTaken()  { return _flags2.testAny(HasAddrTaken); }

   inline void setMethodTypeTableEntry();
   inline bool isMethodTypeTableEntry();

   inline void setNotDataAddress();
   inline bool isNotDataAddress();

   inline void setUnsafeShadowSymbol();
   inline bool isUnsafeShadowSymbol();

   inline void setNamedShadowSymbol();
   inline bool isNamedShadowSymbol();

   void setImmutableField() { _flags2.set(ImmutableField); }
   bool isImmutableField()  { return _flags2.testAny(ImmutableField); }

   void setIsPendingPush() { _flags2.set(PendingPush); }
   bool isPendingPush()    { return _flags2.testAny(PendingPush); }

   inline void setDummyResolvedMethod();
   inline bool isDummyResolvedMethod();

   inline void setStaticDefaultValueInstance();
   inline bool isStaticDefaultValueInstance();

   /**
    * Enum values for _flags field.
    */
   enum
      {
      /**
       * The low bits of the flag field are used to store the data type, and
       * this is the mask used to access that data
       */
      DataTypeMask              = 0x000000FF,
      // RegisterMappedSymbols must be before data symbols TODO: Why?
      IsAutomatic               = 0x00000000,
      IsParameter               = 0x00000100,
      IsMethodMetaData          = 0x00000200,
      LastRegisterMapped        = 0x00000200,

      IsStatic                  = 0x00000300,
      IsMethod                  = 0x00000400,
      IsResolvedMethod          = 0x00000500,
      IsShadow                  = 0x00000600,
      IsLabel                   = 0x00000700,

      /**
       * Mask used to access register kind
       */
      KindMask                  = 0x00000700,


      IsInGlobalRegister        = 0x00000800,
      Const                     = 0x00001000,
      Volatile                  = 0x00002000,
      InitializedReference      = 0x00004000,
      ClassObject               = 0x00008000, ///< class object pointer
      NotCollected              = 0x00010000,
      Final                     = 0x00020000,
      InternalPointer           = 0x00040000,
      Private                   = 0x00080000,
      AddressOfClassObject      = 0x00100000, ///< address of a class object pointer
      SlotSharedByRefAndNonRef  = 0x00400000, ///< used in fsd to indicate that an reference symbol shares a slot with a nonreference symbol

      HoldsMonitoredObject      = 0x08000000,
      IsNamed                   = 0x00800000, ///< non-methods: symbol is actually an instance of a named subclass

      // only use by Symbols for which isAuto is true
      //
      SpillTemp                 = 0x80000000,
      IsLocalObject             = 0x40000000,
      BehaveLikeNonTemp         = 0x20000000, ///< used for temporaries that are
                                              ///< to behave as regular locals to
                                              ///< preserve floating point semantics
      PinningArrayPointer       = 0x10000000,
      contiguousArrayDataAddrField = 0x00020000,
      AutoAddressTaken          = 0x04000000, ///< a loadaddr of this auto exists
      SpillTempLoaded           = 0x04000000, ///< share bit with loadaddr because spill temps will never have their address taken. Used to remove store to spill if never loaded
      // Available              = 0x02000000,
      VariableSizeSymbol        = 0x01000000, ///< non-java only?: specially managed automatic symbols that contain both an activeSize and a size
      ThisTempForObjectCtor     = 0x01000000, ///< java only; this temp for j/l/Object constructor

      // only use by Symbols for which isParm is true
      //
      ParmHasToBeOnStack        = 0x80000000, ///< parameter is both loadAddr-ed and assigned a global register,
                                              ///< or parameter has been stored (store opcode)
      ReferencedParameter       = 0x40000000,
      ReinstatedReceiver        = 0x20000000, ///< Receiver reinstated for DLT

      // only use by Symbols for which isStatic is true
      ConstString               = 0x80000000,
      AddressIsCPIndexOfStatic  = 0x40000000,
      RecognizedStatic          = 0x20000000,
      ConstantPoolAddress       = 0x10000000,
      CompiledMethod            = 0x08000000,
      StartPC                   = 0x04000000,
      CountForRecompile         = 0x02000000,
      RecompilationCounter      = 0x01000000,
      GCRPatchPoint             = 0x00400000,
      StaticAddressWithinMethodBounds = 0x00800000, // Address is inside a method body and can be accessed with RIP addressing without relocations

      //Only Used by Symbols for which isResolvedMethod is true;
      IsJittedMethod            = 0x80000000,

      // only use by Symbols for which isShadow is true
      //
      ArrayShadow               = 0x80000000,
      RecognizedShadow          = 0x40000000, // recognized field
      ArrayletShadow            = 0x20000000,
      RecognizedKnownObjectShadow = 0x10000000,
      GlobalFragmentShadow      = 0x08000000,
      MemoryTypeShadow          = 0x04000000,
      Ordered                   = 0x02000000,
      // Available              = 0x01000000,
      // Available              = 0x00800000,

      // only use by Symbols for which isLabel is true
      //
      StartOfColdInstructionStream = 0x80000000, // label at the start of an out-of-line instruction stream
      StartInternalControlFlow     = 0x40000000,
      EndInternalControlFlow       = 0x20000000,
      // Available                 = 0x10000000,
      // Available                 = 0x08000000,
      // Available                 = 0x04000000,
      InternalControlFlowMerge     = 0x02000000, // mainline merge label for OOL instructions
      EndOfColdInstructionStream   = 0x01000000,
      NonLinear                    = 0x01000000, // TAROK and temporary.  This bit is used in conjunction with StartOfColdInstructionStream
                                                 //    to distinguish "classic" OOL instructions and the new form for Tarok.

      LabelKindMask                = 0x30000000,
      OOLMask                      = 0x81000000, // Tarok and temporary

      LastEnum
      };

   enum // For _flags2
      {
      RelativeLabel             = 0x00000001, // Label Symbols only *+N
      ConstMethodType           = 0x00000002, // JSR292
      ConstMethodHandle         = 0x00000004, // JSR292
      CallSiteTableEntry        = 0x00000008, // JSR292
      HasAddrTaken              = 0x00000010, // used to denote that we have a loadaddr of this symbol
      MethodTypeTableEntry      = 0x00000020, // JSR292
      NotDataAddress            = 0x00000040, // isStatic only: AOT
      DebugCounter              = 0x00000080, // Debug Counter for AOT
      UnsafeShadow              = 0x00000100,
      NamedShadow               = 0x00000200,
      ImmutableField            = 0x00000400,
      PendingPush               = 0x00000800,
      ConstantDynamic           = 0x00001000,
      NonSpecificConstObject    = 0x00002000, // Constant object not specific to a type
      BlockFrequency            = 0x00004000,
      RecompQueuedFlag          = 0x00008000,
      /**
       * This flag is used to identify symbols corresponding to dummy TR_ResolvedMethod
       * that are not really resolved, but treated as such as the resolution mechanism
       * involves resolving the arguments instead. An example of that is linkToStatic,
       * which is a VM internal native call that is created for unresolved invokedynamic
       * and invokehandle bytecodes.
       */
      DummyResolvedMethod       = 0x00010000,
      /**
       * This flag is used to identify the value type default value instance slot address
       */
      StaticDefaultValueInstance = 0x00020000,
      CatchBlockCounter          = 0x00040000,
      EnterEventHookAddress      = 0x00080000,
      ExitEventHookAddress       = 0x00100000,
      };

protected:

   size_t        _size;
   const char *  _name;
   TR_OpaqueClassBlock *_declaredClass;
   flags32_t     _flags;
   flags32_t     _flags2;
   uint16_t      _localIndex;

   /**
    * Shadow Symbol
    *
    * Used on indirect loads and stores to cover the data being referenced
    */
public:

   template <typename AllocatorType>
   static TR::Symbol * createShadow(AllocatorType m);

   template <typename AllocatorType>
   static TR::Symbol * createShadow(AllocatorType m, TR::DataType d);

   template <typename AllocatorType>
   static TR::Symbol * createShadow(AllocatorType m, TR::DataType d, uint32_t );

   /**
    * TR_NamedShadowSymbol
    *
    * \deprecated These factories are deprecated and should be removed when possible.
    *
    * @{
    */
public:

   template <typename AllocatorType>
   static TR::Symbol * createNamedShadow(AllocatorType m, TR::DataType d, uint32_t s, char *name = NULL);

   /** @} */

   };
}

#endif
