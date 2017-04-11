/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2017
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

#ifndef TR_LOCALVALUEPROPAGATION_INCL
#define TR_LOCALVALUEPROPAGATION_INCL

#include "optimizer/ValuePropagation.hpp"

namespace TR {

/** \class LocalValuePropagation
 *
 *  \brief An optimization that propagates value constraints within a block.

 *  The local value propagation optimization can conduct constant, type
 *  and relational propagation within a block. It propagates constants
 *  and types based on assignments and performs removal of subsumed checks
 *  and compares. Also performs devirtualization of calls, checkcast and
 *  array store check elimination.  It does not do value numbering, instead
 *  works on the assumption that every expression has a unique value
 *  number (improved effectiveness if expressions having same value are
 *  commoned by a prior local common subexpression elimination pass).
 *
 * \note
 *  The class is not intented to be extended, ValuePropagation is the core
 *  engine and home to extension points of language specific optimizations
 */
class LocalValuePropagation : public TR::ValuePropagation
   {
   public:

   LocalValuePropagation(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR::LocalValuePropagation(manager);
      }

   virtual int32_t perform();
   virtual int32_t performOnBlock(TR::Block *);
   virtual void prePerformOnBlocks();
   virtual void postPerformOnBlocks();

   private:

   TR::TreeTop *processBlock(TR::TreeTop *start);
   };
}
#endif
