/*******************************************************************************
 * Copyright (c) 2004, 2016 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/**
 * @file
 * @ingroup GC_Include
 */

#if !defined(MEMORYSPACESAPI_H_)
#define MEMORYSPACESAPI_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @ingroup GC_Include
 * @name MemorySpace Name/Descriptions
 * @{
 */
#define MEMORY_SPACE_NAME_UNDEFINED "No name"
#define MEMORY_SPACE_DESCRIPTION_UNDEFINED "No MemorySpace Description"

#define MEMORY_SPACE_NAME_FLAT "Flat"
#define MEMORY_SPACE_DESCRIPTION_FLAT "Flat MemorySpace Description"

#define MEMORY_SPACE_NAME_GENERATIONAL "Generational"
#define MEMORY_SPACE_DESCRIPTION_GENERATIONAL "Generational MemorySpace Description"

#define MEMORY_SPACE_NAME_METRONOME "Metronome"
#define MEMORY_SPACE_DESCRIPTION_METRONOME "Metronome MemorySpace Description"

/**
 * @}
 */

/**
 * @ingroup GC_Include
 * @name MemorySubSpace Name/Descriptions
 * @{
 */
#define MEMORY_SUBSPACE_NAME_UNDEFINED "No name"
#define MEMORY_SUBSPACE_DESCRIPTION_UNDEFINED "No MemorySubSpace Description"

#define MEMORY_SUBSPACE_NAME_FLAT "Flat"
#define MEMORY_SUBSPACE_DESCRIPTION_FLAT "Flat MemorySubSpace Description"

#define MEMORY_SUBSPACE_NAME_GENERIC "Generic"
#define MEMORY_SUBSPACE_DESCRIPTION_GENERIC "Generic MemorySubSpace Description"

#define MEMORY_SUBSPACE_NAME_SEMISPACE "SemiSpace"
#define MEMORY_SUBSPACE_DESCRIPTION_SEMISPACE "SemiSpace MemorySubSpace Description"

#define MEMORY_SUBSPACE_NAME_GENERATIONAL "Generational"
#define MEMORY_SUBSPACE_DESCRIPTION_GENERATIONAL "Generational MemorySubSpace Description"

#define MEMORY_SUBSPACE_NAME_METRONOME "Metronome"
#define MEMORY_SUBSPACE_DESCRIPTION_METRONOME "Metronome MemorySubSpace Description"


#ifdef __cplusplus
} /* extern "C" { */
#endif /* __cplusplus */

#endif /* MEMORYSPACESAPI_H_ */
