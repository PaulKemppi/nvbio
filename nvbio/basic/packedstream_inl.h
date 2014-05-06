/*
 * nvbio
 * Copyright (c) 2011-2014, NVIDIA CORPORATION. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the NVIDIA CORPORATION nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NVIDIA CORPORATION BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <nvbio/basic/cached_iterator.h>

namespace nvbio {

template <bool BIG_ENDIAN_T, uint32 SYMBOL_SIZE, typename Symbol, typename InputStream, typename IndexType, typename ValueType>
struct packer {
};

template <bool BIG_ENDIAN_T, uint32 SYMBOL_SIZE, typename Symbol, typename InputStream, typename IndexType>
struct packer<BIG_ENDIAN_T,SYMBOL_SIZE,Symbol,InputStream,IndexType,uint32>
{
    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE Symbol get_symbol(InputStream stream, const IndexType sym_idx)
    {
        const uint32 SYMBOL_COUNT = 1u << SYMBOL_SIZE;
        const uint32 SYMBOL_MASK  = SYMBOL_COUNT - 1u;

        typedef typename unsigned_type<IndexType>::type index_type;

        const uint64     bit_idx  = uint64(sym_idx) * SYMBOL_SIZE;
        const index_type word_idx = index_type( bit_idx >> 5u );

        if (is_pow2<SYMBOL_SIZE>())
        {
            const uint32 word = stream[ word_idx ];
            const uint32 symbol_offset = BIG_ENDIAN_T ? (32u - SYMBOL_SIZE  - uint32(bit_idx & 31u)) : uint32(bit_idx & 31u);
            const uint32 symbol = (word >> symbol_offset) & SYMBOL_MASK;

            return Symbol( symbol );
        }
        else
        {
            const uint32 word1 = stream[ word_idx ];
            const uint32 symbol_offset = uint32(bit_idx & 31u);
            const uint32 symbol1 = (word1 >> symbol_offset) & SYMBOL_MASK;

            // check if we need to read a second word
            const uint32 read_bits = nvbio::min( 32u - symbol_offset, SYMBOL_SIZE );
            const uint32 rem_bits  = SYMBOL_SIZE - read_bits;
            if (rem_bits)
            {
                const uint32 rem_mask = (1u << rem_bits) - 1u;

                const uint32 word2 = stream[ word_idx+1 ];
                const uint32 symbol2 = word2 & rem_mask;

                return Symbol( symbol1 | (symbol2 << read_bits) );
            }
            else
                return Symbol( symbol1 );
        }
    }

    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE void set_symbol(InputStream stream, const IndexType sym_idx, Symbol sym)
    {
        const uint32 SYMBOL_COUNT = 1u << SYMBOL_SIZE;
        const uint32 SYMBOL_MASK  = SYMBOL_COUNT - 1u;

        typedef typename unsigned_type<IndexType>::type index_type;

        const uint64     bit_idx  = uint64(sym_idx) * SYMBOL_SIZE;
        const index_type word_idx = index_type( bit_idx >> 5u );

        if (is_pow2<SYMBOL_SIZE>())
        {
                  uint32 word          = stream[ word_idx ];
            const uint32 symbol_offset = BIG_ENDIAN_T ? (32u - SYMBOL_SIZE - uint32(bit_idx & 31u)) : uint32(bit_idx & 31u);
            const uint32 symbol        = uint32(sym & SYMBOL_MASK) << symbol_offset;

            // clear all bits
            word &= ~(SYMBOL_MASK << symbol_offset);

            // set bits
            stream[ word_idx ] = word | symbol;
        }
        else
        {
                  uint32 word1         = stream[ word_idx ];
            const uint32 symbol_offset = uint32(bit_idx & 31u);
            const uint32 symbol1       = uint32(sym & SYMBOL_MASK) << symbol_offset;

            // clear all bits
            word1 &= ~(SYMBOL_MASK << symbol_offset);

            // set bits
            stream[ word_idx ] = word1 | symbol1;

            // check if we need to write a second word
            const uint32 read_bits = nvbio::min( 32u - symbol_offset, SYMBOL_SIZE );
            const uint32 rem_bits  = SYMBOL_SIZE - read_bits;
            if (rem_bits)
            {
                const uint32 rem_mask = (1u << rem_bits) - 1u;

                      uint32 word2   = stream[ word_idx+1 ];
                const uint32 symbol2 = uint32(sym & SYMBOL_MASK) >> read_bits;

                // clear all bits
                word2 &= ~rem_mask;

                // set bits
                stream[ word_idx+1 ] = word2 | symbol2;
            }
        }
    }
};

template <bool BIG_ENDIAN_T, uint32 SYMBOL_SIZE, typename Symbol, typename InputStream, typename IndexType>
struct packer<BIG_ENDIAN_T,SYMBOL_SIZE,Symbol,InputStream,IndexType,uint64>
{
    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE Symbol get_symbol(InputStream stream, const IndexType sym_idx)
    {
        const uint32 SYMBOL_COUNT = 1u << SYMBOL_SIZE;
        const uint32 SYMBOL_MASK  = SYMBOL_COUNT - 1u;

        typedef typename unsigned_type<IndexType>::type index_type;

        const uint64     bit_idx  = uint64(sym_idx) * SYMBOL_SIZE;
        const index_type word_idx = index_type( bit_idx >> 6u );

        if (is_pow2<SYMBOL_SIZE>())
        {
            const uint64 word          = stream[ word_idx ];
            const uint32 symbol_offset = BIG_ENDIAN_T ? (64u - SYMBOL_SIZE  - uint32(bit_idx & 63u)) : uint32(bit_idx & 63u);
            const uint32 symbol        = uint32((word >> symbol_offset) & SYMBOL_MASK);

            return Symbol( symbol );
        }
        else
        {
            const uint64 word1         = stream[ word_idx ];
            const uint32 symbol_offset = uint32(bit_idx & 63u);
            const uint32 symbol1       = uint32((word1 >> symbol_offset) & SYMBOL_MASK);

            // check if we need to read a second word
            const uint32 read_bits = nvbio::min( 64u - symbol_offset, SYMBOL_SIZE );
            const uint32 rem_bits  = SYMBOL_SIZE - read_bits;
            if (rem_bits)
            {
                const uint64 rem_mask = (uint64(1u) << rem_bits) - 1u;

                const uint64 word2    = stream[ word_idx+1 ];
                const uint32 symbol2  = uint32(word2 & rem_mask);

                return Symbol( symbol1 | (symbol2 << read_bits) );
            }
            else
                return Symbol( symbol1 );
        }
    }

    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE void set_symbol(InputStream stream, const IndexType sym_idx, Symbol sym)
    {
        const uint32 SYMBOL_COUNT = 1u << SYMBOL_SIZE;
        const uint32 SYMBOL_MASK  = SYMBOL_COUNT - 1u;

        typedef typename unsigned_type<IndexType>::type index_type;

        const uint64     bit_idx  = uint64(sym_idx) * SYMBOL_SIZE;
        const index_type word_idx = index_type( bit_idx >> 6u );

        if (is_pow2<SYMBOL_SIZE>())
        {
                  uint64 word = stream[ word_idx ];
            const uint32 symbol_offset = BIG_ENDIAN_T ? (64u - SYMBOL_SIZE - uint32(bit_idx & 63u)) : uint32(bit_idx & 63u);
            const uint64 symbol = uint64(sym & SYMBOL_MASK) << symbol_offset;

            // clear all bits
            word &= ~(uint64(SYMBOL_MASK) << symbol_offset);

            // set bits
            stream[ word_idx ] = word | symbol;
        }
        else
        {
                  uint64 word1 = stream[ word_idx ];
            const uint32 symbol_offset = uint32(bit_idx & 63);
            const uint64 symbol1 = uint64(sym & SYMBOL_MASK) << symbol_offset;

            // clear all bits
            word1 &= ~(uint64(SYMBOL_MASK) << symbol_offset);

            // set bits
            stream[ word_idx ] = word1 | symbol1;

            // check if we need to write a second word
            const uint32 read_bits = nvbio::min( 64u - symbol_offset, SYMBOL_SIZE );
            const uint32 rem_bits  = SYMBOL_SIZE - read_bits;
            if (rem_bits)
            {
                const uint64 rem_mask = (uint64(1u) << rem_bits) - 1u;

                      uint64 word2   = stream[ word_idx+1 ];
                const uint64 symbol2 = uint64(sym & SYMBOL_MASK) >> read_bits;

                // clear all bits
                word2 &= ~rem_mask;

                // set bits
                stream[ word_idx+1 ] = word2 | symbol2;
            }
        }
    }
};

template <bool BIG_ENDIAN_T, uint32 SYMBOL_SIZE, typename Symbol, typename InputStream, typename IndexType>
struct packer<BIG_ENDIAN_T,SYMBOL_SIZE,Symbol,InputStream,IndexType,uint8>
{
    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE Symbol get_symbol(InputStream stream, const IndexType sym_idx)
    {
        const uint8 SYMBOL_COUNT = uint8(1u) << SYMBOL_SIZE;
        const uint8 SYMBOL_MASK  = SYMBOL_COUNT - uint8(1u);

        typedef typename unsigned_type<IndexType>::type index_type;

        const uint64     bit_idx  = uint64(sym_idx) * SYMBOL_SIZE;
        const index_type word_idx = index_type( bit_idx >> 3u );

        if (is_pow2<SYMBOL_SIZE>())
        {
            const uint8 word = stream[ word_idx ];
            const uint8 symbol_offset = BIG_ENDIAN_T ? (8u - SYMBOL_SIZE - uint8(bit_idx & 7u)) : uint8(bit_idx & 7u);
            const uint8 symbol = (word >> symbol_offset) & SYMBOL_MASK;

            return Symbol( symbol );
        }
        else
        {
            const uint8 word1 = stream[ word_idx ];
            const uint8 symbol_offset = uint8(bit_idx & 7u);
            const uint8 symbol1 = (word1 >> symbol_offset) & SYMBOL_MASK;

            // check if we need to read a second word
            const uint32 read_bits = nvbio::min( 8u - symbol_offset, SYMBOL_SIZE );
            const uint32 rem_bits  = SYMBOL_SIZE - read_bits;
            if (rem_bits)
            {
                const uint8 rem_mask = uint8((1u << rem_bits) - 1u);

                const uint8 word2 = stream[ word_idx+1 ];
                const uint8 symbol2 = word2 & rem_mask;

                return Symbol( symbol1 | (symbol2 << read_bits) );
            }
            else
                return Symbol( symbol1 );
        }
    }

    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE void set_symbol(InputStream stream, const IndexType sym_idx, Symbol sym)
    {
        const uint8 SYMBOL_COUNT = uint8(1u) << SYMBOL_SIZE;
        const uint8 SYMBOL_MASK  = SYMBOL_COUNT - uint8(1u);

        typedef typename unsigned_type<IndexType>::type index_type;

        const uint64     bit_idx  = uint64(sym_idx) * SYMBOL_SIZE;
        const index_type word_idx = index_type( bit_idx >> 3u );

        if (is_pow2<SYMBOL_SIZE>())
        {
                  uint8 word = stream[ word_idx ];
            const uint8 symbol_offset = BIG_ENDIAN_T ? (8u - SYMBOL_SIZE - uint8(bit_idx & 7u)) : uint8(bit_idx & 7u);
            const uint8 symbol = uint32(sym & SYMBOL_MASK) << symbol_offset;

            // clear all bits
            word &= ~(SYMBOL_MASK << symbol_offset);

            // set bits
            stream[ word_idx ] = word | symbol;
        }
        else
        {
                  uint8 word1 = stream[ word_idx ];
            const uint8 symbol_offset = uint8(bit_idx & 7u);
            const uint8 symbol1 = uint8(sym & SYMBOL_MASK) << symbol_offset;

            // clear all bits
            word1 &= ~(SYMBOL_MASK << symbol_offset);

            // set bits
            stream[ word_idx ] = word1 | symbol1;

            // check if we need to write a second word
            const uint32 read_bits = nvbio::min( 8u - symbol_offset, SYMBOL_SIZE );
            const uint32 rem_bits  = SYMBOL_SIZE - read_bits;
            if (rem_bits)
            {
                      uint8 word2   = stream[ word_idx+1 ];
                const uint8 symbol2 = uint32(sym & SYMBOL_MASK) >> read_bits;

                const uint8 rem_mask = uint8((1u << rem_bits) - 1u);

                // clear all bits
                word2 &= ~rem_mask;

                // set bits
                stream[ word_idx+1 ] = word2 | symbol2;
            }
        }
    }
};


template <bool BIG_ENDIAN_T, typename Symbol, typename InputStream, typename IndexType>
struct packer<BIG_ENDIAN_T,2u,Symbol,InputStream,IndexType,uint32>
{
    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE Symbol get_symbol(InputStream stream, const IndexType sym_idx)
    {
        const uint32 SYMBOL_MASK = 3u;

        typedef typename unsigned_type<IndexType>::type index_type;

        const index_type word_idx = sym_idx >> 4u;

        const uint32 word = stream[ word_idx ];
        const uint32 symbol_offset = BIG_ENDIAN_T ? (30u - (uint32(sym_idx & 15u) << 1)) : uint32((sym_idx & 15u) << 1);
        const uint32 symbol = (word >> symbol_offset) & SYMBOL_MASK;

        return Symbol( symbol );
    }

    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE void set_symbol(InputStream stream, const IndexType sym_idx, Symbol sym)
    {
        const uint32 SYMBOL_MASK = 3u;

        typedef typename unsigned_type<IndexType>::type index_type;

        const index_type word_idx = sym_idx >> 4u;

              uint32 word = stream[ word_idx ];
        const uint32 symbol_offset = BIG_ENDIAN_T ? (30u - (uint32(sym_idx & 15u) << 1)) : uint32((sym_idx & 15u) << 1);
        const uint32 symbol = uint32(sym & SYMBOL_MASK) << symbol_offset;

        // clear all bits
        word &= ~(SYMBOL_MASK << symbol_offset);

        // set bits
        stream[ word_idx ] = word | symbol;
    }
};
template <bool BIG_ENDIAN_T, typename Symbol, typename InputStream, typename IndexType>
struct packer<BIG_ENDIAN_T,4u,Symbol,InputStream,IndexType,uint32>
{
    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE Symbol get_symbol(InputStream stream, const IndexType sym_idx)
    {
        const uint32 SYMBOL_MASK = 15u;

        typedef typename unsigned_type<IndexType>::type index_type;

        const index_type word_idx = sym_idx >> 3u;

        const uint32 word = stream[ word_idx ];
        const uint32 symbol_offset = BIG_ENDIAN_T ? (28u - (uint32(sym_idx & 7u) << 2)) : uint32((sym_idx & 7u) << 2);
        const uint32 symbol = (word >> symbol_offset) & SYMBOL_MASK;

        return Symbol( symbol );
    }

    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE void set_symbol(InputStream stream, const IndexType sym_idx, Symbol sym)
    {
        const uint32 SYMBOL_MASK = 15u;

        typedef typename unsigned_type<IndexType>::type index_type;

        const index_type word_idx = sym_idx >> 3u;

              uint32 word = stream[ word_idx ];
        const uint32 symbol_offset = BIG_ENDIAN_T ? (28u - (uint32(sym_idx & 7u) << 2)) : uint32((sym_idx & 7u) << 2);
        const uint32 symbol = uint32(sym & SYMBOL_MASK) << symbol_offset;

        // clear all bits
        word &= ~(SYMBOL_MASK << symbol_offset);

        // set bits
        stream[ word_idx ] = word | symbol;
    }
};

template <bool BIG_ENDIAN_T, typename Symbol, typename InputStream, typename IndexType>
struct packer<BIG_ENDIAN_T,2u,Symbol,InputStream,IndexType,uint4>
{
    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE Symbol get_symbol(InputStream stream, const IndexType sym_idx)
    {
        const uint32 SYMBOL_MASK = 3u;

        typedef typename unsigned_type<IndexType>::type index_type;

        const index_type word_idx = sym_idx >> 6u;

        const uint4  word = stream[ word_idx ];
        const uint32 symbol_comp   = (sym_idx & 63u) >> 4u;
        const uint32 symbol_offset = BIG_ENDIAN_T ? (30u - (uint32(sym_idx & 15u) << 1)) : uint32((sym_idx & 15u) << 1);
        const uint32 symbol = (comp( word, symbol_comp ) >> symbol_offset) & SYMBOL_MASK;

        return Symbol( symbol );
    }

    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE void set_symbol(InputStream stream, const IndexType sym_idx, Symbol sym)
    {
        const uint32 SYMBOL_MASK = 3u;

        typedef typename unsigned_type<IndexType>::type index_type;

        const index_type word_idx = sym_idx >> 6u;

              uint4  word = stream[ word_idx ];
        const uint32 symbol_comp   = (sym_idx & 63u) >> 4u;
        const uint32 symbol_offset = BIG_ENDIAN_T ? (30u - (uint32(sym_idx & 15u) << 1)) : uint32((sym_idx & 15u) << 1);
        const uint32 symbol = uint32(sym & SYMBOL_MASK) << symbol_offset;

        // clear all bits
        select( word, symbol_comp ) &= ~(SYMBOL_MASK << symbol_offset);
        select( word, symbol_comp ) |= symbol;

        // set bits
        stream[ word_idx ] = word;
    }
};
template <bool BIG_ENDIAN_T, typename Symbol, typename InputStream, typename IndexType>
struct packer<BIG_ENDIAN_T,4u,Symbol,InputStream,IndexType,uint4>
{
    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE Symbol get_symbol(InputStream stream, const IndexType sym_idx)
    {
        const uint32 SYMBOL_MASK = 15u;

        typedef typename unsigned_type<IndexType>::type index_type;

        const index_type word_idx = sym_idx >> 5u;

        const uint4 word = stream[ word_idx ];
        const uint32 symbol_comp   = (sym_idx & 31u) >> 3u;
        const uint32 symbol_offset = BIG_ENDIAN_T ? (28u - (uint32(sym_idx & 7u) << 2)) : uint32((sym_idx & 7u) << 2);
        const uint32 symbol = (comp( word, symbol_comp ) >> symbol_offset) & SYMBOL_MASK;

        return Symbol( symbol );
    }

    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE void set_symbol(InputStream stream, const IndexType sym_idx, Symbol sym)
    {
        const uint32 SYMBOL_MASK = 15u;

        typedef typename unsigned_type<IndexType>::type index_type;

        const index_type word_idx = sym_idx >> 5u;

              uint4  word = stream[ word_idx ];
        const uint32 symbol_comp   = (sym_idx & 31u) >> 3u;
        const uint32 symbol_offset = BIG_ENDIAN_T ? (28u - (uint32(sym_idx & 7u) << 2)) : uint32((sym_idx & 7u) << 2);
        const uint32 symbol = uint32(sym & SYMBOL_MASK) << symbol_offset;

        // clear all bits
        select( word, symbol_comp ) &= ~(SYMBOL_MASK << symbol_offset);
        select( word, symbol_comp ) |= symbol;

        // set bits
        stream[ word_idx ] = word;
    }
};

template <bool BIG_ENDIAN_T, typename Symbol, typename InputStream, typename IndexType>
struct packer<BIG_ENDIAN_T,2u,Symbol,InputStream,IndexType,uint64>
{
    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE Symbol get_symbol(InputStream stream, const IndexType sym_idx)
    {
        const uint32 SYMBOL_MASK = 3u;

        typedef typename unsigned_type<IndexType>::type index_type;

        const index_type word_idx = sym_idx >> 5u;

        const uint64 word = stream[ word_idx ];
        const uint32 symbol_offset = BIG_ENDIAN_T ? (62u - (uint32(sym_idx & 31u) << 1)) : uint32((sym_idx & 31u) << 1);
        const uint64 symbol = (word >> symbol_offset) & SYMBOL_MASK;

        return Symbol( symbol );
    }

    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE void set_symbol(InputStream stream, const IndexType sym_idx, Symbol sym)
    {
        const uint32 SYMBOL_MASK = 3u;

        typedef typename unsigned_type<IndexType>::type index_type;

        const index_type word_idx = sym_idx >> 5u;

              uint64 word = stream[ word_idx ];
        const uint32 symbol_offset = BIG_ENDIAN_T ? (62u - (uint32(sym_idx & 31u) << 1)) : uint32((sym_idx & 31u) << 1);
        const uint64 symbol = uint64(sym & SYMBOL_MASK) << symbol_offset;

        // clear all bits
        word &= ~(uint64(SYMBOL_MASK) << symbol_offset);

        // set bits
        stream[ word_idx ] = word | symbol;
    }
};
template <bool BIG_ENDIAN_T, typename Symbol, typename InputStream, typename IndexType>
struct packer<BIG_ENDIAN_T,4u,Symbol,InputStream,IndexType,uint64>
{
    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE Symbol get_symbol(InputStream stream, const IndexType sym_idx)
    {
        const uint32 SYMBOL_MASK = 15u;

        typedef typename unsigned_type<IndexType>::type index_type;

        const index_type word_idx = sym_idx >> 5u;

        const uint64 word = stream[ word_idx ];
        const uint32 symbol_offset = BIG_ENDIAN_T ? (60u - (uint32(sym_idx & 15u) << 2)) : uint32((sym_idx & 15u) << 2);
        const uint64 symbol = (word >> symbol_offset) & SYMBOL_MASK;

        return Symbol( symbol );
    }

    static NVBIO_FORCEINLINE NVBIO_HOST_DEVICE void set_symbol(InputStream stream, const IndexType sym_idx, Symbol sym)
    {
        const uint32 SYMBOL_MASK = 15u;

        typedef typename unsigned_type<IndexType>::type index_type;

        const index_type word_idx = sym_idx >> 5u;

              uint64 word = stream[ word_idx ];
        const uint32 symbol_offset = BIG_ENDIAN_T ? (60u - (uint32(sym_idx & 15u) << 2)) : uint32((sym_idx & 15u) << 2);
        const uint64 symbol = uint32(sym & SYMBOL_MASK) << symbol_offset;

        // clear all bits
        word &= ~(SYMBOL_MASK << symbol_offset);

        // set bits
        stream[ word_idx ] = word | symbol;
    }
};


template <typename InputStream, typename Symbol, uint32 SYMBOL_SIZE_T, bool BIG_ENDIAN_T, typename IndexType>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE Symbol PackedStream<InputStream,Symbol, SYMBOL_SIZE_T, BIG_ENDIAN_T, IndexType>::get(const index_type sym_idx) const
{
    return packer<BIG_ENDIAN_T, SYMBOL_SIZE,Symbol,InputStream,IndexType,storage_type>::get_symbol( m_stream, sym_idx );
}
template <typename InputStream, typename Symbol, uint32 SYMBOL_SIZE_T, bool BIG_ENDIAN_T, typename IndexType>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE void PackedStream<InputStream,Symbol, SYMBOL_SIZE_T, BIG_ENDIAN_T, IndexType>::set(const index_type sym_idx, const Symbol sym)
{
    return packer<BIG_ENDIAN_T, SYMBOL_SIZE,Symbol,InputStream,IndexType,storage_type>::set_symbol( m_stream, sym_idx, sym );
}
// return begin iterator
//
template <typename InputStream, typename Symbol, uint32 SYMBOL_SIZE_T, bool BIG_ENDIAN_T, typename IndexType>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
typename PackedStream<InputStream,Symbol, SYMBOL_SIZE_T, BIG_ENDIAN_T, IndexType>::iterator
PackedStream<InputStream,Symbol, SYMBOL_SIZE_T, BIG_ENDIAN_T, IndexType>::begin() const
{
    return iterator( m_stream, 0 );
}

/*
// dereference operator
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
typename PackedStreamIterator<Stream>::Symbol
PackedStreamIterator<Stream>::operator* () const
{
    return m_stream.get( m_index );
}
*/
// dereference operator
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE typename PackedStreamIterator<Stream>::reference PackedStreamIterator<Stream>::operator* () const
{
    return reference( m_stream, m_index );
}

// indexing operator
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE typename PackedStreamIterator<Stream>::reference PackedStreamIterator<Stream>::operator[] (const sindex_type i) const
{
    return reference( m_stream, m_index + i );
}
/*
// indexing operator
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE typename PackedStreamIterator<Stream>::reference PackedStreamIterator<Stream>::operator[] (const index_type i) const
{
    return reference( m_stream, m_index + i );
}
*/
// set value
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE void PackedStreamIterator<Stream>::set(const Symbol s)
{
    m_stream.set( m_index, s );
}

// pre-increment operator
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE PackedStreamIterator<Stream>& PackedStreamIterator<Stream>::operator++ ()
{
    ++m_index;
    return *this;
}

// post-increment operator
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE PackedStreamIterator<Stream> PackedStreamIterator<Stream>::operator++ (int dummy)
{
    This r( m_stream, m_index );
    ++m_index;
    return r;
}

// pre-decrement operator
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE PackedStreamIterator<Stream>& PackedStreamIterator<Stream>::operator-- ()
{
    --m_index;
    return *this;
}

// post-decrement operator
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE PackedStreamIterator<Stream> PackedStreamIterator<Stream>::operator-- (int dummy)
{
    This r( m_stream, m_index );
    --m_index;
    return r;
}

// add offset
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE PackedStreamIterator<Stream>& PackedStreamIterator<Stream>::operator+= (const sindex_type distance)
{
    m_index += distance;
    return *this;
}

// subtract offset
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE PackedStreamIterator<Stream>& PackedStreamIterator<Stream>::operator-= (const sindex_type distance)
{
    m_index -= distance;
    return *this;
}

// add offset
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE PackedStreamIterator<Stream> PackedStreamIterator<Stream>::operator+ (const sindex_type distance) const
{
    return This( m_stream, m_index + distance );
}

// subtract offset
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE PackedStreamIterator<Stream> PackedStreamIterator<Stream>::operator- (const sindex_type distance) const
{
    return This( m_stream, m_index - distance );
}

// difference
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
typename PackedStreamIterator<Stream>::sindex_type
PackedStreamIterator<Stream>::operator- (const PackedStreamIterator it) const
{
    return sindex_type( m_index - it.m_index );
}

// less than
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE bool operator< (
    const PackedStreamIterator<Stream>& it1,
    const PackedStreamIterator<Stream>& it2)
{
    return it1.m_index < it2.m_index;
}

// greater than
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE bool operator>(
    const PackedStreamIterator<Stream>& it1,
    const PackedStreamIterator<Stream>& it2)
{
    return it1.m_index > it2.m_index;
}

// equality test
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE bool operator== (
    const PackedStreamIterator<Stream>& it1,
    const PackedStreamIterator<Stream>& it2)
{
    return /*it1.m_stream == it2.m_stream &&*/ it1.m_index == it2.m_index;
}
// inequality test
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE bool operator!= (
    const PackedStreamIterator<Stream>& it1,
    const PackedStreamIterator<Stream>& it2)
{
    return /*it1.m_stream != it2.m_stream ||*/ it1.m_index != it2.m_index;
}

// assignment operator
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE PackedStreamRef<Stream>& PackedStreamRef<Stream>::operator= (const PackedStreamRef& ref)
{
    return (*this = Symbol( ref ));
}

// assignment operator
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE PackedStreamRef<Stream>& PackedStreamRef<Stream>::operator= (const Symbol s)
{
    m_stream.set( m_index, s );
    return *this;
}

// conversion operator
//
template <typename Stream>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE PackedStreamRef<Stream>::operator Symbol() const
{
    return m_stream.get( m_index );
}

// assign a sequence to a packed stream
//
template <typename InputIterator, typename InputStream, typename Symbol, uint32 SYMBOL_SIZE_T, bool BIG_ENDIAN_T, typename IndexType>
NVBIO_HOST_DEVICE
void assign(
    const uint32                                                                                    input_len,
    InputIterator                                                                                   input_string,
    PackedStreamIterator< PackedStream<InputStream,Symbol,SYMBOL_SIZE_T,BIG_ENDIAN_T,IndexType> >   packed_string)
{
    typedef PackedStream<InputStream,Symbol,SYMBOL_SIZE_T,BIG_ENDIAN_T,IndexType> packed_stream_type;
    typedef typename packed_stream_type::storage_type word_type;

    const uint32 WORD_SIZE = uint32( 8u * sizeof(word_type) );

    const bool   BIG_ENDIAN       = BIG_ENDIAN_T;
    const uint32 SYMBOL_SIZE      = SYMBOL_SIZE_T;
    const uint32 SYMBOLS_PER_WORD = WORD_SIZE / SYMBOL_SIZE;
    const uint32 SYMBOL_COUNT     = 1u << SYMBOL_SIZE;
    const uint32 SYMBOL_MASK      = SYMBOL_COUNT - 1u;

    word_type* words = packed_string.container().stream();

    const IndexType stream_offset = packed_string.index();
    const uint32    word_offset   = stream_offset & (SYMBOLS_PER_WORD-1);
          uint32    word_rem      = 0;

    if (word_offset)
    {
        // compute how many symbols we still need to encode to fill the current word
        word_rem = SYMBOLS_PER_WORD - word_offset;

        // fetch the word in question
        word_type word = words[ stream_offset / SYMBOLS_PER_WORD ];

        // loop through the word's bp's
        for (uint32 i = 0; i < word_rem; ++i)
        {
            // fetch the bp
            const uint8 bp = input_string[i];

            const uint32       bit_idx = (word_offset + i) * SYMBOL_SIZE;
            const uint32 symbol_offset = BIG_ENDIAN ? (WORD_SIZE - SYMBOL_SIZE - bit_idx) : bit_idx;
            const word_type     symbol = word_type(bp) << symbol_offset;

            // clear all bits
            word &= ~(uint64(SYMBOL_MASK) << symbol_offset);

            // set bits
            word |= symbol;
        }

        // write out the word
        words[ stream_offset / SYMBOLS_PER_WORD ] = word;
    }

    //#pragma omp parallel for
    for (int i = word_rem; i < int( input_len ); i += SYMBOLS_PER_WORD)
    {
        // encode a word's worth of characters
        word_type word = 0u;

        const uint32 n_symbols = nvbio::min( SYMBOLS_PER_WORD, input_len - i );

        // loop through the word's bp's
        for (uint32 j = 0; j < SYMBOLS_PER_WORD; ++j)
        {
            if (j < n_symbols)
            {
                // fetch the bp
                const uint8 bp = input_string[i + j];

                const uint32       bit_idx = j * SYMBOL_SIZE;
                const uint32 symbol_offset = BIG_ENDIAN ? (WORD_SIZE - SYMBOL_SIZE - bit_idx) : bit_idx;
                const word_type     symbol = word_type(bp) << symbol_offset;

                // set bits
                word |= symbol;
            }
        }

        // write out the word
        const uint32 word_idx = (stream_offset + i) / SYMBOLS_PER_WORD;

        words[ word_idx ] = word;
    }
}

//
// A utility device function to transpose a set of packed input streams:
//   the symbols of the i-th input stream is supposed to be stored contiguously in the range [offset(i), offset + N(i)]
//   the *words* of i-th output stream will be stored in strided fashion at out_stream[tid, tid + (N(i)+symbols_per_word-1/symbols_per_word) * stride]
//
// The function is warp-synchronous, hence all threads in each warp must be active.
//
// \param stride       output stride
// \param N            length of this thread's string in the input stream
// \param in_offset    offset of this thread's string in the input stream
// \param in_stream    input stream
// \param out_stream   output stream
//
template <uint32 BLOCKDIM, uint32 SYMBOL_SIZE, bool BIG_ENDIAN, typename InStreamIterator, typename OutStreamIterator>
NVBIO_HOST_DEVICE
void transpose_packed_streams(const uint32 stride, const uint32 N, const uint32 in_offset, const InStreamIterator in_stream, OutStreamIterator out_stream)
{
    typedef typename std::iterator_traits<InStreamIterator>::value_type word_type;

    const uint32 SYMBOLS_PER_WORD = (sizeof(word_type)*8) / SYMBOL_SIZE;
          uint32 word_offset      = in_offset & (SYMBOLS_PER_WORD-1);
          uint32 begin_word       = in_offset / SYMBOLS_PER_WORD;
          uint32 end_word         = (in_offset + N + SYMBOLS_PER_WORD-1) / SYMBOLS_PER_WORD;

    // write out the output symbols
    const uint32 N_words = (N + SYMBOLS_PER_WORD-1) / SYMBOLS_PER_WORD;
    word_type cur_word = in_stream[begin_word+0];
    for (uint32 w = 0; w < N_words; ++w)
    {
        if (BIG_ENDIAN == false)
        {
            // fill the first part of the output word
            word_type out_word = cur_word >> (word_offset*SYMBOL_SIZE);

            // fetch the next word
            cur_word = begin_word+w+1 < end_word ? in_stream[begin_word+w+1] : 0u;

            // fill the second part of the output word
            if (word_offset)
                out_word |= cur_word << ((SYMBOLS_PER_WORD - word_offset)*SYMBOL_SIZE);

            out_stream[ stride*w ] = out_word;
        }
        else
        {
            // fill the first part of the output word
            word_type out_word = cur_word << (word_offset*SYMBOL_SIZE);

            // fetch the next word
            cur_word = begin_word+w+1 < end_word ? in_stream[begin_word+w+1] : 0u;

            // fill the second part of the output word
            if (word_offset)
                out_word |= cur_word >> ((SYMBOLS_PER_WORD - word_offset)*SYMBOL_SIZE);

            out_stream[ stride*w ] = out_word;
        }
    }
}

} // namespace nvbio
