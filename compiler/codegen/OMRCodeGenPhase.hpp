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

#ifndef OMR_CODEGEN_PHASE
#define OMR_CODEGEN_PHASE

#include "infra/Annotations.hpp"                      // for OMR_EXTENSIBLE

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_CODEGEN_PHASE_CONNECTOR
#define OMR_CODEGEN_PHASE_CONNECTOR
namespace OMR { class CodeGenPhase; }
namespace OMR { typedef OMR::CodeGenPhase CodeGenPhaseConnector; }
#endif

namespace TR { class CodeGenPhase; }
namespace TR { class CodeGenerator; }

typedef void (* CodeGenPhaseFunctionPointer)(TR::CodeGenerator *, TR::CodeGenPhase *);

namespace OMR
{

class OMR_EXTENSIBLE CodeGenPhase
   {
   public:

   /*
    * This enum is extensible
    */
   enum PhaseValue
      {
      #include "codegen/CodeGenPhaseEnum.hpp"
      };

   TR::CodeGenPhase * self();

   /* This method will step through the PhaseList
    * and call perform on all the phases.
    */
   void performAll();

   /*
    * int operator overload used for comparsion of phases
    */
   operator int() const { return static_cast<int>(_currentPhase); }

   /*
    * Used for debugging purposes
    * Derive class must provide own implementation if more phases are being added to the enums
    */
   static const char* getName(PhaseValue phase);
   const char * getName();
   static int getNumPhases();

   void reportPhase(PhaseValue phase);

   /*
    * There will also be a list of functions that actually does
    * what the phases need to do. These will be divided up into
    * each extensible layer and can call base class if needed.
    */
   static void performUncommonCallConstNodesPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performReserveCodeCachePhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performLowerTreesPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performSetupForInstructionSelectionPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performInstructionSelectionPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performCreateStackAtlasPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performRegisterAssigningPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performMapStackPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performPeepholePhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performBinaryEncodingPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performEmitSnippetsPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performProcessRelocationsPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);

   static void performInliningReportPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performFindAndFixCommonedReferencesPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performRemoveUnusedLocalsPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performShrinkWrappingPhase(TR::CodeGenerator * cg, TR::CodeGenPhase *);
   static void performCleanUpFlagsPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase);
   static void performInsertDebugCountersPhase(TR::CodeGenerator * cg, TR::CodeGenPhase * phase);

   protected:

   CodeGenPhase(TR::CodeGenerator * cg): _cg(cg) {}

   /*
    * Each product/project provide it's own list of phases
    * to perform. It will not depend on parent layers.
    *
    * e.g. J9 will not execute the list in OMR.
    */
   static const PhaseValue PhaseList[];

   static int getListSize();

   /*
    * Function pointer table to dispatch method for each phase
    */
   static CodeGenPhaseFunctionPointer _phaseToFunctionTable[];

   /*
    * Members
    */
   TR::CodeGenerator * _cg;

   PhaseValue _currentPhase;

   };

}


class LexicalXmlTag
   {
   TR::CodeGenerator *cg;

   public:

   LexicalXmlTag(TR::CodeGenerator *cg);
   ~LexicalXmlTag();
   };

#endif
