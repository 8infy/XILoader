#pragma once

#include <vector>

#include "utils.h"

namespace XIL {

    template<typename ConT>
    struct basic_ImageData
    {
    public:
        using Container = ConT;

        basic_ImageData()
            : width(0),
            height(0),
            channels(0)
        {
        }

        Container data;
        uint16_t  width;
        uint16_t  height;
        uint8_t   channels;
    };
    using ImageData = basic_ImageData<std::vector<uint8_t>>;

    class ImageViewer
    {
    private:
        ImageData& m_Image;
        size_t m_AtX;
    public:
        ImageViewer(ImageData& image, size_t x)
            : m_Image(image), m_AtX(x)
        {
        }
    public:
        uint8_t* at_y(uint16_t y)
        {
            size_t pixel_loc = static_cast<size_t>(m_Image.width) * y;
            pixel_loc += m_AtX;
            pixel_loc *= m_Image.channels;

            return &m_Image.data[pixel_loc];
        }

        uint8_t* operator[](uint16_t y)
        {
            return m_Image.channels ? at_y(y) : nullptr;
        }

        operator uint8_t& ()
        {
            return m_Image.data[m_AtX];
        }
    };

    class Image
    {
    public:
        enum Format
        {
            UNKNOWN = 0,
            RGB = 3,
            RGBA = 4
        };

        friend class BMP;
    private:
        ImageData m_Image;
    public:
        Image() = default;
        Image(Image&& other) = default;
        Image& operator=(Image&& other) = default;
        Image(const Image& other) = delete;
        Image& operator=(const Image& other) = delete;
    public:
        uint8_t* data()
        {
            return ok() ? m_Image.data.data() : nullptr;
        }

        Format channels() const
        {
            return static_cast<Format>(m_Image.channels);
        }

        bool ok() const
        {
            return m_Image.width && m_Image.height;
        }

        operator bool() const
        {
            return ok();
        }

        uint16_t width() const
        {
            return m_Image.width;
        }

        uint16_t height() const
        {
            return m_Image.height;
        }

        ImageViewer at_x(size_t x)
        {
            return ImageViewer(m_Image, x);
        }

        ImageViewer operator[](size_t x)
        {
            return at_x(x);
        }

        #if defined(__gl_h_) || defined(__GL_H__)
        decltype(GL_RGB) gl_format()
        {
            return channels() == 3 ? GL_RGB : GL_RGBA;
        }
        #endif
    };
}
