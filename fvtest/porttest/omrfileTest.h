/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2001, 2016
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

#if !defined(J9FILETEST_H_INCLUDED)
#define J9FILETEST_H_INCLUDED

intptr_t omrfile_create_file(OMRPortLibrary *portLibrary, char *filename, int32_t openMode, const char *testName);
intptr_t omrfile_create_status_file(OMRPortLibrary *portLibrary, char *filename, const char *testName);
intptr_t omrfile_status_file_exists(OMRPortLibrary *portLibrary, char *filename, const char *testName);

#endif /* !defined(J9FILETEST_H_INCLUDED) */
