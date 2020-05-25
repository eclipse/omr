/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#ifndef ILGENSTATE_HPP
#define ILGENSTATE_HPP

#include "ilgen/TypeDictionary.hpp"

namespace Tril {

class IlGenState {
    TR::TypeDictionary* _types;
    std::map<std::string, TR::SymbolReference*> _symRefMap;
    std::map<std::string, int> _blockMap;
    std::map<std::string, TR::Node*> _nodeMap;

    private:
        TR::SymbolReferenceTable*  _symRefTab;
        TR::ResolvedMethodSymbol* _methodSymbol;
        TR::Block** _blocks;
        TR::Compilation* _comp;

    public:
        // get _symRefMap, _symRefMap and _nodeMap values based on key
        std::map<std::string, TR::SymbolReference*>::iterator findSymRefByName(const char *symName) {
            return _symRefMap.find(symName);
        }
        std::map<std::string, TR::SymbolReference*>::iterator symRefMapEnd() {
            return _symRefMap.end();
        }
        std::map<std::string, TR::Node*>::iterator findNodeByID(const char* id) {
            return _nodeMap.find(id);
        }
        std::map<std::string, TR::Node*>::iterator nodeMapEnd() {
            return _nodeMap.end();
        }
        int findBlockByName(const char* targetName) {
            return _blockMap[targetName];
        }

        // set _symRefMap, _symRefMap and _nodeMap values based on key
        void setSymRefPair(const char* symName, TR::SymbolReference* symRef) {
            _symRefMap[symName] = symRef;
        }
        void setNodePair(const char* id, TR::Node* node) {
            _nodeMap[id] = node;
        }
        void setBlockPair(const char* name, int blockIndex) {
            _blockMap[name] = blockIndex;
        }

        // set _symRefTab, _methodSymbol, _blocks, and _types based on IlInjector
        void setSymRefTab(TR::SymbolReferenceTable* symRefTab) {
            _symRefTab = symRefTab;
        }
        void setMethodSymbol(TR::ResolvedMethodSymbol* methodSymbol) {
            _methodSymbol = methodSymbol;
        }
        void setBlocks(TR::Block** blocks) {
            _blocks = blocks;
        }
        void setType(TR::TypeDictionary* type) {
            _types = type;
        }

        // get _symRefTab, _methodSymbol, _blocks, and _types 
        TR::SymbolReferenceTable* symRefTab() {
            return _symRefTab;
        }
        TR::ResolvedMethodSymbol* methodSymbol() const {
            return _methodSymbol;
        }
        TR::Block** blocks() const {
            return _blocks;
        }
        TR::TypeDictionary* getTypes() {
            return _types;
        }

        // _comp setter and getter for TraceIL
        TR::Compilation* comp() {
            return _comp;
        }
        void setComp(TR::Compilation* comp) {
            _comp = comp;
        }
};

} // namespace Tril
#endif
