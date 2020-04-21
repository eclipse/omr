/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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

#ifndef ILGEN_HPP
#define ILGEN_HPP

#include "ilgen/TypeDictionary.hpp"
#include "ilgen/IlInjector.hpp"

#include "ast.hpp"

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
        TR::TypeDictionary _types;
        const ASTNode* _trees;    // pointer to the AST node list representing Trees
};

} // namespace Tril

#endif // ILGEN_HPP
