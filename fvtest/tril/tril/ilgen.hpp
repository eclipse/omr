/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2017, 2017
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
 ******************************************************************************/

#ifndef ILGEN_HPP
#define ILGEN_HPP

#include "ilgen/TypeDictionary.hpp"
#include "ilgen/IlInjector.hpp"

#include "ast.hpp"

#include <map>
#include <string>
#include <vector>
#include <type_traits>

namespace Tril {

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
         */
        TRLangBuilder(const ASTNode* trees, TR::TypeDictionary* d)
              : TR::IlInjector(d), _trees(trees) {}

        bool injectIL() override;

        /**
         * @brief Given an AST structure, returns a TR::Node represented by the AST
         * @param tree the AST structure
         * @return TR::Node represented by the AST
         */
        TR::Node* toTRNode(const ASTNode* const tree);

        /**
         * @brief Given an AST structure, generates a corresponding CFG
         * @param tree the AST structure that the CFG will be generated from
         * @return true if a fall-through edge needs to be added, false otherwise
         */
        bool cfgFor(const ASTNode* const tree);

        TR::Symbol* generateSymbol(const ASTNode* symbolNode);
        TR::SymbolReference* generateSymRef(const ASTNode* symrefNode);

    private:
        TR::TypeDictionary _types;
        const ASTNode* _trees;    // pointer to the AST node list representing Trees
        std::map<std::string, TR::SymbolReference*> _symRefMap; // mapping of string names to symrefs
        std::map<std::string, int> _blockMap;   // mapping of string names to basic block numbers
        std::map<std::string, TR::Node*> _nodeMap;
};

} // namespace Tril

#endif // ILGEN_HPP
