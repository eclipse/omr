/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1998, 2016
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

#ifndef OMR_TRACE_INTERNAL_H_INCLUDED
#define OMR_TRACE_INTERNAL_H_INCLUDED

#include "omrpool.h"
#include "omrtrace.h"
#include "ute_core.h"
#include "ute_dataformat.h"

#ifdef  __cplusplus
extern "C" {
#endif


#define OS_THREAD_FROM_TRACE_THREAD(thr)	((omrthread_t)(thr)->synonym1)
#define OMR_VMTHREAD_FROM_TRACE_THREAD(thr)	((OMR_VMThread *)(thr)->synonym2)
#define OMR_TRACE_THREAD_FROM_ENV(env) \
	( env? OMR_TRACE_THREAD_FROM_VMTHREAD((OMR_VMThread *)env) : twThreadSelf() )

/*
 * =============================================================================
 *  Constants
 * =============================================================================
 */
/* Enable logging to separate exception trace buffers. This is not useful because
 * the exception trace buffers can't be sent to any subscribers.
 */
#define OMR_ENABLE_EXCEPTION_OUTPUT 0

/* Temporarily allow and ignore the output=<filename> command-line option until
 * it has been removed from all test scripts.
 */
#define OMR_ALLOW_OUTPUT_OPTION 1

#define UT_DEBUG                      "UTE_DEBUG"
#if OMR_ENABLE_EXCEPTION_OUTPUT
#define UT_EXCEPTION_THREAD_NAME      "Exception trace pseudo-thread"
#endif
#define UT_TPID                       "TPID"
#define UT_TPNID                      "TPNID"

#define UT_SUSPEND_USER               8

#define UT_CONTEXT_INITIAL            0
#define UT_CONTEXT_COMPONENT          1
#define UT_CONTEXT_CLASS              2
#define UT_CONTEXT_PROCESS            3
#define UT_CONTEXT_FINAL              4

#define UT_NORMAL_BUFFER              0
#if OMR_ENABLE_EXCEPTION_OUTPUT
#define UT_EXCEPTION_BUFFER           1
#endif /* OMR_ENABLE_EXCEPTION_OUTPUT */

#define UT_TRC_BUFFER_FULL			  0x00000001 /* indicates a buffer that has been published */
#define UT_TRC_BUFFER_NEW             0x20000000 /* indicates an empty new buffer in use by a thread. cleared when buffer is written to. */
#define UT_TRC_BUFFER_ACTIVE          0x80000000 /* indicates a buffer in use by a thread */

/*
 * =============================================================================
 * Constants for trace point actions.
 * =============================================================================
 */
#define UT_MINIMAL                    1
#define UT_MAXIMAL                    2
#define UT_COUNT                      4
#define UT_PRINT                      8
#define UT_EXCEPTION                  32
#define UT_NONE                       0
/* Note that another flag, UT_SPECIAL_ASSERTION, is defined in
 * ut_module.h and occupies the top byte, not the bottom.
 */

#define UT_POINTER_SPEC        "0x%zx"

/* assert() and abort() are a bit useless on Windows - they
 * just print a message. Force a GPF instead.
 */
#if defined(WIN32)

#include <stdio.h>
#include <stdlib.h>

#define UT_ASSERT(expr) do { \
		if (! (expr)) { \
			fprintf(stderr,"UT_ASSERT FAILED: %s at %s:%d\n",#expr,__FILE__,__LINE__); \
			*((int*)0) = 42; \
		} \
	} while (0)

#else /* defined(WIN32) */

#include <assert.h>
#define UT_ASSERT(expr) assert(expr)

#endif /* defined(WIN32) */

#define DBG_ASSERT(expr) \
	if (OMR_TRACEGLOBAL(traceDebug) > 0) { \
		UT_ASSERT((expr)); \
	}

#define UT_DBG_PRINT( data )\
	twFprintf data;

#define UT_DBGOUT(level, data) \
	if (OMR_TRACEGLOBAL(traceDebug) >= level) { \
		UT_DBG_PRINT(data); \
	} else;

#define UT_DBGOUT_CHECKED(level, data) \
	if ((NULL != omrTraceGlobal) && (OMR_TRACEGLOBAL(traceDebug) >= level)) { \
		UT_DBG_PRINT(data); \
	} else;

#define UT_DBGOUT_NOGLOBAL(level, actualLevel, data) \
	do { \
		if ((actualLevel) >= (level)) { \
			UT_DBG_PRINT(data); \
		} \
	} while (0)

#define OMR_TRACEGLOBAL(x) (omrTraceGlobal->x)

/*
 *  Forward declaration of OMR_TraceGlobal
 */
typedef struct OMR_TraceGlobal OMR_TraceGlobal;

/*
 * =============================================================================
 *  Trace configuration buffer (UTCF)
 * =============================================================================
 */
#define UT_TRACE_CONFIG_NAME "UTCF"
typedef struct  UtTraceCfg {
	UtDataHeader       header;
	struct UtTraceCfg  *next;             /* Next trace config command        */
	char               command[1];       /* Start of variable length section */
} UtTraceCfg;

typedef struct UtDeferredConfigInfo {
	char *componentName;
	int32_t all;
	int32_t firstTracePoint;
	int32_t lastTracePoint;
	unsigned char value;
	int level;
	char *groupName;
	struct UtDeferredConfigInfo *next;
	int32_t setActive;
} UtDeferredConfigInfo;

/* The following structures will form the core of the new trace control mechanisms. */
#define UT_TRACE_COMPONENT_DATA "UTCD"
typedef struct UtComponentData {
	UtDataHeader           header;
	char                   *componentName;
	char                   *qualifiedComponentName;
	UtModuleInfo           *moduleInfo;
	int                    tracepointCount;
	int                    numFormats;
	char                   **tracepointFormattingStrings;
	uint64_t               *tracepointcounters;
	int                    alreadyfailedtoloaddetails;
	char                   *formatStringsFileName;
	struct UtComponentData *prev;
	struct UtComponentData *next;
} UtComponentData;

#define UT_TRACE_COMPONENT_LIST "UTCL"
typedef struct UtComponentList {
	UtDataHeader          header;
	UtComponentData       *head;
	UtDeferredConfigInfo  *deferredConfigInfoHead;
} UtComponentList;

typedef enum OMR_TraceEngineInitState {
	OMR_TRACE_ENGINE_UNINITIALIZED = 0,
	OMR_TRACE_ENGINE_ENABLED, /* initialized, but no threads can attach */
	OMR_TRACE_ENGINE_MT_ENABLED, /* threads can attach */
	OMR_TRACE_ENGINE_SHUTDOWN_STARTED
} OMR_TraceEngineInitState;

#define OMR_TRACE_ENGINE_IS_ENABLED(initState)	\
	(((initState) >= OMR_TRACE_ENGINE_ENABLED) && ((initState) <= OMR_TRACE_ENGINE_SHUTDOWN_STARTED))

/*
 * =============================================================================
 *  Trace Global Data
 * =============================================================================
 */
struct OMR_TraceGlobal {
	const OMR_VM *vm;				/* Client identifier               */
	OMRPortLibrary *portLibrary;    /* Port Library                    */
	OMR_TraceEngineInitState initState;
	uint64_t startPlatform;          /* Platform timer                  */
	uint64_t startSystem;            /* Time relative 1/1/1970          */
	int32_t bufferSize;             /* Trace buffer size               */
	uint32_t lostRecords;            /* Lost record counter             */
	int32_t traceDebug;             /* Trace debug level               */
	int32_t initialSuspendResume;   /* Initial thread suspend count    */
	uint32_t traceSuspend;           /* Global suspend flag             */
	int32_t dynamicBuffers;         /* Dynamic buffering requested     */
	int32_t indentPrint;            /* Indent print trace              */
	omrthread_monitor_t traceLock;              /* Trace monitor pointer           */
	int32_t traceCount;             /* trace counters enabled          */
	char *properties;             /* System properties               */
	char *serviceInfo;            /* Service information             */
	char *traceFormatSpec;        /* Printf template filespec        */
#if OMR_ENABLE_EXCEPTION_OUTPUT
	OMR_TraceThread *exceptionContext;	/* OMR_TraceThread for last excptn    */
	OMR_TraceBuffer *exceptionTrcBuf;	/* Exception trace buffers         */
#endif /* OMR_ENABLE_EXCEPTION_OUTPUT */
	OMR_TraceThread *lastPrint;		/* OMR_TraceThread for last print     */
	OMR_TraceBuffer *freeQueue;		/* Free buffer queue               */
	omrthread_monitor_t freeQueueLock;	/* lock for free queue */
	UtTraceCfg *config;				/* Trace selection cmds link/list  */
	UtTraceFileHdr *traceHeader;	/* Trace file header               */
	UtComponentList *componentList;	/* registered or configured component */
	UtComponentList *unloadedComponentList;	/* unloaded component */
	volatile uint32_t threadCount;	/* Number of threads being traced  */
	volatile UtSubscription *subscribers;	/* List of external trace subscribers */
	omrthread_monitor_t subscribersLock;	/* Enforces atomicity of updates to the list of external trace subscribers */
	int32_t traceInCore;            /* If true then we don't queue buffers */
	volatile uint32_t allocatedTraceBuffers;	/* The number of allocated trace buffers ????*/
	int fatalassert;				/* Whether assertion type trace points are fatal or not. */
	OMR_TraceLanguageInterface languageIntf;				 /* Language interface */
	J9Pool *bufferPool;				/* Pool for allocating all UtTraceBuffers */
	omrthread_monitor_t bufferPoolLock;	/* Lock for buffer pool. Do not allow tracepoints while locking, holding, or releasing this monitor. */
	J9Pool *threadPool;				/* Pool for allocating all UtThreadData */
	omrthread_monitor_t threadPoolLock;	/* Lock for thread pool. Do not allow tracepoints while locking, holding, or releasing this monitor. */
};

/*
 * =============================================================================
 *  Internal function prototypes
 * =============================================================================
 */

omr_error_t initialiseComponentData(UtComponentData **componentDataPtr, UtModuleInfo *moduleInfo, const char *componentName);
void freeComponentData(OMR_TraceGlobal *global, UtComponentData *componentDataPtr);
omr_error_t initialiseComponentList(UtComponentList **componentListPtr);
omr_error_t freeComponentList(OMR_TraceGlobal *global, UtComponentList *componentList);
omr_error_t addComponentToList(UtComponentData *componentData, UtComponentList *componentList);
omr_error_t removeModuleFromList(UtModuleInfo *module, UtComponentList *componentList);
UtComponentData *getComponentData(const char *componentName, UtComponentList *componentList);
omr_error_t setTracePointsTo(const char *componentName, UtComponentList *componentList, int32_t all, int32_t first, int32_t last, unsigned char value, int level, const char *groupName, BOOLEAN suppressMessages, int32_t setActive);
omr_error_t setTracePointsToParsed(const char *componentName, UtComponentList *componentList, int32_t all, int32_t first, int32_t last, unsigned char value, int level, const char *groupName, BOOLEAN suppressMessages, int32_t setActive);
char *getFormatString(const char *componentName, int32_t tracepoint);
char *getTracePointName(const char *componentName, UtComponentList *componentList, int32_t tracepoint);
omr_error_t setTracePointGroupTo(const char *groupName, UtComponentData *componentData, unsigned char value, BOOLEAN suppressMessages, int32_t setActive);
omr_error_t setTracePointsByLevelTo(UtComponentData *componentData, int level, unsigned char value, int32_t setActive);
omr_error_t processComponentDefferedConfig(UtComponentData *componentData, UtComponentList *componentList);
uint64_t incrementTraceCounter(UtModuleInfo *moduleInfo, UtComponentList *componentList, int32_t tracepoint);
omr_error_t addTraceConfig(OMR_TraceThread *thr, const char *cmd);
omr_error_t addTraceConfigKeyValuePair(OMR_TraceThread *thr, const char *cmdKey, const char *cmdValue);


/*
 * =============================================================================
 *  Functions called by users of the trace library at initialisation/shutdown time.
 *  (Runtime functions are called vi OMR_TraceInterface once initialised)
 * =============================================================================
 */

/**
 * @brief Fill in the interfaces the runtime will use to access trace functions.
 *
 * Fill in the interfaces the runtime will use to access trace functions.
 * This function fills in the function tables that an application can use to access
 * the trace engine functions.
 *
 * OMR_TraceInterface contains the functions the application can use to access and control
 * the trace engine.
 *
 * UtModuleInterface contains the functions used by a module (library) to initialize
 * it's trace setup and to take trace points. These functions are usually accessed via
 * the macros generated from the TDF files.
 *
 * @param[in,out] utIntf Will be returned a pointer to a newly initialized UtInterface structure.
 * @param[in,out] omrTraceIntf
 * @param[in,out] utModuleIntf
 *
 * @return OMR_ERROR_NONE if successful, an error code describing the failure otherwise.
 */
omr_error_t fillInUTInterfaces(UtInterface **utIntf, OMR_TraceInterface *omrTraceIntf, UtModuleInterface *utModuleIntf);

/**
 * @brief Initialize the trace engine.
 *
 * Initialize the trace engine.
 *
 * The gbl pointer is intended to allow the trace global data structure
 * to be linked to somewhere public so that it can be found by debugging
 * tools, this allows them to do things like extract trace buffers from
 * core files.
 * It is not intended to give read/write access to the trace global data
 * at runtime and is deliberately a void** for a structure that is not
 * publicly defined.
 *
 * The opts field is used for the same purpose as the opts field in
 * setOptions. See the documentation for that function for full details.
 *
 * The function pointer types for the fields on OMR_TraceLanguageInterface are
 * documented with the function pointer definitions.
 *
 * @param[in,out] vm The OMR_VM the trace engine will be running inside.
 * @param[in]     datDir Location of trace format files.
 * @param[in]     languageIntf A set of function pointers for accessing function in the OMR based runtime.
 *
 * @return OMR_ERROR_NONE if successful, an error code describing the failure otherwise.
 */
omr_error_t initializeTrace(OMR_VM *vm, const char *datDir, const OMR_TraceLanguageInterface *languageIntf);

/**
 * @brief Start trace engine shutdown.
 *
 * @pre The current thread must be attached to the trace engine.
 *
 * Requests the trace engine to start shutting down.
 * Prevents new threads from attaching.
 * Prevents new tracepoints from being logged.
 *
 * @return OMR_ERROR_NONE if successful, an error code describing the failure otherwise.
 */
omr_error_t utTerminateTrace(void);

/**
 * @brief Free the trace engines internal data structures.
 *
 * @pre The current thread must be attached to the trace engine.
 *
 * This must only be called after utTerminateTrace and will free any remaining
 * memory held by the trace engine.
 *
 * After calling this no further calls may be made to the trace engine.
 */
void freeTrace(void);

/**
 * @brief Notify trace that a new thread has started.
 *
 * Notify the trace engine that a new thread has started. Trace will
 * allocate a new OMR_TraceThread and store a pointer to it in the thr
 * parameter.
 *
 * A thread *must not* attempt to take a trace point or invoke trace
 * functions until this function has been called.
 *
 * Note that passing another threads OMR_TraceThread when taking a
 * trace point may work - but will lead to the event represented
 * by that trace point appearing to have occurred on the other thread
 * once the trace is formatted. This is extremely undesirable when
 * using trace for problem determination.
 *
 * NOTE: Currently thrSynonym1 must be the omrthread_t for this thread and
 * thrSynonym2 must be the OMR_VMThread.
 *
 * @param[out] thr The OMR_TraceThread structure for this thread.
 * @param[in]  threadId The id of this thread. Must be unique. (The address of a per thread structure is recommended)
 * @param[in]  threadName A name for this thread. Will be displayed when trace data is formatted. May be NULL.
 * @param[in]  thrSynonym1 A synonym for this threads id to aid cross referencing in problem diagnosis.
 * @param[in]  thrSynonym2 A synonym for this threads id to aid cross referencing in problem diagnosis.
 *
 * @return OMR_ERROR_NONE if successful, an error code describing the failure otherwise.
 */
omr_error_t threadStart(OMR_TraceThread **thr, const void *threadId, const char *threadName, const void *thrSynonym1, const void *thrSynonym2);

/**
 * @brief Notify trace that a thread has stopped.
 *
 * Inform the trace engine that the thread represented by this
 * OMR_TraceThread has terminated and free it's allocated OMR_TraceThread.
 *
 * @param[in,out] thr The OMR_TraceThread structure for this thread.
 *
 * @return OMR_ERROR_NONE if successful, an error code describing the failure otherwise.
 */
omr_error_t threadStop(OMR_TraceThread **thr);

/**
 * @brief Set trace options
 *
 * Set trace options, options are supplied in an array of strings.
 * For options that are key/value pairs (for example maximal=all)
 * the key is passed in position n and the value in n+1:
 * opts[n] = "maximal";
 * opts[n+1] = "all";
 * For simple flag options (for example nofatalassert) the flag is
 * passed in opts[n] and opts[n+1] is null.
 * opts[n] = "nofatalassert";
 * opts[n+1] = NULL;
 *
 * The atRuntime flag is set to TRUE if trace startup is complete
 * (startTraceWorkerThread has been called) and FALSE if it
 * setOptions is called before that. Certain options may only be
 * set during startup. If atRuntime is TRUE setOptions will not attempt
 * to set those options.
 *
 * The SetOptions call available on OMR_TraceInterface is identical but
 * is only intended to be called once trace startup is complete and
 * effectively is the same call with atRuntime set to TRUE.
 *
 * @param[in] thr The OMR_TraceThread for the currently executing thread. Must not be NULL.
 * @param[in] opts An array of options to be passed to the trace engine.
 * @param[in] atRuntime A flag indicating whether trace startup is complete.
 *
 * @return The integer debug level the trace engine is using.
 */
omr_error_t setOptions(OMR_TraceThread *thr, const char **opts, BOOLEAN atRuntime);

/**
 * @brief Set the directory to search for trace format files.
 * @param[in] datDir Path of the directory containing trace format files (*TraceFormat.dat).
 * @return an OMR error code
 */
omr_error_t setFormat(const char *datDir);

/**
 * @brief Creates a trace point
 *
 * Creates a trace point to record a trace event for the specified thread (generally the
 * currently running thread). The trace point is in the module described by modInfo and
 * has it's trace id masked with it's activation information. (This is generated as part
 * of the trace macros and should simply be passed through.)
 *
 * The spec parameter describes the arguments to the trace point (and is also generated)
 * and those arguments (if any) are contained in the va_list.
 *
 * This function exists to allow users to override the default implementation of
 * UtModuleInterface.Trace(void *env, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, ...);
 * and pass in the var args arguments (since the use of var args prevents simply calling
 * the original UtModuleInterface.Trace function). This allows trace implementors to
 * map from a per thread env parameter that is convenient for their application to a
 * OMR_TraceThread. All the other parameters should be left unchanged.
 *
 * @param[in] thr The OMR_TraceThread for the currently executing thread. Must not be NULL.
 * @param[in] modInfo A pointer to the UtModuleInfo for the module this trace point belongs to.
 * @param[in] traceId The trace point id for this trace point.
 * @param[in] spec    The trace point format specification for this trace point
 * @param[in] varArgs The va_list of parameters for this trace point.
 */
void doTracePoint(OMR_TraceThread *thr, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, va_list varArgs);

void enlistRecordSubscriber(UtSubscription *subscription);
void delistRecordSubscriber(UtSubscription *subscription);
void deleteRecordSubscriber(OMR_TraceGlobal *global, UtSubscription *subscription);
BOOLEAN findRecordSubscriber(UtSubscription *subscription);
omr_error_t destroyRecordSubscriber(OMR_TraceThread *thr, UtSubscription *subscription, BOOLEAN callAlarm);

void listCounters(void);
void initHeader(UtDataHeader *header, const char *name, uintptr_t size);
int32_t getTraceLock(OMR_TraceThread *thr);
int32_t freeTraceLock(OMR_TraceThread *thr);
void checkGetTraceLock(OMR_TraceThread *thr);
void checkFreeTraceLock(OMR_TraceThread *thr);

omr_error_t setTraceState(const char *cmd, BOOLEAN atRuntime);
omr_error_t processEarlyOptions(const char **opts);
omr_error_t processOptions(OMR_TraceThread *thr, const char **opts, BOOLEAN atRuntime);
omr_error_t expandString(char *returnBuffer, const char *original, BOOLEAN atRuntime);
int hexStringLength(const char *str);
void getTimestamp(int64_t time, uint32_t *pHours, uint32_t *pMinutes, uint32_t *pSeconds, uint32_t *pMillis);
void incrementRecursionCounter(OMR_TraceThread *thr);
void decrementRecursionCounter(OMR_TraceThread *thr);
omr_error_t initTraceHeader(void);

/** Unpacked trace wrappers. **/
void  twFprintf(const char *fmtTemplate, ...);
omr_error_t  twE2A(char *str);
OMR_TraceThread *twThreadSelf(void);

omr_error_t getComponentGroup(char *name, char *group, int32_t *count, int32_t **tracePts);
void internalTrace(OMR_TraceThread *thr, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, ...);
/**************************************************************************
 * name        - reportCommandLineError
 * description - Report an error in a command line option and put the given
 * 			   - string in as detail in an NLS enabled message.
 * parameters  - detailStr, args for formatting into detailStr.
 *************************************************************************/
void reportCommandLineError(BOOLEAN atRuntime, const char *detailStr, ...);

/** Functions exposed to the outside world via the server interface and used in rastrace code
 *  outside of main.c
 *  All functions on the server interface (and only functions on the server interface) start
 *  with trc **/
omr_error_t trcRegisterRecordSubscriber(OMR_TraceThread *thr, const char *description, utsSubscriberCallback subscriber,
										utsSubscriberAlarmCallback alarm, void *userData, UtSubscription **subscriptionReference);
omr_error_t trcDeregisterRecordSubscriber(OMR_TraceThread *thr, UtSubscription *subscriptionID);

/** Functions exposed to the outside world via the module interface and used outside main.c
 *  All functions on the module interface (and only functions on the module interface) start
 *  with j9 **/
void omrTrace(void *env, UtModuleInfo *modInfo, uint32_t traceId, const char *spec, ...);


/**
 * @brief Publish a trace buffer.
 * @param[in] currentThr The current thread. Might not be the thread that wrote buf.
 * @param[in] buf The trace buffer to publish.
 * @return an OMR error code
 */
omr_error_t publishTraceBuffer(OMR_TraceThread *currentThr, OMR_TraceBuffer *buf);

/**
 * @brief Release a trace buffer.
 *
 * A published message buffer can be released after all subscribers have
 * processed it.
 *
 * @param[in] currentThr The current thread. Might not be the thread that wrote buf.
 * @param[in] buf The trace buffer to release.
 * @return an OMR error code
 */
omr_error_t releaseTraceBuffer(OMR_TraceThread *currentThr, OMR_TraceBuffer *buf);

/**
 * @brief Get a recycled trace buffer.
 *
 * @param[in] currentThr The current thread.
 * @return a trace buffer, or NULL if none is available
 */
OMR_TraceBuffer *recycleTraceBuffer(OMR_TraceThread *currentThr);

/*
 * =============================================================================
 *  Externs
 * =============================================================================
 */

extern OMR_TraceGlobal *omrTraceGlobal;
extern omrthread_tls_key_t j9uteTLSKey;

#ifdef  __cplusplus
}
#endif

#endif /* !OMR_TRACE_INTERNAL_H_INCLUDED */
