/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2017, 2017
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

/**************************************************************
 * NOTE: THIS FILE INTENTIONALLY DOES NOT HAVE INCLUDE GUARDS *
 **************************************************************/

      /**
       * The low bits of the flag field are used to store the data type, and
       * this is the mask used to access that data
       */
ENUM_V(DataTypeMask              , 0x000000FF),
      // RegisterMappedSymbols must be before data symbols TODO: Why?
ENUM_V(IsAutomatic               , 0x00000000),
ENUM_V(IsParameter               , 0x00000100),
ENUM_V(IsMethodMetaData          , 0x00000200),
ENUM_V(LastRegisterMapped        , 0x00000200),

ENUM_V(IsStatic                  , 0x00000300),
ENUM_V(IsMethod                  , 0x00000400),
ENUM_V(IsResolvedMethod          , 0x00000500),
ENUM_V(IsShadow                  , 0x00000600),
ENUM_V(IsLabel                   , 0x00000700),

      /**
       * Mask used to access register kind
       */
ENUM_V(KindMask                  , 0x00000700),


ENUM_V(IsInGlobalRegister        , 0x00000800),
ENUM_V(Const                     , 0x00001000),
ENUM_V(Volatile                  , 0x00002000),
ENUM_V(InitializedReference      , 0x00004000),
ENUM_V(ClassObject               , 0x00008000), ///< class object pointer
ENUM_V(NotCollected              , 0x00010000),
ENUM_V(Final                     , 0x00020000),
ENUM_V(InternalPointer           , 0x00040000),
ENUM_V(Private                   , 0x00080000),
ENUM_V(AddressOfClassObject      , 0x00100000), ///< address of a class object pointer
ENUM_V(SlotSharedByRefAndNonRef  , 0x00400000), ///< used in fsd to indicate that an reference symbol shares a slot with a nonreference symbol

ENUM_V(HoldsMonitoredObject      , 0x08000000),
ENUM_V(IsNamed                   , 0x00800000), ///< non-methods: symbol is actually an instance of a named subclass

      // only use by Symbols for which isAuto is true
      //
ENUM_V(SpillTemp                 , 0x80000000),
ENUM_V(IsLocalObject             , 0x40000000),
ENUM_V(BehaveLikeNonTemp         , 0x20000000), ///< used for temporaries that are
                                              ///< to behave as regular locals to
                                              ///< preserve floating point semantics
ENUM_V(PinningArrayPointer       , 0x10000000),
ENUM_V(RegisterAuto              , 0x00020000), ///< Symbol to be translated to register at instruction selection
ENUM_V(AutoAddressTaken          , 0x04000000), ///< a loadaddr of this auto exists
ENUM_V(SpillTempLoaded           , 0x04000000), ///< share bit with loadaddr because spill temps will never have their address taken. Used to remove store to spill if never loaded
ENUM_V(AutoMarkerSymbol          , 0x02000000), ///< dummy symbol marking some auto boundary
ENUM_V(VariableSizeSymbol        , 0x01000000), ///< non-java only?: specially managed automatic symbols that contain both an activeSize and a size
ENUM_V(ThisTempForObjectCtor     , 0x01000000), ///< java only; this temp for j/l/Object constructor

      // only use by Symbols for which isParm is true
      //
ENUM_V(ParmHasToBeOnStack        , 0x80000000), ///< parameter is both loadAddr-ed and assigned a global register,
                                              ///< or parameter has been stored (store opcode)
ENUM_V(ReferencedParameter       , 0x40000000),
ENUM_V(ReinstatedReceiver        , 0x20000000), ///< Receiver reinstated for DLT

      // only use by Symbols for which isStatic is true
ENUM_V(ConstString               , 0x80000000),
ENUM_V(AddressIsCPIndexOfStatic  , 0x40000000),
ENUM_V(RecognizedStatic          , 0x20000000),
//ENUM_V(Available               , 0x10000000),
ENUM_V(SetUpDLPFlags             , 0xF0000000), ///< Used by TR::StaticSymbol::SetupDLPFlags(), == ConstString | AddressIsCPIndexOfStatic | RecognizedStatic
ENUM_V(CompiledMethod            , 0x08000000),
ENUM_V(StartPC                   , 0x04000000),
ENUM_V(CountForRecompile         , 0x02000000),
ENUM_V(RecompilationCounter      , 0x01000000),
ENUM_V(GCRPatchPoint             , 0x00400000),

      //Only Used by Symbols for which isResolvedMethod is true;
ENUM_V(IsJittedMethod            , 0x80000000),

      // only use by Symbols for which isShadow is true
      //
ENUM_V(ArrayShadow               , 0x80000000),
ENUM_V(RecognizedShadow          , 0x40000000), // recognized field
ENUM_V(ArrayletShadow            , 0x20000000),
ENUM_V(PythonLocalVariable       , 0x10000000), // Python local variable shadow  TODO: If we ever move this somewhere else, can we move UnsafeShadow from flags2 to here?
ENUM_V(GlobalFragmentShadow      , 0x08000000),
ENUM_V(MemoryTypeShadow          , 0x04000000),
ENUM_V(Ordered                   , 0x02000000),
ENUM_V(PythonConstant            , 0x01000000), // Python constant shadow
ENUM_V(PythonName                , 0x00800000), // Python name shadow

      // only use by Symbols for which isLabel is true
      //
ENUM_V(StartOfColdInstructionStream , 0x80000000), // label at the start of an out-of-line instruction stream
ENUM_V(StartInternalControlFlow     , 0x40000000),
ENUM_V(EndInternalControlFlow       , 0x20000000),
//ENUM_V(Available                  , 0x10000000),
ENUM_V(IsVMThreadLive               , 0x08000000), // reg assigner has determined that vmthread must be in the proper register at this label
//ENUM_V(Available                  , 0x04000000),
ENUM_V(InternalControlFlowMerge     , 0x02000000), // mainline merge label for OOL instructions
ENUM_V(EndOfColdInstructionStream   , 0x01000000),
ENUM_V(NonLinear                    , 0x01000000), // TAROK and temporary.  This bit is used in conjunction with StartOfColdInstructionStream
                                                 //    to distinguish "classic" OOL instructions and the new form for Tarok.

ENUM_V(IsGlobalLabel                , 0x30000000),
ENUM_V(LabelKindMask                , 0x30000000),
ENUM_V(OOLMask                      , 0x81000000), // Tarok and temporary

ENUM(LastEnum)
