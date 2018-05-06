#if !defined(OMR_OM_HANDLE_HPP_)
#define OMR_OM_HANDLE_HPP_

namespace OMR
{
namespace Om
{
template <typename T>
class Handle
{
public:
	constexpr Handle() : root_(nullptr) {}

	constexpr Handle(std::nullptr_t) : root_(nullptr) {}

	template <typename U>
	explicit constexpr Handle(U* const& root) : root_(&root)
	{
	}

	template <typename U>
	constexpr Handle(U* const* rootAddr) : root_(rootAddr)
	{
	}

	template <typename U>
	constexpr Handle(Handle<U> other) : root_(reinterpret_cast<T* const*>(other.raw()))
	{
	}

#if 0
  template <typename U>
  constexpr Handle(const RootRef<U>& root)
      : root_(reinterpret_cast<T* const*>(root.address())) {}
#endif

	T* get() const noexcept { return *root_; }

	T* const* raw() const { return root_; }

	T& operator*() const { return *ptr(); }

	T* operator->() const noexcept { return ptr(); }

	template <typename U>
	U& operator->*(U T::*mptr) const noexcept
	{
		return ptr()->*mptr;
	}

	template <typename U>
	Handle<U> reinterpret() const
	{
		return Handle<U>(reinterpret_cast<U* const*>(root_));
	}

	template <typename U = T>
	constexpr U* ptr() const
	{
		static_assert(sizeof(U**) == sizeof(T**), "Pointers must be compatible.");
		static_assert(sizeof(U*) == sizeof(T*), "Pointers must be compatible.");
		return *reinterpret_cast<U* const*>(root_);
	}

	template <typename U = T>
	constexpr operator U*() const
	{
		return ptr<U>();
	}

private:
	T* const* root_;
};

}  // namespace Om
}  // namespace OMR

#endif  // OMR_OM_HANDLE_HPP_
