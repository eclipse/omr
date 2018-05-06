#if !defined(OMR_OM_OBJECTSCANNER_HPP_)
#define OMR_OM_OBJECTSCANNER_HPP_

#include <OMR/Om/MarkingVisitor.hpp>
#include <OMR/Om/Object.inl.hpp>
#include <OMR/Om/RootRef.inl.hpp>
#include <OMR/Om/TransitionSet.inl.hpp>

#include <cstdint>

namespace OMR
{
namespace Om
{

class Object final
{
public:
	/**
	 * Public object scanning method. Called from external context, eg concurrent GC. Scans object slots
	 * and marks objects referenfced from specified object.
	 *
	 * For pointer arrays, which may be split for scanning, caller may specify a maximum number
	 * of bytes to scan. For scalar object type, the default value for this parameter works fine.
	 *
	 * @param env        calling thread environment
	 * @param marking    thread-local marking facilities.
	 * @param objectPtr  pointer to object to be marked (must not be NULL)
	 * @param reason     enumerator identifying scanning context
	 * @param sizeToDo   maximum number of bytes to scan, for pointer arrays
	 */
	std::size_t scanObject(
		GC::EnvironmentMetaData* metaData, GC::MarkingEnvironment* marking, omrobjectptr_t cell,
		MM_MarkingSchemeScanReason reason = SCAN_REASON_PACKET, std::size_t sizeToDo = UDATA_MAX)
	{
		MarkingVisitor marker(*metaData, *marking);

		switch (cell->map()->kind()) {
		case Shape::Kind::META_SHAPE: scanMap(marker, reinterpret_cast<Shape*>(cell)); break;
		case Shape::Kind::OBJECT_MAP: scanObj(marker, reinterpret_cast<Object*>(cell)); break;
		case Shape::Kind::ARRAY_BUFFER_MAP:
			scanArrayBuffer(marker, reinterpret_cast<ArrayBuffer*>(cell));
			break;
		default: assert(0); break;
		}
		return 0;
	}

private:
	inline void scanMap(MarkingVisitor& marker, Shape* map)
	{
		switch (map->kind()) {
		case Shape::Kind::OBJECT_MAP: reinterpret_cast<ObjectMap*>(map)->visit(marker); break;
		case Shape::Kind::META_SHAPE: reinterpret_cast<MetaShape*>(map)->visit(marker); break;
		case Shape::Kind::ARRAY_BUFFER_MAP: reinterpret_cast<ArrayBufferShape*>(map)->visit(marker); break;
		default: assert(0); break;
		}
	}

	inline void scanObj(MarkingVisitor& marker, Object* object) { object->visit(marker); }

	inline void scanArrayBuffer(MarkingVisitor& marker, ArrayBuffer* array) { array->visit(marker); }
};

}  // namespace Om
}  // namespace OMR

namespace OMR
{
namespace App
{

using ObjectScanner = OMR::Om::ObjectScanner;

}  // namespace App
}  // namespace OMR

#endif  // OMR_OM_OBJECTSCANNER_HPP_
