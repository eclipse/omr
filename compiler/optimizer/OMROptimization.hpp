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

#ifndef OMR_OPTIMIZATION_INCL
#define OMR_OPTIMIZATION_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_OPTIMIZATION_CONNECTOR
#define OMR_OPTIMIZATION_CONNECTOR
namespace OMR { class Optimization; }
namespace OMR { typedef OMR::Optimization OptimizationConnector; }
#endif

#include <stddef.h>                                   // for NULL
#include <stdint.h>                                   // for int32_t
#include "compile/Compilation.hpp"                    // for Compilation
#include "env/TRMemory.hpp"                           // for Allocator, Allocatable, etc
#include "il/ILOpCodes.hpp"                           // for ILOpCodes
#include "infra/Assert.hpp"                           // for TR_ASSERT
#include "infra/Random.hpp"                           // for TR_HasRandomGenerator
#include "optimizer/OptimizationManager.hpp"          // for OptimizationManager
#include "optimizer/OptimizationManager_inlines.hpp"  // for OptimizationManager::self, etc
#include "optimizer/Optimizations.hpp"                // for Optimizations, etc
#include "optimizer/Optimizer.hpp"                    // for Optimizer
class TR_Debug;
class TR_FrontEnd;
namespace TR { class SymbolReferenceTable; }
namespace TR { class Block; }
namespace TR { class CodeGenerator; }
namespace TR { class Node; }
namespace TR { class Optimization; }

namespace OMR
{

class OMR_EXTENSIBLE Optimization: public TR_HasRandomGenerator
   {
   public:

   static void *operator new(size_t size, TR::Allocator a)
      { return a.allocate(size); }
   static void  operator delete(void *ptr, size_t size)
      { ((Optimization*)ptr)->allocator().deallocate(ptr, size); } /* t->allocator() better return the same allocator as used for new */

   /* Virtual destructor is necessary for the above delete operator to work
    * See "Modern C++ Design" section 4.7
    */
   virtual ~Optimization() {}

   inline TR::Optimization * self();

   Optimization(TR::OptimizationManager *manager)
      : TR_HasRandomGenerator(manager->comp()),
        _manager(manager)
      {}

   virtual bool shouldPerform()
      {
      // Opts that need to do additional checks should override this method.
      return true;
      }

   virtual int32_t perform() = 0;

   // called once before perform is executed
   virtual void prePerform();

   // called once after perform is executed
   virtual void postPerform();

   virtual int32_t performOnBlock(TR::Block *block)
      {
      TR_ASSERT(0, "performOnBlock should be implemented if this opt is being enabled block by block\n");
      return 0;
      }

   // called once before performOnBlock is executed for all relevant blocks
   virtual void prePerformOnBlocks() {}

   // called once after performOnBlock is executed for all relevant blocks
   virtual void postPerformOnBlocks() {}

   TR::OptimizationManager *  manager() 	{ return _manager; }
   TR::Compilation *          comp() 		{ return _manager->comp(); }
   TR::Optimizer *            optimizer() { return _manager->optimizer(); }

   TR::CodeGenerator *        cg();
   TR_FrontEnd *              fe();
   TR_Debug *             getDebug();
   TR::SymbolReferenceTable *getSymRefTab();

   TR_Memory *                trMemory();
   TR_StackMemory             trStackMemory();
   TR_HeapMemory              trHeapMemory();
   TR_PersistentMemory *      trPersistentMemory();

   TR::Allocator              allocator();

   OMR::Optimizations         id();
   const char *               name();
   virtual const char *       optDetailString() const throw() = 0;

   inline bool                trace();
   void                       setTrace(bool trace = true);

   bool                       getLastRun();

   void requestOpt(OMR::Optimizations optNum, bool value = true, TR::Block *block = NULL);

   // useful utility functions for opts
   void requestDeadTreesCleanup(bool value = true, TR::Block *block = NULL);

   void prepareToReplaceNode(TR::Node * node);
   void prepareToReplaceNode(TR::Node * node, TR::ILOpCodes opcode);

   // Code refactored from SimplifierCommon. In all methods using anchorTree,
   // children needing to be anchored will be anchored before the specified
   // treetop (via insertBefore)
   //
   // FIXME: The majority of these functions don't belong here, and should be moved.
   bool nodeIsOrderDependent(TR::Node *node, uint32_t depth, bool hasCommonedAncestor);
   void anchorChildren(TR::Node *node, TR::TreeTop* anchorTree, uint32_t depth=0, bool hasCommonedAncestor=false, TR::Node *replacement=0);
   void anchorAllChildren(TR::Node *node, TR::TreeTop* anchorTree);
   void generateAnchor(TR::Node *node, TR::TreeTop* anchorTree);
   void anchorNode(TR::Node *node, TR::TreeTop* anchorTree);

   //lower-level methods for folding branches
   //promoted to TR_Optimization, so the other opts can call this method
   bool removeOrconvertIfToGoto(TR::Node*& node, TR::Block* block, int takeBranch, TR::TreeTop* curTree, TR::TreeTop*& reachableTarget, TR::TreeTop*& unreachableTarget, const char* opt_details);
   TR::CFGEdge* changeConditionalToUnconditional(TR::Node*& node, TR::Block* block, int takeBranch, TR::TreeTop* curTree, const char* opt_details = "UNKNOWN OPT");

   void prepareToStopUsingNode(TR::Node *node, TR::TreeTop* anchorTree, bool anchorChildren=true);
   TR::Node *replaceNodeWithChild(TR::Node *node, TR::Node *child, TR::TreeTop* anchorTree, TR::Block *block, bool correctBCDPrecision=true);
   TR::Node *replaceNode(TR::Node * node, TR::Node *other, TR::TreeTop* anchorTree, bool anchorChildren=true);
   void removeNode(TR::Node * node, TR::TreeTop* anchorTree);


   protected:

   TR::OptimizationManager   *_manager;

   };

}

#endif
