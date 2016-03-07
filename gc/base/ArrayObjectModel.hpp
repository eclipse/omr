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

#if !defined(ARRAYOBJECTMODEL_HPP_)
#define ARRAYOBJECTMODEL_HPP_

#include "omrcfg.h"

/*
 * GC_ArrayObjectModel isn't a true class -- instead it's just a typedef for GC_ArrayletObjectModel
 */

#if defined(OMR_GC_ARRAYLETS)
#include "ArrayletObjectModel.hpp"
typedef GC_ArrayletObjectModel GC_ArrayObjectModel; /**< object model for arrays (arraylet configuration) */
#else /* OMR_GC_ARRAYLETS */
#error "Non-arraylet indexable object model is not implemented"
#endif /* OMR_GC_ARRAYLETS */

#endif /* ARRAYOBJECTMODEL_HPP_ */
