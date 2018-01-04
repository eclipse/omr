###############################################################################
# Copyright (c) 2017, 2017 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at http://eclipse.org/legal/epl-2.0
# or the Apache License, Version 2.0 which accompanies this distribution
# and is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following Secondary
# Licenses when the conditions for such availability set forth in the
# Eclipse Public License, v. 2.0 are satisfied: GNU General Public License,
# version 2 with the GNU Classpath Exception [1] and GNU General Public
# License, version 2 with the OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
#############################################################################

compiler_library(optimizer
	${CMAKE_CURRENT_SOURCE_DIR}/AsyncCheckInsertion.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BackwardBitVectorAnalysis.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BackwardIntersectionBitVectorAnalysis.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BackwardUnionBitVectorAnalysis.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/BitVectorAnalysis.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/CatchBlockRemover.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/CFGSimplifier.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/CompactLocals.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/CopyPropagation.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/DataFlowAnalysis.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/DeadStoreElimination.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/DeadTreesElimination.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/DebuggingCounters.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Delayedness.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Dominators.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/DominatorVerifier.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/DominatorsChk.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Earliestness.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ExpressionsSimplification.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/FieldPrivatizer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/GeneralLoopUnroller.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/GlobalAnticipatability.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/GlobalRegisterAllocator.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Inliner.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RematTools.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/InductionVariable.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/IntersectionBitVectorAnalysis.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Isolatedness.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/IsolatedStoreElimination.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Latestness.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LiveOnAllPaths.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LiveVariableInformation.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Liveness.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LoadExtensions.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LocalAnalysis.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LocalAnticipatability.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LocalLiveRangeReducer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LocalReordering.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LocalTransparency.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LoopCanonicalizer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LoopReducer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LoopReplicator.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LoopVersioner.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRLocalCSE.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LocalDeadStoreElimination.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/LocalOpts.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMROptimization.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMROptimizationManager.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRTransformUtil.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMROptimizer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OrderBlocks.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OSRDefAnalysis.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/PartialRedundancy.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/PreExistence.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/PrefetchInsertion.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Reachability.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ReachingDefinitions.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRRecognizedCallTransformer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RedundantAsyncCheckRemoval.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RegisterAnticipatability.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RegisterAvailability.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RegisterCandidate.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ShrinkWrapping.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRSimplifier.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRSimplifierHelpers.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRSimplifierHandlers.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/StructuralAnalysis.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Structure.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/TranslateTable.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/UnionBitVectorAnalysis.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/UseDefInfo.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ValueNumberInfo.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/VirtualGuardCoalescer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/VirtualGuardHeadMerger.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/RegDepCopyRemoval.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ReorderIndexExpr.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/SinkStores.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/StripMiner.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/VPConstraint.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/VPHandlers.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/VPHandlersCommon.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/OMRValuePropagation.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/ValuePropagationCommon.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/TrivialDeadBlockRemover.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/FEInliner.cpp
)
