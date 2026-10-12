/*******************************************************************************
 * Copyright IBM Corp. and others 2014
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#ifndef OMR_RESOLVEDMETHOD_INCL
#define OMR_RESOLVEDMETHOD_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef OMR_RESOLVEDMETHOD_CONNECTOR
#define OMR_RESOLVEDMETHOD_CONNECTOR

namespace OMR {
class ResolvedMethod;
}

namespace OMR {
typedef OMR::ResolvedMethod ResolvedMethodConnector;
}
#endif

#include <string.h>
#include "compile/Method.hpp"
#include "compile/TRResolvedMethod.hpp"

class TR_IlGenerator;

namespace TR {
class IlGeneratorMethodDetails;
class FrontEnd;
} // namespace TR

// quick and dirty implementation to get up and running
// needs major overhaul

namespace OMR {

const int16_t MAX_SIGNATURE_LENGTH = 128;

class ResolvedMethod
    : public ::TR_ResolvedMethod
    , public TR::Method {
public:
    ResolvedMethod(TR_OpaqueMethodBlock *method);
    ResolvedMethod(const char *fileName, const char *lineNumber, const char *name, int32_t numParms,
        const char **parmNames, TR::DataType *parmTypes, TR::DataType returnType, void *entryPoint,
        TR_IlGenerator *ilgen);

    virtual TR::Method *convertToMethod() { return this; }

    virtual const char *signature(TR_Memory *, TR_AllocationKind);
    virtual const char *externalName(TR_Memory *, TR_AllocationKind);
    char *localName(uint32_t slot, uint32_t bcIndex, int32_t &nameLength, TR_Memory *trMemory);

    virtual char *classNameChars() { return (char *)_fileName; }

    virtual char *nameChars() { return const_cast<char *>(_name); } // ugly

    virtual char *signatureChars() { return _signatureChars; }

    virtual uint16_t signatureLength() { return static_cast<uint16_t>(strlen(signatureChars())); }

    virtual void *resolvedMethodAddress() { return (void *)_ilgen; }

    virtual uint16_t numberOfParameterSlots() { return _numParms; }

    virtual TR::DataType parmType(uint32_t slot);

    virtual uint16_t numberOfTemps() { return 0; }

    virtual char *getParameterTypeSignature(int32_t parmIndex);

    virtual void *startAddressForJittedMethod() { return (getEntryPoint()); }

    virtual void *startAddressForInterpreterOfJittedMethod() { return 0; }

    virtual uint32_t maxBytecodeIndex() { return 0; }

    virtual uint8_t *code() { return NULL; }

    virtual TR_OpaqueMethodBlock *getPersistentIdentifier()
    {
        /*
         * This has to be kept in sync with OMR::VMMethodEnv::startPC().
         * If changed, startPC() has to be update accordingly.
         */
        return (TR_OpaqueMethodBlock *)getEntryPoint();
    }

    virtual bool isInterpreted() { return startAddressForJittedMethod() == 0; }

    const char *getLineNumber() { return _lineNumber; }

    char *getSignature() { return _signature; }

    TR::DataType returnType();

    int32_t getNumArgs() { return _numParms; }

    void setEntryPoint(void *ep) { _entryPoint = ep; }

    void *getEntryPoint() { return _entryPoint; }

    void computeSignatureCharsPrimitive();
    void computeSignatureChars();

    TR_IlGenerator *getIlGenerator(TR::IlGeneratorMethodDetails *details, TR::ResolvedMethodSymbol *methodSymbol,
        TR::FrontEnd *fe, TR::SymbolReferenceTable *symRefTab);

protected:
    const char *_fileName;
    const char *_lineNumber;

    const char *_name;
    char *_signature;
    char _signatureChars[MAX_SIGNATURE_LENGTH];
    const char *_externalName;

    int32_t _numParms;
    const char **_parmNames;
    TR::DataType *_parmTypes;
    TR::DataType _returnType;
    void *_entryPoint;
    TR_IlGenerator *_ilgen;

    static const char *signatureNameForType[];
    static const char *signatureNameForVectorType[];
    static const char *signatureNameForMaskType[];
};

} // namespace OMR

#endif // !defined(OMR_RESOLVEDMETHOD_INCL)
