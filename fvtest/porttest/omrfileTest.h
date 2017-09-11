/*******************************************************************************
 * Copyright (c) 2001, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#if !defined(J9FILETEST_H_INCLUDED)
#define J9FILETEST_H_INCLUDED

intptr_t omrfile_create_file(OMRPortLibrary *portLibrary, char *filename, int32_t openMode, const char *testName);
intptr_t omrfile_create_status_file(OMRPortLibrary *portLibrary, char *filename, const char *testName);
intptr_t omrfile_status_file_exists(OMRPortLibrary *portLibrary, char *filename, const char *testName);

#endif /* !defined(J9FILETEST_H_INCLUDED) */
