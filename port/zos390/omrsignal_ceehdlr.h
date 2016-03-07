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

int32_t omrsig_protect_ceehdlr(struct OMRPortLibrary *portLibrary, omrsig_protected_fn fn, void *fn_arg, omrsig_handler_fn handler, void *handler_arg, uint32_t flags, uintptr_t *result);
uint32_t omrsig_info_ceehdlr(struct OMRPortLibrary *portLibrary, void *info, uint32_t category, int32_t index, const char **name, void **value);
intptr_t omrsig_get_current_signal_ceehdlr(struct OMRPortLibrary *portLibrary);
int32_t ceehdlr_startup(struct OMRPortLibrary *portLibrary);
void ceehdlr_shutdown(struct OMRPortLibrary *portLibrary);
