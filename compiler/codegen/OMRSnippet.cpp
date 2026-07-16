/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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

#include <stddef.h>
#include "il/LabelSymbol.hpp"
#include "codegen/Snippet.hpp"
#include "codegen/CodeGenerator.hpp"
#include "ras/Logger.hpp"

OMR::Snippet::Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label, bool isGCSafePoint)
    : _cg(cg)
    , _node(node)
    , _snippetLabel(label)
    , _block(0)
    , _flags(0)
{
    self()->setSnippetLabel(label);

    if (isGCSafePoint) {
        self()->prepareSnippetForGCSafePoint();
    }

    self()->gcMap().setGCRegisterMask(0xFF00FFFF);
}

OMR::Snippet::Snippet(TR::CodeGenerator *cg, TR::Node *node, TR::LabelSymbol *label)
    : _cg(cg)
    , _node(node)
    , _snippetLabel(label)
    , _block(0)
    , _flags(0)
{
    self()->setSnippetLabel(label);
    self()->gcMap().setGCRegisterMask(0xFF00FFFF);
}

TR::Snippet *OMR::Snippet::self() { return static_cast<TR::Snippet *>(this); }

void OMR::Snippet::setSnippetLabel(TR::LabelSymbol *label)
{
    if (_snippetLabel) {
        _snippetLabel->setSnippet(NULL);
    }

    label->setSnippet(static_cast<TR::Snippet *>(this));
    _snippetLabel = label;
}

int32_t OMR::Snippet::setEstimatedCodeLocation(int32_t p)
{
    return self()->getSnippetLabel()->setEstimatedCodeLocation(p);
}

uint8_t *OMR::Snippet::emitSnippet() { return self()->emitSnippetBody(); }

void OMR::Snippet::prepareSnippetForGCSafePoint()
{
    self()->gcMap().setGCSafePoint();
    self()->setBlock(self()->cg()->getCurrentEvaluationBlock());
    self()->setNeedsExceptionTableEntry();
}

void OMR::Snippet::print(OMR::Logger *log, TR_Debug *debug)
{
    uint8_t *cursor = self()->getSnippetLabel()->getCodeLocation();

    debug->printSnippetLabel(log, self()->getSnippetLabel(), cursor, "<Unknown Snippet>");

    for (auto i = 0; i < self()->getLength(0) / sizeof(uint64_t); ++i) {
        debug->printPrefix(log, NULL, cursor, sizeof(uint64_t));
        cursor += sizeof(uint64_t);
    }

    if (self()->getLength(0) % sizeof(uint64_t) != 0) {
        debug->printPrefix(log, NULL, cursor, self()->getLength(0) % sizeof(uint64_t));
    }
}

void OMR::Snippet::printName(OMR::Logger *log) { log->prints("<unknown snippet>"); }

const char *OMR::Snippet::getName(TR::Region &region)
{
    const int32_t maxNameLen = 64;
    char buf[maxNameLen];

    // Render the name into a local buffer
    //
    OMR::Logger *log = OMR::MemoryBufferLogger::create(region, buf, maxNameLen);
    self()->printName(log);

    size_t len = strlen(buf);

    // Trim the buffer that will be returned to the exact length
    //
    char *name = (char *)region.allocate(len + 1); // +1 for NUL-terminator
    if (name) {
        // Include the NUL-terminator
        memcpy(name, buf, len + 1);
    }

    return name;
}
