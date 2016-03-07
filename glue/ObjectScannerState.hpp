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

#if !defined(OBJECTSCANNERSTATE_HPP_)
#define OBJECTSCANNERSTATE_HPP_

#include "ObjectScanner.hpp"

/**
 * This union is not intended for runtime usage -- it is required only to determine the maximal size of
 * GC_ObjectScanner subclasses used in the client language. An analogous definition
 * of GC_ObjectScannerState must be specified in the glue layer for the client language and that definition
 * will be used when OMR is built for the client language.
 */
typedef union GC_ObjectScannerState
{
	uint8_t scanner[sizeof(GC_ObjectScanner)];
} GC_ObjectScannerState;

#endif /* OBJECTSCANNERSTATE_HPP_ */
