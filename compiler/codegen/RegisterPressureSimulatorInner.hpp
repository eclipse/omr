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

#ifndef REGISTER_PRESSURE_SIMULATOR_INNER
#define REGISTER_PRESSURE_SIMULATOR_INNER

   /*
    * The register pressure simulator is included within the CodeGenerator
    * class interface.
    */

   bool hasRegisterPressureInfo(){ return (_blockRegisterPressureCache != NULL); }
   void computeSimulatedSpilledRegs(TR_BitVector *spilledRegisters, int32_t blockNum, TR::SymbolReference *candidate);

   bool traceSimulateTreeEvaluation();
   bool terseSimulateTreeEvaluation();

   struct TR_RegisterPressureState;
   struct TR_RegisterPressureSummary;
   struct TR_SimulatedNodeState;
   struct TR_SimulatedMemoryReference;
   friend struct TR_RegisterPressureState;
   friend struct TR_RegisterPressureSummary;
   friend struct TR_SimulatedNodeState;
   friend struct TR_SimulatedMemoryReference;
   struct TR_RegisterPressureState
      {
      TR::Block     *_currentBlock;
      TR::TreeTop   *_currentTreeTop;
      TR_RegisterCandidate *_candidate;       // NULL when candidate is to be ignored
      TR_BitVector &_alreadyAssignedOnEntry;  // One bit per symbol reference
      TR_BitVector &_alreadyAssignedOnExit;   // (ditto)
      TR_LinkHead<TR_RegisterCandidate> *_candidatesAlreadyAssigned;

      int32_t       _gprPressure;             // (int32_t so increments will match decrements even with enormous register pressure)
      int32_t       _fprPressure;             // (ditto)
      int32_t       _vrfPressure;             // (ditto)
      int32_t       _gprPreviousPressure;     // Pressure immediately before the latest call to simulateNodeEvaluation
      int32_t       _fprPreviousPressure;     // (ditto)
      int32_t       _vrfPreviousPressure;     // (ditto)
      int32_t       _gprLimit;                // Number of GPRs the platform has available for GRA
      int32_t       _fprLimit;                // Number of FPRs the platform has available for GRA
      int32_t       _vrfLimit;                // Number of VRFs the platform has available for GRA
      bool          _candidateIsLiveOnEntry;  // TODO: Why do we need this?  Why doesn't it work when I check the candidate's getBlocksLiveOnEntry?
      bool          _pressureRiskFromStart;   // True if candidate was live on entry and would not be in a register without GRA
      uint32_t      _pressureRiskUntilEnd;    // Nonzero if a value has been stored to the candidate, the value would not be in a register without GRA, and the candidate is live on an exit we haven't reached yet
      uint32_t      _numLiveCandidateLoads;   // Number of isCandidateLoad nodes that have been evaluated, but whose refcount is not yet zero
      uint32_t      _rematNestDepth;          // How many nodes between current node and current treetop will be duplicated by rematerialization
      vcount_t      _visitCountForInit;       // Used to indicate that a node's entry in the node state array has been initialized

      uint32_t                     _memrefNestDepth; // Recursion depth of simulateMemoryReference; used for diagnostics
      TR_SimulatedMemoryReference *_currentMemref;

      TR_RegisterPressureState(TR_RegisterCandidate *candidate, bool candidateIsLiveOnEntry, TR_BitVector &alreadyAssignedOnEntry, TR_BitVector &alreadyAssignedOnExit, TR_LinkHead<TR_RegisterCandidate> *candidatesAlreadyAssigned, uint32_t gprLimit, uint32_t fprLimit, uint32_t vrfLimit, vcount_t visitCountForInit):
         _currentBlock(NULL),
         _currentTreeTop(NULL),
         _candidate(candidate),
         _alreadyAssignedOnEntry(alreadyAssignedOnEntry),
         _alreadyAssignedOnExit(alreadyAssignedOnExit),
         _candidatesAlreadyAssigned(candidatesAlreadyAssigned),
         _gprPressure(0),
         _fprPressure(0),
         _vrfPressure(0),
         _gprLimit(gprLimit),
         _fprLimit(fprLimit),
         _vrfLimit(vrfLimit),
         _candidateIsLiveOnEntry(candidateIsLiveOnEntry),
         _pressureRiskFromStart(candidateIsLiveOnEntry),
         _pressureRiskUntilEnd(0),
         _numLiveCandidateLoads(0),
         _visitCountForInit(visitCountForInit),
         _memrefNestDepth(0),
         _rematNestDepth(0),
         _currentMemref(0)
         {}
      TR_RegisterPressureState(const TR_RegisterPressureState &other, TR_RegisterCandidate *candidate, bool candidateIsLiveOnEntry, vcount_t visitCountForInit):
         _currentBlock              (other._currentBlock),
         _currentTreeTop            (other._currentTreeTop),
         _candidate                 (candidate),
         _visitCountForInit         (visitCountForInit),
         _alreadyAssignedOnEntry    (other._alreadyAssignedOnEntry),
         _alreadyAssignedOnExit     (other._alreadyAssignedOnExit),
         _candidatesAlreadyAssigned (other._candidatesAlreadyAssigned),
         _gprPressure               (other._gprPressure),
         _fprPressure               (other._fprPressure),
         _vrfPressure               (other._vrfPressure),
         _candidateIsLiveOnEntry    (candidateIsLiveOnEntry),
         _pressureRiskFromStart     (candidateIsLiveOnEntry),
         _pressureRiskUntilEnd      (0),
         _gprLimit                  (other._gprLimit),
         _fprLimit                  (other._fprLimit),
         _vrfLimit                  (other._vrfLimit),
         _numLiveCandidateLoads     (other._numLiveCandidateLoads),
         _memrefNestDepth           (other._memrefNestDepth),
         _rematNestDepth            (other._rematNestDepth),
         _currentMemref             (other._currentMemref)
         {}

       TR::SymbolReference *getCandidateSymRef();

       bool candidateIsLiveOnEntry(){ return _candidateIsLiveOnEntry; }

       // this method is used during calculation of actual register
       // pressure, based on instructions
       void addVirtualRegister(TR::Register *reg);
       void removeVirtualRegister(TR::Register *reg);

      // This indicates that we have already prepared this node for the simulation process.
      // It's an internal bookkeeping thing.
      //
      bool isInitialized(TR::Node *node);

      // This indicates that simulation must not proceed because it has reached
      // an unusual state that we don't want to cope with (such as register
      // pressure so high that it would overflow our counters).
      //
      bool mustAbort()
         {
         return _gprPressure >= TR_RegisterPressureSummary::PRESSURE_LIMIT
             || _fprPressure >= TR_RegisterPressureSummary::PRESSURE_LIMIT
             || _vrfPressure >= TR_RegisterPressureSummary::PRESSURE_LIMIT;
         }

      bool pressureIsAtRisk()       { return _pressureRiskFromStart || (_pressureRiskUntilEnd > 0); }
      bool candidateIsLiveAfterGRA(){ return pressureIsAtRisk() || (_numLiveCandidateLoads > 0); }

      void updateRegisterPressure(TR::Symbol *symbol);

      };

   struct TR_RegisterPressureSummary
      {
      // Note: Keep this struct very small so they are cheap to store.
      // Don't put anything in here that is only needed during the simulation
      // process itself, because that is what TR_RegisterPressureState is for.

      TR_ALLOC(TR_Memory::CodeGenerator)

      uint32_t _gprPressure:8;
      uint32_t _fprPressure:8;
      uint32_t _vrfPressure:8;

      // PRESSURE_LIMIT must be no more than 255 minus max regs that can be
      // used by a single node.  This simplifies things by ensuring that
      // checking this limit before each node is sufficient to avoid catastrophe.
      //
      enum { PRESSURE_LIMIT = 253 };

      // This mask has a bit set for the linkage convention of each call node
      // in the block.
      //
      // NOTE: This is somewhat of a hack intended to keep this struct small.
      // Ideally, we would replace _linkageConventionMask and _spillMask with a
      // TR_BitVector of individual registers, but that would make this struct
      // much larger.
      //
      uint32_t _linkageConventionMask:TR_NumLinkages;

      bool isLinkagePresent (TR_LinkageConventions lc){ return 0 != (_linkageConventionMask & (1<<lc)); }
      void setLinkagePresent(TR_LinkageConventions lc, TR::CodeGenerator *cg);

      // A bit is set in this mask for each kind of register is at risk of
      // being spilled if assigned to a global reg.  Hence, the spill state is
      // conservative (that is, isSpilled could actually be called
      // mayBeSpilled).
      //
      uint32_t _spillMask:TR_numSpillKinds;

      bool isSpilled(TR_SpillKinds kind){ return (_spillMask & (1 << kind)) != 0; }
      void spill    (TR_SpillKinds kind, TR::CodeGenerator *cg);

      void dumpSpillMask(TR::CodeGenerator *cg);
      void dumpLinkageConventionMask(TR::CodeGenerator *cg);

      bool gprIsStale(uint32_t threshold){ return !isSpilled(TR_gprSpill) && _gprPressure > threshold; }
      bool fprIsStale(uint32_t threshold){ return !isSpilled(TR_fprSpill) && _fprPressure > threshold; }
      bool vrfIsStale(uint32_t threshold){ return !isSpilled(TR_vrfSpill) && _vrfPressure > threshold; }

      void accumulate(TR_RegisterPressureSummary other)
         {
         accumulate(other._gprPressure, other._fprPressure, other._vrfPressure, other._linkageConventionMask, other._spillMask);
         }

      void accumulate(TR_RegisterPressureState *state, TR::CodeGenerator *cg, uint32_t gprTemps=0, uint32_t fprTemps=0, uint32_t vrfTemps=0);

      void reset(uint8_t gprPressure, uint8_t fprPressure, uint8_t vrfPressure)
         { _gprPressure = gprPressure; _fprPressure = fprPressure; _vrfPressure = vrfPressure; _linkageConventionMask = 0; _spillMask = 0; }

      TR_RegisterPressureSummary() // Initially stale
         :_gprPressure(PRESSURE_LIMIT),_fprPressure(PRESSURE_LIMIT),_vrfPressure(PRESSURE_LIMIT),_linkageConventionMask(0),_spillMask(0){}
      TR_RegisterPressureSummary(uint8_t gprPressure, uint8_t fprPressure, uint8_t vrfPressure)
         :_gprPressure(gprPressure),_fprPressure(fprPressure),_vrfPressure(vrfPressure),_linkageConventionMask(0),_spillMask(0){}

      private:

      void accumulate(uint32_t gprPressure, uint32_t fprPressure=0, uint32_t vrfPressure=0, uint32_t linkageConventionMask=0, uint32_t spillMask=0)
         {
         _linkageConventionMask  |= linkageConventionMask;
         _spillMask              |= spillMask;

         // Cap at 255 because that's effectively infinite anyway (until we hit
         // a platform with 255 registers) and it prevents the counter from
         // rolling over under situations with enormous register pressure.
         //
         _gprPressure = std::min<uint32_t>(std::max(_gprPressure, gprPressure), PRESSURE_LIMIT);
         _fprPressure = std::min<uint32_t>(std::max(_fprPressure, fprPressure), PRESSURE_LIMIT);
         _vrfPressure = std::min<uint32_t>(std::max(_vrfPressure, vrfPressure), PRESSURE_LIMIT);
/*
         TR_ASSERT(_gprPressure <= INT_MAX/2 && gprPressure <= INT_MAX/2, "gprPressure is getting too high!");
         TR_ASSERT(_fprPressure <= INT_MAX/2 && fprPressure <= INT_MAX/2, "fprPressure is getting too high!");
         TR_ASSERT(_vrfPressure <= INT_MAX/2 && vrfPressure <= INT_MAX/2, "vrfPressure is getting too high!");

         _gprPressure = std::max(_gprPressure, gprPressure);
         _fprPressure = std::max(_fprPressure, fprPressure);
         _vrfPressure = std::max(_vrfPressure, vrfPressure);
*/

         }

      };

   void computeSpilledRegsForAllPresentLinkages(TR_BitVector *spilledRegisters, TR_RegisterPressureSummary &summary);
   void setSpilledRegsForAllPresentLinkages(TR_BitVector *spilledRegisters, TR_RegisterPressureSummary &summary, TR_SpillKinds spillKind);

   bool isCandidateLoad(TR::Node *node,  TR::SymbolReference *candidate);

   bool isCandidateLoad(TR::Node *node,  TR_RegisterPressureState *state);

   bool isLoadAlreadyAssignedOnEntry(TR::Node *node, TR_RegisterPressureState *state);

   struct TR_SimulatedNodeState
      {
      TR_ALLOC(TR_Memory::CodeGenerator)

      TR::TreeTop *_keepLiveUntil;                 // Register pressure contribution should not be subtracted until this tree has been simulated
      uint16_t _isCausingPressureRiskUntilEnd:1;  // For nodes included in _pressureRiskUntilEnd

      uint16_t _liveGPRs:2;
      uint16_t _liveFPRs:2;
      uint16_t _liveVRFs:2;
      uint16_t _liveSSRs:2;
      uint16_t _liveARs:1;
      uint16_t _liveCandidateLoad:1;                 // Set for nodes included in state->_numLiveCandidateLoads
      uint16_t _childRefcountsHaveBeenDecremented:1; // Used for lazy recursivelyDecReferenceCount
      uint16_t _willBeRematerialized:1;

      uint8_t _height;  // Deepest descendent node is this many steps down from this node; leaf node has height=0

      bool hasRegister(){ return _liveGPRs + _liveFPRs + _liveVRFs + _liveSSRs + _liveARs>= 1; } // Equivalent to "node->getRegister() != NULL" during tree evaluation
      void initialize(){ TR_SimulatedNodeState initialState = {0}; *this = initialState; }
      void setHeight(uint32_t newHeight){ _height = std::min<uint32_t>(newHeight, 255); }
      };

   struct TR_SimulatedMemoryReference
      {
      TR_SimulatedMemoryReference(TR_Memory * m) : _undecrementedNodes(m), _numRegisters(0), _numConsolidatedRegisters(0) { }

      List<TR::Node> _undecrementedNodes;    // Nodes that need a simulateDecReferenceCount call
      int32_t       _numRegisters;
      int32_t       _numConsolidatedRegisters;

      enum { MAX_NUM_REGISTERS=2, MAX_STRIDE=8 }; // X86

      void add(TR::Node *node, TR_RegisterPressureState *state, TR::CodeGenerator *cg);
      void simulateDecNodeReferenceCounts(TR_RegisterPressureState *state, TR::CodeGenerator *cg);
      };

   uint8_t gprCount(TR::DataType type, int32_t size);
   uint8_t fprCount(TR::DataType type){ return type.isFloatingPoint() ? 1 : 0; }
   uint8_t vrfCount(TR::DataType type){ return type.isVector() ? 1 : 0; }

   void initializeRegisterPressureSimulator();

   void simulateBlockEvaluation       (TR::Block *block, TR_RegisterPressureState *state, TR_RegisterPressureSummary *summary);
   void simulateTreeEvaluation        (TR::Node  *node,  TR_RegisterPressureState *state, TR_RegisterPressureSummary *summary);
   void simulateSkippedTreeEvaluation (TR::Node  *node,  TR_RegisterPressureState *state, TR_RegisterPressureSummary *summary, char tagChar='s'); // 's' for "skipped"
   void simulateNodeInitialization    (TR::Node  *node,  TR_RegisterPressureState *state); // ok to call if node is already initialized
   void simulateNodeGoingLive         (TR::Node  *node,  TR_RegisterPressureState *state);
   void simulateDecReferenceCount     (TR::Node  *node,  TR_RegisterPressureState *state);
   void simulateNodeGoingDead         (TR::Node  *node,  TR_RegisterPressureState *state);
   bool nodeResultConsumesNoRegisters (TR::Node  *node,  TR_RegisterPressureState *state);
   void simulateMemoryReference(TR_SimulatedMemoryReference *memref, TR::Node *node, TR_RegisterPressureState *state, TR_RegisterPressureSummary *summary);
   void simulationPrePass(TR::TreeTop *tt, TR::Node  *node, TR_RegisterPressureState *state, TR_RegisterPressureSummary *summary);

   // Platform hooks
   //
   // These generally have decent default implementations.  Platforms can override
   // them, and their implementations can call these ones to handle the mundane cases.
   //
   virtual TR_BitVector *getGlobalRegisters(TR_SpillKinds kind, TR_LinkageConventions lc){ return NULL; /* There is really no suitable default */ }
   virtual uint8_t nodeResultGPRCount         (TR::Node  *node,  TR_RegisterPressureState *state);
   virtual uint8_t nodeResultFPRCount         (TR::Node  *node,  TR_RegisterPressureState *state);
   virtual uint8_t nodeResultSSRCount         (TR::Node  *node,  TR_RegisterPressureState *state);
   virtual uint8_t nodeResultARCount          (TR::Node  *node,  TR_RegisterPressureState *state);
   virtual uint8_t nodeResultVRFCount         (TR::Node  *node,  TR_RegisterPressureState *state);
   virtual bool    nodeCanBeFoldedIntoMemref  (TR::Node  *node,  TR_RegisterPressureState *state);
   virtual bool    nodeWillBeRematerialized   (TR::Node  *node,  TR_RegisterPressureState *state);
   virtual void    simulateNodeEvaluation     (TR::Node  *node,  TR_RegisterPressureState *state, TR_RegisterPressureSummary *summary);
   virtual void    getNumberOfTemporaryRegistersUsedByCall (TR::Node * node, TR_RegisterPressureState * state, uint32_t & numGPRTemporaries, uint32_t & numFPRTemporaries, uint32_t & numVRFTemporaries);

   virtual TR_RegisterPressureSummary *calculateRegisterPressure();

   // Summary cache; one summary per extended basic block
   //
   TR_RegisterPressureSummary *_blockRegisterPressureCache;
#if defined(CACHE_CANDIDATE_SPECIFIC_RESULTS)
   TR_RegisterPressureSummary *_blockAndCandidateRegisterPressureCache;
   int32_t                    *_blockAndCandidateRegisterPressureCacheTags;
#endif

   // Note state array: one per node, stack allocated (so invalid after GRA is finished)
   //
   TR_SimulatedNodeState *_simulatedNodeStates;
   TR_SimulatedNodeState &simulatedNodeState(TR::Node *node, TR_RegisterPressureState *state);
   TR_SimulatedNodeState &simulatedNodeState(TR::Node *node);


#endif
