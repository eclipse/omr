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

#ifndef LRA_INCL
#define LRA_INCL

#include <stdint.h>
#include "il/ILOpCodes.hpp"
#include "il/Node.hpp"
#include "infra/List.hpp"
#include "infra/TreeServices.hpp"
#include "optimizer/OptimizationManager.hpp"
#include "optimizer/LoopCanonicalizer.hpp"

class TR_ArrayLoop;
class TR_InductionVariable;
class TR_ParentOfChildNode;
class TR_RegionStructure;
namespace TR { class Block; }
namespace TR { class CFGEdge; }
namespace TR { class CodeGenerator; }
namespace TR { class Compilation; }
namespace TR { class Optimization; }
namespace TR { class Symbol; }
namespace TR { class SymbolReference; }

//
// The intent of the loop reduction phase will be to transform simple loops
// that can be run as individual instructions on CISC platforms, or
// a hand-crafted set of instructions on non-CISC platforms.
// Loop examples include:
//  Copy byte into a portion of an array (C memset) (clearing with value 0 special)
//  Copy a portion of an array into a portion of an array (C memcpy/memmove)
//  Translate a portion of an array via a translation table (S/390 TRT/TROT/TRTO/TRTT/TROO instructions)
//

class TR_ArrayLoop;

class TR_LRAddressTree : public TR_AddressTree
   {
public:
   TR_LRAddressTree(TR::Compilation *, TR_InductionVariable * indVar);

   bool checkIndVarStore(TR::Node * indVarNode);
   bool checkAiadd(TR::Node * aiaddNode, int32_t elementSize);
   void updateAiaddSubTree(TR_ParentOfChildNode * indVarNode, TR_ArrayLoop* loop);

   TR::Node * updateMultiply(TR_ParentOfChildNode * multiplyNode);

   TR_InductionVariable * getIndVar() { return _indVar; }

   TR::SymbolReference * getIndVarSymRef() { return _indVarSymRef; }

   int32_t getIncrement() { return _increment; }

   void setIncrement(int32_t value) { _increment = value; }

   TR::Node * getIndVarLoad() { return _indVarLoad; }

   TR::SymbolReference * getMaterializedIndVarSymRef() { return _matIndVarSymRef; }
   void setMaterializedIndVarSymRef(TR::SymbolReference *symRef) { _matIndVarSymRef = symRef; }
protected:
   bool processBaseAndIndex(TR::Node* parent);

private:
   TR_InductionVariable * _indVar;
   TR::SymbolReference   * _indVarSymRef;
   TR::Node * _indVarLoad;
   int32_t _increment;
   TR::SymbolReference * _matIndVarSymRef;
   };

class TR_ArrayLoop
   {
public:
   TR_ArrayLoop(TR::Compilation *, TR_InductionVariable * indVar);
   TR_ArrayLoop(TR::Compilation *, TR_InductionVariable * firstIndVar, TR_InductionVariable * secondIndVar);

   TR::Compilation * comp() { return _comp; }

   bool checkLoopCmp(TR::Node * loopCmpNode, TR::Node * indVarNode, TR_InductionVariable * indVar);

   int32_t checkForPostIncrement(TR::Block *loopHeader, TR::Node *indVarStoreNode, TR::Node *loopCmpNode, TR::Symbol *ivSym);

   void findIndVarLoads(TR::Node *node, TR::Node *indVarStoreNode, bool &storeFound, List<TR::Node> *ivLoads, TR::Symbol *ivSym, vcount_t visitCount);


   TR::Node * updateIndVarStore(TR_ParentOfChildNode * indVarNode, TR::Node * indVarStoreNode, TR_LRAddressTree* tree, int32_t postIncrement = 0);

   TR::Node * getFinalNode() { return  _finalNode; }

   bool forwardLoop() { return _forwardLoop; }

   bool getAddInc() { return _addInc; }

   TR_LRAddressTree * getFirstAddress() { return &_firstAddress; }

   TR_LRAddressTree * getSecondAddress() { return &_secondAddress; }

   TR_LRAddressTree * getThirdAddress() { return &_thirdAddress; }

private:
   TR::Compilation * _comp;

   TR::Node * _finalNode;

   TR_LRAddressTree _firstAddress;
   TR_LRAddressTree _secondAddress;
   TR_LRAddressTree _thirdAddress;

   bool _addInc;
   bool _forwardLoop;
   };

class TR_Arrayset : public TR_ArrayLoop
   {
public:
   TR_Arrayset(TR::Compilation *, TR_InductionVariable * indVar);

   bool checkArrayStore(TR::Node * storeNode);

   TR_ParentOfChildNode * getMultiplyNode() { return getFirstAddress()->getMultiplyNode(); }

   TR_ParentOfChildNode * getIndVarNode() { return getFirstAddress()->getIndVarNode(); }

   TR_InductionVariable * getIndVar() { return getFirstAddress()->getIndVar(); }

   TR_LRAddressTree * getStoreAddress() { return getFirstAddress(); }
   };

class TR_Arraycopy : public TR_ArrayLoop
   {
public:
   TR_Arraycopy(TR::Compilation *, TR_InductionVariable * indVar);

   bool checkArrayStore(TR::Node * storeNode);

   TR_ParentOfChildNode * getStoreMultiplyNode() { return getFirstAddress()->getMultiplyNode(); }
   TR_ParentOfChildNode * getLoadMultiplyNode() { return (getSecondAddress()->getMultiplyNode()); }
   TR_ParentOfChildNode * getLoadIndVarNode() { return (getFirstAddress()->getIndVarNode()); }
   TR_ParentOfChildNode * getStoreIndVarNode() { return (getSecondAddress()->getIndVarNode()); }

   int32_t getCopySize() { return _copySize; }

   TR_LRAddressTree * getStoreAddress() { return getFirstAddress(); }
   TR_LRAddressTree * getLoadAddress() { return getSecondAddress(); }

   TR::Node * getStoreNode() { return _storeNode; }

   bool hasWriteBarrier() { return _hasWriteBarrier; }

private:
   int32_t _copySize;
   TR::Node * _storeNode;
   bool _hasWriteBarrier;
   };

class TR_ByteToCharArraycopy : public TR_ArrayLoop
   {
public:
   TR_ByteToCharArraycopy(TR::Compilation *, TR_InductionVariable * firstIndVar, TR_InductionVariable * secondIndVar, bool bigEndian);

   bool checkArrayStore(TR::Node * storeNode);
   bool checkByteLoads(TR::Node * loadNodes);

   TR_LRAddressTree * getStoreAddress()    { return getFirstAddress(); }
   TR_LRAddressTree * getHighLoadAddress() { return getSecondAddress(); }
   TR_LRAddressTree * getLowLoadAddress()  { return getThirdAddress(); }

   TR_ParentOfChildNode * getStoreMultiplyNode()    { return (getStoreAddress()->getMultiplyNode()); }
   TR_ParentOfChildNode * getStoreIndVarNode()      { return (getStoreAddress()->getIndVarNode()); }
   TR_ParentOfChildNode * getHighLoadMultiplyNode() { return (getHighLoadAddress()->getMultiplyNode()); }
   TR_ParentOfChildNode * getHighLoadIndVarNode()   { return (getHighLoadAddress()->getIndVarNode()); }
   TR_ParentOfChildNode * getLowLoadMultiplyNode()  { return (getLowLoadAddress()->getMultiplyNode()); }
   TR_ParentOfChildNode * getLowLoadIndVarNode()    { return (getLowLoadAddress()->getIndVarNode()); }

   private:
     bool _bigEndian;

   };
class TR_CharToByteArraycopy : public TR_ArrayLoop
   {
public:
   TR_CharToByteArraycopy(TR::Compilation *, TR_InductionVariable * firstIndVar, TR_InductionVariable * secondIndVar, bool isBigEndian);

   bool checkArrayStores(TR::Node * highStoreNode, TR::Node * lowStoreNode);

   TR_ParentOfChildNode * getLoadMultiplyNode() { return (getFirstAddress()->getMultiplyNode()); }
   TR_ParentOfChildNode * getLoadIndVarNode() { return (getFirstAddress()->getIndVarNode()); }

   TR_LRAddressTree * getHighStoreAddress() { return getSecondAddress(); }
   TR_LRAddressTree * getLowStoreAddress() { return getThirdAddress(); }
   TR_LRAddressTree * getLoadAddress() { return getFirstAddress(); }

   TR_ParentOfChildNode * getHighStoreMultiplyNode() { return (getHighStoreAddress()->getMultiplyNode()); }
   TR_ParentOfChildNode * getHighStoreIndVarNode() { return (getHighStoreAddress()->getIndVarNode()); }
   TR_ParentOfChildNode * getLowStoreMultiplyNode() { return (getLowStoreAddress()->getMultiplyNode()); }
   TR_ParentOfChildNode * getLowStoreIndVarNode() { return (getLowStoreAddress()->getIndVarNode()); }

   private:
     bool _bigEndian;

   };


class TR_Arraycmp : public TR_ArrayLoop
   {
public:
   TR_Arraycmp(TR::Compilation *, TR_InductionVariable * indVar);

   bool checkGoto(TR::Block * gotoBlock, TR::Node * gotoNode, TR::Node * finalNode);
   bool checkElementCompare(TR::Node * compareNode);

   TR::Block * targetOfGotoBlock() { return _targetOfGotoBlock; }

   TR_ParentOfChildNode * getFirstMultiplyNode() { return (getFirstAddress()->getMultiplyNode()); }
   TR_ParentOfChildNode * getSecondMultiplyNode() { return (getSecondAddress()->getMultiplyNode()); }
   TR_ParentOfChildNode * getSecondIndVarNode() { return (getFirstAddress()->getIndVarNode()); }
   TR_ParentOfChildNode * getFirstIndVarNode() { return (getSecondAddress()->getIndVarNode()); }

   TR::Node *getFirstLoad()    { return _firstLoad; }
   TR::Node *getSecondLoad()   { return _secondLoad; }

   void setFirstLoad(TR::Node * n)    { _firstLoad = n; }
   void setSecondLoad(TR::Node * n)   { _secondLoad = n; }

private:
   TR::Block * _targetOfGotoBlock;
   TR::Node *_firstLoad;
   TR::Node *_secondLoad;
   };

class TR_Arraytranslate : public TR_ArrayLoop
   {
public:
   TR_Arraytranslate(TR::Compilation *, TR_InductionVariable * indVar, bool hasBranch, bool hasBreak);

   bool checkGoto(TR::Block * gotoBlock, TR::Node * gotoNode, TR::Block * subsequentBlock);
   bool checkLoad(TR::Node * loadNode);
   bool checkStore(TR::Node * storeNode);
   bool checkBreak(TR::Block * breakBlock, TR::Node * breakNode, TR::Block * nextBlock);
   bool checkMatIndVarStore(TR::Node * matIndVarStoreNode, TR::Node * indVarStoreNode);

   TR::Node * getMulChild(TR::Node * child);

   TR::Node * getTableNode();

   bool hasBranch() { return _hasBranch; }
   bool hasBreak()  { return _hasBreak; }
   bool compilerGeneratedTable() { return _compilerGeneratedTable; }

   int32_t getTermValue();

   TR::Node * getInputNode() { return _inputNode; }
   TR::Node * getOutputNode() { return _outputNode; }
   TR::Node * getTermCharNode();

   bool tableBackedByRawStorage() { return _usesRawStorage; }
   bool getByteInput() { return _byteInput; }
   bool getByteOutput() { return _byteOutput; }

   TR_LRAddressTree * getStoreAddress() { return getFirstAddress(); }
   TR_LRAddressTree * getLoadAddress() { return getSecondAddress(); }

   TR::Node * getResultUnconvertedNode() { return _resultUnconvertedNode; }
   TR::Node * getResultNode() { return _resultNode; }
private:
   TR::Node * _tableNode;
   TR::Node * _resultNode;
   TR::Node * _resultUnconvertedNode;
   TR::Node * _inputNode;
   TR::Node * _outputNode;
   TR::Node * _termCharNode;
   TR::ILOpCodes     _compareOp;

   bool _byteInput;
   bool _byteOutput;
   bool _usesRawStorage;
   bool _compilerGeneratedTable;
   bool _hasBranch;
   bool _hasBreak;
   };

class TR_ArraytranslateAndTest : public TR_ArrayLoop
   {
public:
   TR_ArraytranslateAndTest(TR::Compilation *, TR_InductionVariable * indVar);

   bool checkLoad(TR::Block * loadBlock, TR::Node * loadNode);
   bool checkFrequency(TR::CodeGenerator * cg, TR::Block * loadBlock, TR::Node * loadNode);

   TR::Node * getInputNode() { return _inputNode; }
   TR::Node * getTermCharNode() { return _termCharNode; }

   TR_LRAddressTree * getStoreAddress() { return getFirstAddress(); }

   TR_ParentOfChildNode * getIndVarNode() { return getFirstAddress()->getIndVarNode(); }

private:
   TR::Node * _inputNode;
   TR::Node * _termCharNode;
   };


/**
 * Class TR_LoopReducer
 * ====================
 *
 * The loop reduction optimization can reduce loops that match a particular 
 * pattern to a tree consisting of a helper-method call-out, taking a series 
 * of parameters. To date, loop reduction pattern matching catches 
 * array-compare, array-init, array-translate, and array-copy loops. 
 * array-init and array-translate are very close to loops one would code 
 * for the ANSI C functions memcmp() and memset(). Code generator support 
 * is required for each platform wanting to exploit array-compare, array-init, 
 * and array-translate nodes and is similar to array-copy. Only S/390 
 * supports code generation for array-init, array-compare, array-translate 
 * nodes. array-copy reductions are done for all platforms that support 
 * primitive array-copy (which is currently all platforms). At present, there 
 * should not be issues for enabling S390 only nodes on other platfforms, 
 * other than writing the code generator support for the new nodes.
 */

class TR_LoopReducer : public TR_LoopTransformer
   {
public:

   TR_LoopReducer(TR::OptimizationManager *manager);
   static TR::Optimization *create(TR::OptimizationManager *manager)
      {
      return new (manager->allocator()) TR_LoopReducer(manager);
      }

   virtual int32_t perform();

   virtual TR_LoopReducer * asLoopReducer() { return this; }
   virtual const char * optDetailString() const throw();

private:
   void reduceNaturalLoop(TR_RegionStructure * whileLoop);
   void removeSelfEdge(TR::CFGEdgeList &succList, int32_t selfNumber);
   int addBlock(TR::Block * newBlock, TR::Block ** blocks, int numBlock, const int maxNumBlock);
   int addRegionBlocks(TR_RegionStructure * region, TR::Block ** blocks, int numBlock, const int maxNumBlock);
   bool constrainedIndVar(TR_InductionVariable * indVar);

   TR::ILOpCodes convertIf(TR::ILOpCodes ifCmp);

   bool generateArraytranslateAndTest(TR_RegionStructure * whileLoop, TR_InductionVariable * indVar,
      TR::Block * firstBlock, TR::Block * secondBlock);
   bool generateArraycopy(TR_InductionVariable * indVar, TR::Block * loopHeader);
   bool generateByteToCharArraycopy(TR_InductionVariable * firstIndVar, TR_InductionVariable * secondIndVar, TR::Block * loopHeader);
   bool generateCharToByteArraycopy(TR_InductionVariable * firstIndVar, TR_InductionVariable * secondIndVar, TR::Block * loopHeader);

   bool generateArraytranslate(TR_RegionStructure * whileLoop, TR_InductionVariable * indVar,
      TR::Block * firstBlock, TR::Block * secondBlock, TR::Block * thirdBlock, TR::Block * fourthBlock);
   bool generateArrayset(TR_InductionVariable * indVar, TR::Block * loopHeader);
   bool generateArraycmp(TR_RegionStructure * whileLoop, TR_InductionVariable * indVar, TR::Block * compareBlock,
      TR::Block * incrementBlock);
   bool blockInVersionedLoop(List<TR::CFGEdge> succList, TR::Block * block);

   bool mayNeedGlobalDeadStoreElimination(TR::Block * storeBlock, TR::Block * compareBlock);
   bool replaceInductionVariable(TR::Node *parent, TR::Node *node, int32_t, int32_t, TR::Node *, vcount_t);
   };

#endif
