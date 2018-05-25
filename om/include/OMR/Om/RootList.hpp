#if !defined(OMR_OM_ROOTLIST_HPP_)
#define OMR_OM_ROOTLIST_HPP_

namespace OMR {
namespace Om {

/// A node in a RootList.
struct RootListNode {

	RootListNode(void* ref, RootListNode* tail) : ref(ref), tail(tail) {}

	void clear() noexcept {
		ref = nullptr;
		tail = nullptr;
	}

	void* ref;
	RootListNode* tail;
};

/// A foward iterator through a RootList.
/// @see-also: RootList::begin()
class RootListIterator
{
public:

	constexpr RootListIterator(const RootListIterator& other) noexcept : current_(other.current_) {}

	constexpr const RootListNode& operator*() const noexcept { return *current_; }

	constexpr const RootListNode* operator->() const noexcept { return current_; }

	RootListIterator& operator++() noexcept
	{
		current_ = current_->tail;
		return *this;
	}

	RootListIterator operator++(int) noexcept
	{
		RootListIterator tmp(*this);
		current_ = current_->tail;
		return tmp;
	}

	constexpr bool operator==(const RootListIterator& rhs) const noexcept { return current_ == rhs.current_; }

	constexpr bool operator!=(const RootListIterator& rhs) const noexcept { return current_ != rhs.current_; }

	RootListIterator& operator=(const RootListIterator& rhs) noexcept {
		current_ = rhs.current_;
		return *this;
	}

protected:
	friend class RootList;
	friend class ConstRootListIterator;

	constexpr RootListIterator() noexcept : current_(nullptr) {}

	constexpr RootListIterator(const RootListNode* start) noexcept : current_(start) {}

private:
	const RootListNode* current_;
};

/// A forward iterator through a const RootList.
/// @see-also: RootList::cbegin()
class ConstRootListIterator
{
public:

	constexpr ConstRootListIterator(const ConstRootListIterator& other) noexcept : current_(other.current_) {}

	constexpr ConstRootListIterator(const RootListIterator& other) noexcept : current_(other.current_) {}

	constexpr const RootListNode& operator*() const noexcept { return *current_; }

	constexpr const RootListNode* operator->() const noexcept { return current_; }

	ConstRootListIterator& operator++() noexcept
	{
		current_ = current_->tail;
		return *this;
	}

	ConstRootListIterator operator++(int) noexcept
	{
		ConstRootListIterator tmp(*this);
		current_ = current_->tail;
		return tmp;
	}

	constexpr bool operator==(const ConstRootListIterator& rhs) const noexcept { return current_ == rhs.current_; }

	constexpr bool operator!=(const ConstRootListIterator& rhs) const noexcept { return current_ != rhs.current_; }

	constexpr bool operator==(const RootListIterator& rhs) const noexcept { return current_ == rhs.current_; }

	constexpr bool operator!=(const RootListIterator& rhs) const noexcept { return current_ != rhs.current_; }

	ConstRootListIterator& operator=(const ConstRootListIterator& rhs) noexcept {
		current_ = rhs.current_;
		return *this;
	}

	ConstRootListIterator& operator=(const RootListIterator& rhs) noexcept {
		current_ = rhs.current_;
		return *this;
	}

protected:
	friend class RootList;

	constexpr ConstRootListIterator() noexcept : current_(nullptr) {}

	constexpr ConstRootListIterator(const RootListNode* start) noexcept : current_(start) {}

private:
	const RootListNode* current_;
};

/// The RootList is an untyped linked list of rooted, untagged pointers.
/// Nodes are manually added and removed from the list.
/// The RootList explicitly tracks untyped node elements.
/// Users should not use RootList directly, instead root objects using the automatic RootRef type, or register a RootFn callback.
class RootList
{
public:
	constexpr RootList() noexcept : head_(nullptr) {}

	RootListIterator begin() noexcept { return RootListIterator(head()); }

	RootListIterator end() noexcept { return RootListIterator(nullptr); }

	ConstRootListIterator begin() const noexcept { return cbegin(); }

	ConstRootListIterator end() const noexcept { return cend(); }

	ConstRootListIterator cbegin() const noexcept { return ConstRootListIterator(head()); }

	ConstRootListIterator cend() const noexcept { return ConstRootListIterator(nullptr); }

	RootListNode* head() const noexcept { return head_; }

	RootList& head(RootListNode* head) noexcept
	{
		head_ = head;
		return *this;
	}

	RootList& push(RootListNode* node) noexcept {
		node->tail = head_;
		head_ = node;
		return *this;
	}

private:
	RootListNode* head_;
};

}  // namespace Om
}  // namespace OMR

#endif // OMR_OM_ROOTLIST_HPP_
