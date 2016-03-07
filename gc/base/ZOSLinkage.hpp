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
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 ******************************************************************************/

/**
 * @file
 * @ingroup GC_Base_Core
 */

#if !defined(ZOSLINKAGE_HPP_)
#define ZOSLINKAGE_HPP_

/**
 * This file is used to house any strange tricks that Modron must perform
 * to get around ZOS platform quirks. 
 */

/** 
 * On ZOS, ar command does not export long symbol names on certain releases.
 * So, certain objects containing such long symbol names end up unresolved.
 * To resolve, we place a hack short symbol name in the object and reference
 * this from an object that does get resolved.
 */
#if defined(J9ZOS390)
extern int j9zos390LinkTrickMemorySubSpaceGenerational;
extern int j9zos390LinkTrickMemorySubSpaceGeneric;
extern int j9zos390LinkTrickMemorySubSpaceFlat;
extern int j9zos390LinkTrickVMThreadStackSlotIterator;
extern int j9zos390LinkTrickPhysicalSubArenaVirtualMemory;
extern int j9zos390LinkTrickPhysicalSubArenaVirtualMemoryFlat;
extern int j9zos390LinkTrickPhysicalSubArenaVirtualMemorySemiSpace;

#endif /* J9ZOS390 */

#endif /* ZOSLINKAGE_HPP_ */

