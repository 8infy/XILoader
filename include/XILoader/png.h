#pragma once

#include <algorithm>

#include "image.h"
#include "data_stream.h"
#include "decompressor.h"

namespace XIL {

    class PNG
    {
    private:
        struct chunk
        {
            uint32_t length;
            uint8_t type[4];
            DataStream data;
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
        static void load(DataStream& file_stream, Image& image, bool force_flip)
        {
            chunk chnk{};
            png_data idata{};
            ChunkedBitReader bit_stream{};

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

            // reconstruct the values by removing filters
            unfilter_values(idata, uncompressed_data);

            // - just return if RGB/RGBA
            // - do some more processing for palleted/sampled data
            switch (idata.color_type)
            {
            case 0: // grayscale
                throw std::runtime_error("Grayscale PNGs are not yet supported");
            case 2: // RGB
                image.m_Image.channels = 3;
                image.m_Image.width = idata.width;
                image.m_Image.height = idata.height;

                if (idata.bit_depth == 8)
                    image.m_Image.data = std::move(uncompressed_data);
                else // translate 16bpc into 8 bpc
                    throw std::runtime_error("16bpc PNGs are not yet supported");
                break;
            case 3: // decode the palleted data
                throw std::runtime_error("Palleted PNGs are not yet supported");
            case 4: // grayscale + alpha
                throw std::runtime_error("Grayscale PNGs are not yet supported");
            case 6: // RGBA
                image.m_Image.channels = 4;
                image.m_Image.width = idata.width;
                image.m_Image.height = idata.height;

                if (idata.bit_depth == 8)
                    image.m_Image.data = std::move(uncompressed_data);
                else // translate 16bpc into 8 bpc
                    throw std::runtime_error("16bpc PNGs are not yet supported");
                break;
            }

            if (force_flip)
                image.flip();
        }
    private:
        static uint8_t pixel_to_the_left(uint8_t* start_of_row, size_t x, size_t pixel_stride)
        {
            if (x <= pixel_stride) return 0;

            auto* pixel = start_of_row + x - pixel_stride;

            return *pixel;
        }

        static uint8_t pixel_above(uint8_t* start_of_row, size_t x, size_t y, size_t image_width)
        {
            if (y == 0) return 0;

            auto* pixel = start_of_row + x - image_width - 1;

            return *pixel;
        }

        static uint8_t pixel_above_and_to_the_left(uint8_t* start_of_row, size_t x, size_t y, size_t pixel_stride, size_t image_width)
        {
            if (x <= pixel_stride || y == 0) return 0;

            auto* pixel = start_of_row + x - image_width - pixel_stride - 1;

            return *pixel;
        }

        static void unfilter_values(const png_data& idata, ImageData::Container& in_out)
        {
            float channel_stride = 8.0f / idata.bit_depth;

            size_t channels_per_pixel = 0;

            if (idata.color_type == 0 || idata.color_type == 3)
                channels_per_pixel = 1;
            else if (idata.color_type == 2)
                channels_per_pixel = 3;
            else if (idata.color_type == 4)
                channels_per_pixel = idata.bit_depth * 2;  // is this correct?
            else if (idata.color_type == 6)
                channels_per_pixel = 4;

            // TODO: make sure this works for weird bpcs
            size_t pixel_stride = static_cast<size_t>(channels_per_pixel / channel_stride);
            size_t true_byte_width = idata.width * pixel_stride;

            for (size_t y = 0; y < idata.height; y++)
            {
                auto start_of_row = y * true_byte_width + y;
                auto* row_begin = &in_out[start_of_row];

                uint8_t filter_method = in_out[start_of_row];

                switch (filter_method)
                {
                case 0:
                    continue;
                case 1:
                    for (size_t x = 1; x < true_byte_width + 1; x++)
                    {
                        auto ptl = pixel_to_the_left(row_begin, x, pixel_stride);
                        in_out[start_of_row + x] = in_out[start_of_row + x] + ptl;
                    }
                    continue;
                case 2:
                    for (size_t x = 1; x < true_byte_width + 1; x++)
                    {
                        auto pa = pixel_above(row_begin, x, y, true_byte_width);
                        in_out[start_of_row + x] = in_out[start_of_row + x] + pa;
                    }
                    continue;
                case 3:
                    for (size_t x = 1; x < true_byte_width + 1; x++)
                    {
                        auto ptl = pixel_to_the_left(row_begin, x, pixel_stride);
                        auto pa = pixel_above(row_begin, x, y, true_byte_width);

                        uint8_t unfiltered_value = static_cast<uint8_t>((ptl + pa) / 2);

                        in_out[start_of_row + x] = in_out[start_of_row + x] + unfiltered_value;
                    }
                    continue;
                case 4:
                    for (size_t x = 1; x < true_byte_width + 1; x++)
                    {
                        int32_t above = pixel_above(row_begin, x, y, true_byte_width);
                        int32_t left = pixel_to_the_left(row_begin, x, pixel_stride);
                        int32_t above_and_left = pixel_above_and_to_the_left(row_begin, x, y, pixel_stride, true_byte_width);

                        // Paeth
                        int32_t p  = left + above - above_and_left;
                        int32_t pa = abs(p - left);
                        int32_t pb = abs(p - above);
                        int32_t pc = abs(p - above_and_left);

                        uint8_t value = 0;

                        if (pa <= pb && pa <= pc)
                            value = left;
                        else if (pb <= pc)
                            value = above;
                        else
                            value = above_and_left;

                        in_out[start_of_row + x] = in_out[start_of_row + x] + value;
                    }
                    continue;
                default:
                    throw std::runtime_error("Unknown filter method (!= 4)");
                }
            }

            // remove all scanline filter method bytes
            size_t byte_count = 0;

            auto is_filter_method_byte =
                [&](uint8_t)
                {
                    if ((byte_count % (true_byte_width + 1)) == 0)
                    {
                        byte_count++;
                        return true;
                    }
                    else
                    {
                        byte_count++;
                        return false;
                    }
                };

            auto erase_begin = std::remove_if(in_out.begin(), in_out.end(), is_filter_method_byte);
            in_out.erase(erase_begin, in_out.end());
        }

        static void validate_zlib_header(const zlib_header& header)
        {
            if (header.compression_method != 8)
                throw std::runtime_error("Compression method for PNG has to be DEFLATE (8)");
            if (header.fdict != 0)
                throw std::runtime_error("PNG can't be compressed with preset dictionaries");
        }

        static void read_chunk(DataStream& file, chunk& into)
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
            into.zheader.compression_info   = from.data.get_bits(4, 4);
            from.data.next_byte();
            into.zheader.fcheck = from.data.get_bits(0, 5);
            into.zheader.fdict  = from.data.get_bit(5);
            into.zheader.flevel = from.data.get_bits(6, 2);
            from.data.next_byte();
        }

        static void read_header(chunk& from, png_data& into)
        {
            into.width  = from.data.get_u32_big();
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
