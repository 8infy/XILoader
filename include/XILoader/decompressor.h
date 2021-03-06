#pragma once

#include "data_stream.h"

namespace XIL {

    class Inflator
    {
    private:
        static constexpr size_t fixed_litlen = 288;
        static constexpr size_t max_litlen   = 286;
        static constexpr size_t max_dist     = 30;
        static constexpr size_t max_bits     = 15;

        template <size_t sym_count, size_t len_count = max_bits + 1>
        struct huffman_tree
        {
            huffman_tree()
            {
                memset(lengths, 0, len_count * sizeof(uint16_t));
                memset(symbols, 0, sym_count * sizeof(uint16_t));
            }

            static constexpr size_t symbol_count() { return sym_count; }
            static constexpr size_t length_count() { return len_count; }

            uint16_t lengths[len_count];
            uint16_t symbols[sym_count];
        };

    public:
        static void inflate(ChunkedBitReader& bit_stream, ImageData::Container& uncompressed_stream)
        {
            bool is_final_block = false; // aka BFINAL
            do
            {
                is_final_block = bit_stream.get_bits(1);

                auto compression_method = bit_stream.get_bits(2); // aka BTYPE

                switch (compression_method)
                {
                case 0:
                    // uncompressed
                    inflate_uncompressed(bit_stream, uncompressed_stream);
                    break;
                case 1:
                    // fixed huffman codes
                    inflate_fixed(bit_stream, uncompressed_stream);
                    break;
                case 2:
                    // dynamic huffman codes
                    inflate_dynamic(bit_stream, uncompressed_stream);
                    break;
                default:
                    throw std::runtime_error("Unknown compression method (BTYPE == 2)");
                }
            } while (!is_final_block);
        }
    private:
        static void inflate_dynamic(ChunkedBitReader& bit_stream, ImageData::Container& uncompressed_stream)
        {
            static const uint8_t symbol_order[19] =
            { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

            uint16_t lengths[max_litlen + max_dist];
            memset(lengths, 0, sizeof(uint16_t) * (max_litlen + max_dist));

            auto hlit  = bit_stream.get_bits(5) + 257ull;
            auto hdist = bit_stream.get_bits(5) + 1ull;
            auto hclen = bit_stream.get_bits(4) + 4;

            if (hlit > max_litlen)
                throw std::runtime_error("HLIT cannot be greater than 286");
            if (hdist > max_dist)
                throw std::runtime_error("HDIST cannot be greater than 30");

            for (uint16_t i = 0; i < hclen; i++)
                lengths[symbol_order[i]] = bit_stream.get_bits(3);

            huffman_tree<XIL_BITS(4) + 4, XIL_BITS(3) + 1> length_lengths_tree;

            construct_tree(length_lengths_tree, lengths, 19);

            size_t index = 0;
            while (index < hdist + hlit)
            {
                uint16_t symbol;
                uint16_t length = 0;

                symbol = decode_one(bit_stream, length_lengths_tree);

                if (symbol < 16)
                    lengths[index++] = symbol;
                else
                {
                    if (symbol == 16)
                    {
                        if (!index)
                            throw std::runtime_error("Repeat instruction for an empty buffer");
                        length = lengths[index - 1];
                        symbol = 3 + bit_stream.get_bits(2);
                    }
                    else if (symbol == 17)
                        symbol = 3 + bit_stream.get_bits(3);
                    else
                        symbol = 11 + bit_stream.get_bits(7);

                    if (index + symbol > hdist + hlit)
                        throw std::runtime_error("Too many lengths");

                    while (symbol--)
                        lengths[index++] = length;
                }
            }

            if (!lengths[256])
                throw std::runtime_error("End of block code (256) is not present in the data");

            huffman_tree<max_litlen> litlen_tree;
            huffman_tree<max_dist>   distance_tree;

            construct_tree(litlen_tree, lengths, hlit);
            construct_tree(distance_tree, lengths + hlit, hdist);

            decompress_block(bit_stream, litlen_tree, distance_tree, uncompressed_stream);
        }

        static void inflate_fixed(ChunkedBitReader& bit_stream, ImageData::Container& uncompressed_stream)
        {
            static huffman_tree<fixed_litlen> litlen_tree;
            static huffman_tree<max_dist>     distance_tree;

            static bool constructed = false;

            if (!constructed)
            {
                uint16_t symbol;
                uint16_t lengths[fixed_litlen];

                for (symbol = 0; symbol < 144; symbol++)
                    lengths[symbol] = 8;
                for (; symbol < 256; symbol++)
                    lengths[symbol] = 9;
                for (; symbol < 280; symbol++)
                    lengths[symbol] = 7;
                for (; symbol < fixed_litlen; symbol++)
                    lengths[symbol] = 8;

                construct_tree(litlen_tree, lengths, fixed_litlen);

                for (symbol = 0; symbol < max_dist; symbol++)
                    lengths[symbol] = 5;

                construct_tree(distance_tree, lengths, max_dist);

                constructed = true;
            }

            decompress_block(bit_stream, litlen_tree, distance_tree, uncompressed_stream);
        }

        static void inflate_uncompressed(ChunkedBitReader& bit_stream, ImageData::Container& uncompressed_stream)
        {
            bit_stream.flush_byte(false);

            uint16_t length  = bit_stream.get_bits(16);
            uint16_t nlength = bit_stream.get_bits(16);

            if (length != static_cast<uint16_t>(~nlength))
                throw std::runtime_error("LEN/NLEN mismatch");

            uncompressed_stream.reserve(uncompressed_stream.size() + length);

            while (length--)
                uncompressed_stream.push_back(bit_stream.get_bits(8));
        }

        template<typename HuffmanT>
        static void construct_tree(HuffmanT& out_tree, const uint16_t* lengths, size_t lengths_length)
        {
            uint16_t offsets[out_tree.length_count()];

            // count number of codes for each length
            for (uint16_t symbol = 0; symbol < lengths_length; symbol++)
                out_tree.lengths[lengths[symbol]]++;

            // no codes
            if (out_tree.lengths[0] == out_tree.symbol_count())
                throw std::runtime_error("All codes in the tree are zero length/not present");

            // check if all code lengths have valid counts
            int32_t codes_left = 1;
            for (uint16_t length = 1; length < out_tree.length_count(); length++)
            {
                codes_left <<= 1;
                codes_left -= out_tree.lengths[length];
                if (codes_left < 0)
                    throw std::runtime_error("Encountered more codes for a length than allowed");
            }

            // calculate offsets
            offsets[1] = 0;
            for (uint16_t length = 1; length < out_tree.length_count() - 1; length++)
                offsets[length + 1] = offsets[length] + out_tree.lengths[length];

            // add a symbol for each length
            for (uint16_t symbol = 0; symbol < lengths_length; symbol++)
                if (lengths[symbol])
                    out_tree.symbols[offsets[lengths[symbol]]++] = symbol;
        }

        template<typename HuffmanT>
        static uint16_t decode_one(ChunkedBitReader& from, HuffmanT& with_tree)
        {
            int64_t length = 1;
            int64_t code   = 0;
            int64_t first  = 0;
            int64_t index  = 0;
            int64_t count;
            auto next = &with_tree.lengths[1];

            for (;;)
            {
                code |= from.get_bits(1);
                count = *next++;
                if (code - count < first)
                    return with_tree.symbols[index + (code - first)];
                index += count;
                first += count;
                first <<= 1;
                code <<= 1;
                length++;
            }
        }

        template<typename HuffmanTL, typename HuffmanTD>
        static void decompress_block(
            ChunkedBitReader& from,
            HuffmanTL& litlen_tree,
            HuffmanTD& distance_tree,
            ImageData::Container& uncompressed_stream)
        {
            size_t initial_size = uncompressed_stream.size();
            size_t block_index = initial_size;

            uint16_t symbol;
            size_t length;
            size_t distance;

            static constexpr uint16_t length_base[29] =
            { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
              35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };

            static constexpr uint16_t length_extra[29] =
            { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2,
              2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };

            static constexpr uint16_t distance_base[30] =
            { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 
              97, 129, 193, 257, 385, 513, 769, 1025, 1537,
              2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };

            static constexpr uint16_t distance_extra[30] =
            { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4,
              4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,
              10, 10, 11, 11, 12, 12, 13, 13 };

            do {
                symbol = decode_one(from, litlen_tree);

                if (symbol < 256)
                {
                    uncompressed_stream.push_back(static_cast<uint8_t>(symbol));
                    block_index++;
                }
                else if (symbol > 256)
                {
                    symbol -= 257;
                    if (symbol >= 29)
                        throw std::runtime_error("Length symbol is outside of [29] range");

                    auto extra_len_bits = static_cast<uint8_t>(length_extra[symbol]);
                    length = static_cast<size_t>(length_base[symbol]) + from.get_bits(extra_len_bits);

                    symbol = decode_one(from, distance_tree);

                    auto extra_dist_bits = static_cast<uint8_t>(distance_extra[symbol]);
                    distance = static_cast<size_t>(distance_base[symbol]) + from.get_bits(extra_dist_bits);

                    if (distance > block_index)
                        throw std::runtime_error("Distance is outside of the out block");

                    while (length--)
                    {
                        uncompressed_stream.push_back(uncompressed_stream[block_index - distance]);
                        block_index++;
                    }
                }
            } while (symbol != 256);
        }
    };
}
