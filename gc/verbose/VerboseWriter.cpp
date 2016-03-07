/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

#include "modronapicore.hpp"

#include "VerboseWriter.hpp"

#include "GCExtensionsBase.hpp"

#undef _UTE_MODULE_HEADER_
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_j9vgc.h"

/* Output constants */
#define VERBOSEGC_HEADER "<?xml version=\"1.0\" ?>\n\n<verbosegc xmlns=\"http://www.ibm.com/j9/verbosegc\" version=\"%s\">\n\n"
#define VERBOSEGC_FOOTER "</verbosegc>\n"

MM_VerboseWriter::MM_VerboseWriter(WriterType type)
	: MM_Base()
	,_nextWriter(NULL)
	,_header(NULL)
	,_footer(NULL)
	,_type(type)
	,_isActive(false)
{}

const char*
MM_VerboseWriter::getHeader(MM_EnvironmentBase *env)
{
	return _header;
}

const char*
MM_VerboseWriter::getFooter(MM_EnvironmentBase *env)
{
	return _footer;
}

MM_VerboseWriter*
MM_VerboseWriter::getNextWriter()
{
	return _nextWriter;
}

void
MM_VerboseWriter::setNextWriter(MM_VerboseWriter* writer)
{
	_nextWriter = writer;
}

void
MM_VerboseWriter::tearDown(MM_EnvironmentBase* env)
{
	MM_GCExtensionsBase* ext = env->getExtensions();
	ext->getForge()->free(_header);
	_header = NULL;
	ext->getForge()->free(_footer);
	_footer = NULL;
}

void
MM_VerboseWriter::kill(MM_EnvironmentBase* env) {
	tearDown(env);
	env->getExtensions()->getForge()->free(this);
}

bool
MM_VerboseWriter::initialize(MM_EnvironmentBase* env)
{
	OMRPORT_ACCESS_FROM_OMRPORT(env->getPortLibrary());
	MM_GCExtensionsBase* ext = env->getExtensions();

	/* Initialize _header */
	const char* version = omrgc_get_version(env->getOmrVM());
	/* The length is -2 for the "%s" in VERBOSEGC_HEADER and +1 for '\0' */
	uintptr_t headerLength = strlen(version) + strlen(VERBOSEGC_HEADER) - 1;
	_header = (char*)ext->getForge()->allocate(sizeof(char) * headerLength, MM_AllocationCategory::DIAGNOSTIC, OMR_GET_CALLSITE());
	if (NULL == _header) {
		return false;
	}
	omrstr_printf(_header, headerLength, VERBOSEGC_HEADER, version);

	/* Initialize _footer */
	uintptr_t footerLength = strlen(VERBOSEGC_FOOTER) + 1;
	_footer = (char*)ext->getForge()->allocate(sizeof(char) * footerLength, MM_AllocationCategory::DIAGNOSTIC, OMR_GET_CALLSITE());
	if (NULL == _footer) {
		ext->getForge()->free(_header);
		return false;
	}
	omrstr_printf(_footer, footerLength, VERBOSEGC_FOOTER);
	
	return true;
}
