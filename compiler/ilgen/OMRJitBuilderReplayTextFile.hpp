/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

 #ifndef OMR_JITBUILDERREPLAY_TEXTFILE_INCL
 #define OMR_JITBUILDERREPLAY_TEXTFILE_INCL

 #include "ilgen/JitBuilderReplay.hpp"

 #include <iostream>
 #include <sstream>
 #include <cstring>
 #include <fstream>
 #include <map>

 namespace OMR
 {

 class JitBuilderReplayTextFile : public TR::JitBuilderReplay
    {
    public:

    // enum to handle Service method to distinguish between a MethodBuilder and an IlBuilder
    enum BuilderFlag {METHOD_BUILDER, IL_BUILDER};

    JitBuilderReplayTextFile(const char *fileName);

    void start();
    void processFirstLineFromTextFile();
    char * getLineAsChar();
    char * getTokensFromLine(std::string);

    bool parseConstructor();
    bool parseBuildIL();

    void addIDPointerPairToMap(char * tokens);
    char * getServiceStringFromToken(std::string tokens);
    const char * getServiceStringFromMap(char ** tokens);

    bool handleService(BuilderFlag builderFlag, char * tokens);
    bool handleServiceMethodBuilder(uint32_t mbID, char * tokens);
    bool handleServiceIlBuilder(uint32_t mbID, char * tokens);

    void handleMethodBuilder(uint32_t serviceID, char * tokens);
    void handleDefineLine(TR::MethodBuilder * mb, char * tokens);
    void handleDefineFile(TR::MethodBuilder * mb, char * tokens);
    void handleDefineName(TR::MethodBuilder * mb, char * tokens);
    void handleDefineParameter(TR::MethodBuilder * mb, char * tokens);
    void handleDefineArrayParameter(TR::MethodBuilder * mb, char * tokens);
    void handleDefineReturnType(TR::MethodBuilder * mb, char * tokens);
    void handleDefineFunction(TR::MethodBuilder * mb, char * tokens);
    void handleAllLocalsHaveBeenDefined(TR::MethodBuilder * mb, char * tokens);
    void handleDefineLocal(TR::MethodBuilder *mb, char *tokens);
    void handleDefineLocal(TR::IlBuilder *ilmb, char *tokens);
    void handlePrimitiveType(TR::IlBuilder * ilmb, char * tokens);


    void handleConstInt8(TR::IlBuilder * ilmb, char * tokens);
    void handleConstInt32(TR::IlBuilder * ilmb, char * tokens);
    void handleConstInt64(TR::IlBuilder * ilmb, char * tokens);
    void handleConstDouble(TR::IlBuilder * ilmb, char * tokens);
    void handleConstAddress(TR::IlBuilder * ilmb, char * tokens);
    void handlePointerType(TR::IlBuilder * ilmb, char * tokens);
    void handleCreateLocalArray(TR::IlBuilder * ilmb, char * tokens);
    void handleLoad(TR::IlBuilder * ilmb, char * tokens);
    void handleLoadAt(TR::IlBuilder * ilmb, char * tokens);
    void handleAdd(TR::IlBuilder * ilmb, char * tokens);
    void handleSub(TR::IlBuilder * ilmb, char * tokens);
    void handleMul(TR::IlBuilder * ilmb, char * tokens);
    void handleDiv(TR::IlBuilder * ilmb, char * tokens);
    void handleAnd(TR::IlBuilder * ilmb, char * tokens);
    void handleOr(TR::IlBuilder * ilmb, char * tokens);
    void handleXor(TR::IlBuilder * ilmb, char * tokens);
    void handleStore(TR::IlBuilder * ilmb, char * tokens);
    void handleStoreAt(TR::IlBuilder * ilmb, char * tokens);
    void handleForLoop(TR::IlBuilder * ilmb, char * tokens);

    void handleNewIlBuilder(TR::IlBuilder * ilmb, char * tokens);
    void handleLessThan(TR::IlBuilder * ilmb, char * tokens);
    void handleGreaterThan(TR::IlBuilder * ilmb, char * tokens);
    void handleNotEqualTo(TR::IlBuilder * ilmb, char * tokens);
    void handleIfThenElse(TR::IlBuilder * ilmb, char * tokens);
    void handleCall(TR::IlBuilder * ilmb, char * tokens);
    void handleConvertTo(TR::IlBuilder * ilmb, char * tokens);

    void handleReturnValue(TR::IlBuilder * ilmb, char * tokens);
    void handleReturn(TR::IlBuilder * ilmb, char * tokens);

    void handleUnsignedShiftR(TR::IlBuilder * ilmb, char * tokens);
    void handleIfCmpEqualZero(TR::IlBuilder * ilmb, char * tokens);
    void handleIndexAt(TR::IlBuilder * ilmb, char * tokens);

    uint32_t getNumberFromToken(char * token);

    private:
    std::fstream _file;
    std::istringstream _fileStream;
    bool _isFile;

    };

 } // namespace OMR

 #endif // !defined(OMR_JITBUILDERREPLAY_TEXTFILE_INCL)
