/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#ifndef OMR_STATEMENTNAMES_INCL
#define OMR_STATEMENTNAMES_INCL

#include <stdint.h>

namespace OMR
{

namespace StatementName {

static const int16_t VERSION_MAJOR              = 0;
static const int16_t VERSION_MINOR              = 0;
static const int16_t VERSION_PATCH              = 0;
static const char * const RECORDER_SIGNATURE    = "JBIL";
static const char * const JBIL_COMPLETE         = "Done";

static const char * const STATEMENT_DEFINENAME                   = "DefineName";
static const char * const STATEMENT_DEFINEFILE                   = "DefineFile";
static const char * const STATEMENT_DEFINELINESTRING             = "DefineLineString";
static const char * const STATEMENT_DEFINELINENUMBER             = "DefineLineNumber";
static const char * const STATEMENT_DEFINEPARAMETER              = "DefineParameter";
static const char * const STATEMENT_DEFINEARRAYPARAMETER         = "DefineArrayParameter";
static const char * const STATEMENT_DEFINERETURNTYPE             = "DefineReturnType";
static const char * const STATEMENT_DEFINELOCAL                  = "DefineLocal";
static const char * const STATEMENT_DEFINEMEMORY                 = "DefineMemory";
static const char * const STATEMENT_DEFINEFUNCTION               = "DefineFunction";
static const char * const STATEMENT_DEFINESTRUCT                 = "DefineStruct";
static const char * const STATEMENT_DEFINEUNION                  = "DefineUnion";
static const char * const STATEMENT_DEFINEFIELD                  = "DefineField";
static const char * const STATEMENT_PRIMITIVETYPE                = "PrimitiveType";
static const char * const STATEMENT_POINTERTYPE                  = "PointerType";
static const char * const STATEMENT_NEWMETHODBUILDER             = "NewMethodBuilder";
static const char * const STATEMENT_NEWILBUILDER                 = "NewIlBuilder";
static const char * const STATEMENT_NEWBYTECODEBUILDER           = "NewBytecodeBuilder";
static const char * const STATEMENT_ALLLOCALSHAVEBEENDEFINED     = "AllLocalsHaveBeenDefined";
static const char * const STATEMENT_NULLADDRESS                  = "NullAddress";
static const char * const STATEMENT_CONSTINT8                    = "ConstInt8";
static const char * const STATEMENT_CONSTINT16                   = "ConstInt16";
static const char * const STATEMENT_CONSTINT32                   = "ConstInt32";
static const char * const STATEMENT_CONSTINT64                   = "ConstInt64";
static const char * const STATEMENT_CONSTFLOAT                   = "ConstFloat";
static const char * const STATEMENT_CONSTDOUBLE                  = "ConstDouble";
static const char * const STATEMENT_CONSTSTRING                  = "ConstString";
static const char * const STATEMENT_CONSTADDRESS                 = "ConstAddress";
static const char * const STATEMENT_INDEXAT                      = "IndexAt";
static const char * const STATEMENT_LOAD                         = "Load";
static const char * const STATEMENT_LOADAT                       = "LoadAt";
static const char * const STATEMENT_LOADINDIRECT                 = "LoadIndirect";
static const char * const STATEMENT_STORE                        = "Store";
static const char * const STATEMENT_STOREOVER                    = "StoreOver";
static const char * const STATEMENT_STOREAT                      = "StoreAt";
static const char * const STATEMENT_STOREINDIRECT                = "StoreIndirect";
static const char * const STATEMENT_CREATELOCALARRAY             = "CreateLocalArray";
static const char * const STATEMENT_CREATELOCALSTRUCT            = "CreateLocalStruct";
static const char * const STATEMENT_VECTORLOAD                   = "VectorLoad";
static const char * const STATEMENT_VECTORLOADAT                 = "VectorLoadAt";
static const char * const STATEMENT_VECTORSTORE                  = "VectorStore";
static const char * const STATEMENT_VECTORSTOREAT                = "VectorStoreAt";
static const char * const STATEMENT_STRUCTFIELDINSTANCEADDRESS   = "StructFieldInstance";
static const char * const STATEMENT_UNIONFIELDINSTANCEADDRESS    = "UnionFieldInstance";
static const char * const STATEMENT_CONVERTTO                    = "ConvertTo";
static const char * const STATEMENT_UNSIGNEDCONVERTTO            = "UnsignedConvertTo";
static const char * const STATEMENT_ATOMICADD                    = "AtomicAdd";
static const char * const STATEMENT_ADD                          = "Add";
static const char * const STATEMENT_ADDWITHOVERFLOW              = "AddWithOverflow";
static const char * const STATEMENT_ADDWITHUNSIGNEDOVERFLOW      = "AddWithUnsignedOverflow";
static const char * const STATEMENT_SUB                          = "Sub";
static const char * const STATEMENT_SUBWITHOVERFLOW              = "SubWithOverflow";
static const char * const STATEMENT_SUBWITHUNSIGNEDOVERFLOW      = "SubWithUnsignedOverflow";
static const char * const STATEMENT_MUL                          = "Mul";
static const char * const STATEMENT_MULWITHOVERFLOW              = "MulWithOverflow";
static const char * const STATEMENT_DIV                          = "Div";
static const char * const STATEMENT_REM                          = "Rem";
static const char * const STATEMENT_AND                          = "And";
static const char * const STATEMENT_OR                           = "Or";
static const char * const STATEMENT_XOR                          = "Xor";
static const char * const STATEMENT_LESSTHAN                     = "LessThan";
static const char * const STATEMENT_UNSIGNEDLESSTHAN             = "UnsignedLessThan";
static const char * const STATEMENT_UNSIGNEDLESSOREQUALTO        = "UnsignedLessOrEqualTo";
static const char * const STATEMENT_NEGATE                       = "Negate";
static const char * const STATEMENT_CONVERTBITSTO                = "ConvertBitsTo";
static const char * const STATEMENT_GREATERTHAN                  = "GreaterThan";
static const char * const STATEMENT_GREATEROREQUALTO             = "GreaterOrEqualTo";
static const char * const STATEMENT_UNSIGNEDGREATERTHAN          = "UnsignedGreaterThan";
static const char * const STATEMENT_UNSIGNEDGREATEROREQUALTO     = "UnsignedGreaterOrEqualTo";
static const char * const STATEMENT_EQUALTO                      = "EqualTo";
static const char * const STATEMENT_LESSOREQUALTO                = "LessOrEqualTo";
static const char * const STATEMENT_NOTEQUALTO                   = "NotEqualTo";
static const char * const STATEMENT_APPENDBUILDER                = "AppendBuilder";
static const char * const STATEMENT_APPENDBYTECODEBUILDER        = "AppendBytecodeBuilder";
static const char * const STATEMENT_GOTO                         = "Goto";
static const char * const STATEMENT_SHIFTL                       = "ShiftL";
static const char * const STATEMENT_SHIFTR                       = "ShiftR";
static const char * const STATEMENT_UNSIGNEDSHIFTR               = "UnsignedShiftR";
static const char * const STATEMENT_RETURN                       = "Return";
static const char * const STATEMENT_RETURNVALUE                  = "ReturnValue";
static const char * const STATEMENT_IFTHENELSE                   = "IfThenElse";
static const char * const STATEMENT_SELECT                       = "Select";
static const char * const STATEMENT_IFCMPEQUALZERO               = "IfCmpEqualZero";
static const char * const STATEMENT_IFCMPNOTEQUALZERO            = "IfCmpNotEqualZero";
static const char * const STATEMENT_IFCMPNOTEQUAL                = "IfCmpNotEqual";
static const char * const STATEMENT_IFCMPEQUAL                   = "IfCmpEqual";
static const char * const STATEMENT_IFCMPLESSTHAN                = "IfCmpLessThan";
static const char * const STATEMENT_IFCMPLESSOREQUAL             = "IfCmpLessOrEqual";
static const char * const STATEMENT_IFCMPUNSIGNEDLESSOREQUAL     = "IfCmpUnsignedLessOrEqual";
static const char * const STATEMENT_IFCMPUNSIGNEDLESSTHAN        = "IfCmpUnsignedLessThan";
static const char * const STATEMENT_IFCMPGREATERTHAN             = "IfCmpGreaterThan";
static const char * const STATEMENT_IFCMPUNSIGNEDGREATERTHAN     = "IfCmpUnsignedGreaterThan";
static const char * const STATEMENT_IFCMPGREATEROREQUAL          = "IfCmpGreaterOrEqual";
static const char * const STATEMENT_IFCMPUNSIGNEDGREATEROREQUAL  = "IfCmpUnsignedGreaterOrEqual";
static const char * const STATEMENT_DOWHILELOOP                  = "DoWhileLoop";
static const char * const STATEMENT_WHILEDOLOOP                  = "WhileDoLoop";
static const char * const STATEMENT_FORLOOP                      = "ForLoop";
static const char * const STATEMENT_CALL                         = "Call";
static const char * const STATEMENT_COMPUTEDCALL                 = "ComputedCall";
static const char * const STATEMENT_DONECONSTRUCTOR              = "DoneConstructor";
static const char * const STATEMENT_IFAND                        = "IfAnd";
static const char * const STATEMENT_IFOR                         = "IfOr";
static const char * const STATEMENT_SWITCH                       = "Switch";
static const char * const STATEMENT_TRANSACTION                  = "Transaction";
static const char * const STATEMENT_TRANSACTIONABORT             = "TransactionAbort";

/*
* @brief constant strings only used internally by recorders
*/
static const char * const STATEMENT_ID16BIT                      = "ID16BIT";
static const char * const STATEMENT_ID32BIT                      = "ID32BIT";
}

} // namespace OMR

#endif // !defined(OMR_STATEMENTNAMES_INCL)
