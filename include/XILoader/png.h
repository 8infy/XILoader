#pragma once

#include "image.h"
#include "file_reader.h"

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
        static void load(DataReader& file, Image& image, bool force_flip)
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
        static void read_chunk(DataReader& file, CHUNK& into)
        {
            into.length = file.get_u32_big();
            into.data.resize(into.length);

            file.get_n(4, into.type);
            file.get_n(into.length, into.data.data());

            into.crc = file.get_u32_big();
        }

        static void read_header(CHUNK& from, IMAGE_DATA& into)
        {
            // TODO: get rid of the STL stuff...
            auto data = DataReader(from.data.data(), from.data.size());

            into.width = data.get_u32_big();
            into.height = data.get_u32_big();

            into.bit_depth          = data.get_u8();
            into.color_type         = data.get_u8();
            into.compression_method = data.get_u8();
            into.filter_method      = data.get_u8();
            into.interlace_method   = data.get_u8();
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