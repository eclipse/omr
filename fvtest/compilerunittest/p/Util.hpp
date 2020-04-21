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

#ifndef P_UTIL_HPP
#define P_UTIL_HPP

#include <ostream>
#include <vector>

#include "codegen/CodeGenerator.hpp"
#include "codegen/CodeGenerator_inlines.hpp"
#include "codegen/InstOpCode.hpp"
#include "codegen/MemoryReference.hpp"
#include "codegen/RealRegister.hpp"

inline std::ostream& operator<<(std::ostream& os, const TR::InstOpCode::Mnemonic& opCode) {
    os << TR::InstOpCode::metadata[opCode].name;
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const TR::RealRegister::RegNum& reg) {
    if (reg >= TR::RealRegister::FirstGPR && reg <= TR::RealRegister::LastGPR)
        os << "gr" << (static_cast<int>(reg) - static_cast<int>(TR::RealRegister::FirstGPR));
    else if (reg >= TR::RealRegister::FirstFPR && reg <= TR::RealRegister::LastFPR)
        os << "fp" << (static_cast<int>(reg) - static_cast<int>(TR::RealRegister::FirstFPR));
    else if (reg >= TR::RealRegister::FirstVRF && reg <= TR::RealRegister::LastVRF)
        os << "vr" << (static_cast<int>(reg) - static_cast<int>(TR::RealRegister::FirstVRF));
    else if (reg >= TR::RealRegister::FirstCCR && reg <= TR::RealRegister::LastCCR)
        os << "cr" << (static_cast<int>(reg) - static_cast<int>(TR::RealRegister::FirstCCR));
    else
        os << "?" << static_cast<int>(reg);

    return os;
}

class MemoryReference {
public:
    TR::RealRegister::RegNum _baseReg;
    TR::RealRegister::RegNum _indexReg;
    int32_t _displacement;

    MemoryReference() : _baseReg(TR::RealRegister::NoReg), _indexReg(TR::RealRegister::NoReg), _displacement(0) {}

    MemoryReference(TR::RealRegister::RegNum baseReg, TR::RealRegister::RegNum indexReg)
        : _baseReg(baseReg), _indexReg(indexReg), _displacement(0) {}

    MemoryReference(TR::RealRegister::RegNum baseReg, int32_t displacement)
        : _baseReg(baseReg), _indexReg(TR::RealRegister::NoReg), _displacement(displacement) {}

    bool isIndexForm() const {
        return _indexReg != TR::RealRegister::NoReg;
    }

    TR::RealRegister::RegNum baseReg() const {
        return _baseReg;
    }

    TR::RealRegister::RegNum indexReg() const {
        return _indexReg;
    }

    int32_t displacement() const {
        return _displacement;
    }

    TR::MemoryReference* reify(TR::CodeGenerator* cg) const {
        if (isIndexForm()) {
            if (_displacement != 0)
                throw std::invalid_argument("A MemoryReference cannot have a displacement and an index register");

            return new (cg->trHeapMemory()) TR::MemoryReference(
                cg->machine()->getRealRegister(_baseReg),
                cg->machine()->getRealRegister(_indexReg),
                0,
                cg
            );
        } else {
            return new (cg->trHeapMemory()) TR::MemoryReference(
                cg->machine()->getRealRegister(_baseReg),
                _displacement,
                0,
                cg
            );
        }
    }
};

inline std::ostream& operator<<(std::ostream& os, const MemoryReference& mr) {
    if (mr._indexReg != TR::RealRegister::NoReg) {
        os << mr._indexReg << "(" << mr._baseReg << ")";
    } else {
        os << mr._displacement << "(" << mr._baseReg << ")";
    }

    return os;
}

#endif
