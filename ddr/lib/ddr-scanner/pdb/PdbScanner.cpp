/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "ddr/scanner/pdb/PdbScanner.hpp"

#include <algorithm>
#include <assert.h>
#include <comdef.h>
#include <inttypes.h>
#include <stdio.h>
#include "ddr/std/sstream.hpp"

#include "ddr/config.hpp"
#include "ddr/ir/EnumMember.hpp"
#include "ddr/ir/ClassUDT.hpp"
#include "ddr/ir/EnumUDT.hpp"
#include "ddr/ir/Field.hpp"
#include "ddr/ir/Symbol_IR.hpp"
#include "ddr/ir/Type.hpp"
#include "ddr/ir/TypedefUDT.hpp"

using std::ios;
using std::stringstream;

const char *const baseTypeArray[] = {
	"<NoType>",
	"void",
	"I8", /* This could also be char. */
	"wchar_t",
	"I8",
	"U8",
	"I32",
	"U32",
	"float",
	"<BCD>",
	"bool",
	"short",
	"unsigned short",
	"I32", /* This should be just a long */
	"U32", /* his should be unsigned long */
	"I8",
	"I16",
	"I32",
	"I64",
	"__int128",
	"U8",
	"U16",
	"U32",
	"U64",
	"U128",
	"unsigned __int128",
	"<currency>",
	"<date>",
	"VARIANT",
	"<complex>",
	"<bit>",
	"BSTR",
	"HRESULT",
	"double"
};

/* temporary global variable to be removed once PdbScanner is complete */
static string errorNoType = "ERROR_PDBSCANNER_MISSING_THIS_TYPE";

void
PdbScanner::addType(Type *type, bool addToIR = true)
{
	/* Add the type to the map of found types. Subtypes should
	 * not be added to the IR. Types are mapped by their size and
	 * name to be referenced when used as a field.
	 */
	string fullName = type->getFullName();
	if (!fullName.empty() && _typeMap.end() == _typeMap.find(fullName)) {
		if (addToIR) {
			_ir->_types.push_back(type);
		}
		_typeMap[fullName] = type;
	}
}

void
PdbScanner::initBaseTypeList()
{
	/* Add the base types with the typemap, to be referenced as
	 * fields but not added to the IR at this time.
	 */
	size_t length = sizeof(baseTypeArray) / sizeof(char *);
	for (size_t i = 0; i < length; i += 1) {
		Type *type = new Type(0);
		type->_name = baseTypeArray[i];
		_typeMap[type->_name] = type;
		_ir->_types.push_back(type);
	}

	Type *type = new Type(0);
	type->_name = errorNoType;
	_typeMap[type->_name] = type;
	_ir->_types.push_back(type);
}


DDR_RC
getName(IDiaSymbol *symbol, string *name)
{
	DDR_RC rc = DDR_RC_OK;
	/* Attempt to get the name associated with a symbol. */
	BSTR nameBstr;
	HRESULT hr = symbol->get_name(&nameBstr);
	if (FAILED(hr)) {
		ERRMSG("get_name failed with HRESULT = %08X", hr);
		rc = DDR_RC_ERROR;
	} else {
		char *nameChar = _com_util::ConvertBSTRToString(nameBstr);
		if (NULL == nameChar) {
			ERRMSG("Could not convert Bstr");
		} else {
			*name = string(nameChar);
			size_t pos = name->find("`anonymous-namespace'::");
			if (string::npos == pos) {
				pos = name->find("`anonymous namespace'::");
			}
			if (string::npos != pos) {
				*name = name->replace(pos, 23, "");
			}
			free(nameChar);
		}
		SysFreeString(nameBstr);
	}

	return rc;
}

DDR_RC
PdbScanner::startScan(OMRPortLibrary *portLibrary, Symbol_IR *const ir, vector<string> *debugFiles, string blacklistPath)
{
	DDR_RC rc = DDR_RC_OK;
	IDiaDataSource *diaDataSource = NULL;
	IDiaSession *diaSession = NULL;
	IDiaSymbol *diaSymbol = NULL;
	_ir = ir;

	initBaseTypeList();

	if (FAILED(CoInitialize(NULL))) {
		rc = DDR_RC_ERROR;
	}

	if ((DDR_RC_OK == rc) && ("" != blacklistPath)) {
		rc = loadBlacklist(blacklistPath);
	}

	if (DDR_RC_OK == rc) {
		/* For each input Pdb file, load the file, then add the UDTs and Enums.
		 * If findChildren(SymTagNull, ...) were to be used instead of finding
		 * the UDTs and enums separately, duplicate types are returned with
		 * undecorated names. The IR would contain inner classes twice, once as
		 * an inner class, and once with no parent link and an undecorated name.
		 * Finding UDT and enum children separately seems to work around this
		 * quirk in the PDB API.
		 */
		std::size_t lastProgressUpdate = 0;
		std::size_t count = 0;
		for (vector<string>::iterator it = debugFiles->begin(); it != debugFiles->end(); ++it) {
			if (count ++ > lastProgressUpdate + debugFiles->size() / 10) {
				printf("Completed scanning %zu of %zu files...\n", count, debugFiles->size());
				lastProgressUpdate = count;
			}

			string file = *it;
			size_t first = file.find_first_not_of(" \r\n\t");
			size_t last = file.find_last_not_of(" \r\n\t");
			file = file.substr(first, last - first + 1);
			const size_t len = strlen(file.c_str());
			wchar_t *filename = new wchar_t[len + 1];
			mbstowcs(filename, file.c_str(), len + 1);
			rc = loadDataFromPdb(filename, &diaDataSource, &diaSession, &diaSymbol);

			if (DDR_RC_OK == rc) {
				rc = addChildrenSymbols(diaSymbol, SymTagUDT, NULL);
			}

			if (DDR_RC_OK == rc) {
				rc = addChildrenSymbols(diaSymbol, SymTagEnum, NULL);
			}

			if (DDR_RC_OK == rc) {
				rc = addChildrenSymbols(diaSymbol, SymTagTypedef, NULL);
			}

			if (NULL != diaDataSource) {
				diaDataSource->Release();
				diaDataSource = NULL;
			}
			if (NULL != diaSession) {
				diaSession->Release();
				diaSession = NULL;
			}
			if (NULL != diaSymbol) {
				diaSymbol->Release();
				diaSymbol = NULL;
			}
			delete(filename);
			if (DDR_RC_OK != rc) {
				break;
			}
		}
	}
	/* Field and superclass types which are needed before the type is found
	 * are added to a postponed list. After all types are found, process the
	 * postponed list to add these missing references.
	 */
	if (DDR_RC_OK == rc) {
		rc = updatePostponedFieldNames();
	}
	if (DDR_RC_OK == rc) {
		renameAnonymousTypes();
	}
	CoUninitialize();

	return rc;
}

void
PdbScanner::renameAnonymousTypes()
{
	/* Anonymous types have the name format "<unnamed-type-[fieldName]".
	 * Instead of handling them in the Pdb scanner, add as much info to
	 * the IR as possible and let the output generators decide how to
	 * print it.
	 */
	ULONGLONG unnamedTypeCount = 0;
	for (vector<Type *>::iterator it = _ir->_types.begin(); it != _ir->_types.end(); it ++) {
		renameAnonymousType(*it, &unnamedTypeCount);
	}
}

void
PdbScanner::renameAnonymousType(Type *type, ULONGLONG *unnamedTypeCount)
{
	if ((string::npos != type->_name.find("<unnamed-type-")) || ("<unnamed-tag>" == type->_name)) {
		if ((NULL == type->getNamespace()) && (string::npos == type->_name.find("::"))) {
			/* Anonymous global types would ideally be named by file name,
			 * but PDB info does not associate types with source files.
			 * Since they also cannot be referenced by outer type, give them
			 * a placeholder name.
			 */
			stringstream ss;
			ss << "AnonymousType" << (*unnamedTypeCount) ++;
			type->_name = ss.str();
		} else {
			type->_name = "";
		}
	}
	if (NULL != type->getSubUDTS()) {
		for (vector<UDT *>::iterator it = type->getSubUDTS()->begin(); it != type->getSubUDTS()->end(); it ++) {
			renameAnonymousType(*it, unnamedTypeCount);
		}
	}
}

DDR_RC
PdbScanner::loadDataFromPdb(const wchar_t *filename, IDiaDataSource **dataSource, IDiaSession **session, IDiaSymbol **symbol)
{
	DDR_RC rc = DDR_RC_OK;
	/* Attemt to co-create the DiaSource instance. On failure to find the required
	 * dll, instead attempt to find and load the dll first.
	 */
	HRESULT hr = CoCreateInstance(__uuidof(DiaSource), NULL, CLSCTX_INPROC_SERVER, __uuidof(IDiaDataSource), (void **)dataSource);
	if (FAILED(hr)) {
		const char *libraries[] = {"MSDIA100", "MSDIA80", "MSDIA70", "MSDIA60"};
		rc = DDR_RC_ERROR;
		for (size_t i = 0; i < sizeof(libraries) / sizeof(char *); i += 1) {
			HMODULE hmodule = LoadLibrary(libraries[i]);
			BOOL (WINAPI *DllGetClassObject)(REFCLSID, REFIID, LPVOID) = NULL;
			if (NULL != hmodule) {
				DllGetClassObject = (BOOL (WINAPI *)(REFCLSID, REFIID, LPVOID))GetProcAddress(hmodule, "DllGetClassObject");
			} else {
				ERRMSG("Cannot find %s.dll\n", libraries[i]);
			}

			IClassFactory *classFactory = NULL;
			if (NULL != DllGetClassObject) {
				hr = DllGetClassObject(__uuidof(DiaSource), IID_IClassFactory, &classFactory);
			}
			if (SUCCEEDED(hr)) {
				hr = classFactory->CreateInstance(NULL, __uuidof(IDiaDataSource), (void**)dataSource);
			}
			if (SUCCEEDED(hr)) {
				rc = DDR_RC_OK;
				break;
			}
		}

		if (DDR_RC_OK != rc) {
			ERRMSG("Creating instance of IDiaDataSource failed with HRESULT = %08X\n", hr);
		}
	}

	if (DDR_RC_OK == rc) {
		hr = (*dataSource)->loadDataFromPdb(filename);
		if (FAILED(hr)) {
			ERRMSG("loadDataFromPdb() failed for file with HRESULT = %08X. Ensure the input is a pdb and not an exe.\nFile: %ls", hr, filename);
			rc = DDR_RC_ERROR;
		}
	}

	if (DDR_RC_OK == rc) {
		hr = (*dataSource)->openSession(session);
		if (FAILED(hr)) {
			ERRMSG("openSession() failed with HRESULT = %08x", hr);
			rc = DDR_RC_ERROR;
		}
	}

	if (DDR_RC_OK == rc) {
		hr = (*session)->get_globalScope(symbol);
		if (FAILED(hr)) {
			ERRMSG("get_globalScope() failed with HRESULT = %08x", hr);
			rc = DDR_RC_ERROR;
		}
	}

	return rc;
}

DDR_RC
PdbScanner::updatePostponedFieldNames()
{
	DDR_RC rc = DDR_RC_OK;

	/* Update field type references for fields which were
	 * processed before their type was added to the IR.
	 */
	for (vector<PostponedType>::iterator it = _postponedFields.begin(); it != _postponedFields.end(); it ++) {
		Type **type = it->type;
		if (_typeMap.end() != _typeMap.find(it->name)) {
			*type = _typeMap[it->name];
		} else {
			*type = new ClassUDT(0);
			(*type)->_name = it->name;
		}
	}

	return rc;
}

DDR_RC
PdbScanner::addChildrenSymbols(IDiaSymbol *symbol, enum SymTagEnum symTag, NamespaceUDT *outerNamespace)
{
	DDR_RC rc = DDR_RC_OK;
	IDiaEnumSymbols *classSymbols = NULL;

	/* Find children symbols. SymTagUDT covers struct, union, and class symbols. */
	HRESULT hr = symbol->findChildren(symTag, NULL, nsNone, &classSymbols);
	if (FAILED(hr)) {
		ERRMSG("findChildren() failed with HRESULT = %08X", hr);
		rc = DDR_RC_ERROR;
	}

	/* Allocate space for the children symbols. */
	LONG count = 0;
	if (DDR_RC_OK == rc) {
		hr = classSymbols->get_Count(&count);
		if (FAILED(hr)) {
			ERRMSG("Failed to get count of children symbols with HRESULT = %08X", hr);
			rc = DDR_RC_ERROR;
		}
	}
	IDiaSymbol **childSymbols = NULL;
	ULONG celt = 0;
	if ((DDR_RC_OK == rc) && (0 != count)) {
		childSymbols = new IDiaSymbol*[count];
		hr = classSymbols->Next(count, childSymbols, &celt);
		if (FAILED(hr)) {
			ERRMSG("Failed to get children symbols with HRESULT = %08X", hr);
			rc = DDR_RC_ERROR;
		}
	}

	/* Iterate the children symbols, adding them to the IR.
	 * Inner symbols are first found with a decorated name and no
	 * parent reference. Ignore these for now and add the outer
	 * types first.
	 */
	vector<ULONG> innerTypeSymbols;
	if (DDR_RC_OK == rc) {
		bool alreadyHadFields = false;
		bool alreadyCheckedFields = false;
		bool alreadyHadSubtypes = false;
		if (NULL != outerNamespace) {
			alreadyHadSubtypes = !outerNamespace->_subUDTs.empty();
		}
		for (ULONG index = 0; index < celt; index += 1) {
			string udtName = "";
			DWORD symTag = 0;
			hr = childSymbols[index]->get_symTag(&symTag);
			if (FAILED(hr)) {
				rc = DDR_RC_ERROR;
				ERRMSG("Failed to get child symbol SymTag with HRESULT = %08X", hr);
			}
			if ((DDR_RC_OK == rc) && ((SymTagEnum == symTag) || (SymTagUDT == symTag))) {
				rc = getName(childSymbols[index], &udtName);
			}
			if (DDR_RC_OK == rc) {
				if ((NULL == outerNamespace) || (string::npos == udtName.find("::"))) {
					/* When adding fields/subtypes to a type, only add fields to a type with no fields
					 * and only add subtypes to a type with no subtypes. This avoids adding duplicates
					 * fields/subtypes when scanning multiple files. Children symbols must be processed
					 * for already existing symbols because fields and subtypes can appear separately.
					 */
					if (!alreadyCheckedFields && (SymTagData == symTag)) {
						alreadyCheckedFields = true;
						alreadyHadFields = !((ClassType *)outerNamespace)->_fieldMembers.empty();
					}
					if ((SymTagData == symTag && !alreadyHadFields) || (SymTagData != symTag && !alreadyHadSubtypes)) {
						addSymbol(childSymbols[index], outerNamespace);
					}
					childSymbols[index]->Release();
				} else {
					innerTypeSymbols.push_back(index);
				}
			}
			if (DDR_RC_OK != rc) {
				break;
			}
		}
	}

	/* Iterate the inner types. Only namespaces will be added to the IR
	 * here, since they have no associated symbol.
	 */
	if (DDR_RC_OK == rc) {
		for (vector<ULONG>::iterator it = innerTypeSymbols.begin(); it != innerTypeSymbols.end() && DDR_RC_OK == rc; it ++) {
			rc = addSymbol(childSymbols[*it], NULL);
			childSymbols[*it]->Release();
		}
	}

	if (NULL != childSymbols) {
		delete(childSymbols);
	}
	if (NULL != classSymbols) {
		classSymbols->Release();
	}
	return rc;
}

DDR_RC
PdbScanner::createTypedef(IDiaSymbol *symbol, NamespaceUDT *outerNamespace)
{
	DDR_RC rc = DDR_RC_OK;
	/* Get the typedef name and check if it is blacklisted. */
	string typedefName = "";
	rc = getName(symbol, &typedefName);

	if ((DDR_RC_OK == rc) && (!checkBlacklistedType(typedefName))) {
		/* Get the typedef's type's name to check the blacklist. */
		IDiaSymbol *baseSymbol = symbol;
		DWORD symTag = 0;
		while (SymTagUDT != symTag
			&& SymTagEnum != symTag
			&& SymTagBaseType != symTag
			&& SymTagFunctionType != symTag
		) {
			/* Ignore sym tags Array and Pointer to find the actual type of the typedef.
			 * setType() has similar logic, but black listed types would not be found
			 * and errenously added to the postponed type list.
			 */
			HRESULT hr = baseSymbol->get_type(&baseSymbol);
			if (FAILED(hr)) {
				rc = DDR_RC_ERROR;
				ERRMSG("Failed to get type of typedef");
			}
			if (DDR_RC_OK == rc) {
				hr = baseSymbol->get_symTag(&symTag);
				if (FAILED(hr)) {
					ERRMSG("get_symTag() failed with HRESULT = %08X", hr);
					rc = DDR_RC_ERROR;
				}
			}
		}
		/* NOTE: Cannot get the name of function and base types -- attempting to causes crash. */
		string baseName = "";
		if ((DDR_RC_OK == rc) && ((SymTagUDT == symTag) || (SymTagEnum == symTag))) {
			rc = getName(baseSymbol, &baseName);
		}
		if (DDR_RC_OK == rc) {
			if (!checkBlacklistedType(baseName)) {
				TypedefUDT *newTypedef = new TypedefUDT();
				newTypedef->_name = typedefName;
				newTypedef->_aliasedType = NULL;
				newTypedef->_outerNamespace = outerNamespace;
				if (NULL != outerNamespace) {
					outerNamespace->_subUDTs.push_back(newTypedef);
				}
				/* Get the typedef type. */
				rc = setType(symbol, &newTypedef->_aliasedType, &newTypedef->_modifiers, NULL);
				if (DDR_RC_OK == rc) {
					newTypedef->_sizeOf = newTypedef->_aliasedType->_sizeOf;
					addType(newTypedef, NULL == outerNamespace);
				} else {
					delete(newTypedef);
				}
			}
		}
		if (NULL != baseSymbol) {
			baseSymbol->Release();
		}
	}
	return rc;
}

DDR_RC
PdbScanner::addEnumMembers(IDiaSymbol *symbol, EnumUDT *const enumUDT)
{
	DDR_RC rc = DDR_RC_OK;

	/* All children symbols of a symbol for an Enum type should be
	 * enum members.
	 */
	IDiaEnumSymbols *enumSymbols = NULL;
	HRESULT hr = symbol->findChildren(SymTagNull, NULL, nsNone, &enumSymbols);
	if (FAILED(hr)) {
		ERRMSG("findChildren() failed with HRESULT = %08X", hr);
		rc = DDR_RC_ERROR;
	}

	if (DDR_RC_OK == rc) {
		IDiaSymbol *childSymbol = NULL;
		ULONG celt = 0;
		LONG count = 0;
		enumSymbols->get_Count(&count);
		enumUDT->_enumMembers.reserve(count);
		while (SUCCEEDED(enumSymbols->Next(1, &childSymbol, &celt))
			&& (1 == celt)
			&& (DDR_RC_OK == rc)
		) {
			string name = "";
			rc = getName(childSymbol, &name);

			if (DDR_RC_OK == rc) {
				EnumMember *enumMember = new EnumMember();
				enumMember->_name = name;
				enumUDT->_enumMembers.push_back(enumMember);
			}
			childSymbol->Release();
		}

		enumSymbols->Release();
	}

	return rc;
}

DDR_RC
PdbScanner::createEnumUDT(IDiaSymbol *symbol, NamespaceUDT *outerNamespace)
{
	DDR_RC rc = DDR_RC_OK;

	/* Verify the given symbol is for an enum. */
	DWORD symTag = 0;
	HRESULT hr = symbol->get_symTag(&symTag);
	if (FAILED(hr)) {
		ERRMSG("get_symTag() failed with HRESULT = %08X", hr);
		rc = DDR_RC_ERROR;
	} else if (SymTagEnum != symTag) {
		ERRMSG("symTag is not Enum");
		rc = DDR_RC_ERROR;
	}

	string name = "";
	if (DDR_RC_OK == rc) {
		 rc = getName(symbol, &name);
	}

	if (DDR_RC_OK == rc) {
		/* Sub enums are added by their undecorated name when found as a
		 * child symbol of another UDT symbol. They are also found with
		 * a decorated "Parent::SubUDT" name again while iterating all Enums.
		 */
		if (!checkBlacklistedType(name)) {
			ULONGLONG size = 0ULL;
			if (DDR_RC_OK == rc) {
				rc = getSize(symbol, &size);
			}
			if (DDR_RC_OK == rc) {
				string fullName = "";
				if (NULL == outerNamespace) {
					fullName = name;
				} else {
					fullName = outerNamespace->_name + "::" + name;
				}
				if (fullName.empty() || (_typeMap.end() == _typeMap.find(fullName)) || ("<unnamed-tag>" == name)) {
					getNamespaceFromName(&name, &outerNamespace);

					/* If this is a new enum, get its members and add it to the IR. */
					EnumUDT *enumUDT = new EnumUDT();
					enumUDT->_name = name;
					rc = addEnumMembers(symbol, enumUDT);

					/* If successful, add the new enum to the IR. */
					if (DDR_RC_OK == rc) {
						enumUDT->_outerNamespace = outerNamespace;
						if (NULL != outerNamespace) {
							outerNamespace->_subUDTs.push_back(enumUDT);
						}
						addType(enumUDT, NULL == outerNamespace);
					} else {
						delete(enumUDT);
					}
				} else if (_typeMap.end() != _typeMap.find(fullName)) {
					EnumUDT *enumUDT = (EnumUDT *)(getType(fullName));
					if ((NULL == enumUDT->_outerNamespace) && (NULL != outerNamespace)) {
						enumUDT->_outerNamespace = outerNamespace;
						outerNamespace->_subUDTs.push_back(enumUDT);
					}
					if (enumUDT->_enumMembers.empty()) {
						rc = addEnumMembers(symbol, enumUDT);
					}
				}
			}
		}
	}

	return rc;
}

DDR_RC
PdbScanner::setMemberOffset(IDiaSymbol *symbol, Field *newField)
{
	DDR_RC rc = DDR_RC_OK;
	size_t offset = 0;
	DWORD locType = 0;
	HRESULT hr = symbol->get_locationType(&locType);
	if (FAILED(hr)) {
		ERRMSG("Symbol in optimized code");
		rc = DDR_RC_ERROR;
	}

	if (DDR_RC_OK == rc) {
		switch (locType) {
		case LocIsThisRel:
		{
			LONG loffset = 0;
			hr = symbol->get_offset(&loffset);
			if (FAILED(hr)) {
				ERRMSG("get_offset() failed with HRESULT = %08X", hr);
				rc = DDR_RC_ERROR;
			} else {
				offset = (size_t)loffset;
			}
			break;
		}
		case LocIsStatic:
		{
			/* Get offset of static class members. */
			newField->_isStatic = true;
			LONG loffset = 0;
			hr = symbol->get_offset(&loffset);
			if (SUCCEEDED(hr)) {
				offset = (size_t)loffset;
			}
			break;
		}
		case LocIsBitField:
		{
			LONG loffset = 0;
			hr = symbol->get_offset(&loffset);
			if (FAILED(hr)) {
				ERRMSG("get_offset() failed with HRESULT = %08X", hr);
				rc = DDR_RC_ERROR;
			} else {
				offset = (size_t)loffset;
			}
			if (DDR_RC_OK == rc) {
				DWORD bitposition = 0;
				hr = symbol->get_bitPosition(&bitposition);
				if (FAILED(hr)) {
					ERRMSG("get_offset() failed with HRESULT = %08X", hr);
					rc = DDR_RC_ERROR;
				} else {
					newField->_bitField = bitposition;
				}
			}
			break;
		}
		default:
			ERRMSG("Unknown offset type: %d, name: %s", locType, newField->_name.c_str());
			rc = DDR_RC_ERROR;
		}
	}

	if (DDR_RC_OK == rc) {
		newField->_offset = offset;
	}

	return rc;
}

DDR_RC
PdbScanner::setTypeModifier(IDiaSymbol *symbol, Modifiers *modifiers)
{
	DDR_RC rc = DDR_RC_OK;
	/* Get const, volatile, and unaligned type modifiers for a field. */
	BOOL bSet = TRUE;
	HRESULT hr = symbol->get_constType(&bSet);
	if (FAILED(hr)) {
		ERRMSG("get_constType() failed with HRESULT = %08X", hr);
		rc = DDR_RC_ERROR;
	} else if (bSet) {
		modifiers->_modifierFlags |= Modifiers::CONST_TYPE;
	}

	if (DDR_RC_OK == rc) {
		hr = symbol->get_volatileType(&bSet);
		if (FAILED(hr)) {
			ERRMSG("get_volatileType() failed with HRESULT = %08X", hr);
			rc = DDR_RC_ERROR;
		} else if (bSet) {
			modifiers->_modifierFlags |= Modifiers::VOLATILE_TYPE;
		}
	}

	if (DDR_RC_OK == rc) {
		hr = symbol->get_unalignedType(&bSet);
		if (FAILED(hr)) {
			ERRMSG("get_unalignedType() failed with HRESULT = %08X", hr);
			rc = DDR_RC_ERROR;
		} else if (bSet) {
			modifiers->_modifierFlags |= Modifiers::UNALIGNED_TYPE;
		}
	}
	return rc;
}

DDR_RC
PdbScanner::setTypeUDT(IDiaSymbol *typeSymbol, Type **type, NamespaceUDT *outerUDT)
{
	DDR_RC rc = DDR_RC_OK;

	/* Get the type for a field. */
	DWORD udtKind = 0;
	HRESULT hr = typeSymbol->get_udtKind(&udtKind);
	if (FAILED(hr)) {
		ERRMSG("get_udtKind() failed with HRESULT = %08X", hr);
		rc = DDR_RC_ERROR;
	}

	string name = "";
	if (DDR_RC_OK == rc) {
		rc = getName(typeSymbol, &name);
	}
	if (DDR_RC_OK == rc) {
		if (!name.empty() && _typeMap.end() != _typeMap.find(name)) {
			*type = _typeMap[name];
		} else if (("<unnamed-tag>" == name) && (NULL != outerUDT)) {
			/* Anonymous inner union UDTs are missing the parent
			 * relationship and cannot be added later.
			 */
			rc = createClassUDT(typeSymbol, outerUDT);
			if (DDR_RC_OK == rc) {
				*type = outerUDT->_subUDTs.back();
				(*type)->_name = "";
			}
		} else if (!name.empty()) {
			PostponedType p = {type, name};
			_postponedFields.push_back(p);
		}
	}

	return rc;
}

DDR_RC
PdbScanner::setPointerType(IDiaSymbol *symbol, Modifiers *modifiers)
{
	DDR_RC rc = DDR_RC_OK;
	/* Get the pointer and reference count for a field. */
	BOOL bSet = TRUE;
	HRESULT hr = symbol->get_reference(&bSet);
	if (FAILED(hr)) {
		ERRMSG("get_reference failed with HRESULT = %08X", hr);
		rc = DDR_RC_ERROR;
	} else if (bSet) {
		modifiers->_referenceCount++;
	} else {
		modifiers->_pointerCount++;
	}

	return rc;
}

Type *
PdbScanner::getType(string s)
{
	/* Get a type in the map by name. This used only for base types. */
	Type *type = NULL;
	if (!s.empty() && _typeMap.end() != _typeMap.find(s)) {
		type = _typeMap[s];
	}

	return type;
}

DDR_RC
PdbScanner::setBaseTypeInt(ULONGLONG ulLen, Type **type)
{
	DDR_RC rc = DDR_RC_OK;
	/* Set a field type to an integer base type depending on its size. */
	switch (ulLen) {
	case 1:
		*type = getType("I8"); /* This could also be a signed char. */
		break;
	case 2:
		*type = getType("I16"); /* This also could be a short int. */
		break;
	case 4:
		*type = getType("I32");
		break;
	case 8:
		*type = getType("I64");
		break;
	default:
		ERRMSG("Unknown int length: %zu", ulLen);
		rc = DDR_RC_ERROR;
		break;
	}

	return rc;
}

DDR_RC
PdbScanner::setBaseTypeFloat(ULONGLONG ulLen, Type **type)
{
	DDR_RC rc = DDR_RC_OK;
	/* Set a field type to a float/double base type depending on its size. */
	switch (ulLen) {
	case 4:
		*type = getType("float"); /* This could also be an unsigned char */
		break;
	case 8:
		*type = getType("double");
		break;
	default:
		ERRMSG("Unknown float length: %zu", ulLen);
		rc = DDR_RC_ERROR;
		break;
	}

	return rc;
}


DDR_RC
PdbScanner::setBaseTypeUInt(ULONGLONG ulLen, Type **type)
{
	DDR_RC rc = DDR_RC_OK;
	/* Set a field type to an unsigned integer base type depending on its size. */
	switch (ulLen) {
	case 1:
		*type = getType("U8"); /* This could also be an unsigned char. */
		break;
	case 2:
		*type = getType("U16");
		break;
	case 4:
		*type = getType("U32");
		break;
	case 8:
		*type = getType("U64");
		break;
	case 16:
		*type = getType("U128");
		break;
	default:
		ERRMSG("Unknown int length: %zu", ulLen);
		rc = DDR_RC_ERROR;
		break;
	}

	return rc;
}

DDR_RC
PdbScanner::setBaseType(IDiaSymbol *typeSymbol, Type **type)
{
	/* Select a base type from the map for a field based on its
	 * Pdb base type and size.
	 */
	DDR_RC rc = DDR_RC_OK;
	DWORD baseType = 0;
	HRESULT hr = typeSymbol->get_baseType(&baseType);
	if (FAILED(hr)) {
		ERRMSG("get_baseType() failed with HRESULT = %08X", hr);
		rc = DDR_RC_ERROR;
	}

	ULONGLONG ulLen = 0ULL;
	if (DDR_RC_OK == rc) {
		rc = getSize(typeSymbol, &ulLen);
	}

	if (DDR_RC_OK == rc) {
		switch (baseType) {
		case btUInt:
			rc = setBaseTypeUInt(ulLen, type);
			baseType = 0xFFFFFFFF;
			break;
		case btInt:
			rc = setBaseTypeInt(ulLen, type);
			baseType = 0xFFFFFFFF;
			break;
		case btFloat:
			rc = setBaseTypeFloat(ulLen, type);
			baseType = 0xFFFFFFFF;
			break;
		}

		if (0xFFFFFFFF != baseType) {
			*type = getType(string(baseTypeArray[baseType])); /* This could also be an unsigned char */
		}
	}

	return rc;
}

static const char *
symTagToString(DWORD value)
{
	switch (value) {
	case SymTagNull:
		return "SymTagNull";
	case SymTagExe:
		return "SymTagExe";
	case SymTagCompiland:
		return "SymTagCompiland";
	case SymTagCompilandDetails:
		return "SymTagCompilandDetails";
	case SymTagCompilandEnv:
		return "SymTagCompilandEnv";
	case SymTagFunction:
		return "SymTagFunction";
	case SymTagBlock:
		return "SymTagBlock";
	case SymTagData:
		return "SymTagData";
	case SymTagAnnotation:
		return "SymTagAnnotation";
	case SymTagLabel:
		return "SymTagLabel";
	case SymTagPublicSymbol:
		return "SymTagPublicSymbol";
	case SymTagUDT:
		return "SymTagUDT";
	case SymTagEnum:
		return "SymTagEnum";
	case SymTagFunctionType:
		return "SymTagFunctionType";
	case SymTagPointerType:
		return "SymTagPointerType";
	case SymTagArrayType:
		return "SymTagArrayType";
	case SymTagBaseType:
		return "SymTagBaseType";
	case SymTagTypedef:
		return "SymTagTypedef";
	case SymTagBaseClass:
		return "SymTagBaseClass";
	case SymTagFriend:
		return "SymTagFriend";
	case SymTagFunctionArgType:
		return "SymTagFunctionArgType";
	case SymTagFuncDebugStart:
		return "SymTagFuncDebugStart"; 
	case SymTagFuncDebugEnd:
		return "SymTagFuncDebugEnd";
	case SymTagUsingNamespace:
		return "SymTagUsingNamespace";
	case SymTagVTableShape:
		return "SymTagVTableShape";
	case SymTagVTable:
		return "SymTagVTable";
	case SymTagCustom:
		return "SymTagCustom";
	case SymTagThunk:
		return "SymTagThunk";
	case SymTagCustomType:
		return "SymTagCustomType";
	case SymTagManagedType:
		return "SymTagManagedType";
	case SymTagDimension:
		return "SymTagDimension";
	/* Note: The following are not present in all versions. */
	/*case SymTagCallSite:
		return "SymTagCallSite";
	case SymTagInlineSite:
		return "SymTagInlineSite";
	case SymTagBaseInterface:
		return "SymTagBaseInterface";
	case SymTagVectorType:
		return "SymTagVectorType";
	case SymTagMatrixType:
		return "SymTagMatrixType";
	case SymTagHLSLType:
		return "SymTagHLSLType";*/
	};
	return "value " + value;
}

DDR_RC
PdbScanner::setType(IDiaSymbol *symbol, Type **type, Modifiers *modifiers, NamespaceUDT *outerUDT)
{
	DDR_RC rc = DDR_RC_OK;
	/* Get all type information, such as type and modifiers, for abort
	 * field symbol.
	 */
	IDiaSymbol *typeSymbol = NULL;
	HRESULT hr = symbol->get_type(&typeSymbol);
	if (FAILED(hr)) {
		ERRMSG("get_type failed with HRESULT = %08X", hr);
		rc = DDR_RC_ERROR;
	}

	if (DDR_RC_OK == rc) {
		rc = setTypeModifier(typeSymbol, modifiers);
	}

	DWORD symTag = 0;
	if (DDR_RC_OK == rc) {
		hr = typeSymbol->get_symTag(&symTag);
		if (FAILED(hr)) {
			ERRMSG("get_symTag failed with HRESULT = %08X", hr);
			typeSymbol->Release();
			rc = DDR_RC_ERROR;
		}
	}

	if (DDR_RC_OK == rc) {
		switch (symTag) {
		case SymTagEnum:
		case SymTagUDT:
			rc = setTypeUDT(typeSymbol, type, outerUDT);
			break;
		case SymTagArrayType:
		{
			DWORD size = 0;
			if (FAILED(typeSymbol->get_count(&size))) {
				ERRMSG("Failed to get array dimensions.");
				rc = DDR_RC_ERROR;
			} else {
				modifiers->addArrayDimension(size);
				rc = setType(typeSymbol, type, modifiers, outerUDT);
			}
			break;
		}
		case SymTagPointerType:
			rc = setPointerType(symbol, modifiers);
			if (DDR_RC_OK == rc) {
				rc = setType(typeSymbol, type, modifiers, outerUDT);
			}
			break;
		case SymTagBaseType:
			rc = setBaseType(typeSymbol, type);
			break;
		case SymTagFunctionType:
			*type = getType("void");
			break;
		default:
			ERRMSG("unknown symtag: %d", symTag);
			rc = DDR_RC_ERROR;
		}
	}

	typeSymbol->Release();
	return rc;
}

DDR_RC
PdbScanner::getName(IDiaSymbol *symbol, string *name)
{
	DDR_RC rc = DDR_RC_OK;
	/* Attempt to get the name associated with a symbol. */
	BSTR nameBstr;
	HRESULT hr = symbol->get_name(&nameBstr);
	if (FAILED(hr)) {
		ERRMSG("get_name failed with HRESULT = %08X", hr);
		rc = DDR_RC_ERROR;
	} else {
		char *nameChar = _com_util::ConvertBSTRToString(nameBstr);
		if (NULL == nameChar) {
			ERRMSG("Could not convert Bstr");
		} else {
			*name = string(nameChar);
			size_t pos = name->find("`anonymous-namespace'::");
			if (string::npos == pos) {
				pos = name->find("`anonymous namespace'::");
			}
			if (string::npos != pos) {
				*name = name->replace(pos, 23, "");
			}
			free(nameChar);
		}
		SysFreeString(nameBstr);
	}

	return rc;
}

DDR_RC
PdbScanner::getSize(IDiaSymbol *symbol, ULONGLONG *size)
{
	DDR_RC rc = DDR_RC_OK;
	/* Attempt to get the size associated with a symbol. */
	ULONGLONG ul = 0ULL;
	HRESULT hr = symbol->get_length(&ul);
	if (FAILED(hr)) {
		ERRMSG("get_length() failed for symbol with HRESULT = %08X", hr);
		rc = DDR_RC_ERROR;
	} else {
		*size = ul;
	}
	return rc;
}

DDR_RC
PdbScanner::addFieldMember(IDiaSymbol *symbol, ClassUDT *const udt)
{
	DDR_RC rc = DDR_RC_OK;
	/* Add a new field to a class. Find its name, type, size,
	 * modifiers, and offset.
	 */
	Field *newField = new Field();
	rc = getName(symbol, &newField->_name);

	if (DDR_RC_OK == rc) {
		rc = setMemberOffset(symbol, newField);
	}
	if (DDR_RC_OK == rc) {
		rc = setType(symbol, &newField->_fieldType, &newField->_modifiers, udt);
	}
	if (DDR_RC_OK == rc) {
		udt->_fieldMembers.push_back(newField);
	}

	return rc;
}

DDR_RC
PdbScanner::setSuperClassName(IDiaSymbol *symbol, ClassUDT *newUDT)
{
	string name = "";
	DDR_RC rc = getName(symbol, &name);

	if (DDR_RC_OK == rc) {
		/* Find the superclass UDT from the map by size and name.
		 * If its not found, add it to a list to check later.
		 */
		if (!name.empty()) {
			if (_typeMap.end() == _typeMap.find(name)) {
				PostponedType p = {(Type **)&newUDT->_superClass, name};
				_postponedFields.push_back(p);
			} else {
				newUDT->_superClass = (ClassUDT *)_typeMap[name];
			}
		}
	}
	return DDR_RC_OK;
}

DDR_RC
PdbScanner::createClassUDT(IDiaSymbol *symbol, NamespaceUDT *outerUDT)
{
	DDR_RC rc = DDR_RC_OK;

	/* Verify this symbol is for a UDT. */
	DWORD symTag = 0;
	HRESULT hr = symbol->get_symTag(&symTag);
	if (FAILED(hr)) {
		ERRMSG("get_symTag() failed with HRESULT = %08X", hr);
		rc = DDR_RC_ERROR;
	} else if (SymTagUDT != symTag) {
		ERRMSG("symTag is unexpectedly not UDT");
		rc = DDR_RC_ERROR;
	}

	string name = "";
	if (DDR_RC_OK == rc) {
		rc = getName(symbol, &name);
	}

	if (DDR_RC_OK == rc) {
		/* Sub UDT's are added by their undecorated name when found as a
		 * child symbol of another UDT symbol. They are also found with
		 * a decorated "Parent::SubUDT" name again while iterating all UDTs.
		 */
		if (!checkBlacklistedType(name)) {
			ULONGLONG size = 0ULL;
			if (DDR_RC_OK == rc) {
				rc = getSize(symbol, &size);
			}
			if (DDR_RC_OK == rc) {
				string fullName = "";
				if (NULL == outerUDT) {
					fullName = name;
				} else {
					fullName = outerUDT->_name + "::" + name;
				}
				if (fullName.empty() || (_typeMap.end() == _typeMap.find(fullName)) || ("<unnamed-tag>" == name)) {
					getNamespaceFromName(&name, &outerUDT);

					ClassUDT *classUDT = new ClassUDT(0);
					classUDT->_sizeOf = (size_t)size;
					classUDT->_name = name;
					classUDT->_outerNamespace = outerUDT;
					if (NULL != outerUDT) {
						outerUDT->_subUDTs.push_back(classUDT);
					}
					rc = addChildrenSymbols(symbol, SymTagNull, classUDT); 

					/* If successful, add the new UDT to the IR. Otherwise, free it. */
					if (DDR_RC_OK == rc) {
						addType(classUDT, NULL == classUDT->_outerNamespace);
					} else {
						delete(classUDT);
						classUDT = NULL;
					}
				} else if (_typeMap.end() != _typeMap.find(fullName)) {
					/* If a type of this name has already been added before, it may
					 * have been either as a class or a typedef of a class.
					 */
					Type *previouslyCreatedType = getType(fullName);
					while (NULL != previouslyCreatedType->getBaseType()) {
						previouslyCreatedType = previouslyCreatedType->getBaseType();
					}
					ClassUDT *classUDT = (ClassUDT *)previouslyCreatedType;
					if ((NULL == classUDT->_outerNamespace) && (NULL != outerUDT)) {
						classUDT->_outerNamespace = outerUDT;
						outerUDT->_subUDTs.push_back(classUDT);
					}
					rc = addChildrenSymbols(symbol, SymTagNull, classUDT);
				}
			}
		}
	}

	return rc;
}

void
PdbScanner::getNamespaceFromName(string *name, NamespaceUDT **outerUDT)
{
	/* In Pdb info, namespaces do not have their own symbol. If a symbol has
	 * a name of the format "OuterType::InnerType" and no separate symbol for
	 * the outer type, it could be a namespace or a class that wasn't found
	 * yet.
	 */
	if (NULL == *outerUDT) {
		size_t pos = 0;
		size_t previousPos = 0;
		NamespaceUDT *outerNamespace = NULL;
		while (string::npos != pos) {
			pos = name->find("::", previousPos);
			if (string::npos != pos) {
				string namespaceName = name->substr(previousPos, pos - previousPos);
				NamespaceUDT *ns = (NamespaceUDT *)getType(namespaceName);
				if (NULL == ns) {
					/* If no previously created type exists, create class for it.
					 * It could be a class or a namespace, but class derives from
					 * namespace and thus works for both cases.
					 */
					ns = new ClassUDT(0);
					ns->_name = namespaceName;
					ns->_outerNamespace = outerNamespace;
					addType(ns, previousPos == 0);
				}
				if ((NULL != outerNamespace) && (NULL == ns->_outerNamespace)) {
					ns->_outerNamespace = outerNamespace;
					outerNamespace->_subUDTs.push_back(ns);
					if (_ir->_types.end() != find(_ir->_types.begin(), _ir->_types.end(), ns)) {
						_ir->_types.erase(remove(_ir->_types.begin(), _ir->_types.end(), ns));
					}
				}

				outerNamespace = ns;
				previousPos = pos + 2;
			}
		}
		*name = name->substr(previousPos);
		*outerUDT = outerNamespace;
	}
}

DDR_RC
PdbScanner::addSymbol(IDiaSymbol *symbol, NamespaceUDT *outerNamespace)
{
	DDR_RC rc = DDR_RC_OK;
	DWORD symTag = 0;
	HRESULT hr = symbol->get_symTag(&symTag);
	if (FAILED(hr)) {
		ERRMSG("get_symTag() failed with HRESULT = %08X", hr);
		rc = DDR_RC_ERROR;
	}

	if (DDR_RC_OK == rc) {
		switch (symTag) {
		case SymTagEnum:
			rc = createEnumUDT(symbol, outerNamespace);
			break;
		case SymTagUDT:
			rc = createClassUDT(symbol, outerNamespace);
			break;
		case SymTagData:
			rc = addFieldMember(symbol, (ClassUDT *)outerNamespace);
			break;
		case SymTagBaseClass:
			rc = setSuperClassName(symbol, (ClassUDT *)outerNamespace);
			break;
		case SymTagVTableShape:
		case SymTagVTable:
		case SymTagFunction:
			/* Do nothing. */
			break;
		case SymTagTypedef:
			/* At global scope, all typedefs do not have a decorated name showing
			 * an outer type. Processing only typedefs which are children to
			 * another type would miss most of them. Instead, process all typedefs
			 * as if they had global scope and do not process them again if found
			 * as an inner type.
			 */
			if (NULL == outerNamespace) {
				createTypedef(symbol, outerNamespace);
			}
			break;
		default:
			ERRMSG("Unhandled symbol returned by get_symTag: %s", symTagToString(symTag));
			rc = DDR_RC_ERROR;
		}
	}

	return rc;
}

string
PdbScanner::getUDTname(UDT *u)
{
	assert(NULL != u);

	string name = "";
	if (NULL != u->_outerNamespace) {
		name = getUDTname(u->_outerNamespace) + "::" + u->_name;
	} else {
		name = u->_name;
	}

	return name;
}
