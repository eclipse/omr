###############################################################################
# Copyright (c) 2017, 2021 IBM Corp. and others
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
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
#############################################################################

compiler_library(optimizer
	${CMAKE_CURRENT_LIST_DIR}/AsyncCheckInsertion.cpp
	${CMAKE_CURRENT_LIST_DIR}/BackwardBitVectorAnalysis.cpp
	${CMAKE_CURRENT_LIST_DIR}/BackwardIntersectionBitVectorAnalysis.cpp
	${CMAKE_CURRENT_LIST_DIR}/BackwardUnionBitVectorAnalysis.cpp
	${CMAKE_CURRENT_LIST_DIR}/BitVectorAnalysis.cpp
	${CMAKE_CURRENT_LIST_DIR}/CatchBlockRemover.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRCFGSimplifier.cpp
	${CMAKE_CURRENT_LIST_DIR}/CompactLocals.cpp
	${CMAKE_CURRENT_LIST_DIR}/CopyPropagation.cpp
	${CMAKE_CURRENT_LIST_DIR}/DataFlowAnalysis.cpp
	${CMAKE_CURRENT_LIST_DIR}/DeadStoreElimination.cpp
	${CMAKE_CURRENT_LIST_DIR}/DeadTreesElimination.cpp
	${CMAKE_CURRENT_LIST_DIR}/DebuggingCounters.cpp
	${CMAKE_CURRENT_LIST_DIR}/Delayedness.cpp
	${CMAKE_CURRENT_LIST_DIR}/Dominators.cpp
	${CMAKE_CURRENT_LIST_DIR}/DominatorVerifier.cpp
	${CMAKE_CURRENT_LIST_DIR}/DominatorsChk.cpp
	${CMAKE_CURRENT_LIST_DIR}/Earliestness.cpp
	${CMAKE_CURRENT_LIST_DIR}/ExpressionsSimplification.cpp
	${CMAKE_CURRENT_LIST_DIR}/FieldPrivatizer.cpp
	${CMAKE_CURRENT_LIST_DIR}/GeneralLoopUnroller.cpp
	${CMAKE_CURRENT_LIST_DIR}/GlobalAnticipatability.cpp
	${CMAKE_CURRENT_LIST_DIR}/GlobalRegisterAllocator.cpp
	${CMAKE_CURRENT_LIST_DIR}/Inliner.cpp
	${CMAKE_CURRENT_LIST_DIR}/RematTools.cpp
	${CMAKE_CURRENT_LIST_DIR}/InductionVariable.cpp
	${CMAKE_CURRENT_LIST_DIR}/IntersectionBitVectorAnalysis.cpp
	${CMAKE_CURRENT_LIST_DIR}/Isolatedness.cpp
	${CMAKE_CURRENT_LIST_DIR}/IsolatedStoreElimination.cpp
	${CMAKE_CURRENT_LIST_DIR}/Latestness.cpp
	${CMAKE_CURRENT_LIST_DIR}/LiveOnAllPaths.cpp
	${CMAKE_CURRENT_LIST_DIR}/LiveVariableInformation.cpp
	${CMAKE_CURRENT_LIST_DIR}/Liveness.cpp
	${CMAKE_CURRENT_LIST_DIR}/LoadExtensions.cpp
	${CMAKE_CURRENT_LIST_DIR}/LocalAnalysis.cpp
	${CMAKE_CURRENT_LIST_DIR}/LocalAnticipatability.cpp
	${CMAKE_CURRENT_LIST_DIR}/LocalLiveRangeReducer.cpp
	${CMAKE_CURRENT_LIST_DIR}/LocalReordering.cpp
	${CMAKE_CURRENT_LIST_DIR}/LocalTransparency.cpp
	${CMAKE_CURRENT_LIST_DIR}/LoopCanonicalizer.cpp
	${CMAKE_CURRENT_LIST_DIR}/LoopReducer.cpp
	${CMAKE_CURRENT_LIST_DIR}/LoopReplicator.cpp
	${CMAKE_CURRENT_LIST_DIR}/LoopVersioner.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRLocalCSE.cpp
	${CMAKE_CURRENT_LIST_DIR}/LocalDeadStoreElimination.cpp
	${CMAKE_CURRENT_LIST_DIR}/LocalOpts.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMROptimization.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMROptimizationManager.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRTransformUtil.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMROptimizer.cpp
	${CMAKE_CURRENT_LIST_DIR}/OrderBlocks.cpp
	${CMAKE_CURRENT_LIST_DIR}/OSRDefAnalysis.cpp
	${CMAKE_CURRENT_LIST_DIR}/PartialRedundancy.cpp
	${CMAKE_CURRENT_LIST_DIR}/PreExistence.cpp
	${CMAKE_CURRENT_LIST_DIR}/Reachability.cpp
	${CMAKE_CURRENT_LIST_DIR}/ReachingDefinitions.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRRecognizedCallTransformer.cpp
	${CMAKE_CURRENT_LIST_DIR}/RedundantAsyncCheckRemoval.cpp
	${CMAKE_CURRENT_LIST_DIR}/RegisterCandidate.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRSimplifier.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRSimplifierHelpers.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRSimplifierHandlers.cpp
	${CMAKE_CURRENT_LIST_DIR}/StructuralAnalysis.cpp
	${CMAKE_CURRENT_LIST_DIR}/Structure.cpp
	${CMAKE_CURRENT_LIST_DIR}/SwitchAnalyzer.cpp
	${CMAKE_CURRENT_LIST_DIR}/TranslateTable.cpp
	${CMAKE_CURRENT_LIST_DIR}/UnionBitVectorAnalysis.cpp
	${CMAKE_CURRENT_LIST_DIR}/UseDefInfo.cpp
	${CMAKE_CURRENT_LIST_DIR}/ValueNumberInfo.cpp
	${CMAKE_CURRENT_LIST_DIR}/VirtualGuardCoalescer.cpp
	${CMAKE_CURRENT_LIST_DIR}/VirtualGuardHeadMerger.cpp
	${CMAKE_CURRENT_LIST_DIR}/RegDepCopyRemoval.cpp
	${CMAKE_CURRENT_LIST_DIR}/ReorderIndexExpr.cpp
	${CMAKE_CURRENT_LIST_DIR}/SinkStores.cpp
	${CMAKE_CURRENT_LIST_DIR}/StripMiner.cpp
	${CMAKE_CURRENT_LIST_DIR}/VPConstraint.cpp
	${CMAKE_CURRENT_LIST_DIR}/VPHandlers.cpp
	${CMAKE_CURRENT_LIST_DIR}/VPHandlersCommon.cpp
	${CMAKE_CURRENT_LIST_DIR}/OMRValuePropagation.cpp
	${CMAKE_CURRENT_LIST_DIR}/ValuePropagationCommon.cpp
	${CMAKE_CURRENT_LIST_DIR}/TrivialDeadBlockRemover.cpp
	${CMAKE_CURRENT_LIST_DIR}/FEInliner.cpp
	${CMAKE_CURRENT_LIST_DIR}/abstractinterpreter/AbsValue.cpp
	${CMAKE_CURRENT_LIST_DIR}/abstractinterpreter/AbsOpStack.cpp
	${CMAKE_CURRENT_LIST_DIR}/abstractinterpreter/AbsOpArray.cpp
)
