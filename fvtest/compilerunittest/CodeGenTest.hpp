/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#ifndef CODEGENTEST_HPP
#define CODEGENTEST_HPP

#include <gtest/gtest.h>
#include <exception>

#include "Jit.hpp"
#include "codegen/CodeGenerator.hpp"
#include "compile/Compilation.hpp"
#include "env/ConcreteFE.hpp"
#include "env/SystemSegmentProvider.hpp"
#include "ilgen/IlGenRequest.hpp"
#include "ilgen/IlGeneratorMethodDetails.hpp"
#include "ilgen/TypeDictionary.hpp"

#include "codegen/CodeGenerator_inlines.hpp"
#include "compile/Compilation_inlines.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"

namespace TRTest {

class NullIlGenRequest : public TR::IlGenRequest {
    TR::IlGeneratorMethodDetails _details;

public:
    NullIlGenRequest() : TR::IlGenRequest(_details) {}

    virtual TR_IlGenerator *getIlGenerator(
        TR::ResolvedMethodSymbol *methodSymbol,
        TR_FrontEnd *fe,
        TR::Compilation *comp,
        TR::SymbolReferenceTable *symRefTab
    ) {
        throw std::runtime_error("The mock JIT environment does not support calling TR::IlGenRequest::getIlGenerator");
    }

    virtual void print(TR_FrontEnd *fe, TR::FILE *file, const char *suffix) {}
};

class JitInitializer {
public:
    JitInitializer() {
        initializeJit();
    }

    ~JitInitializer() {
        shutdownJit();
    }
};

class CodeGenTest : public ::testing::Test {
    JitInitializer _jitInit;

    TR::RawAllocator _rawAllocator;
    TR::SystemSegmentProvider _segmentProvider;
    TR::Region _dispatchRegion;
    TR_Memory _trMemory;

    TR::TypeDictionary _types;

    TR::Options _options;
    NullIlGenRequest _ilGenRequest;
    TR::ResolvedMethod _method;
    TR::Compilation _comp;
public:
    CodeGenTest() :
        _jitInit(),
        _rawAllocator(),
        _segmentProvider(1 << 16, _rawAllocator),
        _dispatchRegion(_segmentProvider, _rawAllocator),
        _trMemory(*OMR::FrontEnd::singleton().persistentMemory(), _dispatchRegion),
        _types(),
        _options(),
        _ilGenRequest(),
        _method("compunittest", "0", "test", 0, NULL, _types.NoType, NULL, NULL),
        _comp(0, NULL, &OMR::FrontEnd::singleton(), &_method, _ilGenRequest, _options, _dispatchRegion, &_trMemory, TR_OptimizationPlan::alloc(warm)) {
    }

    TR::CodeGenerator* cg() { return _comp.cg(); }
};

template <typename T>
class MakeVector {
    std::vector<T> _vals;

    void add_vals() {}

    template <typename... Ts>
    void add_vals(T next_val, Ts... more_vals) {
        _vals.push_back(next_val);
        add_vals(more_vals...);
    }
public:
    template <typename... Ts>
    MakeVector(Ts... vals) {
        add_vals(vals...);
    }

    const std::vector<T>& operator*() const {
        return _vals;
    }
};

}

#endif
