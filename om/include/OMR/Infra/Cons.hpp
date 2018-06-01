#if !defined(OMR_INFRA_CONS_HPP_)
#define OMR_INFRA_CONS_HPP_

#include <utility>

namespace OMR {
namespace Infra {
/// Linked list element. A specialization of std::pair that supports forward
/// iteration. cons.first is an instance of T. cons.second is a pointer to the
/// next.
template<typename T>
class Cons : public std::pair<T, Cons<T>*> {
public:
	class ConstIterator {
	public:
		ConstIterator() : current_(nullptr) {}

		ConstIterator(const Cons<T>* head) : current_(head) {}

		ConstIterator(const Iterator& other) : current_(other.current_) {}

		const T& operator*() const { return current_->first; }

		const T* operator->() const { return &current_->first; }

		ConstIterator& operator++() {
			current_ = current_->second;
			return *this;
		}

		ConstIterator operator++(int) {
			Iterator tmp(*this);
			current_ = current_->second;
			return tmp;
		}

		bool operator==(const ConstIterator& rhs) const { return current_ == rhs.current_; }

		bool operator!=(const ConstIterator& rhs) const { return current_ != rhs.current_; }

	private:
		const Cons<T>* current_;
	};

	using std::pair<T, Cons<T>*>::pair;

	Iterator begin() noexcept { return Iterator(this); }

	Iterator end() noexcept { return Iterator(); }

	ConstIterator begin() const noexcept { return cbegin(); }

	ConstIterator end() const noexcept { return cend(); }

	ConstIterator cbegin() const noexcept { return ConstIterator(this); }

	ConstIterator cend() const noexcept { return ConstIterator(); }
};

} // namespace Infra
} // namespace OMR

#endif // OMR_INFRA_CONS_HPP_
