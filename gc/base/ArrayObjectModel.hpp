/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
