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
 *******************************************************************************/

/*
 * This file will be included within a table of function pointers.
 * Only valid static methods within CodeGenPhase class should be included.
 */


 // The entries in this file must be kept in sync with compiler/codegen/OMRCodeGenPhaseEnum.hpp
   TR::CodeGenPhase::performReserveCodeCachePhase,                                           //ReserveCodeCachePhase
   TR::CodeGenPhase::performLowerTreesPhase,                                                 //LowerTreesPhase
   TR::CodeGenPhase::performUncommonCallConstNodesPhase,                                     //UncommonCallConstNodesPhase
   TR::CodeGenPhase::performSetupForInstructionSelectionPhase,                               //SetupForInstructionSelectionPhase
   TR::CodeGenPhase::performInstructionSelectionPhase,                                       //InstructionSelectionPhase
   TR::CodeGenPhase::performCreateStackAtlasPhase,                                           //CreateStackAtlasPhase
   TR::CodeGenPhase::performRegisterAssigningPhase,                                          //RegisterAssigningPhase
   TR::CodeGenPhase::performMapStackPhase,                                                   //MapStackPhase
   TR::CodeGenPhase::performPeepholePhase,                                                   //PeepholePhase
   TR::CodeGenPhase::performBinaryEncodingPhase,                                             //BinaryEncodingPhase
   TR::CodeGenPhase::performEmitSnippetsPhase,                                               //EmitSnippetsPhase
   TR::CodeGenPhase::performProcessRelocationsPhase,                                         //ProcessRelocationsPhase
   TR::CodeGenPhase::performFindAndFixCommonedReferencesPhase,                               //FindAndFixCommonedReferencesPhase
   TR::CodeGenPhase::performRemoveUnusedLocalsPhase,                                         //RemoveUnusedLocalsPhase
   TR::CodeGenPhase::performShrinkWrappingPhase,                                             //ShrinkWrappingPhase
   TR::CodeGenPhase::performInliningReportPhase,                                             //InliningReportPhase
   TR::CodeGenPhase::performInsertDebugCountersPhase,
   TR::CodeGenPhase::performCleanUpFlagsPhase,


