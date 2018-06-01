#if !defined(OMR_OM_STANDARDOBJECTSCANNER_HPP_)
#define OMR_OM_STANDARDOBJECTSCANNER_HPP_

#include <OMR/Om/Shape.hpp>

#if 0
	/**
	 * Public object scanning method. Called from external context, eg concurrent GC. Scans object slots
	 * and marks objects referenfced from specified object.
	 *
	 * For pointer arrays, which may be split for scanning, caller may specify a maximum number
	 * of bytes to scan. For scalar object type, the default value for this parameter works fine.
	 *
	 * @param[in] env calling thread environment
	 * @param[in] objectPtr pointer to object to be marked (must not be NULL)
	 * @param[in] reason enumerator identifying scanning context
	 * @param sizeToDo maximum number of bytes to scan, for pointer arrays
	 */
	MMINLINE uintptr_t
	scanObject(MM_EnvironmentBase *env, omrobjectptr_t objectPtr, MM_MarkingSchemeScanReason reason, uintptr_t sizeToDo = UDATA_MAX)
	{
		GC_ObjectScannerState objectScannerState;
		GC_ObjectScanner *objectScanner = _delegate.getObjectScanner(env, objectPtr, &objectScannerState, reason, &sizeToDo);
		if (NULL != objectScanner) {
			bool isLeafSlot = false;
			GC_SlotObject *slotObject;
#if defined(OMR_GC_LEAF_BITS)
			while (NULL != (slotObject = objectScanner->getNextSlot(isLeafSlot))) {
#else  /* OMR_GC_LEAF_BITS */
			while (NULL != (slotObject = objectScanner->getNextSlot())) {
#endif /* OMR_GC_LEAF_BITS */
				fixupForwardedSlot(slotObject);

				/* with concurrentMark mutator may NULL the slot so must fetch and check here */
				inlineMarkObject(env, slotObject->readReferenceFromSlot(), isLeafSlot);
			}
		}

		/* Due to concurrent marking and packet overflow _bytesScanned may be much larger than the total live set
		 * because objects may be scanned multiple times.
		 */
		env->markStats()->_bytesScanned += sizeToDo;
		if (SCAN_REASON_PACKET == reason) {
			env->markStats()->_objectsScanned += 1;
		}

		return sizeToDo;
	}
#endif

namespace OMR {
namespace Om {

class StandardObjectScanner {
public:
	MMINLINE uintptr_t scanObject(MM_EnvironmentBase* env, omrobjectptr_t object) {
#ifdef OMR_OM_TRACE
		std::cerr << "Scanning: " << cell << std::endl;
#endif

		Context& cx = getContext(env->getOmrVMThread());

		Marker marker(_markingScheme);

		switch (cell->shape()->kind()) {
			switch
				cellKind(cell) {
				case CellKind::OBJECT:
					reinterpret_cast<Object*>(cell)->visit(cx, marker);
				case CellKind::SHAPE:
					reinterpret_cast<Shape*>(cell)->visit(cx, marker);
				case CellKind::ARRAY:
					reinterpret_cast<Array*>(cell)->visit(cx, marker);
				default: assert(0); break;
				}

			return 0;
		};

	} // namespace Om
} // namespace OMR

#endif // OMR_OM_STANDARDOBJECTSCANNER_HPP_
