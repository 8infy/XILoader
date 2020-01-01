#include <string>
#include <vector>
#include <assert.h>

#ifdef _WIN32
    #define XIL_READ_EXACTLY(bytes, dest, dest_size, file) (bytes == fread_s(dest, dest_size, sizeof(uint8_t), bytes, file))
#else
    #define XIL_READ_EXACTLY(bytes, dest, dest_size, file) (bytes == fread(dest, sizeof(uint8_t), bytes, file))
#endif

struct XImageViewer
{
public:
    friend class XImage;
private:
    std::vector<uint8_t>& m_Data;
    uint16_t m_RequestedX;
    uint16_t m_Width;
    uint8_t m_Components;

    XImageViewer(
        std::vector<uint8_t>& data,
        uint16_t width,
        uint8_t component_count,
        uint16_t xcoord
    )
        : m_Data(data),
        m_RequestedX(xcoord),
        m_Width(width),
        m_Components(component_count)
    {
    }
public:
    uint8_t* at_y(uint16_t y)
    {
        auto pixel_loc = m_Width * y;
        pixel_loc += m_RequestedX;
        pixel_loc *= m_Components;

        return &m_Data[pixel_loc];
    }

    uint8_t* operator[](uint16_t ycoord)
    {
        return at_y(ycoord);
    }
};

class XImage
{
public:
    enum Format
    {
        UNKNOWN   = 0,
        RGB       = 3,
        RGBA      = 4
    };

    friend class XILoader;
public:
    XImage(XImage&& other)                 = default;
    XImage& operator=(XImage&& other)      = default;
    XImage(const XImage& other)            = delete;
    XImage& operator=(const XImage& other) = delete;
private:
    XImage() 
        : m_Width(0),
        m_Height(0),
        m_Format(UNKNOWN)
    {
    }
private:
    std::vector<uint8_t> m_Data;
    uint16_t m_Width;
    uint16_t m_Height;
    Format   m_Format;
public:
    uint8_t* data()
    {
        return ok() ? m_Data.data() : nullptr;
    }

    Format format() const
    {
        return m_Format;
    }

    bool ok() const
    {
        return format();
    }

    operator bool() const
    {
        return ok();
    }

    uint16_t width() const
    {
        return m_Width;
    }

    uint16_t height() const
    {
        return m_Height;
    }

    XImageViewer at_x(uint16_t x)
    {
        return XImageViewer(m_Data, m_Width, m_Format, x);
    }

    XImageViewer operator[](uint16_t xcoord)
    {
        return at_x(xcoord);
    }
};

class XILoader
{
private:
    enum class FileFormat
    {
        UNKNOWN = 0,
        BMP = 1
    };
public:
    static XImage load(const std::string& path)
    {
        XImage image;

        FILE* file;

        fopen_s(&file, path.c_str(), "rb");

        if (!file)
            // TODO: A way to tell what error has occured?
            return image;

        switch (deduce_file_format(file))
        {
        case FileFormat::BMP:
            BMP::load(file, image);
            break;
        }

        fclose(file);

        return image;
    }
private:
    static FileFormat deduce_file_format(FILE* file)
    {
        auto h1 = fgetc(file);
        auto h2 = fgetc(file);
        ungetc(h2, file);
        ungetc(h1, file);

        if (h1 == 'B' && h2 == 'M')
            return FileFormat::BMP;
        else
            return FileFormat::UNKNOWN;
    }

    class BMP
    {
    private:
        enum dib_type
        {
            UNKNOWN           = 0,
            BITMAPCOREHEADER  = 12,
            OS21XBITMAPHEADER = 12,
            BITMAPINFOHEADER  = 40
        };
    public:
        static void load(FILE* file, XImage& image)
        {
            uint8_t header[14];
            if (!XIL_READ_EXACTLY(14, header, 14, file)) return;

            // pixel array offset
            uint32_t pao = *reinterpret_cast<uint32_t*>(&header[10]);

            uint32_t dib_size;
            if (!XIL_READ_EXACTLY(4, &dib_size, 4, file)) return;

            // BMP dib type is distiguished by its size
            switch (dib_size)
            {
            case BITMAPCOREHEADER | OS21XBITMAPHEADER:
                load_as<BITMAPCOREHEADER | OS21XBITMAPHEADER>(file, image, pao);
                break;
            case BITMAPINFOHEADER:
                load_as<BITMAPINFOHEADER>(file, image, pao);
            }
        }
    private:
        template<size_t>
        static void load_as(FILE* file, XImage& image, uint32_t pao) {}

        template<>
        static void load_as<BITMAPCOREHEADER | OS21XBITMAPHEADER>(FILE* file, XImage& image, uint32_t pao)
        {

        }

        template<>
        static void load_as<BITMAPINFOHEADER>(FILE* file, XImage& image, uint32_t pao)
        {
            XImage itempo;

            uint8_t dib[36];
            if (!XIL_READ_EXACTLY(36, dib, 36, file)) return;

            // width/height
            itempo.m_Width = *reinterpret_cast<int32_t*>(&dib[0]);
            itempo.m_Height = abs(*reinterpret_cast<int32_t*>(&dib[4]));

            // number of color planes
            if (*reinterpret_cast<uint16_t*>(&dib[8]) != 1) return;

            // bits per pixel
            uint16_t bpp = *reinterpret_cast<uint16_t*>(&dib[10]);

            itempo.m_Format = XImage::RGB;

            uint32_t compression_method = *reinterpret_cast<uint32_t*>(&dib[12]);
            // no compressed bmp support for now
            if (compression_method) return;

            // skip N bytes to get to the pixel array
            auto pixel_array_gap = pao - 54;
            if (pixel_array_gap) fseek(file, pixel_array_gap, SEEK_CUR);

            if (load_pixel_array(file, itempo, bpp))
                image = std::move(itempo);
        }

        static bool load_pixel_array(FILE* file, XImage& image, uint16_t bpp) 
        {
            switch (bpp)
            {
            case 24:
                return load_pixel_array_as<24>(file, image);
            default:
                return false;
            }
        }

        template<uint16_t bpp>
        static bool load_pixel_array_as(FILE* file, XImage& image)
        {
            return false;
        }

        template<>
        static bool load_pixel_array_as<24>(FILE* file, XImage& image) 
        {
            bool result = true;

            int32_t row_padded = (image.m_Width * 3 + 3) & (~3);
            image.m_Data.resize(3ull * image.m_Width * image.m_Height);

            uint8_t tmp;
            uint8_t* row_buffer = new uint8_t[row_padded];

            for (size_t i = 0; i < image.m_Height; i++)
            {
                if (!XIL_READ_EXACTLY(row_padded, row_buffer, row_padded, file))
                {
                    result = false;
                    break;
                }

                for (int j = 0; j < image.m_Width * 3; j += 3)
                {
                    // assert here to get rid of the warning
                    assert(j + 2 < row_padded);

                    tmp = row_buffer[j];
                    row_buffer[j] = row_buffer[j + 2];
                    row_buffer[j + 2] = tmp;

                    auto row_offset = image.m_Data.size() - image.m_Height * (i + 1) * 3;
                    auto total_offset = row_offset + j;

                    memcpy_s(
                        image.m_Data.data() + total_offset,
                        image.m_Data.size() - total_offset,
                        row_buffer + j, 3
                    );
                }
            }

            delete[] row_buffer;

            return result;
        }
    };

    XILoader() = default;
};