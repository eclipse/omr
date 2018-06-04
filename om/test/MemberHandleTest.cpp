
#include <OMR/Om/MemberHandle.hpp>

#include <gtest/gtest.h>

namespace OMR {
namespace Om {
namespace Test {
TEST(MemberHandleTest, constructor) {
	Root<Cell> root;
	auto handle = root.handle();

	RootRef<Cell> root;
	Handle<Cell> handle(root);
	MemberHandle<CellHeader> cellHeaderHandle(handle, &Cell::header_);
}

} // namespace Test
} // namespace Om
} // namespace OMR
