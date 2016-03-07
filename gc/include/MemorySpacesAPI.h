/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2004, 2016
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
