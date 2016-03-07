/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2015
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

#ifndef _PREFIXPATH_
#define _PREFIXPATH_
/* Insert a prefix (COMPILER_HEADER_PATH_PREFIX) before the path */
#ifndef COMPILER_HEADER_PATH_PREFIX
#define COMPILER_HEADER_PATH_PREFIX /usr/include
#endif
#define STR2(x) <##x##>
/* Need to do double indirection to force the macro to be expanded */
#define STR(x) STR2(x)
#define PREFIXPATH(h) STR(COMPILER_HEADER_PATH_PREFIX/h)
#endif
