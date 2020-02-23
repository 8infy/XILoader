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

        struct palette
        {
            uint8_t* data;
            size_t size;

            bool set() const noexcept { return data; }
            uint8_t* at_index(size_t i) const noexcept { return data + (i * m_Stride); }

            void set_stride(size_t stride) { m_Stride = stride; }
        private:
            size_t m_Stride;
        };

    public:
        static void load(DataStream& file_stream, Image& image, bool force_flip)
        {
            chunk chnk{};
            png_data idata{};
            ChunkedBitReader bit_stream{};

            palette alpha_plt{};
            palette plt{};

            // skip file signature
            file_stream.skip_n(8);

            // go through the entire file
            // collect all the necessary image data
            // and concatenate all the idat chunks
            for (;;)
            {
                read_chunk(file_stream, chnk);

                if (is_iend(chnk)) break;

                if (is_trns(chnk))
                {
                    alpha_plt.data = chnk.data.data_ptr();
                    alpha_plt.size = chnk.data.bytes_left();
                    alpha_plt.set_stride(1);
                }

                if (is_ancillary(chnk)) continue;

                if (is_ihdr(chnk))
                {
                    read_header(chnk, idata);
                    continue;
                }

                if (is_plte(chnk))
                {
                    plt.data = chnk.data.data_ptr();
                    plt.size = chnk.data.bytes_left();
                    plt.set_stride(3);
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

            if (idata.interlace_method == 1)
                deinterlace(idata, uncompressed_data);

            // - just return if 8bpc RGB/RGBA
            // - do some more processing for palleted/sampled data/16bpc
            switch (idata.color_type)
            {
            case 0: // grayscale
            case 4: // grayscale + alpha
                image.m_Image.channels = idata.color_type ? 2 : 1;
                image.m_Image.width = idata.width;
                image.m_Image.height = idata.height;

                grayscale_transform(idata, uncompressed_data);

                image.m_Image.data = std::move(uncompressed_data);
                break;
            case 2: // RGB
                image.m_Image.channels = 3;
                image.m_Image.width = idata.width;
                image.m_Image.height = idata.height;

                if (idata.bit_depth == 16)
                    downscale(uncompressed_data, image.channels());

                image.m_Image.data = std::move(uncompressed_data);
                break;
            case 3: // decode the palleted data
                image.m_Image.channels = alpha_plt.set() ? 4 : 3;
                image.m_Image.width = idata.width;
                image.m_Image.height = idata.height;
                reconstruct_from_palette(idata, uncompressed_data, plt, alpha_plt);

                image.m_Image.data = std::move(uncompressed_data);
                break;
            case 6: // RGBA
                image.m_Image.channels = 4;
                image.m_Image.width = idata.width;
                image.m_Image.height = idata.height;

                if (idata.bit_depth == 16)
                    downscale(uncompressed_data, image.channels());

                image.m_Image.data = std::move(uncompressed_data);
                break;
            }

            if (force_flip)
                image.flip();
        }
    private:
        static void reconstruct_from_palette(png_data& idata, ImageData::Container& in_out, const palette& plt, const palette& alpha_plt)
        {
            auto paletted_data = std::move(in_out);
            ChunkedBitReader palette_stream(paletted_data.data(), paletted_data.size());

            ImageData::Container reconstructed_data;
            reconstructed_data.reserve(idata.width * idata.height * (alpha_plt.set() ? 4 : 3));

            for (size_t y = 0; y < idata.height; y++)
            {
                for (size_t x = 0; x < idata.width; x++)
                {
                    auto palette_index = palette_stream.get_bits_reversed(idata.bit_depth);
                    auto* RGB = plt.at_index(palette_index);
                    reconstructed_data.push_back(RGB[0]);
                    reconstructed_data.push_back(RGB[1]);
                    reconstructed_data.push_back(RGB[2]);

                    if (alpha_plt.set())
                        reconstructed_data.push_back(*alpha_plt.at_index(palette_index));
                }

                if (y != idata.height - 1)
                    palette_stream.flush_byte_reversed();
            }

            in_out = std::move(reconstructed_data);
        }

        static uint8_t upscale_to_8(uint8_t value, uint8_t width)
        {
            uint8_t upscaled = value << (8 - width);

            upscaled = upscaled | value;

            return upscaled;
        }

        static void grayscale_transform(png_data& idata, ImageData::Container& in_out)
        {
            auto grayscaled_data = std::move(in_out);

            ImageData::Container transformed_data;
            ChunkedBitReader data_stream(grayscaled_data.data(), grayscaled_data.size());

            transformed_data.reserve(idata.width * idata.height * (idata.color_type ? 1 : 2));

            for (size_t y = 0; y < idata.height; y++)
            {
                for (size_t x = 0; x < idata.width; x++)
                {
                    uint32_t gray_value = 0;

                    if (idata.bit_depth == 16)
                       gray_value = data_stream.get_two_bytes_big_reversed();
                    else
                        gray_value = data_stream.get_bits_reversed(idata.bit_depth);

                    switch (idata.bit_depth)
                    {
                    case 16:
                        gray_value = downscale_16_to_8(gray_value);
                    case 4:
                    case 2:
                    case 1:
                        gray_value = upscale_to_8(gray_value, idata.bit_depth);
                    }

                    transformed_data.push_back(gray_value);

                    if (idata.color_type == 4)
                    {
                        uint32_t alpha_value = 0;

                        if (idata.bit_depth == 16)
                            alpha_value = data_stream.get_two_bytes_big_reversed();
                        else
                            alpha_value = data_stream.get_bits_reversed(idata.bit_depth);

                        switch (idata.bit_depth)
                        {
                        case 16:
                            alpha_value = downscale_16_to_8(alpha_value);
                        case 4:
                        case 2:
                        case 1:
                            alpha_value = upscale_to_8(alpha_value, idata.bit_depth);
                        }

                        transformed_data.push_back(alpha_value);
                    }
                }

                if (y < idata.height - 1)
                    data_stream.flush_byte_reversed();
            }

            in_out = std::move(transformed_data);
        }

        static void deinterlace(png_data& idata, ImageData::Container& in_out)
        {

        }

        static uint8_t downscale_16_to_8(uint16_t channel)
        {
            #ifdef XIL_PRECISE_DOWNSCALING
                return floor((static_cast<float>(channel) * XIL_BITS(8) / XIL_BITS(16)) + 0.5f);
            #else
                return channel >> 8;
            #endif
        }

        static void downscale(ImageData::Container& in_out, uint8_t channels)
        {
            auto upscaled = std::move(in_out);
            ImageData::Container downscaled;
            downscaled.resize(in_out.size() / 2);

            assert(!(upscaled.size() % 2));
            for (size_t pix = 0; pix < upscaled.size(); pix += 2)
            {
                uint16_t upscaled_channel = *reinterpret_cast<uint16_t*>(&upscaled[pix]);

                if XIL_CONSTEXPR (host_endiannes() == byte_order::BIG)
                        upscaled_channel = XIL_U16_SWAP(upscaled_channel);

                uint8_t downscaled_channel = downscale_16_to_8(upscaled_channel);


                downscaled.push_back(downscaled_channel);
            }

            in_out = std::move(downscaled);
        }

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
            float channel_stride = idata.bit_depth / 8.0f;

            size_t channels_per_pixel = 0;

            if (idata.color_type == 0 || idata.color_type == 3)
                channels_per_pixel = 1;
            else if (idata.color_type == 2)
                channels_per_pixel = 3;
            else if (idata.color_type == 4)
                channels_per_pixel = 2;
            else if (idata.color_type == 6)
                channels_per_pixel = 4;

            float true_pixel_stride = channels_per_pixel * channel_stride;
            size_t pixel_stride = static_cast<size_t>(ceil(true_pixel_stride));
            size_t true_byte_width = static_cast<size_t>(ceil(idata.width * true_pixel_stride));

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
                        uint8_t ptl = pixel_to_the_left(row_begin, x, pixel_stride);
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

        static bool is_trns(const chunk& chnk)
        {
            return (chnk.type[0] == 't') &&
                   (chnk.type[1] == 'R') &&
                   (chnk.type[2] == 'N') &&
                   (chnk.type[3] == 'S');
        }

        static bool is_plte(const chunk& chnk)
        {
            return (chnk.type[0] == 'P') &&
                   (chnk.type[1] == 'L') &&
                   (chnk.type[2] == 'T') &&
                   (chnk.type[3] == 'E');
        }
    };
}
