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

#ifndef OMR_LABELSYMBOL_INCL
#define OMR_LABELSYMBOL_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_LABELSYMBOL_CONNECTOR
#define OMR_LABELSYMBOL_CONNECTOR
namespace OMR { class LabelSymbol; }
namespace OMR { typedef OMR::LabelSymbol LabelSymbolConnector; }
#endif

#include "il/Symbol.hpp"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "infra/Assert.hpp"

class TR_Debug;
namespace TR { class Block; }
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class Instruction; }
namespace TR { class LabelSymbol; }
namespace TR { class ParameterSymbol; }
namespace TR { class Snippet; }
namespace TR { class StaticSymbol; }
namespace TR { class SymbolReference; }
template <class T> class List;

typedef uintptr_t TR_UniqueCompilationId;
const TR_UniqueCompilationId nullCompilationId(~(TR_UniqueCompilationId) 0);

namespace OMR
{

/**
 * A symbol representing a label.
 *
 * A label has an instruction, a code location...
 */
class OMR_EXTENSIBLE LabelSymbol : public TR::Symbol
   {
public:
   TR::LabelSymbol * self();

   template <typename AllocatorType>
   static TR::LabelSymbol * create(AllocatorType);

   template <typename AllocatorType>
   static TR::LabelSymbol * create(AllocatorType, TR::CodeGenerator*);

   template <typename AllocatorType>
   static TR::LabelSymbol * create(AllocatorType, TR::CodeGenerator*, TR::Block*);

protected:

   LabelSymbol();
   LabelSymbol(TR::CodeGenerator *codeGen);
   LabelSymbol(TR::CodeGenerator *codeGen, TR::Block *labb);

public:

   // Using declaration is required here or else the
   // debug getName will hide the parent class getName
   using TR::Symbol::getName;
   const char * getName(TR_Debug * debug);

   TR::Instruction * getInstruction()                   { return _instruction;       }
   TR::Instruction * setInstruction(TR::Instruction *p) { return (_instruction = p); }

   uint8_t * getCodeLocation()           { return _codeLocation; }
   void      setCodeLocation(uint8_t *p) { _codeLocation = p; }

   int32_t getEstimatedCodeLocation()          { return _estimatedCodeLocation; }
   int32_t setEstimatedCodeLocation(int32_t p) { return (_estimatedCodeLocation = p); }

   TR::Snippet * getSnippet()               { return _snippet; }
   TR::Snippet * setSnippet(TR::Snippet *s) { return (_snippet = s); }

   void setDirectlyTargeted() { _directlyTargeted = true; }
   TR_YesNoMaybe isTargeted();

private:

   TR::Instruction *  _instruction;

   uint8_t *          _codeLocation;

   int32_t            _estimatedCodeLocation;

   TR::Snippet *      _snippet;

   bool               _directlyTargeted;

public:
   /*------------- TR_RelativeLabelSymbol -----------------*/
   template <typename AllocatorType>
   static TR::LabelSymbol *createRelativeLabel(AllocatorType m, TR::CodeGenerator * cg, intptr_t offset);

   /**
    * Mark a LabelSymbol as a RelativeLabelSymbol, inializing members as
    * appropriate.
    *
    * A relative label provides an offset/distance (and changes the name of a
    * symbol to the offset).
    *
    * @todo This leaks memory right now.
    */
   void makeRelativeLabelSymbol(intptr_t offset);

   intptr_t getDistance();
private:
   intptr_t     _offset;
  };

}
/**
 * Static creation function.
 */
TR::LabelSymbol *generateLabelSymbol(TR::CodeGenerator *cg);

#endif
