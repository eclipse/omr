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

#include <stddef.h>                 // for size_t
#include <stdint.h>                 // for uint32_t, uint16_t, uint8_t, etc
#include "infra/Annotations.hpp"    // for OMR_EXTENSIBLE
#include "env/TRMemory.hpp"         // for TR_Memory, etc
#include "il/DataTypes.hpp"         // for TR::DataType, DataTypes
#include "infra/Assert.hpp"         // for TR_ASSERT
#include "infra/Flags.hpp"          // for flags32_t

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
    * Generic constructor, sets size to 0
    */
   Symbol() :
      _size(0),
      _name(0),
      _flags(0),
      _flags2(0),
      _localIndex(0),
      _restrictedRegisterNumber(-1)
   { }

   /**
    * Create symbol of specified data type, inferring size
    * from type.
    */
   Symbol(TR::DataType d);

   /**
    * Create symbol of specified data type, inferring size
    * from type.
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
   TR::RegisterMappedSymbol                   *getRegisterMappedSymbol();
   TR::AutomaticSymbol                        *getAutoSymbol();
   TR::ParameterSymbol                        *getParmSymbol();
   TR::AutomaticSymbol                        *getInternalPointerAutoSymbol();
   TR::AutomaticSymbol                        *getLocalObjectSymbol();
   TR::StaticSymbol                           *getStaticSymbol();
   TR::ResolvedMethodSymbol                   *getResolvedMethodSymbol();
   TR::MethodSymbol                           *getMethodSymbol();
   TR::Symbol                                 *getShadowSymbol();
   TR::Symbol                                 *getNamedShadowSymbol();
   TR::RegisterMappedSymbol                   *getMethodMetaDataSymbol();
   TR::LabelSymbol                            *getLabelSymbol();
   TR::ResolvedMethodSymbol                   *getJittedMethodSymbol();
   TR::StaticSymbol                           *getRecognizedStaticSymbol();
   TR::AutomaticSymbol                        *getVariableSizeSymbol();
   TR::StaticSymbol                           *getCallSiteTableEntrySymbol();
   TR::StaticSymbol                           *getMethodTypeTableEntrySymbol();
   TR::AutomaticSymbol                        *getRegisterSymbol();

   // These methods perform an explicit (debug) assume that the symbol is a correct type
   // and then return the symbol explicitly cast to the type.
   //
   TR::RegisterMappedSymbol            *castToRegisterMappedSymbol();
   TR::AutomaticSymbol                 *castToAutoSymbol();
   TR::AutomaticSymbol                 *castToVariableSizeSymbol();
   TR::AutomaticSymbol                 *castToAutoMarkerSymbol();
   TR::ParameterSymbol                 *castToParmSymbol();
   TR::AutomaticSymbol                 *castToInternalPointerAutoSymbol();
   TR::AutomaticSymbol                 *castToLocalObjectSymbol();
   TR::ResolvedMethodSymbol            *castToResolvedMethodSymbol();
   TR::MethodSymbol                    *castToMethodSymbol();
   TR::Symbol                          *castToShadowSymbol();
   TR::RegisterMappedSymbol            *castToMethodMetaDataSymbol();
   TR::LabelSymbol                     *castToLabelSymbol();
   TR::ResolvedMethodSymbol            *castToJittedMethodSymbol();
   TR::AutomaticSymbol                 *castToRegisterSymbol();

   TR::StaticSymbol                    *castToStaticSymbol();
   TR::StaticSymbol                    *castToNamedStaticSymbol();
   TR::StaticSymbol                    *castToCallSiteTableEntrySymbol();
   TR::StaticSymbol                    *castToMethodTypeTableEntrySymbol();

   int32_t getOffset();

   uint32_t getNumberOfSlots();

   bool dontEliminateStores(TR::Compilation *comp, bool isForLocalDeadStore = false);

   bool isReferenced();

   static uint32_t convertTypeToSize(TR::DataType dt);

   static uint32_t convertTypeToNumberOfSlots(TR::DataType dt);

   static TR::DataType convertSigCharToType(char sigChar);

   /**
    * Field functions
    */

   size_t   getSize()                       { return _size; }
   void     setSize(size_t s)               { _size = s; }
   uint32_t getRoundedSize();

   const char * getName()                   { return _name; }
   void         setName(const char * name)  { _name = name; }

   uint32_t getFlags()                      { return _flags.getValue(); }
   uint32_t getFlags2()                     { return _flags2.getValue(); }
   void setFlagValue(uint32_t v, bool b)    { _flags.setValue(v, b); }

   uint16_t getLocalIndex()                 { return _localIndex; }
   uint16_t setLocalIndex(uint16_t li)      { return (_localIndex = li); }

   uint8_t getRestrictedRegisterNumber()    { return _restrictedRegisterNumber; }

   /**
    * Flag functions
    */

   void          setDataType(TR::DataType dt);
   TR::DataType  getDataType() { return (TR::DataTypes)_flags.getValue(DataTypeMask);}
   TR::DataType  getType();

   int32_t getKind()             { return _flags.getValue(KindMask);}

   bool isAuto()                 { return _flags.testValue(KindMask, IsAutomatic); }
   bool isParm()                 { return _flags.testValue(KindMask, IsParameter); }
   bool isMethodMetaData()       { return _flags.testValue(KindMask, IsMethodMetaData); }
   bool isResolvedMethod()       { return _flags.testValue(KindMask, IsResolvedMethod); }
   bool isMethod();
   bool isStatic()               { return _flags.testValue(KindMask, IsStatic); }
   bool isShadow()               { return _flags.testValue(KindMask, IsShadow); }
   bool isLabel()                { return _flags.testValue(KindMask, IsLabel); }
   void setIsLabel()             { _flags.setValue(KindMask, IsLabel);}

   bool isRegisterMappedSymbol();

   bool isAutoOrParm();
   bool isAutoField()            { return false; }
   bool isParmField()            { return false; }
   bool isWeakSymbol()           { return false; }
   bool isRegularShadow();

   void setIsInGlobalRegister(bool b)       { _flags.set(IsInGlobalRegister, b); }
   bool isInGlobalRegister()                { return _flags.testAny(IsInGlobalRegister); }

   void setConst()                          { _flags.set(Const); }
   bool isConst()                           { return _flags.testAny(Const); }

   void setVolatile()                       { _flags.set(Volatile); }
   void resetVolatile()                     { _flags.reset(Volatile); }
   bool isVolatile()                        { return _flags.testAny(Volatile); }
   bool isSyncVolatile();

   void setInitializedReference()           { _flags.set(InitializedReference); }
   void setUninitializedReference()         { _flags.reset(InitializedReference); }
   bool isInitializedReference()            { return _flags.testAny(InitializedReference); }

   void setClassObject()                    { _flags.set(ClassObject); }
   bool isClassObject()                     { return _flags.testAny(ClassObject); }

   void setNotCollected()                   { _flags.set(NotCollected); }
   bool isNotCollected()                    { return _flags.testAny(NotCollected); }
   bool isCollectedReference();

   void setFinal()                          { _flags.set(Final); }
   bool isFinal()                           { return _flags.testAny(Final); }

   void setInternalPointer()                { _flags.set(InternalPointer); }
   bool isInternalPointer()                 { return _flags.testAny(InternalPointer); }
   bool isInternalPointerAuto();

   void setPrivate()                        { _flags.set(Private); }
   bool isPrivate()                         { return _flags.testAny(Private); }

   void setAddressOfClassObject()           { _flags.set(AddressOfClassObject); }
   bool isAddressOfClassObject()            { return _flags.testAny(AddressOfClassObject); }

   void setSlotSharedByRefAndNonRef(bool b) { _flags.set(SlotSharedByRefAndNonRef, b); }
   bool isSlotSharedByRefAndNonRef()        { return _flags.testAny(SlotSharedByRefAndNonRef); }

   void setHoldsMonitoredObject()           { _flags.set(HoldsMonitoredObject); }
   bool holdsMonitoredObject()              { return _flags.testAny(HoldsMonitoredObject); }

   bool isNamed();

   // flag methods specific to Autos
   //
   void setSpillTempAuto();
   bool isSpillTempAuto();

   void setLocalObject();
   bool isLocalObject();

   void setBehaveLikeNonTemp();
   bool behaveLikeNonTemp();

   void setPinningArrayPointer();
   bool isPinningArrayPointer();

   bool isRegisterSymbol();
   void setIsRegisterSymbol();

   void setAutoAddressTaken();
   bool isAutoAddressTaken();

   void setSpillTempLoaded();
   bool isSpillTempLoaded();

   void setAutoMarkerSymbol();
   bool isAutoMarkerSymbol();

   void setVariableSizeSymbol();
   bool isVariableSizeSymbol();

   void setThisTempForObjectCtor();
   bool isThisTempForObjectCtor();

   // flag methods specific to Parms
   //
   void setParmHasToBeOnStack();
   bool isParmHasToBeOnStack();

   void setReferencedParameter();
   void resetReferencedParameter();
   bool isReferencedParameter();

   void setReinstatedReceiver();
   bool isReinstatedReceiver();

   // flag methods specific to statics
   //
   void setConstString();
   bool isConstString();

   void setAddressIsCPIndexOfStatic(bool b);
   bool addressIsCPIndexOfStatic();

   bool isRecognizedStatic();

   void setCompiledMethod();
   bool isCompiledMethod();

   void setStartPC();
   bool isStartPC();

   void setCountForRecompile();
   bool isCountForRecompile();

   void setRecompilationCounter();
   bool isRecompilationCounter();

   void setGCRPatchPoint();
   bool isGCRPatchPoint();

   // flag methods specific to resolved
   //
   bool isJittedMethod();

   // flag methods specific to shadows
   //
   void setArrayShadowSymbol();
   bool isArrayShadowSymbol();

   bool isRecognizedShadow();

   void setArrayletShadowSymbol();
   bool isArrayletShadowSymbol();

   void setPythonLocalVariableShadowSymbol();
   bool isPythonLocalVariableShadowSymbol();

   void setGlobalFragmentShadowSymbol();
   bool isGlobalFragmentShadowSymbol();

   void setMemoryTypeShadowSymbol();
   bool isMemoryTypeShadowSymbol();

   void setOrdered() { _flags.set(Ordered); }
   bool isOrdered()  { return _flags.testAny(Ordered); }

   void setPythonConstantShadowSymbol();
   bool isPythonConstantShadowSymbol();

   void setPythonNameShadowSymbol();
   bool isPythonNameShadowSymbol();

   // flag methods specific to labels
   //
   void setStartOfColdInstructionStream();
   bool isStartOfColdInstructionStream();

   void setStartInternalControlFlow();
   bool isStartInternalControlFlow();

   void setEndInternalControlFlow();
   bool isEndInternalControlFlow();

   void setVMThreadLive();
   bool isVMThreadLive();

   void setInternalControlFlowMerge();
   bool isInternalControlFlowMerge();

   void setEndOfColdInstructionStream();
   bool isEndOfColdInstructionStream();

   bool isNonLinear();
   void setNonLinear();

   void setGlobalLabel();
   bool isGlobalLabel();

   void setRelativeLabel();
   bool isRelativeLabel();

   void setConstMethodType();
   bool isConstMethodType();

   void setConstMethodHandle();
   bool isConstMethodHandle();

   bool isConstObjectRef();
   bool isStaticField();
   bool isFixedObjectRef();

   void setCallSiteTableEntry();
   bool isCallSiteTableEntry();

   void setHasAddrTaken() {  _flags2.set(HasAddrTaken); }
   bool isHasAddrTaken()  { return _flags2.testAny(HasAddrTaken); }

   void setMethodTypeTableEntry();
   bool isMethodTypeTableEntry();

   void setNotDataAddress();
   bool isNotDataAddress();

   void setUnsafeShadowSymbol();
   bool isUnsafeShadowSymbol();

   void setNamedShadowSymbol();
   bool isNamedShadowSymbol();

   void setImmutableField() { _flags2.set(ImmutableField); }
   bool isImmutableField()  { return _flags2.testAny(ImmutableField); }

   void setIsPendingPush() { _flags2.set(PendingPush); }
   bool isPendingPush()    { return _flags2.testAny(PendingPush); }

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
      RegisterAuto              = 0x00020000, ///< Symbol to be translated to register at instruction selection
      AutoAddressTaken          = 0x04000000, ///< a loadaddr of this auto exists
      SpillTempLoaded           = 0x04000000, ///< share bit with loadaddr because spill temps will never have their address taken. Used to remove store to spill if never loaded
      AutoMarkerSymbol          = 0x02000000, ///< dummy symbol marking some auto boundary
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
      // Available              = 0x10000000,
      SetUpDLPFlags             = 0xF0000000, ///< Used by TR::StaticSymbol::SetupDLPFlags(), == ConstString | AddressIsCPIndexOfStatic | RecognizedStatic
      CompiledMethod            = 0x08000000,
      StartPC                   = 0x04000000,
      CountForRecompile         = 0x02000000,
      RecompilationCounter      = 0x01000000,
      GCRPatchPoint             = 0x00400000,

      //Only Used by Symbols for which isResolvedMethod is true;
      IsJittedMethod            = 0x80000000,

      // only use by Symbols for which isShadow is true
      //
      ArrayShadow               = 0x80000000,
      RecognizedShadow          = 0x40000000, // recognized field
      ArrayletShadow            = 0x20000000,
      PythonLocalVariable       = 0x10000000, // Python local variable shadow  TODO: If we ever move this somewhere else, can we move UnsafeShadow from flags2 to here?
      GlobalFragmentShadow      = 0x08000000,
      MemoryTypeShadow          = 0x04000000,
      Ordered                   = 0x02000000,
      PythonConstant            = 0x01000000, // Python constant shadow
      PythonName                = 0x00800000, // Python name shadow

      // only use by Symbols for which isLabel is true
      //
      StartOfColdInstructionStream = 0x80000000, // label at the start of an out-of-line instruction stream
      StartInternalControlFlow     = 0x40000000,
      EndInternalControlFlow       = 0x20000000,
      // Available                 = 0x10000000,
      IsVMThreadLive               = 0x08000000, // reg assigner has determined that vmthread must be in the proper register at this label
      // Available                 = 0x04000000,
      InternalControlFlowMerge     = 0x02000000, // mainline merge label for OOL instructions
      EndOfColdInstructionStream   = 0x01000000,
      NonLinear                    = 0x01000000, // TAROK and temporary.  This bit is used in conjunction with StartOfColdInstructionStream
                                                 //    to distinguish "classic" OOL instructions and the new form for Tarok.

      IsGlobalLabel                = 0x30000000,
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
      RealRegister              = 0x00000080, // RegisterSymbol is machine real register
      UnsafeShadow              = 0x00000100,
      NamedShadow               = 0x00000200,
      ImmutableField            = 0x00000400,
      PendingPush               = 0x00000800,
      };

protected:

   size_t        _size;
   const char *  _name;
   flags32_t     _flags;
   flags32_t     _flags2;
   uint16_t      _localIndex;
   uint8_t       _restrictedRegisterNumber;


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
