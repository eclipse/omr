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

#ifndef omrmmap_h
#define omrmmap_h

void omrmmap_unmap_file(struct OMRPortLibrary *portLibrary, J9MmapHandle *handle);
J9MmapHandle *omrmmap_map_file(struct OMRPortLibrary *portLibrary, intptr_t file, uint64_t offset, uintptr_t size, const char *mappingName, uint32_t flags);
int32_t omrmmap_startup(struct OMRPortLibrary *portLibrary);
int32_t omrmmap_capabilities(struct OMRPortLibrary *portLibrary);
void omrmmap_shutdown(struct OMRPortLibrary *portLibrary);
intptr_t omrmmap_msync(struct OMRPortLibrary *portLibrary, void *start, uintptr_t length, uint32_t flags);


#endif     /* omrmmap_h */


