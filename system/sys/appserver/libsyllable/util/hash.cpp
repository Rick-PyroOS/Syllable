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

#include <util/hash.h>

using namespace os;

static inline uint32 Rotate( uint32 nValue, int nBits )
{
	return ( nValue << nBits ) | ( nValue >> ( 32 - nBits ) );	// GCC makes this a rol
}

/* MurmurHash3_x86_32 (Austin Appleby, public domain), reading the blocks
 * with __builtin_memcpy so they need no alignment */
uint32 os::HashBytes( const void* pData, size_t nSize, uint32 nSeed )
{
	const uint8* pBytes = static_cast<const uint8*>( pData );
	const uint32 c1 = 0xcc9e2d51U;
	const uint32 c2 = 0x1b873593U;
	uint32 nHash = nSeed;

	const size_t nBlocks = nSize / 4;
	for( size_t i = 0; i < nBlocks; i++ )
	{
		uint32 nBlock;
		__builtin_memcpy( &nBlock, pBytes + i * 4, 4 );
		nBlock *= c1;
		nBlock = Rotate( nBlock, 15 );
		nBlock *= c2;
		nHash ^= nBlock;
		nHash = Rotate( nHash, 13 );
		nHash = nHash * 5 + 0xe6546b64U;
	}

	const uint8* pTail = pBytes + nBlocks * 4;
	uint32 nLast = 0;
	switch( nSize & 3 )
	{
		case 3:
			nLast ^= uint32( pTail[2] ) << 16;
			/* fall through */
		case 2:
			nLast ^= uint32( pTail[1] ) << 8;
			/* fall through */
		case 1:
			nLast ^= pTail[0];
			nLast *= c1;
			nLast = Rotate( nLast, 15 );
			nLast *= c2;
			nHash ^= nLast;
	}

	nHash ^= uint32( nSize );
	return HashMix32( nHash );
}
