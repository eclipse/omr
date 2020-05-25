/*******************************************************************************
 * Copyright (c) 2017, 2020 IBM Corp. and others
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

#include <algorithm>

#include "ras/ILValidator.hpp"

#include "compile/Compilation.hpp"
#include "infra/Assert.hpp"
#include "infra/ILWalk.hpp"
#include "ras/ILValidationRules.hpp"
#include "ras/ILValidationStrategies.hpp"


/**************************************************************************
 *
 * OMR ILValidation Strategies
 *
 **************************************************************************/

/* Do not perform any Validation under this Strategy. */
const OMR::ILValidationStrategy OMR::emptyStrategy[] =
   {
   { OMR::endRules }
   };

/* Strategy used to Validate right after ILGeneration. */
const OMR::ILValidationStrategy OMR::postILgenValidatonStrategy[] =
   {
   { OMR::soundnessRule                   },
   { OMR::validateChildCount              },
   { OMR::validateChildTypes              },
   { OMR::validateLivenessBoundaries      },
   { OMR::validateNodeRefCountWithinBlock },
   { OMR::validate_axaddEnvironment       },
   { OMR::endRules                        }
   };

/**
 * Strategy used to Validate right before Codegen.
 * At this point the IL is expected to uphold almost all the Validation Rules.
 */
const OMR::ILValidationStrategy OMR::preCodegenValidationStrategy[] =
   {
   { OMR::soundnessRule                             },
   { OMR::validateChildCount                        },
   { OMR::validateChildTypes                        },
   { OMR::validateLivenessBoundaries                },
   { OMR::validateNodeRefCountWithinBlock           },
   { OMR::validate_ireturnReturnType                },
   { OMR::endRules                                  }
   };

/**************************************************************************
 * The following array of ILValidationStrategy pointers, help provide a
 * convenient way of selecting a set of ILValidation Rules while Validating
 * the IL corresponding to a particular Method.
 *
 * Example:
 * At any point after ILgeneration, a call of the following form will
 * validate the current IL based on preCodegenValidationStrategy.
 *
 * comp->validateIL(TR::omrValidationStrategies[preCodegenValidation]);
 *
 * See ILValidationStrategy.hpp for the possible values of
 * `TR::ILValidationContext` used to index `TR::omrValidationStrategies`.
 *
 **************************************************************************/
const OMR::ILValidationStrategy* TR::omrValidationStrategies[] =
   {
   OMR::emptyStrategy,
   OMR::preCodegenValidationStrategy,
   OMR::postILgenValidatonStrategy
   };

/**************************************************************************
 *
 * Implementation of TR::ILValidator
 *
 **************************************************************************/
template <typename T, size_t N> static
T* begin(T(&reqArray)[N])
  {
  return &reqArray[0];
  }
template <typename T, size_t N> static
T* end(T(&reqArray)[N])
   {
   return &reqArray[0]+N;
   }

TR::ILValidator::ILValidator(TR::Compilation *comp)
   :_comp(comp)
   {
     /**
      * All available `*ValidationRule`s are initialized during the creation of
      * ILValidator and they have the same lifetime as the ILValidator itself.
      * It is during the call to `validate` where we decide which subset
      * of the available ones we should use for Validation.
      * This also removes the need to initialize a particular set of `Rule` objects
      * every time a new Strategy is created, or a call to `validate` is made.
      */
     TR::MethodValidationRule* temp_method_rules[] =
        {
          new  (comp->trHeapMemory()) TR::SoundnessRule(_comp),
          new  (comp->trHeapMemory()) TR::ValidateLivenessBoundaries(_comp)
        };

     TR::BlockValidationRule* temp_block_rules[] =
        { new  (comp->trHeapMemory()) TR::ValidateNodeRefCountWithinBlock(_comp) };

     TR::NodeValidationRule* temp_node_rules[] =
        { 
          new  (comp->trHeapMemory()) TR::ValidateChildCount(_comp),
          new  (comp->trHeapMemory()) TR::ValidateChildTypes(_comp),
          new  (comp->trHeapMemory()) TR::Validate_ireturnReturnType(_comp),
          new  (comp->trHeapMemory()) TR::Validate_axaddEnvironment(_comp)
        };
     /**
      * NOTE: Please initialize any new *ValidationRule here!
      *
      * Also, ILValidationRules.hpp and ILValidationStrategies.hpp
      * need to be updated everytime a new ILValidation Rule
      * is added.
      */

     _methodValidationRules.assign(begin(temp_method_rules), end(temp_method_rules));
     _blockValidationRules.assign(begin(temp_block_rules), end(temp_block_rules));
     _nodeValidationRules.assign(begin(temp_node_rules), end(temp_node_rules));
   }

std::vector<TR::MethodValidationRule *>
TR::ILValidator::getRequiredMethodValidationRules(const OMR::ILValidationStrategy *strategy)
   {
   std::vector<TR::MethodValidationRule *> reqMethodValidationRules;
   while (strategy->id != OMR::endRules)
      {
      for (auto it = _methodValidationRules.begin(); it != _methodValidationRules.end(); ++it)
         {
         /**
          * Each *ValidationRule has a unique `id`. These ids are defined in
          * ILValidationStrategies.hpp and they are assigned in ILValidationRules.cpp. 
          */
         if (strategy->id == (*it)->id())
            {
            reqMethodValidationRules.push_back((*it));
            }
         }
      strategy++;
      }
   return reqMethodValidationRules;
   }

std::vector<TR::BlockValidationRule *>
TR::ILValidator::getRequiredBlockValidationRules(const OMR::ILValidationStrategy *strategy)
   {
   std::vector<TR::BlockValidationRule *> reqBlockValidationRules;
   while (strategy->id != OMR::endRules)
      {
      for (auto it = _blockValidationRules.begin(); it != _blockValidationRules.end(); ++it)
         {
         if (strategy->id == (*it)->id())
            {
            reqBlockValidationRules.push_back((*it));
            }
         }
      strategy++;
      }
   return reqBlockValidationRules;
   }

std::vector<TR::NodeValidationRule *>
TR::ILValidator::getRequiredNodeValidationRules(const OMR::ILValidationStrategy *strategy)
   {
   std::vector<TR::NodeValidationRule *> reqNodeValidationRules;
   while (strategy->id != OMR::endRules)
      {
      for (auto it = _nodeValidationRules.begin(); it != _nodeValidationRules.end(); ++it)
         {
         if (strategy->id == (*it)->id())
            {
            reqNodeValidationRules.push_back((*it));
            }
         }
      strategy++;
      }
   return reqNodeValidationRules;
   }


void TR::ILValidator::validate(const OMR::ILValidationStrategy *strategy)
   {
   /**
    * Selection Phase:
    *    From all the available `ILValidationRule`s, only select the ones
    *    corresponding to the given `OMR::ILValidationStrategy`.
    */
   std::vector<TR::MethodValidationRule *> reqMethodValidationRules =
      getRequiredMethodValidationRules(strategy);
   std::vector<TR::BlockValidationRule *> reqBlockValidationRules =
      getRequiredBlockValidationRules(strategy);
   std::vector<TR::NodeValidationRule *> reqNodeValidationRules =
      getRequiredNodeValidationRules(strategy);


   /**
    * Validation Phase:
    *    Validate against the required set of `ILValidationRule`s.
    */

   /* Rules that are veriified over the entire method. */
   TR::ResolvedMethodSymbol* methodSymbol = comp()->getMethodSymbol();
   for (auto it = reqMethodValidationRules.begin(); it != reqMethodValidationRules.end(); ++it)
      {
      (*it)->validate(methodSymbol);
      }

   /* Checks performed across an extended blocks. */
   for (auto it = reqBlockValidationRules.begin(); it != reqBlockValidationRules.end(); ++it)
      {
      TR::TreeTop *tt, *exitTreeTop;
      for (tt = methodSymbol->getFirstTreeTop(); tt; tt = exitTreeTop->getNextTreeTop())
         {
         TR::TreeTop *firstTreeTop = tt;
         exitTreeTop = tt->getExtendedBlockExitTreeTop();
         (*it)->validate(firstTreeTop, exitTreeTop);
         }
      }

   /* NodeValidationRules only check per node for a specific property. */
   for (auto it = reqNodeValidationRules.begin(); it != reqNodeValidationRules.end(); ++it)
      {
      for (TR::PreorderNodeIterator nodeIter(methodSymbol->getFirstTreeTop(), comp(), "NODE_VALIDATOR");
           nodeIter.currentTree(); ++nodeIter)
         {
         (*it)->validate(nodeIter.currentNode());
         }
      }
   }

TR::ILValidator* TR::createILValidatorObject(TR::Compilation *comp)
   {
   return new (comp->trHeapMemory()) TR::ILValidator(comp);
   }
