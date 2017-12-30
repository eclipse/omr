/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "optimizer/Optimizer.hpp"

#include <stddef.h>                                       // for NULL
#include <stdint.h>                                       // for uint16_t
#include "compile/Compilation.hpp"
#include "compile/Method.hpp"                             // for TR_Method
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"

#include "optimizer/Optimization.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/OptimizationStrategies.hpp"
#include "optimizer/Optimizations.hpp"
#include "optimizer/Structure.hpp"
#include "optimizer/StructuralAnalysis.hpp"
#include "optimizer/UseDefInfo.hpp"
#include "optimizer/ValueNumberInfo.hpp"

#include "optimizer/CFGSimplifier.hpp"
#include "optimizer/CompactLocals.hpp"
#include "optimizer/CopyPropagation.hpp"
#include "optimizer/DeadStoreElimination.hpp"
#include "optimizer/DeadTreesElimination.hpp"
#include "optimizer/ExpressionsSimplification.hpp"
#include "optimizer/GeneralLoopUnroller.hpp"
#include "optimizer/GlobalRegisterAllocator.hpp"
#include "optimizer/LocalCSE.hpp"
#include "optimizer/LocalDeadStoreElimination.hpp"
#include "optimizer/LocalLiveRangeReducer.hpp"
#include "optimizer/LocalOpts.hpp"
#include "optimizer/LocalReordering.hpp"
#include "optimizer/LoopCanonicalizer.hpp"
#include "optimizer/LoopReducer.hpp"
#include "optimizer/LoopReplicator.hpp"
#include "optimizer/LoopVersioner.hpp"
#include "optimizer/OrderBlocks.hpp"
#include "optimizer/PartialRedundancy.hpp"
#include "optimizer/IsolatedStoreElimination.hpp"
#include "optimizer/RegDepCopyRemoval.hpp"
#include "optimizer/Simplifier.hpp"
#include "optimizer/SinkStores.hpp"
#include "optimizer/ShrinkWrapping.hpp"
#include "optimizer/TrivialDeadBlockRemover.hpp"


static const OptimizationStrategy tacticalGlobalRegisterAllocatorOpts[] =
   {
   { OMR::inductionVariableAnalysis,             OMR::IfLoops                      },
   { OMR::loopCanonicalization,                  OMR::IfLoops                      },
   { OMR::liveRangeSplitter,                     OMR::IfLoops                      },
   { OMR::redundantGotoElimination,              OMR::IfNotProfiling               }, // need to be run before global register allocator
   { OMR::treeSimplification,                    OMR::MarkLastRun                  }, // Cleanup the trees after redundantGotoElimination
   { OMR::tacticalGlobalRegisterAllocator,       OMR::IfEnabled                    },
   { OMR::localCSE                                                                 },
// { isolatedStoreGroup,                         OMR::IfEnabled                    }, // if global register allocator created stores from registers
   { OMR::globalCopyPropagation,                 OMR::IfEnabledAndMoreThanOneBlock }, // if live range splitting created copies
   { OMR::localCSE                                                                 }, // localCSE after post-PRE + post-GRA globalCopyPropagation to clean up whole expression remat (rtc 64659)
   { OMR::globalDeadStoreGroup,                  OMR::IfEnabled                    },
   { OMR::redundantGotoElimination,              OMR::IfEnabled                    }, // if global register allocator created new block
   { OMR::deadTreesElimination                                                     }, // remove dangling GlRegDeps
   { OMR::deadTreesElimination,                  OMR::IfEnabled                    }, // remove dead RegStores produced by previous deadTrees pass
   { OMR::deadTreesElimination,                  OMR::IfEnabled                    }, // remove dead RegStores produced by previous deadTrees pass
   { OMR::endGroup                                                                 }
   };

static const OptimizationStrategy cheapTacticalGlobalRegisterAllocatorOpts[] =
   {
   { OMR::redundantGotoElimination,              OMR::IfNotProfiling               }, // need to be run before global register allocator
   { OMR::tacticalGlobalRegisterAllocator,       OMR::IfEnabled                    },
   { OMR::endGroup                        }
   };

static const OptimizationStrategy JBcoldStrategyOpts[] =
   {
   { OMR::deadTreesElimination                                                     },
   { OMR::treeSimplification                                                       },
   { OMR::localCSE                                                                 },
   { OMR::basicBlockExtension                                                      },
   { OMR::cheapTacticalGlobalRegisterAllocatorGroup                                },
   };

static const OptimizationStrategy JBwarmStrategyOpts[] =
   {
   { OMR::deadTreesElimination                                                     },
   { OMR::inlining                                                                 },
   { OMR::treeSimplification                                                       },
   { OMR::localCSE                                                                 },
   { OMR::basicBlockOrdering                                                       }, // straighten goto's
   { OMR::globalCopyPropagation                                                    },
   { OMR::globalDeadStoreElimination,                OMR::IfMoreThanOneBlock       },
   { OMR::deadTreesElimination                                                     },
   { OMR::treeSimplification                                                       },
   { OMR::basicBlockHoisting                                                       },
   { OMR::treeSimplification                                                       },

   { OMR::globalValuePropagation,                    OMR::IfMoreThanOneBlock       },
   { OMR::localValuePropagation,                     OMR::IfOneBlock               },
   { OMR::localCSE                                                                 },
   { OMR::treeSimplification                                                       },
   { OMR::trivialDeadTreeRemoval,                    OMR::IfEnabled                },

   { OMR::basicBlockOrdering,                        OMR::IfLoops                  }, // clean up block order for loop canonicalization, if it will run
   { OMR::loopCanonicalization,                      OMR::IfLoops                  }, // canonicalization must run before inductionVariableAnalysis else indvar data gets messed up
   { OMR::inductionVariableAnalysis,                 OMR::IfLoops                  }, // needed for loop unroller
   { OMR::generalLoopUnroller,                       OMR::IfLoops                  },
   { OMR::basicBlockExtension,                       OMR::MarkLastRun              }, // clean up order and extend blocks now
   { OMR::treeSimplification                                                       },
   { OMR::localCSE                                                                 },
   { OMR::treeSimplification,                        OMR::IfEnabled                },
   { OMR::trivialDeadTreeRemoval,                    OMR::IfEnabled                },
   { OMR::cheapTacticalGlobalRegisterAllocatorGroup                                },
   { OMR::globalDeadStoreGroup,                                                    },
   { OMR::rematerialization                                                        },
   { OMR::deadTreesElimination,                      OMR::IfEnabled                }, // remove dead anchors created by check/store removal
   { OMR::deadTreesElimination,                      OMR::IfEnabled                }, // remove dead RegStores produced by previous deadTrees pass
   { OMR::regDepCopyRemoval                                                        },

   { OMR::endOpts                                                                  },
   };


namespace JitBuilder
{

Optimizer::Optimizer(TR::Compilation *comp, TR::ResolvedMethodSymbol *methodSymbol, bool isIlGen,
      const OptimizationStrategy *strategy, uint16_t VNType)
   : OMR::Optimizer(comp, methodSymbol, isIlGen, strategy, VNType)
   {
   _opts[OMR::isolatedStoreElimination] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_IsolatedStoreElimination::create, OMR::isolatedStoreElimination);
   _opts[OMR::trivialStoreSinking] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_TrivialSinkStores::create, OMR::trivialStoreSinking);
   _opts[OMR::trivialDeadBlockRemover] =
      new (comp->allocator()) TR::OptimizationManager(self(), TR_TrivialDeadBlockRemover::create, OMR::trivialDeadBlockRemover);
   // NOTE: Please add new IBM optimizations here!

   // initialize additional IBM optimization groups

   _opts[OMR::isolatedStoreGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::isolatedStoreGroup, isolatedStoreOpts);
   _opts[OMR::cheapTacticalGlobalRegisterAllocatorGroup] =
      new (comp->allocator()) TR::OptimizationManager(self(), NULL, OMR::cheapTacticalGlobalRegisterAllocatorGroup, cheapTacticalGlobalRegisterAllocatorOpts);

   // NOTE: Please add new IBM optimization groups here!

   // turn requested on for optimizations/groups
   self()->setRequestOptimization(OMR::cheapTacticalGlobalRegisterAllocatorGroup, true);
   self()->setRequestOptimization(OMR::tacticalGlobalRegisterAllocatorGroup, true);
   self()->setRequestOptimization(OMR::tacticalGlobalRegisterAllocator, true);


   omrCompilationStrategies[noOpt] = JBwarmStrategyOpts;
   omrCompilationStrategies[cold]  = JBwarmStrategyOpts;
   omrCompilationStrategies[warm]  = JBwarmStrategyOpts;
   omrCompilationStrategies[hot]   = JBwarmStrategyOpts;

   }

inline
TR::Optimizer *Optimizer::self()
   {
   return (static_cast<TR::Optimizer *>(this));
   }

} // namespace JitBuilder
