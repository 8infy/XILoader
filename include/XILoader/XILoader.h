#pragma once

#include "utils.h"
#include "data_stream.h"
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
        Loader() = delete;
        Loader(const Loader&) = delete;
        Loader(Loader&&) = delete;

        static Image load(const std::string& path, bool flip = false)
        {
            try {
                return load_verbose(path, flip);
            }
            catch (const std::exception&) // suppress any exceptions
            {
                return {};
            }
        }

        static Image load_raw(void* data, size_t size, bool flip = false)
        {
            try {
                return load_raw_verbose(data, size, flip);
            }
            catch (const std::exception&) // suppress any exceptions
            {
                return {};
            }
        }

        // Any exceptions encountered during the process of loading are rethrown to the caller
        static Image load_verbose(const std::string& path, bool flip = false)
        {
            Image image;
            DataStream file_stream;

            read_file(path, file_stream);
            load_image(file_stream, image, flip);

            return image;
        }

        // Any exceptions encountered during the process of loading are rethrown to the caller
        static Image load_raw_verbose(void* data, size_t size, bool flip = false)
        {
            Image image;
            DataStream data_stream(data, size);

            load_image(data_stream, image, flip);

            return image;
        }
    private:
        static void load_image(DataStream& file, Image& image, bool flip)
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
                throw std::runtime_error("JPEG loading is not yet implemented");
            default:
                throw std::runtime_error("Unknown image format");
            }
        }

        static FileFormat deduce_file_format(DataStream& file)
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
    };
}

using XILoader = XIL::Loader;
using XImage   = XIL::Image;
