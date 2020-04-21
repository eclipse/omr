/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

#include "ddr/scanner/pdb/PdbScanner.hpp"

#include <algorithm>
#include <assert.h>
#include <comdef.h>
#if !defined(OMR_OS_WINDOWS)
#include <inttypes.h>
#endif /* !defined(OMR_OS_WINDOWS) */
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

static const char * const baseTypeArray[] = {
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
	"U32", /* This should be unsigned long */
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

void
PdbScanner::addType(UDT *type, NamespaceUDT *outerNamespace)
{
	/* Add the type to the map of found types. Subtypes should
	 * not be added to the IR. Types are mapped by their size and
	 * name to be referenced when used as a field.
	 */
	string fullName = type->getFullName();
	if (!fullName.empty() && _typeMap.end() == _typeMap.find(fullName)) {
		_typeMap[fullName] = type;
	}
	if (NULL != outerNamespace) {
		outerNamespace->_subUDTs.push_back(type);
	} else {
		_ir->_types.push_back(type);
	}
}

void
PdbScanner::initBaseTypeList()
{
	/* Add the base types to the typemap and to the IR. */
	size_t length = sizeof(baseTypeArray) / sizeof(*baseTypeArray);
	for (size_t i = 0; i < length; ++i) {
		Type *type = new Type(0);
		type->_name = baseTypeArray[i];
		_typeMap[type->_name] = type;
		_ir->_types.push_back(type);
	}
}

DDR_RC
PdbScanner::startScan(OMRPortLibrary *portLibrary, Symbol_IR *ir, vector<string> *debugFiles, const char *blacklistPath)
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

	if (DDR_RC_OK == rc) {
		rc = loadBlacklist(portLibrary, blacklistPath);
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
		for (vector<string>::iterator it = debugFiles->begin(); it != debugFiles->end(); ++it) {
			const string &file = *it;
			const size_t len = file.length();
			wchar_t *filename = new wchar_t[len + 1];
			mbstowcs(filename, file.c_str(), len + 1);
			rc = loadDataFromBinary(filename, &diaDataSource, &diaSession, &diaSymbol);

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
			delete[] filename;
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

	CoUninitialize();

	return rc;
}

DDR_RC
PdbScanner::loadDataFromBinary(const wchar_t *filename, IDiaDataSource **dataSource, IDiaSession **session, IDiaSymbol **symbol)
{
	DDR_RC rc = DDR_RC_OK;
	bool isPDBFile = false;
	/*  get location of the file extension */
	const wchar_t *fileExtension = wcsrchr(filename, L'.');

	/* check if file extension is '.pdb' */
	if ((NULL!= fileExtension) && (0 == wcscmp(fileExtension, L".pdb"))){
		isPDBFile = true;
	}

	/* Attemt to co-create the DiaSource instance. On failure to find the required
	 * dll, instead attempt to find and load the dll first.
	 */
	HRESULT hr = CoCreateInstance(__uuidof(DiaSource), NULL, CLSCTX_INPROC_SERVER, __uuidof(IDiaDataSource), (void **)dataSource);
	if (FAILED(hr)) {
		ERRMSG("CoCreateInstance failed with HRESULT = %08lX", hr);
		static const char * const libraries[] = { "MSDIA140", "MSDIA120", "MSDIA100", "MSDIA80", "MSDIA70", "MSDIA60" };
		rc = DDR_RC_ERROR;
		for (size_t i = 0; i < sizeof(libraries) / sizeof(*libraries); ++i) {
			HMODULE hmodule = LoadLibrary(libraries[i]);
			if (NULL == hmodule) {
				ERRMSG("LoadLibrary failed for %s.dll", libraries[i]);
				continue;
			}
			BOOL (WINAPI *DllGetClassObject)(REFCLSID, REFIID, LPVOID) =
					(BOOL (WINAPI *)(REFCLSID, REFIID, LPVOID))
					GetProcAddress(hmodule, "DllGetClassObject");
			if (NULL == DllGetClassObject) {
				ERRMSG("Could not find DllGetClassObject in %s.dll", libraries[i]);
				continue;
			}
			IClassFactory *classFactory = NULL;
			hr = DllGetClassObject(__uuidof(DiaSource), IID_IClassFactory, &classFactory);
			if (FAILED(hr)) {
				ERRMSG("DllGetClassObject failed with HRESULT = %08lX", hr);
				continue;
			}
			hr = classFactory->CreateInstance(NULL, __uuidof(IDiaDataSource), (void **)dataSource);
			if (FAILED(hr)) {
				ERRMSG("CreateInstance failed with HRESULT = %08lX", hr);
			} else {
				ERRMSG("Instance of IDiaDataSource created using %s.dll", libraries[i]);
				rc = DDR_RC_OK;
				break;
			}
		}
	}

	if (DDR_RC_OK == rc) {
		if(isPDBFile){
			hr = (*dataSource)->loadDataFromPdb(filename);
		} else {
			hr = (*dataSource)->loadDataForExe(filename, NULL, NULL);
		}
		if (FAILED(hr)) {
			ERRMSG("loadDataFromPdb() failed for file with HRESULT = %08lX.\nFile: %ls", hr, filename);
			rc = DDR_RC_ERROR;
		}
	}

	if (DDR_RC_OK == rc) {
		hr = (*dataSource)->openSession(session);
		if (FAILED(hr)) {
			ERRMSG("openSession() failed with HRESULT = %08lX", hr);
			rc = DDR_RC_ERROR;
		}
	}

	if (DDR_RC_OK == rc) {
		hr = (*session)->get_globalScope(symbol);
		if (FAILED(hr)) {
			ERRMSG("get_globalScope() failed with HRESULT = %08lX", hr);
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
	for (vector<PostponedType>::iterator it = _postponedFields.begin(); it != _postponedFields.end(); ++it) {
		Type **type = it->type;
		const string &fullName = it->name;
		unordered_map<string, Type *>::const_iterator map_it = _typeMap.find(fullName);
		if (_typeMap.end() != map_it) {
			*type = map_it->second;
		} else {
			*type = new ClassUDT(0);
			(*type)->_name = getSimpleName(fullName);
			(*type)->_blacklisted = checkBlacklistedType((*type)->_name);
		}
	}
	_postponedFields.clear();

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
		ERRMSG("findChildren() failed with HRESULT = %08lX", hr);
		rc = DDR_RC_ERROR;
	}

	/* Allocate space for the children symbols. */
	LONG count = 0;
	if (DDR_RC_OK == rc) {
		hr = classSymbols->get_Count(&count);
		if (FAILED(hr)) {
			ERRMSG("Failed to get count of children symbols with HRESULT = %08lX", hr);
			rc = DDR_RC_ERROR;
		}
	}
	IDiaSymbol **childSymbols = NULL;
	ULONG celt = 0;
	if ((DDR_RC_OK == rc) && (0 != count)) {
		childSymbols = new IDiaSymbol*[count];
		hr = classSymbols->Next(count, childSymbols, &celt);
		if (FAILED(hr)) {
			ERRMSG("Failed to get children symbols with HRESULT = %08lX", hr);
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
		for (ULONG index = 0; index < celt; ++index) {
			string udtName = "";
			DWORD childTag = 0;
			hr = childSymbols[index]->get_symTag(&childTag);
			if (FAILED(hr)) {
				ERRMSG("Failed to get child symbol SymTag with HRESULT = %08lX", hr);
				rc = DDR_RC_ERROR;
			}
			if ((DDR_RC_OK == rc) && ((SymTagEnum == childTag) || (SymTagUDT == childTag))) {
				rc = getName(childSymbols[index], &udtName);
			}
			if (DDR_RC_OK == rc) {
				if ((NULL == outerNamespace) || (string::npos == udtName.find("::"))) {
					/* When adding fields/subtypes to a type, only add fields to a type with no fields
					 * and only add subtypes to a type with no subtypes. This avoids adding duplicates
					 * fields/subtypes when scanning multiple files. Children symbols must be processed
					 * for already existing symbols because fields and subtypes can appear separately.
					 */
					if (!alreadyCheckedFields && (SymTagData == childTag)) {
						alreadyCheckedFields = true;
						alreadyHadFields = !((ClassType *)outerNamespace)->_fieldMembers.empty();
					}

					if (!((SymTagData == childTag) ? alreadyHadFields : alreadyHadSubtypes)) {
						rc = addSymbol(childSymbols[index], outerNamespace);
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
		for (vector<ULONG>::iterator it = innerTypeSymbols.begin(); it != innerTypeSymbols.end() && DDR_RC_OK == rc; ++it) {
			rc = addSymbol(childSymbols[*it], outerNamespace);
			childSymbols[*it]->Release();
		}
	}

	if (NULL != childSymbols) {
		delete childSymbols;
	}
	if (NULL != classSymbols) {
		classSymbols->Release();
	}
	return rc;
}

DDR_RC
PdbScanner::createTypedef(IDiaSymbol *symbol, NamespaceUDT *outerNamespace)
{
	/* Get the typedef name and check if it is blacklisted. */
	string symbolName = "";
	DDR_RC rc = getName(symbol, &symbolName);

	if (DDR_RC_OK == rc) {
		string typedefName = getSimpleName(symbolName);
		TypedefUDT *newTypedef = new TypedefUDT;
		newTypedef->_name = typedefName;
		newTypedef->_blacklisted = checkBlacklistedType(typedefName);
		newTypedef->_outerNamespace = outerNamespace;
		/* Get the base type. */
		rc = setType(symbol, &newTypedef->_aliasedType, &newTypedef->_modifiers, NULL);
		if (DDR_RC_OK == rc) {
			Type *aliasedType = newTypedef->_aliasedType;
			if (NULL != aliasedType) {
				newTypedef->_sizeOf = aliasedType->_sizeOf;
			}
			addType(newTypedef, outerNamespace);
		} else {
			delete newTypedef;
		}
	}
	return rc;
}

DDR_RC
PdbScanner::addEnumMembers(IDiaSymbol *symbol, EnumUDT *enumUDT)
{
	DDR_RC rc = DDR_RC_OK;

	/* All children symbols of a symbol for an Enum type should be
	 * enum members.
	 */
	IDiaEnumSymbols *enumSymbols = NULL;
	HRESULT hr = symbol->findChildren(SymTagNull, NULL, nsNone, &enumSymbols);
	if (FAILED(hr)) {
		ERRMSG("findChildren() failed with HRESULT = %08lX", hr);
		rc = DDR_RC_ERROR;
	}

	if (DDR_RC_OK == rc) {
		vector<EnumMember *> *members = NULL;
		NamespaceUDT *outerUDT = enumUDT->_outerNamespace;
		/* literals of a nested anonymous enum are collected in the enclosing namespace */
		if ((NULL != outerUDT) && enumUDT->isAnonymousType()) {
			members = &outerUDT->_enumMembers;
		} else {
			members = &enumUDT->_enumMembers;
		}
		set<string> memberNames;
		for (vector<EnumMember *>::iterator m = members->begin(); m != members->end(); ++m) {
			memberNames.insert((*m)->_name);
		}
		IDiaSymbol *childSymbol = NULL;
		ULONG celt = 0;
		LONG count = 0;
		enumSymbols->get_Count(&count);
		members->reserve(count);
		while (SUCCEEDED(enumSymbols->Next(1, &childSymbol, &celt))
			&& (1 == celt)
			&& (DDR_RC_OK == rc)
		) {
			string name = "";
			int value = 0;
			rc = getName(childSymbol, &name);

			if (DDR_RC_OK == rc) {
				VARIANT variantValue;
				variantValue.vt = VT_EMPTY;

				hr = childSymbol->get_value(&variantValue);
				if (FAILED(hr)) {
					ERRMSG("get_value() failed with HRESULT = %08lX", hr);
					rc = DDR_RC_ERROR;
				} else {
					switch (variantValue.vt) {
					case VT_I1:
						value = variantValue.cVal;
						break;
					case VT_I2:
						value = variantValue.iVal;
						break;
					case VT_I4:
						value = (int)variantValue.lVal;
						break;
					case VT_UI1:
						value = variantValue.bVal;
						break;
					case VT_UI2:
						value = variantValue.uiVal;
						break;
					case VT_UI4:
						value = (int)variantValue.ulVal;
						break;
					case VT_INT:
						value = variantValue.intVal;
						break;
					case VT_UINT:
						value = (int)variantValue.uintVal;
						break;
					default:
						ERRMSG("get_value() unexpected variant: 0x%02X", variantValue.vt);
						rc = DDR_RC_ERROR;
						break;
					}
				}
			}

			if (DDR_RC_OK == rc) {
				if (memberNames.end() == memberNames.find(name)) {
					EnumMember *enumMember = new EnumMember;
					enumMember->_name = name;
					enumMember->_value = value;
					members->push_back(enumMember);
					memberNames.insert(name);
				}
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
		ERRMSG("get_symTag() failed with HRESULT = %08lX", hr);
		rc = DDR_RC_ERROR;
	} else if (SymTagEnum != symTag) {
		ERRMSG("symTag is not Enum");
		rc = DDR_RC_ERROR;
	}

	string symbolName = "";
	if (DDR_RC_OK == rc) {
		 rc = getName(symbol, &symbolName);
	}

	if (DDR_RC_OK == rc) {
		/* Sub enums are added by their undecorated name when found as a
		 * child symbol of another UDT symbol. They are also found with
		 * a decorated "Parent::SubUDT" name again while iterating all Enums.
		 */
		ULONGLONG size = 0ULL;
		rc = getSize(symbol, &size);
		if (DDR_RC_OK == rc) {
			getNamespaceFromName(symbolName, &outerNamespace);
			string name = getSimpleName(symbolName);
			string fullName = "";
			if (NULL == outerNamespace) {
				fullName = name;
			} else {
				fullName = outerNamespace->getFullName() + "::" + name;
			}
			unordered_map<string, Type *>::const_iterator it = _typeMap.find(fullName);
			if (_typeMap.end() == it) {
				/* If this is a new enum, get its members and add it to the IR. */
				EnumUDT *enumUDT = new EnumUDT;
				enumUDT->_name = name;
				enumUDT->_blacklisted = checkBlacklistedType(name);
				enumUDT->_outerNamespace = outerNamespace;
				rc = addEnumMembers(symbol, enumUDT);

				/* If successful, add the new enum to the IR. */
				if (DDR_RC_OK == rc) {
					addType(enumUDT, outerNamespace);
				} else {
					delete enumUDT;
				}
			} else {
				Type *existingType = it->second;
				if ("enum" == existingType->getSymbolKindName()) {
					EnumUDT *enumUDT = (EnumUDT *)existingType;
					if ((NULL != outerNamespace) && (NULL == enumUDT->_outerNamespace)) {
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
				ERRMSG("get_offset() failed with HRESULT = %08lX", hr);
				rc = DDR_RC_ERROR;
			} else {
				offset = (size_t)loffset;
			}
			break;
		}
		case LocIsStatic:
		case LocIsConstant:
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
				ERRMSG("get_offset() failed with HRESULT = %08lX", hr);
				rc = DDR_RC_ERROR;
			} else {
				offset = (size_t)loffset;
			}
			if (DDR_RC_OK == rc) {
				ULONGLONG bitwidth = 0;
				/* For bit-fields, the 'length' is measured in bits. */
				hr = symbol->get_length(&bitwidth);
				if (FAILED(hr)) {
					ERRMSG("get_length() failed with HRESULT = %08lX", hr);
					rc = DDR_RC_ERROR;
				} else {
					newField->_bitField = (size_t)bitwidth;
				}
			}
			break;
		}
		default:
			ERRMSG("Unknown offset type: %lu, name: %s", locType, newField->_name.c_str());
			rc = DDR_RC_ERROR;
			break;
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
		ERRMSG("get_constType() failed with HRESULT = %08lX", hr);
		rc = DDR_RC_ERROR;
	} else if (bSet) {
		modifiers->_modifierFlags |= Modifiers::CONST_TYPE;
	}

	if (DDR_RC_OK == rc) {
		hr = symbol->get_volatileType(&bSet);
		if (FAILED(hr)) {
			ERRMSG("get_volatileType() failed with HRESULT = %08lX", hr);
			rc = DDR_RC_ERROR;
		} else if (bSet) {
			modifiers->_modifierFlags |= Modifiers::VOLATILE_TYPE;
		}
	}

	if (DDR_RC_OK == rc) {
		hr = symbol->get_unalignedType(&bSet);
		if (FAILED(hr)) {
			ERRMSG("get_unalignedType() failed with HRESULT = %08lX", hr);
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
		ERRMSG("get_udtKind() failed with HRESULT = %08lX", hr);
		rc = DDR_RC_ERROR;
	}

	string symbolName = "";
	if (DDR_RC_OK == rc) {
		rc = getName(typeSymbol, &symbolName);
	}
	if (DDR_RC_OK == rc) {
		getNamespaceFromName(symbolName, &outerUDT);
		string name = getSimpleName(symbolName);
		string fullName = "";
		if (NULL == outerUDT) {
			fullName = name;
		} else {
			fullName = outerUDT->getFullName() + "::" + name;
		}
		unordered_map<string, Type *>::const_iterator map_it = _typeMap.find(fullName);
		if (!fullName.empty() && _typeMap.end() != map_it) {
			*type = map_it->second;
		} else if (fullName.empty() && (NULL != outerUDT)) {
			/* Anonymous inner union UDTs are missing the parent
			 * relationship and cannot be added later.
			 */
			ClassUDT *newClass = NULL;
			rc = createClassUDT(typeSymbol, &newClass, outerUDT);
			if ((DDR_RC_OK == rc) && (NULL != newClass)) {
				newClass->_name = "";
				*type = newClass;
			}
		} else if (!name.empty()) {
			PostponedType p = { type, fullName };
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
		ERRMSG("get_reference failed with HRESULT = %08lX", hr);
		rc = DDR_RC_ERROR;
	} else if (bSet) {
		modifiers->_referenceCount += 1;
	} else {
		modifiers->_pointerCount += 1;
	}

	return rc;
}

Type *
PdbScanner::getType(const string &name)
{
	/* Get a type in the map by name. This used only for base types. */
	Type *type = NULL;
	unordered_map<string, Type *>::const_iterator map_it = _typeMap.find(name);
	if (!name.empty() && _typeMap.end() != map_it) {
		type = map_it->second;
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
		ERRMSG("Unknown int length: %llu", ulLen);
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
		ERRMSG("Unknown float length: %llu", ulLen);
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
		ERRMSG("Unknown int length: %llu", ulLen);
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
		ERRMSG("get_baseType() failed with HRESULT = %08lX", hr);
		rc = DDR_RC_ERROR;
	}

	ULONGLONG ulLen = 0ULL;
	if (DDR_RC_OK == rc) {
		rc = getSize(typeSymbol, &ulLen);
	}

	if (DDR_RC_OK == rc) {
		switch (baseType) {
		case btChar:
		case btWChar:
		case btUInt:
			rc = setBaseTypeUInt(ulLen, type);
			break;
		case btInt:
			rc = setBaseTypeInt(ulLen, type);
			break;
		case btFloat:
			rc = setBaseTypeFloat(ulLen, type);
			break;
		default:
			*type = getType(string(baseTypeArray[baseType])); /* This could also be an unsigned char */
			break;
		}
	}

	return rc;
}

static string
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
#if 0 /* Note: The following are not present in all versions. */
	case SymTagCallSite:
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
		return "SymTagHLSLType";
#endif
	}
	stringstream result;
	result << "value " << value;
	return result.str();
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
		ERRMSG("get_type failed with HRESULT = %08lX", hr);
		rc = DDR_RC_ERROR;
	}

	if (DDR_RC_OK == rc) {
		rc = setTypeModifier(typeSymbol, modifiers);
	}

	DWORD symTag = 0;
	if (DDR_RC_OK == rc) {
		hr = typeSymbol->get_symTag(&symTag);
		if (FAILED(hr)) {
			ERRMSG("get_symTag failed with HRESULT = %08lX", hr);
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
			ERRMSG("unknown symtag: %lu", symTag);
			rc = DDR_RC_ERROR;
			break;
		}
	}

	if (NULL != typeSymbol) {
		typeSymbol->Release();
	}

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
		ERRMSG("get_name failed with HRESULT = %08lX", hr);
		rc = DDR_RC_ERROR;
	} else {
		char *nameChar = _com_util::ConvertBSTRToString(nameBstr);
		if (NULL == nameChar) {
			ERRMSG("Could not convert Bstr");
			rc = DDR_RC_ERROR;
		} else {
			*name = nameChar;
			free(nameChar);
		}
		SysFreeString(nameBstr);
	}

	return rc;
}

string
PdbScanner::getSimpleName(const string &name)
{
	size_t pos = name.rfind("::");
	string simple = (string::npos == pos) ? name : name.substr(pos + 2);

	if (0 == simple.compare(0, 9, "<unnamed-", 9)) {
		simple = "";
	}

	return simple;
}

DDR_RC
PdbScanner::getSize(IDiaSymbol *symbol, ULONGLONG *size)
{
	DDR_RC rc = DDR_RC_OK;
	/* Attempt to get the size associated with a symbol. */
	ULONGLONG ul = 0ULL;
	HRESULT hr = symbol->get_length(&ul);
	if (FAILED(hr)) {
		ERRMSG("get_length() failed for symbol with HRESULT = %08lX", hr);
		rc = DDR_RC_ERROR;
	} else {
		*size = ul;
	}
	return rc;
}

DDR_RC
PdbScanner::addFieldMember(IDiaSymbol *symbol, ClassUDT *udt)
{
	DDR_RC rc = DDR_RC_OK;
	/* Add a new field to a class. Find its name, type, size,
	 * modifiers, and offset.
	 */
	Field *newField = new Field;
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
		 * If it's not found, add it to a list to check later.
		 */
		if (!name.empty()) {
			unordered_map<string, Type *>::const_iterator map_it = _typeMap.find(name);
			if (_typeMap.end() != map_it) {
				newUDT->_superClass = (ClassUDT *)map_it->second;
			} else {
				PostponedType p = { (Type **)&newUDT->_superClass, name };
				_postponedFields.push_back(p);
			}
		}
	}

	return DDR_RC_OK;
}

DDR_RC
PdbScanner::createClassUDT(IDiaSymbol *symbol, ClassUDT **newClass, NamespaceUDT *outerUDT)
{
	DDR_RC rc = DDR_RC_OK;

	/* Verify this symbol is for a UDT. */
	DWORD symTag = 0;
	HRESULT hr = symbol->get_symTag(&symTag);
	if (FAILED(hr)) {
		ERRMSG("get_symTag() failed with HRESULT = %08lX", hr);
		rc = DDR_RC_ERROR;
	} else if (SymTagUDT != symTag) {
		ERRMSG("symTag is unexpectedly not UDT");
		rc = DDR_RC_ERROR;
	}

	string symbolName = "";
	if (DDR_RC_OK == rc) {
		rc = getName(symbol, &symbolName);
	}

	/* Sub UDTs are added by their undecorated name when found as a
	 * child symbol of another UDT symbol. They are also found with
	 * a decorated "Parent::SubUDT" name again while iterating all UDTs.
	 */
	ULONGLONG size = 0ULL;
	if (DDR_RC_OK == rc) {
		rc = getSize(symbol, &size);
	}
	if (DDR_RC_OK == rc) {
		getNamespaceFromName(symbolName, &outerUDT);
		string name = getSimpleName(symbolName);
		string fullName = "";
		if (NULL == outerUDT) {
			fullName = name;
		} else {
			fullName = outerUDT->getFullName() + "::" + name;
		}
		if (_typeMap.end() == _typeMap.find(fullName)) {
			ClassUDT *classUDT = new ClassUDT((size_t)size);
			classUDT->_name = name;
			classUDT->_blacklisted = checkBlacklistedType(name);
			classUDT->_outerNamespace = outerUDT;
			rc = addChildrenSymbols(symbol, SymTagNull, classUDT);

			/* If successful, add the new UDT to the IR. Otherwise, free it. */
			if (DDR_RC_OK == rc) {
				addType(classUDT, outerUDT);
				if (NULL != newClass) {
					*newClass = classUDT;
				}
			} else {
				delete classUDT;
				classUDT = NULL;
			}
		} else {
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
			if (NULL != newClass) {
				*newClass = classUDT;
			}
		}
	}

	return rc;
}

void
PdbScanner::getNamespaceFromName(const string &name, NamespaceUDT **outerUDT)
{
	/* In Pdb info, namespaces do not have their own symbol. If a symbol has
	 * a name of the format "OuterType::InnerType" and no separate symbol for
	 * the outer type, it could be a namespace or a class that wasn't found
	 * yet.
	 */
	if (NULL == *outerUDT) {
		size_t previousPos = 0;
		NamespaceUDT *outerNamespace = NULL;
		for (;;) {
			size_t pos = name.find("::", previousPos);
			if (string::npos == pos) {
				*outerUDT = outerNamespace;
				break;
			}
			string namespaceName = name.substr(previousPos, pos - previousPos);
			if (0 == namespaceName.compare(0, 9, "<unnamed-", 9)) {
				*outerUDT = outerNamespace;
				break;
			}
			NamespaceUDT *ns = (NamespaceUDT *)getType(namespaceName);
			if (NULL == ns) {
				/* If no previously created type exists, create class for it.
				 * It could be a class or a namespace, but class derives from
				 * namespace and thus works for both cases.
				 */
				ns = new ClassUDT(0);
				ns->_name = namespaceName;
				ns->_blacklisted = checkBlacklistedType(namespaceName);
				ns->_outerNamespace = outerNamespace;
				addType(ns, outerNamespace);
			} else if ((NULL != outerNamespace) && (NULL == ns->_outerNamespace)) {
				ns->_outerNamespace = outerNamespace;
				outerNamespace->_subUDTs.push_back(ns);
				_ir->_types.erase(remove(_ir->_types.begin(), _ir->_types.end(), ns));
			}

			outerNamespace = ns;
			previousPos = pos + 2;
		}
	}
}

DDR_RC
PdbScanner::addSymbol(IDiaSymbol *symbol, NamespaceUDT *outerNamespace)
{
	DDR_RC rc = DDR_RC_OK;
	DWORD symTag = 0;
	HRESULT hr = symbol->get_symTag(&symTag);
	if (FAILED(hr)) {
		ERRMSG("get_symTag() failed with HRESULT = %08lX", hr);
		rc = DDR_RC_ERROR;
	}

	if (DDR_RC_OK == rc) {
		switch (symTag) {
		case SymTagEnum:
			rc = createEnumUDT(symbol, outerNamespace);
			break;
		case SymTagUDT:
			rc = createClassUDT(symbol, NULL, outerNamespace);
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
			ERRMSG("Unhandled symbol returned by get_symTag: %s", symTagToString(symTag).c_str());
			rc = DDR_RC_ERROR;
			break;
		}
	}

	return rc;
}
