#pragma once

#include "image.h"
#include "data_reader.h"
#include "decompressor.h"

namespace XIL {

    class PNG
    {
    private:
        struct chunk
        {
            uint32_t length;
            uint8_t type[4];
            DataReader data;
            uint32_t crc;
        };

        struct zlib_header
        {
            uint8_t compression_method;
            uint8_t compression_info;
            uint8_t fcheck;
            uint8_t fdict;
            uint8_t flevel;
            bool set;
        };

        struct png_data
        {
            uint32_t width;
            uint32_t height;
            uint8_t bit_depth;
            uint8_t color_type;
            uint8_t compression_method;
            uint8_t filter_method;
            uint8_t interlace_method;
            zlib_header zheader;

            bool zlib_set() const noexcept { return zheader.set; }
        };
    public:
        static void load(DataReader& file_stream, Image& image, bool force_flip)
        {
            chunk chnk{};
            png_data idata{};
            ChunkedBitReader bit_stream{};

            try {
                // skip file signature
                file_stream.skip_n(8);

                // go through the entire file
                // collect all the necessary image data
                // and concatenate all the idat chunks
                for (;;)
                {
                    read_chunk(file_stream, chnk);

                    if (is_iend(chnk)) break;
                    if (is_ancillary(chnk)) continue;

                    if (is_ihdr(chnk))
                    {
                        read_header(chnk, idata);
                        continue;
                    }

                    if (is_idat(chnk))
                    {
                        if (!idata.zlib_set())
                        {
                            read_zlib_header(chnk, idata);
                            validate_zlib_header(idata.zheader);
                        }

                        bit_stream.append_chunk(chnk.data);
                    }
                }

                // decompress the data
                ImageData::Container uncompressed_data;
                Inflator::inflate(bit_stream, uncompressed_data);

                // process the decompressed data
            }
            catch (const std::exception& ex)
            {
                XIL_UNUSED(ex);
                return;
            }
        }
    private:
        static void validate_zlib_header(const zlib_header& header)
        {
            if (header.compression_method != 8)
                throw std::runtime_error("Compression method for PNG has to be DEFLATE (8)");
            if (header.fdict != 0)
                throw std::runtime_error("PNG can't be compressed with preset dictionaries");
        }

        static void read_chunk(DataReader& file, chunk& into)
        {
            into.length = file.get_u32_big();

            file.get_n(4, into.type);
            into.data = file.get_subset(into.length);

            into.crc = file.get_u32_big();
        }

        static void read_zlib_header(chunk& from, png_data& into)
        {
            into.zheader.set = true;
            into.zheader.compression_method = from.data.get_bits(0, 4);
            into.zheader.compression_info = from.data.get_bits(4, 4);
            from.data.next_byte();
            into.zheader.fcheck = from.data.get_bits(0, 5);
            into.zheader.fdict = from.data.get_bit(5);
            into.zheader.flevel = from.data.get_bits(6, 2);
            from.data.next_byte();
        }

        static void read_header(chunk& from, png_data& into)
        {
            into.width = from.data.get_u32_big();
            into.height = from.data.get_u32_big();

            into.bit_depth          = from.data.get_u8();
            into.color_type         = from.data.get_u8();
            into.compression_method = from.data.get_u8();
            into.filter_method      = from.data.get_u8();
            into.interlace_method   = from.data.get_u8();
        }

        static bool is_ancillary(const chunk& chnk)
        {
            return islower(chnk.type[0]);
        }

        static bool is_iend(const chunk& chnk)
        {
            return (chnk.type[0] == 'I') &&
                   (chnk.type[1] == 'E') &&
                   (chnk.type[2] == 'N') &&
                   (chnk.type[3] == 'D');
        }

        static bool is_ihdr(const chunk& chnk)
        {
            return (chnk.type[0] == 'I') &&
                   (chnk.type[1] == 'H') &&
                   (chnk.type[2] == 'D') &&
                   (chnk.type[3] == 'R');
        }

        static bool is_idat(const chunk& chnk)
        {
            return (chnk.type[0] == 'I') &&
                   (chnk.type[1] == 'D') &&
                   (chnk.type[2] == 'A') &&
                   (chnk.type[3] == 'T');
        }
    };
}
