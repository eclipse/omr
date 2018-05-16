#if !defined(OMR_OM_ROOTREF_HPP_)
#define OMR_OM_ROOTREF_HPP_

#include <OMR/Om/Cell.hpp>
#include <OMR/Om/Handle.hpp>
#include <OMR/Om/RootList.hpp>
#include <OMR/Om/Context.hpp>

namespace OMR
{
namespace Om
{

/// A rooted GC pointer. T must be a managed allocation. This class must be
/// stack allocated. RootRefs push themselves onto the linked list of stack
/// roots at construction. When A GC occurs, the sequence of RootRefs will be
/// walked, the referants marked, and the references fixed up. RootRefs have a
/// strict LIFO lifetime, when a RootRef is destroyed, it must be the head of
/// the list.
///
/// For users of Om: this is your primary mechanism for rooting references across
/// GC boundaries.
template <typename T>
class RootRef
{
  public:
	/// RootRefs must be stack allocated, so the new operator is explicitly removed.
	void *operator new(std::size_t) = delete;

	RootRef(RootList &list, T *ref) noexcept
		: list_(list), node_(ref, list.head())
	{
		list_.head(&node_);
	}

	/// Attach a new RootRef to the context's stackRoots.
	explicit RootRef(Context &cx, T *ref = nullptr) noexcept : RootRef(cx.stackRoots(), ref)
	{
	}

	/// Create a new RootRef that refers to the same object as an existing RootRef. The new RootRef is attached to the head of the list.
	template <typename U>
	explicit RootRef(const RootRef<U> &other) noexcept
		: RootRef(other.list(), other.ref())
	{
	}

	/// Move from other RootRef. The RootRef we are moving from must be the head
	/// of the List. The moved-from RootRef is invalidated and cleared. This new RootRef replaces the old RootRef in the RootList.
	template <typename U>
	explicit RootRef(RootRef<U> &&other) noexcept
		: list_(other.list()), node_(other.node())
	{
		other.clear();
	}

	~RootRef() noexcept
	{
		assert(isHead());
		list_.head(node_.tail);
	}

	/// Obtain the underlying pointer.
	template <typename U = T>
	U *get() const noexcept
	{
		return reinterpret_cast<U *>(node_.ref);
	}

	T *operator->() const noexcept
	{
		return get();
	}

	T &operator*() const noexcept
	{
		return *get();
	}

	RootRef &operator=(T *p) noexcept
	{
		node_.ref = p;
		return *this;
	}

	bool operator==(const RootRef &rhs) const noexcept
	{
		return get() == rhs.get();
	}

	bool operator==(T *ptr) const noexcept
	{
		return get() == ptr;
	}

	/// The address of the reference slot in this RootRef.
	template <typename U = T>
	U **address() noexcept
	{
		return reinterpret_cast<U **>(&node_.ref);
	}

	template <typename U = T>
	U *const *address() const noexcept
	{
		return reinterpret_cast<U *const *>(&node_.ref);
	}

	bool isHead() const noexcept
	{
		return list_.head() == &node_;
	}

	RootList& list() const noexcept
	{
		return list_;
	}

	RootListNode &node() noexcept
	{
		return node_;
	}

	const RootListNode &node() const noexcept
	{
		return node_;
	}

	RootListNode *tail() const noexcept
	{
		return node_.tail;
	}

	/// The RootRef freely degrades into a Handle.
	operator Handle<T>() const
	{
		return Handle<T>(address());
	}

  private:
	void clear() noexcept
	{
		node_.clear();
	}

	RootList &list_;
	RootListNode node_;
};

template <typename T, typename U>
T get(RootRef<U> &root)
{
	return root.template get<T>();
}

} // namespace Om
} // namespace OMR

#endif // OMR_OM_ROOTREF_HPP_
