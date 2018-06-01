#if !defined(OMR_OM_LAYOUTTREE_HPP_)
#define OMR_OM_LAYOUTTREE_HPP_

#include <OMR/Om/Cell.hpp>
#include <OMR/Om/Handle.hpp>
#include <OMR/Om/Shape.hpp>
#include <OMR/Om/Value.hpp>
#include <type_traits>

namespace OMR {
namespace Om {

/// An adapter class that provides macro operations on the shape hierachy of an
/// object. Most notably, the LayoutTree is iterable.
class LayoutTree {
public:
	class Iterator {
	public:
		explicit constexpr Iterator(Shape* start) noexcept : current_(start) {}

		Iterator(const Iterator&) = default;

		Iterator operator++(int) noexcept {
			Iterator copy(*this);
			current_ = current_->parentLayout();
			return copy;
		}

		Iterator& operator++() noexcept {
			current_ = current_->parentLayout();
			return *this;
		}

		constexpr Shape& operator*() const noexcept { return *current_; }

		constexpr bool operator==(const Iterator& rhs) const noexcept {
			return current_ == rhs.current_;
		}

		constexpr bool operator!=(const Iterator& rhs) const noexcept {
			return current_ != rhs.current_;
		}

	protected:
		friend class LayoutTree;

	private:
		Shape* current_;
	};

	constexpr LayoutTree(Shape* start) : start_(start) {}

	Iterator begin() const noexcept { return Iterator(start_); }

	Iterator end() const noexcept { return Iterator(nullptr); }

private:
	Shape* start_;
};

} // namespace Om
} // namespace OMR

#endif // OMR_OM_LAYOUTTREE_HPP_
