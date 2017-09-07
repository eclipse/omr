/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code is also Distributed under one or more Secondary Licenses,
 * as those terms are defined by the Eclipse Public License, v. 2.0: GNU
 * General Public License, version 2 with the GNU Classpath Exception [1]
 * and GNU General Public License, version 2 with the OpenJDK Assembly
 * Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * Contributors:
 *   Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

#include "omrport.h"

/**
 * Returns a string representing the type of page indicated by the given pageFlags.
 * Useful when printing page type.
 *
 * @param[in] pageFlags indicates type of the page.
 *
 * @return pointer to string representing the page type.
 */
const char *
getPageTypeString(uintptr_t pageFlags)
{
	if (0 != (OMRPORT_VMEM_PAGE_FLAG_PAGEABLE & pageFlags)) {
		return "pageable";
	} else if (0 != (OMRPORT_VMEM_PAGE_FLAG_FIXED & pageFlags)) {
		return "nonpageable";
	} else {
		return "not used";
	}
}

/**
 * Returns a string representing the type of page indicated by the given pageFlags.
 * Useful when printing page type.
 *
 * @param[in] pageFlags indicates type of the page.
 *
 * @return pointer to string representing the page type.
 */
const char *
getPageTypeStringWithLeadingSpace(uintptr_t pageFlags)
{
	if (0 != (OMRPORT_VMEM_PAGE_FLAG_PAGEABLE & pageFlags)) {
		return " pageable";
	} else if (0 != (OMRPORT_VMEM_PAGE_FLAG_FIXED & pageFlags)) {
		return " nonpageable";
	} else {
		return "";
	}
}
