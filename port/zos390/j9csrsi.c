/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2015
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

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "CSRSIC.h"
#include "omrcsrsi.h"

#define CSRSI_DSECT_LGTH                    0x4040U
#define CSRSI_PARAM_LGTH                    4U

struct j9csrsi_t {
	CSRSIRequest request;               /**< Input - CSRSI request type */
	CSRSIInfoAreaLen infoarea_len;      /**< Input - length of the buffer
						area for returning data */
	CSRSIReturnCode ret_code;           /**< Output - CSRSI return code */
	char infoarea[CSRSI_DSECT_LGTH];    /**< Input/Output - supplied buffer
						area that gets filled upon
						service return */
	uint32_t cplist[CSRSI_PARAM_LGTH];  /**< Storage for constructing
						4-argument parameter list */
};

const j9csrsi_t *
j9csrsi_init(int32_t *ret)
{
	struct j9csrsi_t *sess = NULL;

	/* CSRSI is amode31 only so we need memory below the 2GB */
	sess = (struct j9csrsi_t *) __malloc31(sizeof(*sess));

	if (NULL != sess) {
		memset(sess, 0, sizeof(*sess));

		/* Refer to CSRSIIDF programming interface information in
		 * MVS Data Areas Volume 1 (ABEP-DDRCOM). SIV2V3 DSECT maps
		 * the data for LPAR and VM.
		 */
		sess->request = (CSRSI_REQUEST_V1CPC_MACHINE |
						 CSRSI_REQUEST_V2CPC_LPAR | CSRSI_REQUEST_V3CPC_VM);
		sess->infoarea_len = sizeof(sess->infoarea);

		j9csrsi_wrp(sess);

		if (NULL != ret) {
			*ret = sess->ret_code;
		}

		/* Did CSRSI fail? */
		if (CSRSI_SUCCESS != sess->ret_code) {
			free(sess);
			sess = NULL;
		}
	}

	return sess;
}

BOOLEAN
j9csrsi_is_hw_info_available(int32_t ret)
{
	/* CSRSI_STSINOTAVAILABLE should never happen because STSI is available
	 * on all platforms we support (see comment 12 in JAZZ 90568).
	 */
	return !((CSRSI_STSINOTAVAILABLE == ret)
			 || (CSRSI_SERVICENOTAVAILABLE == ret));
}

void
j9csrsi_shutdown(const j9csrsi_t *session)
{
	if (NULL != session) {
		free(session);
	}

	return;
}

BOOLEAN
j9csrsi_is_lpar(const j9csrsi_t *session)
{
	BOOLEAN ret = FALSE;

	if (NULL != session) {
		const siv1v2v3 *const ia =
			(const siv1v2v3 *) &(session->infoarea);
		const si00 *const hdr = (const si00 *) &(ia->siv1v2v3si00);

		ret = (BOOLEAN)
			  (SI00CPCVARIETY_V2CPC_LPAR == hdr->si00cpcvariety)
			  && hdr->si00validsi22v2;
	}

	return ret;
}

BOOLEAN
j9csrsi_is_vmh(const j9csrsi_t *session)
{
	BOOLEAN ret = FALSE;

	if (NULL != session) {
		const siv1v2v3 *const ia =
			(const siv1v2v3 *) &(session->infoarea);
		const si00 *const hdr = (const si00 *) &(ia->siv1v2v3si00);

		ret = (BOOLEAN)
			  (SI00CPCVARIETY_V3CPC_VM == hdr->si00cpcvariety)
			  && hdr->si00validsi22v3;
	}

	return ret;
}

int32_t
j9csrsi_get_vmhpidentifier(const j9csrsi_t *session, uint32_t position,
						   char *buf, uint32_t len)
{
	int32_t ret = -1;

	if ((NULL != session)
		&& (NULL != buf)
		&& (0U < len)
		&& j9csrsi_is_vmh(session)
	) {
		const siv1v2v3 *const ia =
			(const siv1v2v3 *) &(session->infoarea);
		const si22v3 *const info =
			(const si22v3 *) &(ia->siv1v2v3si22v3);

		if (info->si22v3dbcountfield._si22v3dbcount > position) {
			const si22v3db *const db = &(info->si22v3dbe[position]);
			const char *const str =
				&(db->si22v3dbvmhpidentifier[0U]);
			const uint32_t str_sz =
				sizeof(db->si22v3dbvmhpidentifier);

			if (str_sz < len) {
				(void) strncpy(buf, str, str_sz);
				buf[str_sz] = '\0';

				ret = __etoa(buf);
			}
		}
	}

	return ret;
}

int32_t
j9csrsi_get_cpctype(const j9csrsi_t *session, char *buf, uint32_t len)
{
	int32_t ret = -1;

	if ((NULL != session)
		&& (NULL != buf)
		&& (0U < len)
	) {
		const siv1v2v3 *const ia =
			(const siv1v2v3 *) &(session->infoarea);
		const si00 *const hdr = (const si00 *) &(ia->siv1v2v3si00);

		if (hdr->si00validsi11v1
			&& (
				(SI00CPCVARIETY_V1CPC_MACHINE == hdr->si00cpcvariety)
				|| (SI00CPCVARIETY_V2CPC_LPAR == hdr->si00cpcvariety)
				|| (SI00CPCVARIETY_V3CPC_VM   == hdr->si00cpcvariety))
		) {
			const si11v1 *const info =
				(const si11v1 *) &(ia->siv1v2v3si11v1);
			const char *const str = &(info->si11v1cpctype[0U]);
			const uint32_t str_sz = sizeof(info->si11v1cpctype);

			if (str_sz < len) {
				(void) strncpy(buf, str, str_sz);
				buf[str_sz] = '\0';

				ret = __etoa(buf);
			}
		}
	}

	return ret;
}

