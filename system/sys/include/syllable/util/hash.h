/*  Hash - hash functions for os::HashMap and os::UnorderedMap.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 */

#ifndef	__F_UTIL_HASH_H__
#define	__F_UTIL_HASH_H__

#include <atheos/types.h>
#include <util/string.h>
#include <string>
#include <stddef.h>

namespace os
{

/** The hash of a block of bytes.
 * \ingroup util
 * \par Description:
 *	MurmurHash3 (x86, 32 bits) of the nSize bytes at pData, started from
 *	nSeed.  It is what os::Hash uses for strings.  Hashes are for tables
 *	in memory: don't keep them in files or send them to other machines,
 *	since a later libsyllable may hash differently.
 */
uint32 HashBytes( const void* pData, size_t nSize, uint32 nSeed = 0 );

/** Mixes the bits of a 32-bit number (MurmurHash3's finalizer), so that
 *  numbers that differ in one bit get hashes that differ in about half */
inline uint32 HashMix32( uint32 nValue )
{
	nValue ^= nValue >> 16;
	nValue *= 0x85ebca6bU;
	nValue ^= nValue >> 13;
	nValue *= 0xc2b2ae35U;
	nValue ^= nValue >> 16;
	return nValue;
}

/** Mixes the bits of a 64-bit number (MurmurHash3's 64-bit finalizer) */
inline uint64 HashMix64( uint64 nValue )
{
	nValue ^= nValue >> 33;
	nValue *= 0xff51afd7ed558ccdULL;
	nValue ^= nValue >> 33;
	nValue *= 0xc4ceb9fe1a85ec53ULL;
	nValue ^= nValue >> 33;
	return nValue;
}

/** Combines a hash into another, for keys made of several values */
inline size_t HashCombine( size_t nSeed, size_t nHash )
{
	return nSeed ^ ( nHash + 0x9e3779b9U + ( nSeed << 6 ) + ( nSeed >> 2 ) );
}

/** The smallest power of two that is nValue or more (1 for 0) */
inline size_t NextPowerOfTwo( size_t nValue )
{
	if( nValue <= 1 )
		return 1;
	// __builtin_clzl() is undefined for 0, which nValue - 1 isn't here
	return size_t( 1 ) << ( sizeof( unsigned long ) * 8 - __builtin_clzl( (unsigned long)( nValue - 1 ) ) );
}

/** The hash of an integer of any size */
template<class T> inline size_t HashInteger( T nValue )
{
	if( sizeof( T ) <= 4 )
		return size_t( HashMix32( uint32( nValue ) ) );
	return size_t( HashMix64( uint64( nValue ) ) );
}

/** The hash function of a key type.
 * \ingroup util
 * \par Description:
 *	os::Hash<K>()( cKey ) is the hash of cKey, a size_t.  It is defined
 *	for the integer types, bool, float, double, pointers (their address),
 *	std::string and os::String.  For a key type of your own, specialize it
 *	in namespace os (or give the map a hash class of its own):
 * \code
 *	namespace os {
 *	template<> struct Hash<MyKey> {
 *	    size_t operator()( const MyKey& cKey ) const
 *	    { return os::HashCombine( os::Hash<int>()( cKey.nX ), os::Hash<os::String>()( cKey.cName ) ); }
 *	};
 *	}
 * \endcode
 *	A const char* key is hashed by its address, not its text: use
 *	os::String keys for text.
 */
template<class T> struct Hash;

#define __OS_HASH_INTEGER( type ) \
	template<> struct Hash<type> { size_t operator()( type nValue ) const { return HashInteger( nValue ); } };
__OS_HASH_INTEGER( bool )
__OS_HASH_INTEGER( char )
__OS_HASH_INTEGER( signed char )
__OS_HASH_INTEGER( unsigned char )
__OS_HASH_INTEGER( wchar_t )
__OS_HASH_INTEGER( short )
__OS_HASH_INTEGER( unsigned short )
__OS_HASH_INTEGER( int )
__OS_HASH_INTEGER( unsigned int )
__OS_HASH_INTEGER( long )
__OS_HASH_INTEGER( unsigned long )
__OS_HASH_INTEGER( long long )
__OS_HASH_INTEGER( unsigned long long )
#undef __OS_HASH_INTEGER

template<> struct Hash<float>
{
	size_t operator()( float vValue ) const
	{
		if( vValue == 0.0f )
			vValue = 0.0f;		// -0 is 0, so it hashes the same
		uint32 nBits;
		__builtin_memcpy( &nBits, &vValue, sizeof( nBits ) );
		return HashInteger( nBits );
	}
};

template<> struct Hash<double>
{
	size_t operator()( double vValue ) const
	{
		if( vValue == 0.0 )
			vValue = 0.0;
		uint64 nBits;
		__builtin_memcpy( &nBits, &vValue, sizeof( nBits ) );
		return HashInteger( nBits );
	}
};

template<class T> struct Hash<T*>
{
	size_t operator()( T* pValue ) const { return HashInteger( size_t( pValue ) ); }
};

template<> struct Hash<std::string>
{
	size_t operator()( const std::string& cValue ) const { return HashBytes( cValue.data(), cValue.size() ); }
};

template<> struct Hash<String>
{
	size_t operator()( const String& cValue ) const { return HashBytes( cValue.c_str(), cValue.size() ); }
};

/* Comparisons for the Sort methods of the maps, by function or operator< */
namespace map_private
{
template<class P, class K> struct FirstLess
{
	typedef bool (*Compare)( const K&, const K& );
	FirstLess( Compare pfCompare, bool bReversed ) : m_pfCompare( pfCompare ), m_bReversed( bReversed ) {}
	bool Less( const K& cA, const K& cB ) const { return m_pfCompare != NULL ? m_pfCompare( cA, cB ) : cA < cB; }
	bool operator()( const P& cA, const P& cB ) const
	{
		return m_bReversed ? Less( cB.first, cA.first ) : Less( cA.first, cB.first );
	}
	Compare m_pfCompare;
	bool m_bReversed;
};
template<class P, class V> struct SecondLess
{
	typedef bool (*Compare)( const V&, const V& );
	SecondLess( Compare pfCompare, bool bReversed ) : m_pfCompare( pfCompare ), m_bReversed( bReversed ) {}
	bool Less( const V& cA, const V& cB ) const { return m_pfCompare != NULL ? m_pfCompare( cA, cB ) : cA < cB; }
	bool operator()( const P& cA, const P& cB ) const
	{
		return m_bReversed ? Less( cB.second, cA.second ) : Less( cA.second, cB.second );
	}
	Compare m_pfCompare;
	bool m_bReversed;
};
} // namespace map_private

} // namespace os

#endif // __F_UTIL_HASH_H__
