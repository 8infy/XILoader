#pragma once

#include "utils.h"
#include "file_reader.h"
#include "image.h"
#include "bmp.h"
#include "png.h"

namespace XIL {

    class Loader
    {
    private:
        enum class FileFormat
        {
            UNKNOWN = 0,
            BMP     = 1,
            PNG     = 2,
            JPEG    = 3
        };
    public:
        static Image load(const std::string& path, bool flip = false)
        {
            Image image;
            DataReader file;

            if (read_file(path, file))
                load_file(file, image, flip);

            return image;
        }

        static Image load_raw(void* data, size_t size, bool flip = false)
        {
            Image image;
            DataReader data_viewer(data, size);

            load_file(data_viewer, image, flip);

            return image;
        }
    private:
        static void load_file(DataReader& file, Image& image, bool flip)
        {
            switch (deduce_file_format(file))
            {
            case FileFormat::BMP:
                BMP::load(file, image, flip);
                break;
            case FileFormat::PNG:
                PNG::load(file, image, flip);
                break;
            case FileFormat::JPEG:
                break;
            }
        }

        static FileFormat deduce_file_format(DataReader& file)
        {
            uint8_t magic[4];
            file.peek_n(4, magic);

            if (magic[0] == 'B' &&
                magic[1] == 'M')
                return FileFormat::BMP;

            else if (magic[0] == 0x89 &&
                     magic[1] == 'P'  &&
                     magic[2] == 'N'  &&
                     magic[3] == 'G')
                return FileFormat::PNG;

            else if (magic[0] == 0xff &&
                     magic[1] == 0xd8)
                return FileFormat::JPEG;

            else
                return FileFormat::UNKNOWN;
        }

        Loader() = default;
    };
}

using XILoader = XIL::Loader;
