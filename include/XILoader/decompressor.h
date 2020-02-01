#pragma once

#include "data_reader.h"

namespace XIL {

    class Inflator
    {
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
                default:
                    throw std::runtime_error("Unknown compression method (BTYPE == 2)");
                }
            } while (!is_final_block);
        }
    private:
        static void inflate_dynamic(ChunkedBitReader& bit_stream, ImageData::Container& uncompressed_stream)
        {
            auto hlit = bit_stream.get_bits(5);
            auto hdist = bit_stream.get_bits(5);
            auto hclen = bit_stream.get_bits(4);

            // auto tree = build_huffman_tree(hlit, hdist, hclen, bit_stream)
            // inflate_with_tree(tree)
        }
    };
}
