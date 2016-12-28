/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2014
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

#include "BaseVirtual.hpp"
	
/*
 * Required to force MM_BaseVirtual to have a vtable, otherwise
 * field offsets are wrong in DDR (due to addition of the vpointer
 * in derived classes). Using a virtual destructor causes linking
 * issues because we never use -lstdc++ (outside tests) and the
 * delete implementation will be missing (e.g. needed by stack allocation)
 */
//void MM_BaseVirtual::emptyMethod() { /* No implementation */ }
