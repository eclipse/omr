/*******************************************************************************
 * Copyright (c) 2001, 2015 IBM Corp. and others
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
