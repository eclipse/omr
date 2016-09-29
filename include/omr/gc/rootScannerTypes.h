/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 1991, 2016
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

#if !defined(ROOTSCANNERTYPES_H__)
#define ROOTSCANNERTYPES_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum RootScannerEntity {
	RootScannerEntity_None = 0,
	RootScannerEntity_ScavengeRememberedSet,
	RootScannerEntity_Classes,
	RootScannerEntity_VMClassSlots,
	RootScannerEntity_PermanentClasses,
	RootScannerEntity_ClassLoaders,
	RootScannerEntity_Threads,
	RootScannerEntity_FinalizableObjects,
	RootScannerEntity_UnfinalizedObjects,
	RootScannerEntity_OwnableSynchronizerObjects,
	RootScannerEntity_StringTable,
	RootScannerEntity_JNIGlobalReferences,
	RootScannerEntity_JNIWeakGlobalReferences,
	RootScannerEntity_DebuggerReferences,
	RootScannerEntity_DebuggerClassReferences,
	RootScannerEntity_MonitorReferences,
	RootScannerEntity_WeakReferenceObjects,
	RootScannerEntity_SoftReferenceObjects,
	RootScannerEntity_PhantomReferenceObjects,
	RootScannerEntity_JVMTIObjectTagTables,
	RootScannerEntity_NonCollectableObjects,
	RootScannerEntity_RememberedSet,
	RootScannerEntity_MemoryAreaObjects,
	RootScannerEntity_MetronomeRememberedSet,
	RootScannerEntity_ClassesComplete,
	RootScannerEntity_WeakReferenceObjectsComplete,
	RootScannerEntity_SoftReferenceObjectsComplete,
	RootScannerEntity_PhantomReferenceObjectsComplete,
	RootScannerEntity_UnfinalizedObjectsComplete,
	RootScannerEntity_OwnableSynchronizerObjectsComplete,
	RootScannerEntity_MonitorLookupCaches,
	RootScannerEntity_MonitorLookupCachesComplete,
	RootScannerEntity_MonitorReferenceObjectsComplete,

	/* Must be last, do not use this entity! */
	RootScannerEntity_Count
} RootScannerEntity;

#ifdef __cplusplus
} /* extern "C" { */
#endif /* __cplusplus */

#endif /* !ROOTSCANNERTYPES_H__ */
