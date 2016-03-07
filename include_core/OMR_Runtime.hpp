/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2013, 2014
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

#if !defined(OMR_RUNTIME_HPP_)
#define OMR_RUNTIME_HPP_

#include "omr.h"
#include "OMR_BaseNonVirtual.hpp"
#include "OMR_RuntimeConfiguration.hpp"

extern "C" {

/**
 * Internal: Attach a VM to the runtime.
 *
 * @param[in] *runtime the runtime
 * @param[in] *vm the VM to attach
 *
 * @return an OMR error code
 */
omr_error_t attachVM(OMR_Runtime *runtime, OMR_VM *vm);

/**
 * Internal: Detach a VM from the runtime.
 *
 * @param[in] *runtime the runtime
 * @param[in] *vm the VM to detach
 *
 * @return an OMR error code
 */
omr_error_t detachVM(OMR_Runtime *runtime, OMR_VM *vm);

}

#endif /* OMR_RUNTIME_HPP_ */
