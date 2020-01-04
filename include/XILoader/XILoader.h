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
    size_t m_AtX;

    XImageViewer(XImageData& image, size_t x)
        : m_Image(image), m_AtX(x)
    {
    }
public:
    uint8_t* at_y(uint16_t y)
    {
        size_t pixel_loc = static_cast<size_t>(m_Image.width) * y;
        pixel_loc += m_AtX;
        pixel_loc *= m_Image.components;

        return &m_Image.data[pixel_loc];
    }

    uint8_t* operator[](uint16_t y)
    {
        return m_Image.components ? at_y(y) : nullptr;
    }

    operator uint8_t&()
    {
        return m_Image.data[m_AtX];
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

    XImageViewer at_x(size_t x)
    {
        return XImageViewer(m_Image, x);
    }

    XImageViewer operator[](size_t x)
    {
        return at_x(x);
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
    public:
        static void load(FILE* file, XImage& image)
        {
            uint8_t header[14];
            if (!XIL_READ_EXACTLY(14, header, 14, file)) return;

            // pixel array offset
            uint32_t pao = *reinterpret_cast<uint32_t*>(&header[10]);

            uint32_t dib_size;
            if (!XIL_READ_EXACTLY(4, &dib_size, 4, file)) return;

            XImageData idata;

            if (dib_size < 12 || dib_size > 124)
                // invalid dib
                return;

            // biggest possible dib
            uint8_t dib[124];
            if (!XIL_READ_EXACTLY(dib_size - 4, dib, 124, file)) return;

            // width/height
            idata.width = *reinterpret_cast<int32_t*>(&dib[0]);
            idata.height = abs(*reinterpret_cast<int32_t*>(&dib[4]));

            // number of color planes
            if (*reinterpret_cast<uint16_t*>(&dib[8]) != 1) return;

            // bits per pixel
            uint16_t bpp = *reinterpret_cast<uint16_t*>(&dib[10]);

            if (bpp < 32)
                idata.components = XImage::RGB;
            else if (bpp == 32)
                idata.components = XImage::RGBA;
            else
                // incorrect bpp
                return;


            uint32_t compression_method = *reinterpret_cast<uint32_t*>(&dib[12]);
            // no compressed bmp support for now
            if (compression_method) return;

            uint8_t* palette = nullptr;
            uint32_t colors = 0;

            if (dib_size >= 40)
            {
                colors = *reinterpret_cast<uint32_t*>(&dib[28]);
                if (!colors && bpp <= 8)
                    colors = static_cast<uint32_t>(pow(2, bpp));
                if (colors)
                    palette = new uint8_t[colors * sizeof(uint32_t)];
            }

            if (!palette)
            {
                // skip N bytes to get to the pixel array
                auto pixel_array_gap = pao - dib_size - 14;
                if (pixel_array_gap) fseek(file, pixel_array_gap, SEEK_CUR);
            }
            else
            {
                if (!XIL_READ_EXACTLY(colors * sizeof(uint32_t), palette, colors * sizeof(uint32_t), file))
                    return;
            }

            if (load_pixel_array(file, idata, bpp, palette))
                image.m_Image = std::move(idata);

            delete[] palette;
        }
    private:
        static bool load_pixel_array(FILE* file, XImageData& idata, uint16_t bpp, uint8_t* palette) 
        {
            switch (bpp)
            {
            case 1:
                return load_1bpp(file, idata, palette);
            case 2:
                return load_2bpp(file, idata, palette);
            case 24:
                return load_24bpp(file, idata);
            default:
                return false;
            }
        }

        static bool load_1bpp(FILE* file, XImageData& idata, uint8_t* palette)
        {
            bool result = true;

            int32_t row_padded = (int)(ceil(idata.width / 8.0) + 3) & (~3);
            idata.data.resize(3ull * idata.width * idata.height);

            uint8_t* row_buffer = new uint8_t[row_padded];

            // current Y of the image
            for (size_t i = 0; i < idata.height; i++)
            {
                if (!XIL_READ_EXACTLY(row_padded, row_buffer, row_padded, file))
                {
                    result = false;
                    break;
                }

                size_t pixel_count = 0;
                // Perhaps theres a prettier way to do this:
                // e.g get rid of the 2 loops and calculate
                // the pixel using the % operator and pixel_count.

                // current byte
                for (size_t j = 0;; j++)
                {
                    // current bit of the byte
                    for (int8_t pixel = 7; pixel >= 0; pixel--)
                    {
                        pixel_count++;

                        uint8_t RGB[3];
                        uint8_t palette_index = (row_buffer[j] >> pixel) & 1;

                        RGB[0] = palette[palette_index * 4 + 0];
                        RGB[1] = palette[palette_index * 4 + 1];
                        RGB[2] = palette[palette_index * 4 + 2];

                        // this is super ugly - to be somehow refactored
                        auto row_offset = idata.data.size() - idata.width * (i + 1) * 3;
                        auto total_offset = row_offset + ((pixel_count - 1) * 3);

                        memcpy_s(
                            idata.data.data() + total_offset,
                            idata.data.size() - total_offset,
                            RGB, 3
                        );

                        if (pixel_count == idata.width)
                            break;
                    }
                    if (pixel_count == idata.width)
                        break;
                }
            }

            delete[] row_buffer;

            return result;
        }

        // havent tested this because there isnt a single 2-bpp bmp on the internet
        static bool load_2bpp(FILE* file, XImageData& idata, uint8_t* palette)
        {
            bool result = true;

            int32_t row_padded = (int)(ceil(idata.width / 4.0) + 3) & (~3);
            idata.data.resize(3ull * idata.width * idata.height);

            uint8_t* row_buffer = new uint8_t[row_padded];

            // current Y of the image
            for (size_t i = 0; i < idata.height; i++)
            {
                if (!XIL_READ_EXACTLY(row_padded, row_buffer, row_padded, file))
                {
                    result = false;
                    break;
                }

                size_t pixel_count = 0;
                // Perhaps theres a prettier way to do this:
                // e.g get rid of the 2 loops and calculate
                // the pixel using the % operator and pixel_count.

                // current byte
                for (size_t j = 0;; j++)
                {
                    // current bit of the byte
                    for (int8_t pixel = 6; pixel >= 0; pixel-=2)
                    {
                        pixel_count++;

                        uint8_t RGB[3];
                        uint8_t palette_index = (row_buffer[j] >> pixel) & 3;

                        RGB[0] = palette[palette_index * 4 + 0];
                        RGB[1] = palette[palette_index * 4 + 1];
                        RGB[2] = palette[palette_index * 4 + 2];

                        // this is super ugly - to be somehow refactored
                        auto row_offset = idata.data.size() - idata.width * (i + 1) * 3;
                        auto total_offset = row_offset + ((pixel_count - 1) * 3);

                        memcpy_s(
                            idata.data.data() + total_offset,
                            idata.data.size() - total_offset,
                            RGB, 3
                        );

                        if (pixel_count == idata.width)
                            break;
                    }
                    if (pixel_count == idata.width)
                        break;
                }
            }

            delete[] row_buffer;

            return result;
        }

        static bool load_24bpp(FILE* file, XImageData& idata)
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