/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2015
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
 ******************************************************************************/

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
