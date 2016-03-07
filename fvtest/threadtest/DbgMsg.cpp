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


#include "DbgMsg.hpp"
#include <stdio.h>
#include <stdarg.h>

#include "testHelper.hpp"

extern ThreadTestEnvironment *omrTestEnv;

namespace DbgMsg
{
static int s_indentLevel = 0;
static int s_verboseLevel = 0;
static void vprint(const char *pMsg, va_list va);
static void vprint(const char *pMsg);
static void indent(void);
};

static void
DbgMsg::vprint(const char *pMsg, va_list va)
{
	OMRPORT_ACCESS_FROM_OMRPORT(omrTestEnv->getPortLibrary());
	omrtty_vprintf(pMsg, va);
}

static void
DbgMsg::vprint(const char *pMsg)
{
	OMRPORT_ACCESS_FROM_OMRPORT(omrTestEnv->getPortLibrary());
	omrtty_printf(pMsg);
}

static void
DbgMsg::indent(void)
{
	for (int i = 0; i < s_indentLevel; i++) {
		vprint("\t");
	}
}

void
DbgMsg::print(const char *pMsg, ...)
{
	va_list va;

	if (0 == pMsg) {
		return;
	}

	indent();

	va_start(va, pMsg);
	vprint(pMsg, va);
	va_end(va);
}

void
DbgMsg::println(const char *pMsg, ...)
{
	va_list va;

	if (0 == pMsg) {
		return;
	}

	indent();

	va_start(va, pMsg);
	vprint(pMsg, va);
	vprint("\n");
	va_end(va);
}

void
DbgMsg::printRaw(const char *pMsg, ...)
{
	va_list va;

	if (0 == pMsg) {
		return;
	}

	va_start(va, pMsg);
	vprint(pMsg, va);
	va_end(va);
}

void
DbgMsg::changeIndent(int delta)
{
	s_indentLevel += delta;
}

void
DbgMsg::setVerboseLevel(int level)
{
	s_verboseLevel = level;
}

void
DbgMsg::verbosePrint(const char *pMsg, ...)
{
	if ((s_verboseLevel > 0) && (0 != pMsg)) {
		va_list va;

		indent();
		va_start(va, pMsg);
		vprint(pMsg, va);
		va_end(va);
	}
}
