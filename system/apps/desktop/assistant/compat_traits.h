
#ifndef TYPE_CHECK_H
#define TYPE_CHECK_H

namespace compat
{

// 1. Resolve the type trait logic cleanly in one block
#if defined( __cplusplus ) && ( __cplusplus >= 201703L || ( defined( __GNUC__ ) && __GNUC__ >= 7 ) )
#include <type_traits>
template <typename T, typename U>
struct is_same
{
	//nomodel -- C98 does not have this, but this is a compat library until we move to a later C++
	static const bool value = std::is_same_v<T, U>;
};
#else
template <typename T, typename U>
struct is_same
{
	static const bool value = false;
};
template <typename T>
struct is_same<T, T>
{
	static const bool value = true;
};
#endif

// 2. The single inline function
template <typename T, typename U>
inline bool are_types_same( const T&, const U& )
{
	return is_same<T, U>::value;
}

} // namespace compat

#endif // TYPE_CHECK_H
