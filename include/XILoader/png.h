#pragma once

#include "image.h"
#include "file.h"

namespace XIL {

    class PNG
    {
    private:
        struct CHUNK
        {
            uint32_t length;
            uint8_t type[4];
            std::vector<uint8_t> data;
            uint32_t crc;
        };

        struct IMAGE_DATA
        {
            uint32_t width;
            uint32_t height;
            uint8_t bit_depth;
            uint8_t color_type;
            uint8_t compression_method;
            uint8_t filter_method;
            uint8_t interlace_method;
        };
    public:
        static void load(File& file, Image& image, bool force_flip)
        {
            CHUNK chnk{};
            IMAGE_DATA idata{};

            try {
                // skip file signature
                file.skip_n(8);

                for (;;)
                {
                    read_chunk(file, chnk);

                    if (is_iend(chnk)) return;
                    if (is_ancillary(chnk)) continue;

                    if (is_ihdr(chnk))
                    {
                        read_header(chnk, idata);
                        continue;
                    }
                }
            }
            catch (const std::exception& ex)
            {
                XIL_UNUSED(ex);
                return;
            }
        }
    private:
        static void read_chunk(File& file, CHUNK& into)
        {
            into.length = file.get_u32_big();
            into.data.resize(into.length);

            file.get_n(4, into.type);
            file.get_n(into.length, into.data.data());

            into.crc = file.get_u32_big();
        }

        static void read_header(const CHUNK& from, IMAGE_DATA& into)
        {
            if constexpr (host_endiannes() == byte_order::LITTLE)
            {
                into.width = XIL_U32_SWAP(*reinterpret_cast<const uint32_t*>(&from.data[0]));
                into.height = XIL_U32_SWAP(*reinterpret_cast<const uint32_t*>(&from.data[4]));
            }
            else
            {
                into.width  = *reinterpret_cast<const uint32_t*>(&from.data[0]);
                into.height = *reinterpret_cast<const uint32_t*>(&from.data[4]);
            }

            into.bit_depth          = from.data[8];
            into.color_type         = from.data[9];
            into.compression_method = from.data[10];
            into.filter_method      = from.data[11];
            into.interlace_method   = from.data[12];
        }

        static bool is_ancillary(const CHUNK& chnk)
        {
            return islower(chnk.type[0]);
        }

        static bool is_iend(const CHUNK& chnk)
        {
            return (chnk.type[0] == 'I') &&
                   (chnk.type[1] == 'E') &&
                   (chnk.type[2] == 'N') &&
                   (chnk.type[3] == 'D');
        }

        static bool is_ihdr(const CHUNK& chnk)
        {
            return (chnk.type[0] == 'I') &&
                   (chnk.type[1] == 'H') &&
                   (chnk.type[2] == 'D') &&
                   (chnk.type[3] == 'R');
        }
    };
}