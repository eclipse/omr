/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "ilgen.hpp"
#include "compiler_util.hpp"

#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

#include <unordered_map>
#include <string>

#define TraceEnabled    (comp()->getOption(TR_TraceILGen))
#define TraceIL(m, ...) {if (TraceEnabled) {traceMsg(comp(), m, ##__VA_ARGS__);}}

/**
 * @brief A table to map the string representation of opcode names to corresponding TR::OpCodes value
 */
class OpCodeTable : public TR::ILOpCode {
   public:
      explicit OpCodeTable(TR::ILOpCodes opcode) : TR::ILOpCode{opcode} {}
      explicit OpCodeTable(const std::string& name) : TR::ILOpCode{getOpCodeFromName(name)} {}

      /**
       * @brief Given an opcode name, returns the corresponding TR::OpCodes value
       */
      static TR::ILOpCodes getOpCodeFromName(const std::string& name) {
         auto opcode = _opcodeNameMap.find(name);
         if (opcode == _opcodeNameMap.cend()) {
            for (int i = TR::FirstOMROp; i< TR::NumIlOps; i++) {
               const auto p_opCode = static_cast<TR::ILOpCodes>(i);
               const auto& p = TR::ILOpCode::_opCodeProperties[p_opCode];
               if (name == p.name) {
                  _opcodeNameMap[name] = p.opcode;
                  return p.opcode;
               }
            }
            return TR::BadILOp;
         }
         else {
            return opcode->second;
         }
      }

   private:
      static std::unordered_map<std::string, TR::ILOpCodes> _opcodeNameMap;
};

std::unordered_map<std::string, TR::ILOpCodes> OpCodeTable::_opcodeNameMap;




/*
 * The general algorithm for generating a TR::Node from it's AST representation
 * is like this:
 *
 * 1. Allocate a new TR::Node instance, using the AST node's name
 *    to detemine the opcode
 * 2. Set any special values, flags, or properties on the newly created TR::Node
 *    based on the AST node arguments
 * 3. Recursively call this function to generate the child nodes and set them
 *    as children of the current TR::Node
 *
 * However, certain opcodes must be created using a special interface. For this
 * reason, special opcodes are detected using opcode properties.
 */
TR::Node* Tril::TRLangBuilder::toTRNode(const ASTNode* const tree) {
     TR::Node* node = NULL;

     auto childCount = tree->getChildCount();

     if (strcmp("@id", tree->getName()) == 0) {
         auto id = tree->getPositionalArg(0)->getValue()->getString();
         auto iter = _nodeMap.find(id);
         if (iter != _nodeMap.end()) {
             auto n = iter->second;
             TraceIL("Commoning node n%dn (%p) from ASTNode %p (@id \"%s\")\n", n->getGlobalIndex(), n, tree, id);
             return n;
         }
         else {
             TraceIL("Failed to find node for commoning (@id \"%s\")\n", id)
             return NULL;
         }
     }
     else if (strcmp("@common", tree->getName()) == 0) {
         auto id = tree->getArgByName("id")->getValue()->getString();
         TraceIL("WARNING: Using @common is deprecated. Please use (@id \"%s\") instead.\n", id);
         fprintf(stderr, "WARNING: Using @common is deprecated. Please use (@id \"%s\") instead.\n", id);
         auto iter = _nodeMap.find(id);
         if (iter != _nodeMap.end()) {
             auto n = iter->second;
             TraceIL("Commoning node n%dn (%p) from ASTNode %p (@id \"%s\")\n", n->getGlobalIndex(), n, tree, id);
             return n;
         }
         else {
             TraceIL("Failed to find node for commoning (@id \"%s\")\n", id)
             return NULL;
         }
     }

     auto opcode = OpCodeTable{tree->getName()};

     TraceIL("Creating %s from ASTNode %p\n", opcode.getName(), tree);
     if (opcode.isLoadConst()) {
        TraceIL("  is load const of ", "");
        node = TR::Node::create(opcode.getOpCodeValue(), childCount);
        auto value = tree->getPositionalArg(0)->getValue();

        // assume the constant to be loaded is the first argument of the AST node
        if (opcode.isIntegerOrAddress()) {
           auto v = value->get<int64_t>();
           node->set64bitIntegralValue(v);
           TraceIL("integral value %d\n", v);
        }
        else {
           switch (opcode.getType()) {
              case TR::Float:
                 node->setFloat(value->get<float>());
                 break;
              case TR::Double:
                 node->setDouble(value->get<double>());
                 break;
              default:
                 return NULL;
           }
           TraceIL("floating point value %f\n", value->getFloatingPoint());
        }
     }
     else if (opcode.isLoadDirect()) {
        TraceIL("  is direct load of ", "");

        // the name of the first argument tells us what kind of symref we're loading
        if (tree->getArgByName("parm") != NULL) {
             auto arg = tree->getArgByName("parm")->getValue()->get<int32_t>();
             TraceIL("parameter %d\n", arg);
             auto symref = symRefTab()->findOrCreateAutoSymbol(_methodSymbol, arg, opcode.getType() );
             node = TR::Node::createLoad(symref);
         }
         else if (tree->getArgByName("temp") != NULL) {
             const auto symName = tree->getArgByName("temp")->getValue()->getString();
             TraceIL("temporary %s\n", symName);
             auto symref = _symRefMap[symName];
             node = TR::Node::createLoad(symref);
         }
         else {
             // symref kind not recognized
             return NULL;
         }
     }
     else if (opcode.isStoreDirect()) {
        TraceIL("  is direct store of ", "");

        // the name of the first argument tells us what kind of symref we're storing to
        if (tree->getArgByName("temp") != NULL) {
            const auto symName = tree->getArgByName("temp")->getValue()->getString();
            TraceIL("temporary %s\n", symName);

            // check if a symref has already been created for the temp
            // and if not, create one
            if (_symRefMap.find(symName) == _symRefMap.end()) {
                _symRefMap[symName] = symRefTab()->createTemporary(methodSymbol(), opcode.getDataType());
            }

            auto symref =_symRefMap[symName];
            node = TR::Node::createWithSymRef(opcode.getOpCodeValue(), childCount, symref);
        }
        else {
            // symref kind not recognized
            return NULL;
        }
     }
     else if (opcode.isLoadIndirect() || opcode.isStoreIndirect()) {
         TraceIL("  is indirect store/load with ");
         // If not specified, offset will default to zero. 
         int32_t offset = 0; 
         if (tree->getArgByName("offset")) {
            offset = tree->getArgByName("offset")->getValue()->get<int32_t>();
         } else { 
            TraceIL(" (default) ");
         }
         TraceIL("offset %d ", offset);

         const auto name = tree->getName();
         auto compilation = TR::comp();
         TR::DataType type;  
         if (opcode.isVector()) { 
            // Vector types in TR IL are "typeless", insofar as they are
            // supposed to infer the vector type depending on the children.
            // Loads determine their data type based on the symref. However,
            // given that we are creating a symref here, we need a hint as to
            // what type of symref to create. So, vloadi and vstorei will take 
            // an extra argument "type" to annotate the type desired.
            if (tree->getArgByName("type") != NULL) { 
               auto nameoftype = tree->getArgByName("type")->getValue()->getString();
               type = getTRDataTypes(nameoftype); 
               TraceIL(" of vector type %s\n", nameoftype);
            } else { 
               return NULL;
            } 
         } else { 
            TraceIL("\n");
            type = opcode.getType();
         }
         TR::Symbol *sym = TR::Symbol::createNamedShadow(compilation->trHeapMemory(), type, TR::DataType::getSize(opcode.getType()), (char*)name);
         TR::SymbolReference *symref = new (compilation->trHeapMemory()) TR::SymbolReference(compilation->getSymRefTab(), sym, compilation->getMethodSymbol()->getResolvedMethodIndex(), -1);
         symref->setOffset(offset);
         node = TR::Node::createWithSymRef(opcode.getOpCodeValue(), childCount, symref);
     }
     else if (opcode.isIf()) {
         const auto targetName = tree->getArgByName("target")->getValue()->getString();
         auto targetId = _blockMap[targetName];
         auto targetEntry = _blocks[targetId]->getEntry();
         TraceIL("  is if with target block %d (%s, entry = %p", targetId, targetName, targetEntry);

         /* If jumps must be created using `TR::Node::createif()`, which expected
          * two child nodes to be given as argument. However, because children
          * are only processed at the end, we create a dummy `BadILOp` node and
          * pass it as both the first and second child. When the children are
          * eventually created, they will override the dummy.
          */
         auto c1 = TR::Node::create(TR::BadILOp);
         auto c2 = c1;
         TraceIL("  created temporary %s n%dn (%p)\n", c1->getOpCode().getName(), c1->getGlobalIndex(), c1);
         node = TR::Node::createif(opcode.getOpCodeValue(), c1, c2, targetEntry);
     }
     else if (opcode.isBranch()) {
         const auto targetName = tree->getArgByName("target")->getValue()->getString();
         auto targetId = _blockMap[targetName];
         auto targetEntry = _blocks[targetId]->getEntry();
         TraceIL("  is branch to target block %d (%s, entry = %p", targetId, targetName, targetEntry);
         node = TR::Node::create(opcode.getOpCodeValue(), childCount);
         node->setBranchDestination(targetEntry);
     }
     else if (opcode.isCall()) { 
        auto compilation = TR::comp();

        const auto addressArg = tree->getArgByName("address");
        if (addressArg == NULL) {
           TraceIL("  Found call without required address associated\n");
           return NULL;
        }

         TraceIL("  is call with target %s", addressArg);
        /* I don't want to extend the ASTValue type system to include pointers at this moment,
         * so for now, we do the reinterpret_cast to pointer type from long
         */
        const auto targetAddress = reinterpret_cast<void*>(addressArg->getValue()->get<long>()); 

        /* To generate a call, will create a ResolvedMethodSymbol, but we need to know the 
         * signature. The return type is intuitable from the call node, but the arguments 
         * must be provided explicitly, hence the args list, that mimics the args list of 
         * (method ...) 
         */
        const auto argList = parseArgTypes(tree); 

        auto argIlTypes = std::vector<TR::IlType*>{argList.size()};
        auto output = argIlTypes.begin(); 
        for (auto iter = argList.cbegin(); iter != argList.cend(); iter++, output++) {
           *output = _types.PrimitiveType(*iter); 
        }

        auto returnIlType = _types.PrimitiveType(opcode.getType());
         
        TR::ResolvedMethod* method = new (compilation->trHeapMemory()) TR::ResolvedMethod("file",
                                                                                          "line",
                                                                                          "name",
                                                                                          argIlTypes.size(),
                                                                                          argIlTypes.data(),
                                                                                          returnIlType,
                                                                                          targetAddress,
                                                                                          0);

        TR::SymbolReference *methodSymRef = symRefTab()->findOrCreateStaticMethodSymbol(JITTED_METHOD_INDEX, -1, method);
        
        /* Default linkage is always system, unless overridden */
        TR_LinkageConventions linkageConvention = TR_System; 

        /* Calls can have a customized linkage */
        const auto* linkageArg= tree->getArgByName("linkage");
        if (linkageArg != NULL) { 
           const auto* linkageString = linkageArg->getValue()->getString();
           linkageConvention = convertStringToLinkage(linkageString); 
           if (linkageConvention == TR_None) {
              TraceIL("  failed to find customized linkage %s, aborting parsing\n", linkageString);
              return NULL; 
           }
           TraceIL("  customizing linakge of call to %s (linkageConvention=%d)\n", linkageString, linkageConvention);
        }

        /* Set linkage explicitly */
        methodSymRef->getSymbol()->castToMethodSymbol()->setLinkage(linkageConvention);     
        node  = TR::Node::createWithSymRef(opcode.getOpCodeValue(), childCount, methodSymRef);
     }
     else {
        TraceIL("  unrecognized opcode; using default creation mechanism\n", "");
        node = TR::Node::create(opcode.getOpCodeValue(), childCount);
     }
     TraceIL("  node address %p\n", node);
     TraceIL("  node index n%dn\n", node->getGlobalIndex());

     auto nodeIdArg = tree->getArgByName("id");
     if (nodeIdArg != NULL) {
         auto id = nodeIdArg->getValue()->getString();
         _nodeMap[id] = node;
         TraceIL("  node ID %s\n", id);
     }

     // create a set child nodes
     const ASTNode* t = tree->getChildren();
     int i = 0;
     while (t) {
         auto child = toTRNode(t);
         if (child == NULL) 
            return NULL; 
         TraceIL("Setting n%dn (%p) as child %d of n%dn (%p)\n", child->getGlobalIndex(), child, i, node->getGlobalIndex(), node);
         node->setAndIncChild(i, child);
         t = t->next;
         ++i;
     }

     return node;
}

/*
 * The CFG is generated by doing a post-order walk of the AST and creating edges
 * whenever opcodes that affect control flow are visited. As is the case in
 * `toTRNode`, the opcode properties are used to determine how a particular
 * opcode affects the control flow.
 *
 * For the fall-through edge, the assumption is that one is always needed unless
 * a node specifically adds one (e.g. goto, return, etc.).
 */
bool Tril::TRLangBuilder::cfgFor(const ASTNode* const tree) {
   auto isFallthroughNeeded = true;

   // visit the children first
   const ASTNode* t = tree->getChildren();
   while (t) {
       isFallthroughNeeded = isFallthroughNeeded && cfgFor(t);
       t = t->next;
   }

   auto opcode = OpCodeTable{tree->getName()};

   if (opcode.isReturn()) {
       cfg()->addEdge(_currentBlock, cfg()->getEnd());
       isFallthroughNeeded = false;
       TraceIL("Added CFG edge from block %d to @exit -> %s\n", _currentBlockNumber, tree->getName());
   }
   else if (opcode.isBranch()) {
      const auto targetName = tree->getArgByName("target")->getValue()->getString();
      auto targetId = _blockMap[targetName];
      cfg()->addEdge(_currentBlock, _blocks[targetId]);
      isFallthroughNeeded = isFallthroughNeeded && opcode.isIf();
      TraceIL("Added CFG edge from block %d to block %d (\"%s\") -> %s\n", _currentBlockNumber, targetId, targetName, tree->getName());
   }

   if (!isFallthroughNeeded) {
       TraceIL("  (no fall-through needed)\n", "");
   }

   return isFallthroughNeeded;
}

/*
 * Generating IL from a Tril AST is done in three steps:
 *
 * 1. Generate basic blocks for each block represented in the AST
 * 2. Generate the IL itself (Trees) by walking the AST
 * 3. Generate the CFG by walking the AST
 */
bool Tril::TRLangBuilder::injectIL() {
    TraceIL("=== %s ===\n", "Generating Blocks");

    // the top level nodes of the AST should be all the basic blocks
    createBlocks(countNodes(_trees));

    // evaluate the arguments for each basic block
    const ASTNode* block = _trees;
    auto blockIndex = 0;

    // assign block names
    while (block) {
       if (block->getArgByName("name") != NULL) {
           auto name = block->getArgByName("name")->getValue()->getString();
           _blockMap[name] = blockIndex;
           TraceIL("Name of block %d set to \"%s\"\n", blockIndex, name);
       }
       ++blockIndex;
       block = block->next;
    }

    TraceIL("=== %s ===\n", "Generating IL");
    block = _trees;
    generateToBlock(0);

    // iterate over each treetop in each basic block
    while (block) {
       const ASTNode* t = block->getChildren();
       while (t) {
           auto node = toTRNode(t);
           if (node == NULL) 
              return false;
           const auto tt = genTreeTop(node);
           TraceIL("Created TreeTop %p for node n%dn (%p)\n", tt, node->getGlobalIndex(), node);
           t = t->next;
       }
       generateToBlock(_currentBlockNumber + 1);
       block = block->next;
    }

    TraceIL("=== %s ===\n", "Generating CFG");
    block = _trees;
    generateToBlock(0);

    // iterate over each basic block
    while (block) {
       auto isFallthroughNeeded = true;

       // create CFG edges from the nodes withing the current basic block
       const ASTNode* t = block->getChildren();
       while (t) {
           isFallthroughNeeded = isFallthroughNeeded && cfgFor(t);
           t = t->next;
       }

       // create fall-through edge
       auto fallthroughArg = block->getArgByName("fallthrough");
       if (fallthroughArg != NULL) {
           auto target = std::string(fallthroughArg->getValue()->getString());
           if (target == "@exit") {
               cfg()->addEdge(_currentBlock, cfg()->getEnd());
               TraceIL("Added fallthrough edge from block %d to \"%s\"\n", _currentBlockNumber, target.c_str());
           }
           else if (target == "@none") {
               // do nothing, no fall-throught block specified
           }
           else {
               auto destBlock = _blockMap.at(target);
               cfg()->addEdge(_currentBlock, _blocks[destBlock]);
               TraceIL("Added fallthrough edge from block %d to block %d \"%s\"\n", _currentBlockNumber, destBlock, target.c_str());
           }
       }
       else if (isFallthroughNeeded) {
           auto dest = _currentBlockNumber + 1 == numBlocks() ? cfg()->getEnd() : _blocks[_currentBlockNumber + 1];
           cfg()->addEdge(_currentBlock, dest);
           TraceIL("Added fallthrough edge from block %d to following block\n", _currentBlockNumber);
       }

       generateToBlock(_currentBlockNumber + 1);
       block = block->next;
    }

    return true;
}
