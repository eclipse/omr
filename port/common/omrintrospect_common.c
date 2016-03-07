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
 *    Multiple authors (IBM Corp.) - initial API and implementation and/or initial documentation
 *******************************************************************************/

/**
 * @file
 * @ingroup Port
 * @brief Stack backtracing support
 */

#include "omrintrospect_common.h"

const char *error_descriptions[] = {
	"success",
	"memory allocation failure",
	"deadline expired",
	"unable to count threads",
	"failed to suspend process",
	"failed to resume process",
	"failure while collecting threads",
	"initialisation failed",
	"caller supplied invalid walk state",
	"signal setup failed",
	"signal received from external process",
	"native stacks not available on this platform",
	"memory fault while constructing backtrace",
	"thread collection in progress on another thread"
};
