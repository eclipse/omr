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

#ifndef omrmmap_h
#define omrmmap_h

void omrmmap_unmap_file(struct OMRPortLibrary *portLibrary, J9MmapHandle *handle);
J9MmapHandle *omrmmap_map_file(struct OMRPortLibrary *portLibrary, intptr_t file, uint64_t offset, uintptr_t size, const char *mappingName, uint32_t flags);
int32_t omrmmap_startup(struct OMRPortLibrary *portLibrary);
int32_t omrmmap_capabilities(struct OMRPortLibrary *portLibrary);
void omrmmap_shutdown(struct OMRPortLibrary *portLibrary);
intptr_t omrmmap_msync(struct OMRPortLibrary *portLibrary, void *start, uintptr_t length, uint32_t flags);

#endif     /* omrmmap_h */


