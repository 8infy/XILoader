#include <string>
#include <vector>
#include <assert.h>

#ifdef _WIN32
    #define XIL_READ_EXACTLY(bytes, dest, dest_size, file) (bytes == fread_s(dest, dest_size, sizeof(uint8_t), bytes, file))
#else
    #define XIL_READ_EXACTLY(bytes, dest, dest_size, file) (bytes == fread(dest, sizeof(uint8_t), bytes, file))
#endif

template<typename ConT>
struct basic_XImageData
{
private:
    friend class XILoader;
    friend class XImage;
    friend class XImageViewer;
private:
    basic_XImageData()
        : width(0),
        height(0),
        components(0)
    {
    }

    ConT     data;
    uint16_t width;
    uint16_t height;
    uint8_t  components;
};
using XImageData = basic_XImageData<std::vector<uint8_t>>;

class XImageViewer
{
friend class XImage;
private:
    XImageData& m_Image;
    uint16_t m_AtX;

    XImageViewer(XImageData& image, uint16_t x)
        : m_Image(image), m_AtX(x)
    {
    }
public:
    uint8_t* at_y(uint16_t y)
    {
        auto pixel_loc = m_Image.width * y;
        pixel_loc += m_AtX;
        pixel_loc *= m_Image.components;

        return &m_Image.data[pixel_loc];
    }

    uint8_t* operator[](uint16_t ycoord)
    {
        return m_Image.components ? at_y(ycoord) : nullptr;
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
    XImage() = default;
private:
    XImageData m_Image;
public:
    uint8_t* data()
    {
        return ok() ? m_Image.data.data() : nullptr;
    }

    Format format() const
    {
        return static_cast<Format>(m_Image.components);
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
        return m_Image.width;
    }

    uint16_t height() const
    {
        return m_Image.height;
    }

    XImageViewer at_x(uint16_t x)
    {
        return XImageViewer(m_Image, x);
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
            XImageData imdata;

            uint8_t dib[36];
            if (!XIL_READ_EXACTLY(36, dib, 36, file)) return;

            // width/height
            imdata.width = *reinterpret_cast<int32_t*>(&dib[0]);
            imdata.height  = abs(*reinterpret_cast<int32_t*>(&dib[4]));

            // number of color planes
            if (*reinterpret_cast<uint16_t*>(&dib[8]) != 1) return;

            // bits per pixel
            uint16_t bpp = *reinterpret_cast<uint16_t*>(&dib[10]);

            imdata.components = XImage::RGB;

            uint32_t compression_method = *reinterpret_cast<uint32_t*>(&dib[12]);
            // no compressed bmp support for now
            if (compression_method) return;

            // skip N bytes to get to the pixel array
            auto pixel_array_gap = pao - 54;
            if (pixel_array_gap) fseek(file, pixel_array_gap, SEEK_CUR);

            if (load_pixel_array(file, imdata, bpp))
                image.m_Image = std::move(imdata);
        }

        static bool load_pixel_array(FILE* file, XImageData& idata, uint16_t bpp) 
        {
            switch (bpp)
            {
            case 24:
                return load_pixel_array_as<24>(file, idata);
            default:
                return false;
            }
        }

        template<uint16_t bpp>
        static bool load_pixel_array_as(FILE* file, XImageData& idata)
        {
            return false;
        }

        template<>
        static bool load_pixel_array_as<24>(FILE* file, XImageData& idata) 
        {
            bool result = true;

            int32_t row_padded = (idata.width * 3 + 3) & (~3);
            idata.data.resize(3ull * idata.width * idata.height);

            uint8_t tmp;
            uint8_t* row_buffer = new uint8_t[row_padded];

            for (size_t i = 0; i < idata.height; i++)
            {
                if (!XIL_READ_EXACTLY(row_padded, row_buffer, row_padded, file))
                {
                    result = false;
                    break;
                }

                for (int j = 0; j < idata.width * 3; j += 3)
                {
                    // assert here to get rid of the warning
                    assert(j + 2 < row_padded);

                    tmp = row_buffer[j];
                    row_buffer[j] = row_buffer[j + 2];
                    row_buffer[j + 2] = tmp;

                    auto row_offset = idata.data.size() - idata.width * (i + 1) * 3;
                    auto total_offset = row_offset + j;

                    memcpy_s(
                        idata.data.data() + total_offset,
                        idata.data.size() - total_offset,
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