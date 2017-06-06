/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#if defined(WIN32)
#include <windows.h>
#define strdup _strdup
#endif /* WIN32 */

#include "HookGen.hpp"
#include "pugixml.hpp"

/**
 * Create a header Macro gate.
 *
 * @NOTE caller is responsible for freeing memory
 */
char *
HookGen::getHeaderGateMacro(const char *fileName)
{
	size_t length = strlen(fileName);
	char *converted = (char *)malloc(length + 1);
	if (NULL != converted) {
		for (size_t i = 0; i < length; ++i) {
			converted[i] = convertChar(fileName[i]);
		}
		converted[length] = '\0';
	}

	return converted;
}

/**
 * Start the public header
 */
RCType
HookGen::startPublicHeader(const char *declarations)
{
	RCType rc = RC_OK;
	char *macroName = getHeaderGateMacro(_publicFileName);
	if (NULL != macroName) {
		fprintf(_publicFile, "/* Auto-generated public header file */\n");
		fprintf(_publicFile, "#ifndef %s\n", macroName);
		fprintf(_publicFile, "#define %s\n\n", macroName);
		fprintf(_publicFile, "#include \"omrhookable.h\"\n\n");
		if (NULL != declarations) {
			fprintf(_publicFile, "\n/* Begin declarations block */\n");
			fprintf(_publicFile, "%s", declarations);
			fprintf(_publicFile, "\n/* End declarations block */\n");
		}

		fprintf(_publicFile, "\n");

		free(macroName);
	} else {
		rc = RC_FAILED;
	}

	return rc;
}

/**
 * Complete the public header
 */
RCType
HookGen::completePublicHeader()
{
	RCType rc = RC_OK;
	char *macroName = getHeaderGateMacro(_publicFileName);
	if (NULL != macroName) {
		fprintf(_publicFile, "#endif /* %s */\n", macroName);

		free(macroName);
	} else {
		rc = RC_FAILED;
	}
	return rc;
}

/**
 * Start the private header
 */
RCType
HookGen::startPrivateHeader()
{
	RCType rc = RC_OK;
	char *macroName = getHeaderGateMacro(_privateFileName);
	if (NULL != macroName) {
		fprintf(_privateFile, "/* Auto-generated private header file */\n\n");
		fprintf(_privateFile, "/* This file should be included by the IMPLEMENTOR of the hook interface\n");
		fprintf(_privateFile, " * It is not required by USERS of the hook interface\n");
		fprintf(_privateFile, " */\n\n");
		fprintf(_privateFile, "#ifndef %s\n", macroName);
		fprintf(_privateFile, "#define %s\n\n", macroName);
		fprintf(_privateFile, "#include \"%s\"\n\n", _publicFileName);

		free(macroName);
	} else {
		rc = RC_FAILED;
	}
	return rc;
}

/**
 * Complete the private header
 */
RCType
HookGen::completePrivateHeader(const char *structName)
{
	RCType rc = RC_OK;
	char *macroName = getHeaderGateMacro(_privateFileName);
	if (NULL != macroName) {
		fprintf(_privateFile, "typedef struct %s {\n", structName);
		fprintf(_privateFile, "\tstruct J9CommonHookInterface common;\n");
		fprintf(_privateFile, "\tU_8 flags[%d];\n", _eventNum);
		/* the structure for saving hook dump information */
		fprintf(_privateFile, "\tstruct OMREventInfo4Dump infos4Dump[%d];\n", _eventNum);
		fprintf(_privateFile, "\tJ9HookRecord* hooks[%d];\n", _eventNum);
		fprintf(_privateFile, "} %s;\n", structName);
		fprintf(_privateFile, "\n");
		fprintf(_privateFile, "#endif /* %s */\n", macroName);

		free(macroName);
	} else {
		rc = RC_FAILED;
	}
	return rc;
}

/**
 * Write a comment to the appropriate file
 */
void
HookGen::writeComment(FILE *file, char *text)
{
	fprintf(file, "/* %s */\n", text);
}

/**
 * Write an event to the public header
 */
void
HookGen::writeEventToPublicHeader(const char *name, const char *description, const char *condition, const char *structName, const char *reverse, pugi::xml_node event)
{
	int thisEventNum = _eventNum++;
	const char *example =
		"%s\n"
		"%s\n\n"
		"Example usage:\n"
		"\t(*hookable)->J9HookRegisterWithCallSite(hookable, %s, eventOccurred, OMR_GET_CALLSITE(), NULL);"
		"\n\n"
		"\tstatic void\n"
		"\teventOccurred(J9HookInterface** hookInterface, UDATA eventNum, void* voidData, void* userData)\n"
		"\t{\n"
		"\t\t%s* eventData = voidData;\n"
		"\t\t. . .\n"
		"\t}\n";

	size_t nameLength = strlen(name);
	size_t bufSize = 2 * nameLength;

	bufSize += strlen(example);
	bufSize += strlen(description);
	bufSize += strlen(structName);

	char *exampleBuf = (char *)malloc(bufSize + 1);
	if (NULL != exampleBuf) {
		/* If we fail to allocate room for the example just skip writing it */
		int length = sprintf(exampleBuf, (char *)example, name, description, name, structName);
		exampleBuf[length] = '\0';

		writeComment(_publicFile, exampleBuf);

		free(exampleBuf);
	}

	if (NULL != condition) {
		fprintf(_publicFile, "#if %s\n", condition);
	}

	if (NULL == reverse) {
		fprintf(_publicFile, "#define %s %d\n", name, thisEventNum);
	} else {
		fprintf(_publicFile, "#define %s (%d | J9HOOK_TAG_REVERSE_ORDER)\n", name, thisEventNum);
	}

	fprintf(_publicFile, "typedef struct %s {\n", structName);
	for (pugi::xml_node data = event.child("data"); data; data = data.next_sibling("data")) {
		fprintf(_publicFile, "\t%s %s;\n", data.attribute("type").as_string(), data.attribute("name").as_string());
	}
	fprintf(_publicFile, "} %s;\n", structName);

	if (NULL != condition) {
		fprintf(_publicFile, "#endif /* %s*/\n", condition);
	}

	fprintf(_publicFile, "\n");
}

/**
 * Write an event to the private header
 */
void
HookGen::writeEventToPrivateHeader(const char *name, const char *condition, const char *once, const char *structName, pugi::xml_node event)
{
	if (NULL != condition) {
		fprintf(_privateFile, "#if %s\n", condition);
	}

	fprintf(_privateFile, "#define ALWAYS_TRIGGER_%s(hookInterface", name);
	for (pugi::xml_node data = event.child("data"); data; data = data.next_sibling("data")) {
		fprintf(_privateFile, ", arg_%s", data.attribute("name").as_string());
	}

	fprintf(_privateFile, ") \\\n\tdo { \\\n");
	fprintf(_privateFile, "\t\tstruct %s eventData; \\\n", structName);
	for (pugi::xml_node data = event.child("data"); data; data = data.next_sibling("data")) {
		fprintf(_privateFile, "\t\teventData.%s = (arg_%s); \\\n", data.attribute("name").as_string(), data.attribute("name").as_string());
	}
	fprintf(_privateFile, "\t\t(*J9_HOOK_INTERFACE(hookInterface))->J9HookDispatch(J9_HOOK_INTERFACE(hookInterface), %s%s, &eventData); \\\n", name, once == NULL ? "" : " | J9HOOK_TAG_ONCE");
	for (pugi::xml_node data = event.child("data"); data; data = data.next_sibling("data")) {
		if (data.attribute("return").as_bool()) {
			fprintf(_privateFile, "\t\t(arg_%s) = eventData.%s; /* return argument */ \\\n", data.attribute("name").as_string(), data.attribute("name").as_string());
		}
	}

	fprintf(_privateFile, "\t} while (0)\n\n");

	fprintf(_privateFile, "#define TRIGGER_%s(hookInterface", name);
	for (pugi::xml_node data = event.child("data"); data; data = data.next_sibling("data")) {
		fprintf(_privateFile, ", arg_%s", data.attribute("name").as_string());
	}
	fprintf(_privateFile, ") \\\n\tdo { \\\n");
	if (NULL == once) {
		fprintf(_privateFile, "\t\tif (J9_EVENT_IS_HOOKED(hookInterface, %s)) { \\\n", name);
	} else {
		fprintf(_privateFile, "\t\t/* always trigger this 'report once' event, so that it will be disabled after this point */ \\\n");
	}
	fprintf(_privateFile, "\t\t\tALWAYS_TRIGGER_%s(hookInterface", name);
	for (pugi::xml_node data = event.child("data"); data; data = data.next_sibling("data")) {
		fprintf(_privateFile, ", arg_%s", data.attribute("name").as_string());
	}
	fprintf(_privateFile, "); \\\n\t");
	if (NULL == once) {
		fprintf(_privateFile, "\t} \\\n\t");
	}
	fprintf(_privateFile, "} while (0)\n");

	if (NULL != condition) {
		fprintf(_privateFile, "#else /* %s */\n", condition);
		fprintf(_privateFile, "#define TRIGGER_%s(hookInterface", name);
		for (pugi::xml_node data = event.child("data"); data; data = data.next_sibling("data")) {
			fprintf(_privateFile, ", arg_%s", data.attribute("name").as_string());
		}
		fprintf(_privateFile, ")\n");
		fprintf(_privateFile, "#endif /* %s */\n", condition);
	}

	fprintf(_privateFile, "\n");
}

/**
 * Write an event to the header files
 */
void
HookGen::writeEvent(pugi::xml_node event)
{
	const char *name = event.child("name").text().as_string();
	const char *description = event.child("description").text().as_string();
	const char *condition = event.child("condition").text().as_string();
	const char *structName = event.child("struct").text().as_string();
	const char *once = event.child("once").text().as_string();
	const char *reverse = event.child("reverse").text().as_string();

	if (event.child("condition").empty()) {
		condition = NULL;
	}
	if (event.child("once").empty()) {
		once = NULL;
	}
	if (event.child("reverse").empty()) {
		reverse = NULL;
	}

	writeEventToPublicHeader(name, description, condition, structName, reverse, event);
	writeEventToPrivateHeader(name, condition, once, structName, event);
}

/**
 * Get the absolute path for the filename passed in
 *
 * @NOTE caller is responsible for freeing memory
 */
char *
HookGen::getAbsolutePath(const char *filename)
{
	char *absolutePath = NULL;
#if defined(WIN32)
	char PATH_SEPARATOR = '\\';
	char resolved_path[MAX_PATH];
	DWORD retval = 0;
	retval = GetFullPathName(filename, MAX_PATH, resolved_path, NULL);
	if (retval == 0) {
		return absolutePath;
	}
#else
#if defined(J9ZOS390)
#define PATH_MAX 1024
#endif /* J9ZOS390 */
	char PATH_SEPARATOR = '/';
	char resolved_path[PATH_MAX];
	if (NULL == realpath(filename, resolved_path)) {
		return absolutePath;
	}
#endif /* WIN32 */

	char *chopped = strrchr(resolved_path, PATH_SEPARATOR);
	if (NULL != chopped) {
		size_t length = ((size_t)(chopped - resolved_path)) + 1;
		absolutePath = (char *)malloc(length + 1);
		if (NULL != absolutePath) {
			strncpy(absolutePath, resolved_path, length);
			absolutePath[length] = '\0';
		}
	}
	return absolutePath;
}

/**
 * create a file for the given filename based on the given path
 */
FILE *
HookGen::createFile(char *path, const char *fileName)
{
	size_t pathLength = strlen(path);
	size_t publicFileNameLength = strlen(fileName);
	char *temp = (char *)malloc(pathLength + publicFileNameLength + 1);
	if (NULL == temp) {
		return NULL;
	}
	strncpy(temp, path, pathLength);
	strncpy(temp + pathLength, fileName, publicFileNameLength);
	temp[pathLength + publicFileNameLength] = '\0';

	FILE *file = fopen(temp, "wb");
#if defined(DEBUG)
	fprintf(stderr, "Opening %s for writing\n", temp);
#endif /* DEBUG */

	free(temp);

	return file;
}

/**
 * Get the actual filename from the relative path which includes the file name
 *
 * @NOTE caller is responsible for freeing memory
 */
const char *
HookGen::getActualFileName(const char *fileName)
{
	const char *location = strrchr((char *)fileName, '/');
	if (NULL != location) {
		/* move past the / */
		location = location + 1;
	} else {
		location = fileName;
	}
	return strdup(location);
}

/**
 * Create the public and private header files based on the names passed in
 *
 * @NOTE the caller is responsible for closing the files
 */
RCType
HookGen::createFiles(const char *publicFileName, const char *privateFileName)
{
	RCType rc = RC_OK;
	char *path = getAbsolutePath(_fileName);
	if (NULL == path) {
		rc = RC_FAILED;
		goto failed;
	}

	_publicFile = createFile(path, publicFileName);
	if (NULL == _publicFile) {
		rc = RC_FAILED;
		goto failed;
	}

	_publicFileName = getActualFileName(publicFileName);
	if (NULL == _publicFileName) {
		rc = RC_FAILED;
		goto failed;
	}

	_privateFile = createFile(path, privateFileName);
	if (NULL == _privateFile) {
		rc = RC_FAILED;
		goto failed;
	}

	_privateFileName = getActualFileName(privateFileName);
	if (NULL == _privateFileName) {
		rc = RC_FAILED;
		goto failed;
	}

failed:
	if (NULL != path) {
		free(path);
	}
	return rc;
}

/**
 * Process the hook definition file and create the output header files
 */
RCType
HookGen::processFile()
{
	RCType rc = RC_OK;

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(_fileName);

	if (pugi::status_ok != result.status) {
		fprintf(stderr, "Error %d loading %s\n", result.status, _fileName);
		return RC_FAILED;
	}

	pugi::xml_node node = doc.select_node("/interface").node();
	const char *publicFileName = node.child("publicHeader").text().as_string();
	const char *privateFileName = node.child("privateHeader").text().as_string();
	const char *structName = node.child("struct").text().as_string();
	const char *declarations = node.child("declarations").text().as_string();
	if (node.child("declarations").empty()) {
		declarations = NULL;
	}

#if defined(DEBUG)
	fprintf(stderr, "Processing %s into public header %s and private header %s\n", _fileName, publicFileName, privateFileName);
#endif /* DEBUG */

	rc = createFiles(publicFileName, privateFileName);
	if (RC_OK != rc) {
		fprintf(stderr, "Error creating output files\n");
		return rc;
	}

	rc = startPublicHeader(declarations);
	if (RC_OK != rc) {
		return rc;
	}
	rc = startPrivateHeader();
	if (RC_OK != rc) {
		return rc;
	}

	for (pugi::xml_node event = node.child("event"); event; event = event.next_sibling("event")) {
		writeEvent(event);
	}

	rc = completePublicHeader();
	if (RC_OK != rc) {
		return rc;
	}
	rc = completePrivateHeader(structName);
	if (RC_OK != rc) {
		return rc;
	}
#if defined(DEBUG)
	fprintf(stderr, "Finished procssing %s into public header %s and private header %s\n", _fileName, publicFileName, privateFileName);
#endif /* DEBUG */

	return rc;
}

void
HookGen::displayUsage()
{
	fprintf(stderr, "hookgen {fileName}\n");
}

RCType
HookGen::parseOptions(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Please provide a file to process\n");
		displayUsage();
		return RC_FAILED;
	}

	_fileName = argv[1];

	return RC_OK;
}

void
HookGen::tearDown()
{
	if (NULL != _publicFile) {
		fclose(_publicFile);
	}
	if (NULL != _privateFile) {
		fclose(_privateFile);
	}
}

RCType
startHookGen(int argc, char *argv[])
{
	RCType rc = RC_OK;
	HookGen hookGen;

	rc = hookGen.parseOptions(argc, argv);
	if (RC_OK != rc) {
		goto finish;
	}
	rc = hookGen.processFile();
	if (RC_OK != rc) {
		goto finish;
	}

	fprintf(stderr, "Processed %s to create public header %s and private header %s\n", hookGen.getFileName(), hookGen.getPublicFileName(), hookGen.getPrivateFileName());

finish:
	hookGen.tearDown();

	return rc;
}
