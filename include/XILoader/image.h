#pragma once

#include <vector>
#include <algorithm>

#include "utils.h"

namespace XIL {

    template<typename ElemT, typename ConT>
    struct basic_ImageData
    {
    public:
        using Container = ConT;
        using Element = ElemT;

        basic_ImageData()
            : width(0),
            height(0),
            channels(0)
        {
        }

        Container data;
        size_t    width;
        size_t    height;
        uint8_t   channels;

        const Element* data_ptr() const noexcept { return data.data(); }
              Element* data_ptr()       noexcept { return data.data(); }
    };
    using ImageData = basic_ImageData<uint8_t, std::vector<uint8_t>>;

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
            if (y >= m_Image.height)
                throw std::runtime_error("The 'y' coordinate exceeded image height");

            if (m_AtX >= m_Image.width)
                throw std::runtime_error("The 'x' coordinate exceeded image width");

            size_t pixel_loc = m_Image.width * y;
            pixel_loc += m_AtX ? m_AtX + 1 : m_AtX;
            pixel_loc *= m_Image.channels;

            return &m_Image.data[pixel_loc];
        }

        uint8_t* operator[](uint16_t y)
        {
            return m_Image.channels ? at_y(y) : nullptr;
        }

        operator uint8_t&()
        {
            if (m_AtX > m_Image.data.size())
                throw std::runtime_error("Subscript out of image size range");

            return m_Image.data[m_AtX];
        }
    };

    class Image
    {
    public:
        enum Format
        {
            UNKNOWN = 0,
            RGB     = 3,
            RGBA    = 4
        };

        friend class BMP;
        friend class PNG;
    private:
        ImageData m_Image;
    public:
        Image() = default;
        Image(Image&& other) = default;
        Image& operator=(Image&& other) = default;
        Image(const Image& other) = delete;
        Image& operator=(const Image& other) = delete;
    public:
        uint8_t* data() noexcept
        {
            return ok() ? m_Image.data_ptr() : nullptr;
        }

        const uint8_t* data() const noexcept
        {
            return ok() ? m_Image.data_ptr() : nullptr;
        }

        #ifdef _MSVC_LANG 
            #pragma warning(push)
            #pragma warning(disable:26812) // unscoped enum is intended here
        #endif
        Format channels() const noexcept
        {
            return static_cast<Format>(m_Image.channels);
        }
        #ifdef _MSVC_LANG
            #pragma warning(pop)
        #endif

        bool ok() const noexcept
        {
            return width() && height();
        }

        operator bool() const noexcept
        {
            return ok();
        }

        size_t width() const noexcept
        {
            return m_Image.width;
        }

        size_t height() const noexcept
        {
            return m_Image.height;
        }

        size_t size() const noexcept
        {
            return width() * height() * channels();
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
        auto gl_format() const noexcept
        {
            if (!ok())
                return static_cast<decltype(GL_RGB)>(-1);

            return channels() == 3 ? GL_RGB : GL_RGBA;
        }
        #endif

        void flip()
        {
            if (width() < 2 || !ok()) return;

            for (size_t y = 0; y < height() / 2; y++)
            {
                std::swap_ranges(
                    at_x(0).at_y(y),
                    at_x(width() - 1).at_y(y),
                    at_x(0).at_y(height() - y - 1));
            }
        }
    };
}
