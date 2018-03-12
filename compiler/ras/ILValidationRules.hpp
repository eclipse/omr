/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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

#ifndef ILVALIDATIONRULES_HPP
#define ILVALIDATIONRULES_HPP

/**
 * @file
 *
 * This file contains classes that specify certain Validation rules
 * that the ILValidator can then use to validate the given IL.
 * The utilities for writing generic ValidationRules are provided
 * in ILValidationUtils.hpp.
 *
 * NOTE: 1. Please add any new `*ValidationRule`s here!
 *
 *       2. ILValidationStrategies.hpp must also be updated
 *          for a newly added `Rule` to become part of a particular
 *          Validation Strategy.
 *     
 *       3. Finally, the ILValidator is responsible for validating
 *          the IL based on a certain ILValidationStrategy.
 *          So please instantiate the said `*ValidationRule` object
 *          during the creation of TR::ILValidator.
 *
 */

#ifdef OMRZTPF
#define __TPF_DO_NOT_MAP_ATOE_REMOVE
#endif

#include "ras/ILValidationStrategies.hpp"

#include "infra/BitVector.hpp"
#include "infra/SideTable.hpp"
#include "ras/ILValidationUtils.hpp"


namespace TR { class Compilation; }
namespace TR { class NodeChecklist; }
namespace TR { class ResolvedMethodSymbol; }
namespace TR { class TreeTop; }

namespace TR {

/**
 * MethodValidationRule:
 *
 * Verify that the IL of a method (ResolvedMethodSymbol) has certain properties.
 *
 */
class MethodValidationRule
   {
   TR::Compilation*      _comp;
   OMR::ILValidationRule _id;
   public:
   MethodValidationRule(TR::Compilation *comp, OMR::ILValidationRule id)
   : _comp(comp)
   , _id(id)
   {
   }
   /**
    * @return returns on success.
    */
   virtual void validate(TR::ResolvedMethodSymbol *methodSymbol) = 0;

   TR::Compilation*      comp() { return _comp; }
   OMR::ILValidationRule id()   { return _id; }
   };


class SoundnessRule : public MethodValidationRule
   {
   public:
   SoundnessRule(TR::Compilation *comp);
   void validate(TR::ResolvedMethodSymbol *methodSymbol);

   private:
   void checkNodeSoundness(TR::TreeTop *location, TR::Node *node,
                           TR::NodeChecklist &ancestorNodes,
                           TR::NodeChecklist &visitedNodes);

   void checkSoundnessCondition(TR::TreeTop *location, bool condition,
                                const char *formatStr, ...);
   };

class ValidateLivenessBoundaries : public MethodValidationRule
   {
   public:
   ValidateLivenessBoundaries(TR::Compilation *comp);
   void validate(TR::ResolvedMethodSymbol *methodSymbol);

   private:
   void validateEndOfExtendedBlockBoundary(TR::Node *node,
                                           LiveNodeWindow &liveNodes);

   void updateNodeState(TR::Node *node,
                        TR::NodeSideTable<TR::NodeState>  &nodeStates,
                        TR::LiveNodeWindow &liveNodes);
   };

/* NOTE: Please add any new MethodValidationRules here */



/**
 * BlockValidationRule: 
 * 
 * Verify that the IL for a particular extended block has certain properties.
 */

class BlockValidationRule
   {
   TR::Compilation*      _comp;
   OMR::ILValidationRule _id;
   public:
   BlockValidationRule(TR::Compilation *comp, OMR::ILValidationRule id)
   : _comp(comp)
   , _id(id)
   {
   }
   /**
    * @return returns on success.
    */
   virtual void validate(TR::TreeTop *firstTreeTop, TR::TreeTop *exitTreeTop) = 0;

   TR::Compilation*      comp() { return _comp; }
   OMR::ILValidationRule id()   { return _id; }
   };


class ValidateNodeRefCountWithinBlock : public BlockValidationRule
   {
   TR_BitVector  _nodeChecklist;

   public:
   ValidateNodeRefCountWithinBlock(TR::Compilation *comp);
   void validate(TR::TreeTop *firstTreeTop, TR::TreeTop *exitTreeTop);

   private:
   void validateRefCountPass1(TR::Node *node);
   void validateRefCountPass2(TR::Node *node);
   };

/* NOTE: Please add any new BlockValidationRules here */



/**
 * NodeValidationRule: 
 * 
 * Verify that the IL for a particular TR::Node has certain properties.
 */
class NodeValidationRule
   {
   TR::Compilation*      _comp;
   OMR::ILValidationRule _id;
   public:
   NodeValidationRule(TR::Compilation *comp, OMR::ILValidationRule id)
   : _comp(comp)
   , _id(id)
   {
   }
   /**
    * @return returns on success.
    */
   virtual void validate(TR::Node *node) = 0;

   TR::Compilation*      comp() { return _comp; }
   OMR::ILValidationRule id()   { return _id; }
   };



class ValidateChildCount : public NodeValidationRule
   {
   public:
   ValidateChildCount(TR::Compilation *comp);

   void validate(TR::Node *node);
   };


class ValidateChildTypes : public NodeValidationRule
   {
   public:
   ValidateChildTypes(TR::Compilation *comp);

   void validate(TR::Node *node);
   };

/**
 * NOTE: As things stand, the expected child type for `ireturn` is
 *       one of Int{8,16,32}.
 *       See Issue #1901 for more details.
 */
class Validate_ireturnReturnType : public NodeValidationRule
   {
   public:
   Validate_ireturnReturnType(TR::Compilation *comp);

   void validate(TR::Node *node);
   };

/* NOTE: Please add any new NodeValidationRules here */

} //namespace TR


#endif
