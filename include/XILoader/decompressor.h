#pragma once

#include "data_reader.h"

#define MAX_LITLEN 286
#define MAX_DIST   30
#define MAX_BITS   15

namespace XIL {

    class Inflator
    {
    private:
        template <size_t sym_count, size_t len_count = MAX_BITS + 1>
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
                    break;
                case 1:
                    // fixed huffman codes
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

            uint16_t lengths[MAX_LITLEN + MAX_DIST];
            memset(lengths, 0, MAX_LITLEN + MAX_DIST);

            auto hlit  = bit_stream.get_bits(5) + 257;
            auto hdist = bit_stream.get_bits(5) + 1;
            auto hclen = bit_stream.get_bits(4) + 4;

            if (hlit > MAX_LITLEN)
                throw std::runtime_error("HLIT cannot be greater than 286");
            if (hdist > MAX_DIST)
                throw std::runtime_error("HDIST cannot be greater than 30");

            for (uint16_t i = 0; i < hclen; i++)
                lengths[symbol_order[i]] = bit_stream.get_bits(3);

            huffman_tree<XIL_BITS(4) + 4, XIL_BITS(3) + 1> length_lengths_tree;

            construct_tree(length_lengths_tree, lengths);

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

            huffman_tree<MAX_LITLEN> litlen_tree;
            huffman_tree<MAX_DIST>   distance_tree;

            construct_tree(litlen_tree, lengths);
            construct_tree(distance_tree, lengths + hlit);

            decompress_block(bit_stream, litlen_tree, distance_tree);
        }

        template<typename HuffmanT>
        static void construct_tree(HuffmanT& out_tree, const uint16_t* lengths)
        {
            uint16_t offsets[out_tree.length_count()];

            // count number of codes for each length
            for (uint16_t symbol = 0; symbol < out_tree.symbol_count(); symbol++)
                out_tree.lengths[lengths[symbol]]++;
            
            // no codes
            if (out_tree.lengths[0] == out_tree.symbol_count())
                throw std::runtime_error("All codes in the tree are zero length/not present");

            // check if all code lengths have valid counts
            int16_t codes_left = 1;
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
            for (uint16_t symbol = 0; symbol < out_tree.symbol_count(); symbol++)
                if (lengths[symbol])
                    out_tree.symbols[offsets[lengths[symbol]]++] = symbol;
        }

        template<typename HuffmanT>
        static uint16_t decode_one(ChunkedBitReader& from, HuffmanT& with_tree)
        {
            uint32_t length = 1;
            uint32_t code   = 0;
            uint32_t first  = 0;
            uint32_t index  = 0;
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
        static void decompress_block(ChunkedBitReader& from, HuffmanTL& litlen_tree, HuffmanTD& distance_tree)
        {

        }
    };
}
