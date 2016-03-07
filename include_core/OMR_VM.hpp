/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2013, 2015
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

#if !defined(OMR_VM_HPP_)
#define OMR_VM_HPP_

#include "omr.h"
#include "OMR_BaseNonVirtual.hpp"
#include "OMR_VMConfiguration.hpp"

extern "C" {

/**
 * Internal: Attach a vmthread to the VM.
 *
 * @param[in] vm The VM
 * @param[in] vmthread The vmthread to attach. NOTE: Need NOT be the current thread!
 *                     vmthread->_os_thread must be initialized.
 *
 * @return an OMR error code
 */
omr_error_t attachThread(OMR_VM *vm, OMR_VMThread *vmthread);

/**
 * Internal: Detach a vmthread from the VM.
 *
 * @param[in] vm The VM
 * @param[in] vmthread The vmthread to detach. NOTE: Need NOT be the current thread!
 *
 * @return an OMR error code
 */
omr_error_t detachThread(OMR_VM *vm, OMR_VMThread *vmthread);

}

#endif /* OMR_VM_HPP_ */
