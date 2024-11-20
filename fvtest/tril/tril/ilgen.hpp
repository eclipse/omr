/*******************************************************************************
 * Copyright IBM Corp. and others 2017
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef ILGEN_HPP
#define ILGEN_HPP

#include "ilgen/TypeDictionary.hpp"
#include "ilgen/IlInjector.hpp"

#include "ast.hpp"

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "omrcfg.h"
#include "compiler_util.hpp"
#include "state.hpp"
#include "converter.hpp"
#include "CallConverter.hpp"
#include "GenericNodeConverter.hpp"

#include "compile/ResolvedMethod.hpp"
#include "il/Block.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"

#include <string>

namespace Tril {

class OpCodeTable : public TR::ILOpCode {
   public:
      explicit OpCodeTable(TR::ILOpCodes opcode) : TR::ILOpCode(opcode) {}
      explicit OpCodeTable(const std::string& name) : TR::ILOpCode(getOpCodeFromName(name)) {}

      /**
       * @brief Given an opcode name, returns the corresponding TR::OpCodes value
       */
      static TR::ILOpCodes getOpCodeFromName(const std::string& name) {
         auto opcode = _opcodeNameMap.find(name);
         if (opcode == _opcodeNameMap.end()) {
            for (int i = TR::FirstOMROp; i< TR::NumScalarIlOps; i++) {
               const auto p_opCode = static_cast<TR::ILOpCodes>(i);
               const auto& p = TR::ILOpCode::_opCodeProperties[p_opCode];
               if (name == p.name) {
                  _opcodeNameMap[name] = p.opcode;
                  return p.opcode;
               }
            }

            // Check if it's a vector opcode, e.g. vaddVector128Int32 or vconvVector256Int32ToVector256Float
            std::string opcodeAndSrcType = name;
            TR::DataType resType = TR::NoType;

            std::size_t pos = name.find("To");
            std::size_t divider = 2;

            if (pos != std::string::npos)
               {
               opcodeAndSrcType = name.substr(0, pos);
               std::size_t resTypeStart = pos + 2;

               if (name.find("Vector", resTypeStart) != resTypeStart &&
                   name.find("Mask", resTypeStart) != resTypeStart)
                  return TR::BadILOp;

               std::string resTypeName = name.substr(resTypeStart);
               const char* cStr = &*resTypeName.begin();
               resType = TR::DataType::getTypeFromName(cStr);

               if (resType == TR::NoType) return TR::BadILOp;
               }

            pos = opcodeAndSrcType.find("Vector");

            if (pos == std::string::npos)
               {
               pos = opcodeAndSrcType.find("Mask");
               if (pos == std::string::npos) return TR::BadILOp;
               }

            std::string vectorOperationName = opcodeAndSrcType.substr(0, pos);
            std::string srcTypeName = opcodeAndSrcType.substr(pos);
            const char* cStr = &*srcTypeName.begin();
            TR::DataType srcType = TR::DataType::getTypeFromName(cStr);

            if (srcType == TR::NoType) return TR::BadILOp;

            int i;
            for (i = TR::NumScalarIlOps; i < TR::NumAllIlOps; i++) {
               const auto p_opCode = static_cast<TR::ILOpCodes>(i);
               const auto& p = TR::ILOpCode::_opCodeProperties[p_opCode];
               if (vectorOperationName == p.name) break;
            }
            if (i == TR::NumAllIlOps) return TR::BadILOp;

            TR::VectorOperation vop = (TR::VectorOperation)(i - TR::NumScalarIlOps);
            TR::ILOpCodes opcode = (resType == TR::NoType) ? TR::ILOpCode::createVectorOpCode(vop, srcType)
                                                           : TR::ILOpCode::createVectorOpCode(vop, srcType, resType);
            _opcodeNameMap[name] = opcode;
            return opcode;
         }
         else {
            return opcode->second;
         }
      }

   private:
      static std::map<std::string, TR::ILOpCodes> _opcodeNameMap;
};

/**
 * @brief IL generator for Tril
 *
 * This class takes care of transformaing the Tril AST into Testarossa IL
 */
class TRLangBuilder : public TR::IlInjector {
    public:

        /**
         * @brief Constructs a Tril IL Generator
         * @param trees is the list of AST nodes representing the TR::Node instances to be generated
         * @param d is the type dictionary to be used this IL generator
         * @param converter is the root of the ASTNode that needed to be converted into TRNode
         */
        TRLangBuilder(const ASTNode* trees, TR::TypeDictionary* d, ASTToTRNode* converter)
              : TR::IlInjector(d), _converter(converter), _trees(trees) {}

        bool injectIL(); /* override */

        /**
         * @brief Given an AST structure and corresponding IlGenState, returns a TR::Node represented by the AST
         * @param tree the AST structure
         * @param state the IlGenState storing AST details
         * @return TR::Node represented by the AST
         */
        TR::Node* toTRNode(const ASTNode* const tree, IlGenState* state);

        /**
         * @brief Given an AST structure and corresponding IlGenState, generates a corresponding CFG
         * @param tree the AST structure that the CFG will be generated from
         * @param state the IlGenState storing AST details
         * @return true if a fall-through edge needs to be added, false otherwise
         */
        bool cfgFor(const ASTNode* const tree, IlGenState* state);

    private:
        ASTToTRNode* _converter;
        const ASTNode* _trees;    // pointer to the AST node list representing Trees
};

} // namespace Tril

#endif // ILGEN_HPP
