#if !defined(OMR_OM_ROOTREF_HPP_)
#define OMR_OM_ROOTREF_HPP_

#include <OMR/Om/Cell.hpp>
#include <OMR/Om/Handle.hpp>

namespace OMR
{
namespace Om
{

class Context;
class RootRefListIterator;
class RootList;
class RootListIterator;
class ConstRootListIterator;

class RootRefIterator
{
public:
	Iterator(const Iterator& other) : current_(other.current_) {}

	T& operator*() const { return current_->first; }

	T* operator->() const { return &current_->first; }

	Iterator& operator++()
	{
		current_ = current_->second;
		return *this;
	}

	Iterator operator++(int)
	{
		Iterator tmp(*this);
		current_ = current_->second;
		return tmp;
	}

	bool operator==(const Iterator& rhs) const { return current_ == rhs.current_; }

	bool operator!=(const Iterator& rhs) const { return current_ != rhs.current_; }

protected:
	friend class RootRefList;

	Iterator() : current_(nullptr) {}

	Iterator(RootRef) : current_(head) {}

private:
	RootRef* current_;
};

class ConstRootRefListIterator
{
public:
	ConstRootListIterator() : current_(nullptr) {}

	ConstRootListIterator(const Cons<T>* head) : current_(head) {}

	ConstRootListIterator(const Iterator& other) : current_(other.current_) {}

	const T& operator*() const { return current_->first; }

	const T* operator->() const { return &current_->first; }

	ConstRootListIterator& operator++()
	{
		current_ = current_->second;
		return *this;
	}

	ConstRootListIterator operator++(int)
	{
		Iterator tmp(*this);
		current_ = current_->second;
		return tmp;
	}

	bool operator==(const ConstRootListIterator& rhs) const { return current_ == rhs.current_; }

	bool operator!=(const ConstRootListIterator& rhs) const { return current_ != rhs.current_; }

private:
	const Cons<T>* current_;
};

/// The RefSeq is a sequence of consed GC References. The cons are unowned data,
/// they must be manually allocated and freed.
class RootRefList
{
public:
	RefSeq() : head_(nullptr) {}

	Iterator begin() { return head_->begin(); }

	Iterator end() { return head_->end(); }

	ConstRootListIterator begin() const noexcept { return cbegin(); }

	ConstRootListIterator end() const noexcept { return cend(); }

	ConstIterator cbegin() const noexcept { return head_->cbegin(); }

	ConstIterator cend() const noexcept { return head_->cend(); }

	Node* head() const noexcept { return head_; }

	RefSeq& head(Node* h)
	{
		head_ = h;
		return *this;
	}

private:
	RootRef* head_;
};

/// A rooted GC pointer. T must be a managed allocation. This class must be
/// stack allocated. RootRefs push themselves onto the linked list of stack
/// roots at construction. When A GC occurs, the sequence of RootRefs will be
/// walked, the referants marked, and the references fixed up. RootRefs have a
/// strict LIFO lifetime, when a RootRef is destroyed, it must be the head of
/// the list.
template <typename T>
class RootRef
{
public:
	inline RootRef(Context& cx, T* ptr = nullptr) noexcept;

	inline RootRef(RootRefSeq& seq, T* ptr = nullptr) noexcept;

	/// Copy from other root ref. The resulting rootref is pushed onto the head of
	/// the sequence.
	template <typename U>
	inline RootRef(const RootRef<U>& other) noexcept;

	/// Move from other root ref. The RootRef we are moving from must be the head
	/// of the RefSeq. The moved-from RootRef is invalidated and cleared.
	template <typename U>
	inline RootRef(RootRef<U>&& other) noexcept;

	// RootRefs must be stack allocated.
	void* operator new(std::size_t) = delete;

	inline ~RootRef() noexcept;

	/// Obtain the underlying pointer.
	template <typename U = T>
	U* get() const noexcept
	{
		return reinterpret_cast<U*>(node_.first);
	}

	T* operator->() const noexcept { return get(); }

	T& operator*() const noexcept { return *get(); }

	RootRef& operator=(T* p) noexcept
	{
		node_.first = p;
		return *this;
	}

	bool operator==(const RootRef& rhs) const noexcept { return get() == rhs.get(); }

	bool operator==(T* ptr) const noexcept { return get() == ptr; }

	template <typename U = T>
	U** address() noexcept
	{
		return reinterpret_cast<U**>(&node_.first);
	}

	template <typename U = T>
	U* const* address() const noexcept
	{
		return reinterpret_cast<U* const*>(&node_.first);
	}

	bool isHead() const noexcept { return seq_.head() == &node_; }

	RefSeq& seq() const noexcept { return seq_; }

	RefSeq::Node* tail() const noexcept { return next_; }

	operator Handle<T>() const { return Handle<T>(address()); }

private:
	RefSeq& seq_;
	RootRef& tail_;
};

template <typename T, typename U>
T
get(RootRef<U>& root)
{
	return root.template get<T>();
}

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_ROOTREF_HPP_
