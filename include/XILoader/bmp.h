#pragma once

#include <stdexcept>
#include <assert.h>

#include "utils.h"
#include "file_reader.h"
#include "image.h"

namespace XIL
{
    class BMP
    {
        struct rgba_mask
        {
            uint32_t r;
            int32_t  r_shift;
            int8_t   r_bits;

            uint32_t g;
            int32_t  g_shift;
            int8_t   g_bits;

            uint32_t b;
            int32_t  b_shift;
            int8_t   b_bits;

            uint32_t a;
            int32_t  a_shift;
            int8_t   a_bits;

            bool has_alpha() { return a; }
        };

        struct bmp_data {
            uint32_t pao;
            uint32_t dib_size;
            bool flipped;
            std::vector<uint8_t> palette;
            uint32_t compression_method;
            uint32_t colors;

            // bytes per color
            // can be 3 or 4 (99% of the time its 4)
            uint16_t bpc;
            uint16_t bpp;
            uint8_t channels;
            uint16_t width;
            uint16_t height;
            rgba_mask masks;

            bool has_palette()   const noexcept { return colors; }
            bool has_rgba_mask() const noexcept { return masks.a | masks.r | masks.g | masks.b; }
        };
    public:
        static void load(DataReader& file, Image& image, bool force_flip)
        {
            bmp_data idata{};

            try {
                // skip magic numbers
                file.skip_n(2);

                // skip file size
                file.skip_n(4);

                // skip reserved
                file.skip_n(4);

                // pixel array offset
                idata.pao = file.get_u32();

                idata.dib_size = file.get_u32();

                // invalid dib
                if ((idata.dib_size < 12) || (idata.dib_size > 124))
                    return;
                // OS21X headers
                else if (
                    idata.dib_size == 12 ||
                    idata.dib_size == 64 ||
                    idata.dib_size == 16
                )
                {
                    idata.width = file.get_u16();
                    idata.height = file.get_u16();
                }
                // Other headers
                else
                {
                    int32_t width = file.get_i32();
                    idata.width = static_cast<uint16_t>(width);

                    int32_t height = file.get_i32();
                    if (height < 0) idata.flipped = true;
                    idata.height = static_cast<uint16_t>(abs(height));
                }

                uint16_t color_planes = file.get_u16();
                if (color_planes != 1) return;

                idata.bpp = file.get_u16();

                // ---- End of the default header size stuff (under 40) ----

                if (idata.dib_size >= 40)
                {
                    idata.compression_method = file.get_u32();

                    // Raw bitmap size
                    file.skip_n(4);

                    // Horizontal resolution
                    file.skip_n(4);

                    // Vertical resolution
                    file.skip_n(4);

                    idata.colors = file.get_u32();

                    // Importan colors
                    file.skip_n(4);
                }

                // Just skip the colors if the image is non-indexed
                // (Basically they're only there for older devices compatibility)
                if (idata.colors && (idata.bpp > 8))
                    idata.colors = 0;
                // This also handles the infamous 16-byte OS22X header
                if (!idata.colors && idata.bpp <= 8)
                    idata.colors = static_cast<uint32_t>(pow(2, idata.bpp));

                // Bit masks
                if (idata.compression_method        &&
                   (idata.compression_method != 3)  &&
                   (idata.compression_method != 6))
                    return;

                // OS22XBITMAPHEADER: Huffman 1D
                if (idata.compression_method == 3 && (idata.dib_size == 16 || idata.dib_size == 64))
                    return;

                if (idata.has_palette())
                {
                    if (idata.dib_size > 12)
                    {
                        idata.palette.resize(idata.colors * sizeof(uint32_t));
                        idata.bpc = sizeof(uint32_t);
                    }
                    else
                    {
                        idata.palette.resize(idata.colors * 3ull);
                        idata.bpc = 3;
                    }
                }

                // BITMAPINFOHEADER stores this after the dib
                if ((idata.compression_method == 3) || (idata.compression_method == 6))
                {
                    idata.masks.r = file.get_u32();
                    idata.masks.r_bits = count_bits(idata.masks.r);
                    idata.masks.r_shift = highest_set_bit(idata.masks.r) - 7;

                    idata.masks.g = file.get_u32();
                    idata.masks.g_bits = count_bits(idata.masks.g);
                    idata.masks.g_shift = highest_set_bit(idata.masks.g) - 7;

                    idata.masks.b = file.get_u32();
                    idata.masks.b_bits = count_bits(idata.masks.b);
                    idata.masks.b_shift = highest_set_bit(idata.masks.b) - 7;

                    if ((idata.compression_method == 6) || (idata.dib_size >= 56))
                    {
                        idata.masks.a = file.get_u32();
                        idata.masks.a_bits = count_bits(idata.masks.a);
                        idata.masks.a_shift = highest_set_bit(idata.masks.a) - 7;
                    }
                }

                // OS22X
                if (idata.dib_size == 64)
                {
                    // Units
                    file.skip_n(2);

                    // Padding
                    file.skip_n(2);

                    // Recording algorithm (e.g how file is stored)
                    // the only valid value is 0, meaning left->right, bottom->top.
                    if (file.get_u16())
                        return;

                    // Halftoning stuff
                    file.skip_n(2);
                    file.skip_n(4);
                    file.skip_n(4);

                    // Color model
                    file.skip_n(4);

                    // Reserved
                    file.skip_n(4);
                }

                // new fancy headers (BITMAPV4 & BITMAPV5)
                // just skip the entire thing, we don't really care
                // or support the gamma stuff.
                if ((idata.dib_size == 108) || (idata.dib_size == 124))
                    file.skip_n(idata.dib_size - file.bytes_read() + 14); // 14 is the constant BMP header size

                if (idata.has_palette())
                {
                    if (idata.dib_size > 12)
                        file.get_n(idata.colors * sizeof(uint32_t), idata.palette.data());
                    else // OS21X stores colors as 24-bit RGB
                        file.get_n(idata.colors * 3ull, idata.palette.data());

                    // indexed images are always RGB (hopefully?)
                    idata.channels = 3;
                }
                else if ((idata.bpp == 16) && idata.has_rgba_mask())
                {
                    idata.channels = idata.masks.has_alpha() ? 4 : 3;
                }
                // 24 bit images are always RGB (hopefully?)
                else if (idata.bpp == 24)
                    idata.channels = 3;
                // We assume all 32 bit images to be RGBA
                else if (idata.bpp == 32)
                    idata.channels = 4;

                // skip N bytes to get to the pixel array
                auto pixel_array_gap = idata.pao - file.bytes_read();
                if (pixel_array_gap) file.skip_n(pixel_array_gap);

                idata.flipped = idata.flipped != force_flip;

                if (load_pixel_array(file, idata, image.m_Image.data))
                {
                    image.m_Image.channels = idata.channels;
                    image.m_Image.width = idata.width;
                    image.m_Image.height = idata.height;
                }
            }
            catch (const std::exception& ex)
            {
                // could be useful at some point
                XIL_UNUSED(ex);

                return;
            }
        }
    private:
        static bool load_pixel_array(DataReader& file, bmp_data& image_data, ImageData::Container& to)
        {
            if (image_data.has_palette())
                return load_indexed(file, image_data, to);
            else if (image_data.has_rgba_mask())
                return load_sampled(file, image_data, to);
            else
                return load_raw(file, image_data, to);
        }

        static bool load_indexed(DataReader& file, bmp_data& idata, ImageData::Container& to)
        {
            bool result = true;
            uint8_t* row_buffer = nullptr;

            try
            {
                uint32_t row_padded = static_cast<uint32_t>((ceil(idata.width / (8.0 / idata.bpp)) + 3)) & (~3);
                to.resize(3ull * idata.width * idata.height);

                DataReader row_buffer;

                // current Y of the image
                for (size_t i = 1; i < idata.height + 1ull; i++)
                {
                    row_buffer = file.get_subset(row_padded);

                    size_t pixel_count = 0;

                    for (;; row_buffer.next_byte())
                    {
                        for (int8_t pixel = 8 - idata.bpp; pixel >= 0; pixel -= idata.bpp)
                        {
                            pixel_count++;

                            uint8_t RGB[3];

                            size_t palette_index = row_buffer.get_bits(pixel, static_cast<uint8_t>(idata.bpp));

                            RGB[0] = idata.palette[palette_index * idata.bpc + 2];
                            RGB[1] = idata.palette[palette_index * idata.bpc + 1];
                            RGB[2] = idata.palette[palette_index * idata.bpc + 0];

                            size_t row_offset;

                            // flipped meaning stored top to bottom
                            if (idata.flipped)
                                row_offset = idata.width * (i - 1) * idata.channels;
                            else
                            {
                                row_offset = idata.width * i * idata.channels;
                                row_offset = to.size() - row_offset;
                            }

                            auto total_offset = row_offset + ((pixel_count - 1) * 3);

                            memcpy_s(
                                to.data() + total_offset,
                                to.size() - total_offset,
                                RGB, 3
                            );

                            if (pixel_count == idata.width)
                                break;
                        }
                        if (pixel_count == idata.width)
                            break;
                    }
                }
            }
            catch (const std::exception& ex)
            {
                XIL_UNUSED(ex);

                result = false;
            }

            delete[] row_buffer;

            if (!result)
                to.clear();

            return result;
        }

        // Tranforms a given RGBAX fraction into a 0-255 range R/G/B/A channel
        static uint8_t shift_signed_as_byte(uint32_t x, int32_t by, uint8_t bits)
        {
            static uint32_t mul_table[9] = {
               0   /*0b00000000*/,
               0xff/*0b11111111*/, 0x55/*0b01010101*/,
               0x49/*0b01001001*/, 0x11/*0b00010001*/,
               0x21/*0b00100001*/, 0x41/*0b01000001*/,
               0x81/*0b10000001*/, 0x01/*0b00000001*/,
            };

            static uint32_t shift_table[9] = {
               0,0,0,
               1,0,2,
               4,6,0,
            };

            if (by < 0)
                x <<= -by;
            else
                x >>= by;

            if (!((x >= 0) && (x < 256)))
                throw std::runtime_error("Invalid conversion (x)");

            x >>= (8 - bits);

            if (!((bits >= 0) && (bits <= 8)))
                throw std::runtime_error("Invalid conversion (bits)");

            return ((x * mul_table[bits]) >> shift_table[bits]) & UINT8_MAX;
        }

        static bool load_sampled(DataReader& file, bmp_data& idata, ImageData::Container& to)
        {
            bool result = true;
            uint8_t* row_buffer = nullptr;

            try {
                uint8_t bytes_per_pixel = idata.bpp / 8;
                uint32_t row_padded = (idata.width * bytes_per_pixel + 3) & (~3);
                to.resize(static_cast<size_t>(idata.channels) * idata.width * idata.height);

                DataReader row_buffer;

                for (size_t i = 1; i < idata.height + 1ull; i++)
                {
                    row_buffer = file.get_subset(row_padded);

                    for (size_t j = 0; j < static_cast<size_t>(idata.width) * bytes_per_pixel; j += bytes_per_pixel)
                    {
                        uint32_t sample;

                        if (bytes_per_pixel == 2)
                            sample = row_buffer.get_u16();
                        else if (bytes_per_pixel == 4)
                            sample = row_buffer.get_u32();
                        else
                            throw std::runtime_error("This image shouldn't be sampled (not 16/32 bpp)");

                        uint8_t RGBA[4];
                        RGBA[0] = shift_signed_as_byte(sample & idata.masks.r, idata.masks.r_shift, idata.masks.r_bits);
                        RGBA[1] = shift_signed_as_byte(sample & idata.masks.g, idata.masks.g_shift, idata.masks.g_bits);
                        RGBA[2] = shift_signed_as_byte(sample & idata.masks.b, idata.masks.b_shift, idata.masks.b_bits);

                        if (idata.channels == 4)
                        {
                            if (idata.masks.has_alpha())
                                RGBA[3] = shift_signed_as_byte(sample & idata.masks.a, idata.masks.a_shift, idata.masks.a_bits);
                            else
                                RGBA[3] = 255;
                        }

                        // since we could be forcing a different number of channels 
                        // we have to account for that
                        auto pixel_offset = idata.channels * (j / bytes_per_pixel);

                        size_t row_offset;

                        // flipped meaning stored top to bottom
                        if (idata.flipped)
                            row_offset = idata.width * (i - 1) * idata.channels;
                        else
                        {
                            row_offset = idata.width * i * idata.channels;
                            row_offset = to.size() - row_offset;
                        }

                        auto total_offset = row_offset + pixel_offset;

                        memcpy_s(
                            to.data() + total_offset,
                            to.size() - total_offset,
                            RGBA, idata.channels
                        );
                    }
                }
            }
            catch (const std::exception& ex)
            {
                XIL_UNUSED(ex);
                result = false;
            }

            if (!result)
                to.clear();

            return result;
        }

        static bool load_raw(DataReader& file, bmp_data& idata, ImageData::Container& to)
        {
            bool result = true;

            try {
                uint8_t bytes_per_pixel = idata.bpp / 8;
                uint32_t row_padded = (idata.width * bytes_per_pixel + 3) & (~3);
                to.resize(static_cast<size_t>(idata.channels) * idata.width * idata.height);

                DataReader row_buffer;

                for (size_t i = 1; i < idata.height + 1ull; i++)
                {
                    row_buffer = file.get_subset(row_padded);

                    for (size_t j = 0; j < static_cast<size_t>(idata.width) * bytes_per_pixel; j += bytes_per_pixel)
                    {
                        // assert here to get rid of the warning
                        assert(j + 2 < row_padded);

                        uint8_t RGB[4]{};

                        RGB[2] = row_buffer.get_u8();
                        RGB[1] = row_buffer.get_u8();
                        RGB[0] = row_buffer.get_u8();
                        if (idata.channels >= 4)
                            RGB[3] = row_buffer.get_u8();

                        // since we could be forcing a different number of channels 
                        // we have to account for that
                        auto pixel_offset = idata.channels * (j / bytes_per_pixel);

                        size_t row_offset;

                        // flipped meaning stored top to bottom
                        if (idata.flipped)
                            row_offset = idata.width * (i - 1) * idata.channels;
                        else
                        {
                            row_offset = idata.width * i * idata.channels;
                            row_offset = to.size() - row_offset;
                        }

                        auto total_offset = row_offset + pixel_offset;

                        memcpy_s(
                            to.data() + total_offset,
                            to.size() - total_offset,
                            RGB, idata.channels
                        );
                    }
                }
            }
            catch (const std::exception& ex)
            {
                XIL_UNUSED(ex);
                result = false;
            }

            if (!result)
                to.clear();

            return result;
        }
    };
}
