/*******************************************************************************
 * Copyright IBM Corp. and others 2026
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

#ifndef RVRELOCATION_INCL
#define RVRELOCATION_INCL

#include "codegen/Relocation.hpp"
#include "il/SymbolReference.hpp"

namespace TR {

class R_RISCV_BRANCH : public LabelRelocation {
public:
    R_RISCV_BRANCH(uint8_t *p, LabelSymbol *l)
        : TR::LabelRelocation(p, l)
    {}

    virtual void apply(TR::CodeGenerator *cg);
};

class R_RISCV_JAL : public LabelRelocation {
public:
    R_RISCV_JAL(uint8_t *p, LabelSymbol *l)
        : TR::LabelRelocation(p, l)
    {}

    virtual void apply(TR::CodeGenerator *cg);
};

class R_RISCV_CALL_PLT : public Relocation {
    SymbolReference *_symbolReference;

public:
    R_RISCV_CALL_PLT(uint8_t *p, SymbolReference *s)
        : TR::Relocation(p)
        , _symbolReference(s)
    {}

    virtual void apply(TR::CodeGenerator *cg);

    SymbolReference *getSymbolReference() { return _symbolReference; }
};

} // namespace TR

#endif // RVRELOCATION_INCL
