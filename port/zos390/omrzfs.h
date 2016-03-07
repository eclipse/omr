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

#ifndef omrzfs_h
#define omrzfs_h

#include "omrcomp.h"

/* ZFS commands and sub-commands */
#define J9ZFSCALL_CONFIG                        0x40000006		/* ZFS configuration command code */
#define J9CFGOP_QUERY_CLIENT_CACHE_SIZE         231             /* Configuration sub-command: query client cache size */

#define J9CFGO_EYE                              "CFOP"			/* Eye catcher for configuration option structure */
#define J9CO_VER_INITIAL                        0x01            /* Version for configuration option structure */
#define J9CO_SLEN                               80              /* Size of configuration option string */

#define J9ZFSCALL_STATS                         0x40000007		/* ZFS statistics command code */
#define J9STATOP_USER_CACHE                     242             /* Statistics sub-command: user data cache information */
#define J9STATOP_META_CACHE                     248             /* Statistics sub-command: metadata cache information */

#define J9SA_EYE                                "STAP"			/* Eye catcher for statistics API structure */
#define J9SA_VER_INITIAL                        0x01			/* Version for statistics API structure */
#define J9SA_RESET                              0x80			/* Flags for statistics API structure - x80 means reset statistics */

#define J9ZFSNAME                               "ZFS     "
#define J9SIZE_CONVERSION_BASE                  10
#define J9G_TO_BYTES_CONVERSION_FACTOR			(1024 * 1024 * 1024)
#define J9M_TO_BYTES_CONVERSION_FACTOR			(1024 * 1024)
#define J9K_TO_BYTES_CONVERSION_FACTOR			1024

/* Format of physical file system control callable service */
#if defined(_LP64)	/* AMODE 64-bit */
#pragma linkage(BPX4PCT, OS64_NOSTACK)
extern void BPX4PCT(char *, int, int, char *, int *, int *, int *);
#else 				/* AMODE 31-bit */
#pragma linkage(BPX1PCT, OS)
extern void BPX1PCT(char *, int, int, char *, int *, int *, int *);
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/* File system control callable service parameter list */
typedef struct J9ZFSSyscallParamList {
	int32_t opcode;            /* Operation code to perform */
	int32_t params[7];         /* Specific to type of operation, provides access to the params */
} J9ZFSSyscallParamList;

/* High and low values for buffer count */
typedef struct j9hyper {
	uint32_t high;
	uint32_t low;
} j9hyper;

/* Data space entry structure */
typedef struct j9ds_entry {
	char ds_name[9];
	uint8_t pad1[3];
	int32_t ds_alloc_segs;
	int32_t ds_free_pages;
	int32_t ds_reserved[5];    /* reserved for future use*/
} j9ds_entry;

/* Reset time structure for statistics API */
typedef struct j9reset_time {
	uint32_t posix_time_high;         /* high order 32 bits since epoch */
	uint32_t posix_time_low;          /* low order 32 bits since epoch */
	uint32_t posix_usecs;             /* microseconds */
	int32_t pad1;
} j9reset_time;

/* Statistics API query control structure */
typedef struct J9ZFSStatApi {
	char sa_eye[4];           		/**< 4-byte Eye Catcher */
	int32_t sa_len;               		/**< length of the buffer that follows this structure for returning data */
	int32_t sa_ver;               		/**< version number (currently always 1) */
	char sa_flags;            		/**< flags field (must be x00 or x80, x80 means reset statistics) */
	uint8_t sa_fill[3];          		/**< spare bytes */
	int32_t sa_reserve[4];        		/**< reserved */
	j9reset_time reset_time_info;	/**< reset time details */
} J9ZFSStatApi;

/* The following structure is part of the user data cache statistics */
typedef struct j9vm_stats {
	/* First set of counters are for external requests to the VM system. */
	uint32_t vm_schedules;
	uint32_t vm_setattrs;
	uint32_t vm_fsyncs;
	uint32_t vm_unmaps;
	uint32_t vm_reads;
	uint32_t vm_readasyncs;
	uint32_t vm_writes;
	uint32_t vm_getattrs;
	uint32_t vm_flushes;
	uint32_t vm_scheduled_deletes;

	/* Next two are fault counters, they measure number of read or write
	 * requests requiring a fault to read in data, this synchronizes
	 * an operation to a DASD read, we want these counters as small as
	 * possible. (These are read I/O counters).
	 */
	uint32_t vm_reads_faulted;
	uint32_t vm_writes_faulted;
	uint32_t vm_read_ios;

	/* Next counters are write counters. They measure number of times
	 * we scheduled and waited for write I/Os.
	 */
	uint32_t vm_scheduled_writes;
	uint32_t vm_error_writes;
	uint32_t vm_reclaim_writes;       /* Wrote dirty data for reclaim */

	/* Next counters are I/O wait counters. They count the number of
	 * times we had to wait for a write I/O and under what conditions.
	 */
	uint32_t vm_read_waits;
	uint32_t vm_write_waits;
	uint32_t vm_fsync_waits;
	uint32_t vm_error_waits;
	uint32_t vm_reclaim_waits;        /* Waited for pending I/O for reclaim */

	/* Final set are memory management counters. */
	uint32_t vm_reclaim_steal;        /* Number of times steal from others function invoked */
	uint32_t vm_waits_for_reclaim;    /* Waits for reclaim thread */
	uint32_t vm_reserved[10];         /* reserved for future use */
} j9vm_stats;

/* User Cache information structure */
typedef struct J9ZFSStatUserCache {
	j9vm_stats stuc[2];              /**< Various statistics for both LOCAL and REMOTE systems*/
	int32_t stuc_dataspaces;            /**< Number of dataspaces in user data cache */
	int32_t stuc_pages_per_ds;          /**< Pages per dataspace */
	int32_t stuc_seg_size_loc;          /**< Local Segment Size (in K) */
	int32_t stuc_seg_size_rmt;          /**< Remote Segment Size (in K) */
	int32_t stuc_page_size;             /**< Page Size (in K) */
	int32_t stuc_cache_pages;           /**< Total number of pages */
	int32_t stuc_total_free;            /**< Total number of free pages */
	int32_t stuc_vmSegTable_cachesize;  /**< Number of segments */
	int32_t stuc_reserved[5];           /**< reserved */
	j9ds_entry stuc_ds_entry[32];    /**< Array of dataspace entries */
} J9ZFSStatUserCache;

/* The parameter area passed to pfsctl() for User Cache information. */
typedef struct J9ZFSStatParam {
	J9ZFSSyscallParamList syscallParamList;
	J9ZFSStatApi statApi;
	J9ZFSStatUserCache statUserCache;
	char systemName[9];
} J9ZFSStatParam;

/* Metadata Primary Space Cache structure
 * Except buffers all the fields in this structure are of type int32_t
 * because zFS metadata statistics API expects those fields should be
 * of type int.
 */
typedef struct j9primary_stats {
	j9hyper buffers;                   /**< Number of buffers in cache */
	int32_t buffsize;                   /**< Size of each buffer in K bytes */
	int32_t amc_res1;                   /**< Reserved for future use, zero in version 1 */
	int32_t requests_reserved;          /**< Reserved */
	int32_t requests;                   /**< Requests to the cache */
	int32_t hits_reserved;              /**< Reserved */
	int32_t hits;                       /**< Hits in the cache */
	int32_t updates_reserved;           /**< Reserved */
	int32_t updates;                    /**< Updates to buffers in the cache */
	int32_t reserved[10];               /**< For future use */
} j9primary_stats;

/* Metadata Backing Cache structure
 * Except buffers all the fields in this structure are of type int32_t
 * because zFS metadata statistics API expects those fields should be
 * of type int.
 */
typedef struct j9backcache_stats {
	j9hyper buffers;                   /**< Number of buffers in cache */
	int32_t buffsize;                   /**< Size of each buffer in K bytes */
	int32_t amc_res1;                   /**< Reserved for future use, zero in version 1 */
	int32_t requests_reserved;          /**< Reserved */
	int32_t requests;                   /**< Requests to the cache */
	int32_t hits_reserved;              /**< Reserved */
	int32_t hits;                       /**< Hits in the cache */
	int32_t discards_reserved;          /**< Reserved */
	int32_t discards;                   /**< Discards of data from backing cache */
	int32_t reserved[10];               /**< For future use */
} j9backcache_stats;

/* Metadata Cache information structure */
typedef struct J9ZFSAPIMetaStats {
	char am_eye[4];                 /**< Eye catcher = AMET */
	int16_t am_size;                  	/**< Size of output structure */
	uint8_t am_version;                 /**< Version of stats */
	uint8_t am_reserved1;              	/**< Reserved byte, 0 in version 1 */
	j9primary_stats am_primary;     /**< Primary space cache statistics */
	j9backcache_stats am_backcache; /**< Backing cache statistics */
	int32_t am_reserved3[10];          /**< Reserved for future use */
} J9ZFSAPIMetaStats;

/* The parameter area passed to pfsctl() for Metadata Cache information. */
typedef struct J9ZFSMetaStatParam {
	J9ZFSSyscallParamList syscallParamList;
	J9ZFSStatApi statApi;
	J9ZFSAPIMetaStats apiMetaStats;
	char systemName[9];
} J9ZFSMetaStatParam;

/* Configuration Option information structure */
typedef struct J9ZFSConfigOption {
	char co_eye[4];                 /**< Eye catcher */
	int16_t co_len;                   	/**< Length of structure */
	uint8_t co_ver;                     /**< Version of structure */
	char co_string[J9CO_SLEN + 1];  /**< String value for option must be 0 terminated */
	int co_value[4];                /**< Place for integer values */
	uint8_t co_reserved[24];           	/**< Reserved for future use */
} J9ZFSConfigOption;

/* The parameter area passed to pfsctl() for Configuration Option information. */
typedef struct J9ZFSConfigParam {
	J9ZFSSyscallParamList syscallParamList;
	J9ZFSConfigOption configOption;
	char systemName[9];
} J9ZFSConfigParam;

/**
 * Retrieve the amount of used ZFS file system user cache.
 * @param[out] userCacheUsedPtr Pointer to the returned used user cache area size (in bytes).
 * @return 0 on success; negative value on failure.
 */
int32_t
getZFSUserCacheUsed(uint64_t *userCacheUsedPtr);

/**
 * Retrieve the size of ZFS file system metadata cache buffers.
 * @param[out] bufferCacheSizePtr Pointer to the returned metadata cache buffer area size (in bytes).
 * @return 0 on success; negative value on failure.
 */
int32_t
getZFSMetaCacheSize(uint64_t *bufferCacheSizePtr);

#if defined(__cplusplus)
}
#endif

#endif /* omrzfs_h */
