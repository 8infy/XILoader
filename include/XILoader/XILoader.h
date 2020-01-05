#include <string>
#include <vector>
#include <assert.h>

#ifdef _WIN32
    #define XIL_READ_EXACTLY(bytes, dest, dest_size, file) \
    (bytes == fread_s(dest, dest_size, sizeof(uint8_t), bytes, file))
#else
    #define XIL_READ_EXACTLY(bytes, dest, dest_size, file) \
    (bytes == fread(dest, sizeof(uint8_t), bytes, file))
#endif

#define BIT(x) (1 << (x))
#define XIL_UNUSED(var) (void)var

template<typename ConT>
struct basic_XImageData
{
public:
    using Container = ConT;

    basic_XImageData()
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
        pixel_loc *= m_Image.channels;

        return &m_Image.data[pixel_loc];
    }

    uint8_t* operator[](uint16_t y)
    {
        return m_Image.channels ? at_y(y) : nullptr;
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

    XImageViewer at_x(size_t x)
    {
        return XImageViewer(m_Image, x);
    }

    XImageViewer operator[](size_t x)
    {
        return at_x(x);
    }
};

class FileController
{
private:
    FILE* m_File;
    size_t m_BytesRead;
public:
    FileController(const std::string& path)
        : m_File(nullptr), m_BytesRead(0)
    {
        fopen_s(&m_File, path.c_str(), "rb");
    }

    FileController(FileController&& other) noexcept
    {
        std::swap(m_File, other.m_File);
        std::swap(m_BytesRead, other.m_BytesRead);
    }

    FileController& operator=(FileController&& other) noexcept
    {
        std::swap(m_File, other.m_File);
        std::swap(m_BytesRead, other.m_BytesRead);
    }

    bool ok()
    {
        return m_File && !ferror(m_File);
    }

    operator bool()
    {
        return ok();
    }

    uint8_t get_u8()
    {
        uint8_t out;
        if (!ok() || !XIL_READ_EXACTLY(sizeof(uint8_t), &out, sizeof(out), m_File))
            throw std::runtime_error("Failed to extract uint8 from file");

        m_BytesRead += sizeof(uint8_t);

        return out;
    }

    uint16_t get_u16()
    {
        uint16_t out;
        if (!ok() || !XIL_READ_EXACTLY(sizeof(uint16_t), &out, sizeof(out), m_File))
            throw std::runtime_error("Failed to extract uint16 from file");

        m_BytesRead += sizeof(uint16_t);

        return out;
    }

    uint32_t get_u32()
    {
        uint32_t out;
        if (!ok() || !XIL_READ_EXACTLY(sizeof(uint32_t), &out, sizeof(out), m_File))
            throw std::runtime_error("Failed to extract uint32 from file");

        m_BytesRead += sizeof(uint32_t);

        return out;
    }

    int32_t get_i32()
    {
        int32_t out;
        if (!ok() || !XIL_READ_EXACTLY(sizeof(int32_t), &out, sizeof(out), m_File))
            throw std::runtime_error("Failed to extract int32 from file");

        m_BytesRead += sizeof(int32_t);

        return out;
    }

    void get_n(size_t bytes, uint8_t* to)
    {
        if (!XIL_READ_EXACTLY(bytes, to, bytes, m_File))
            throw std::runtime_error("Failed to read from file");

        m_BytesRead += bytes;
    }

    void skip_n(size_t bytes)
    {
        if (!ok() || fseek(m_File, static_cast<uint32_t>(bytes), SEEK_CUR))
        {
            std::string ex = "Failed to skip ";
            ex += std::to_string(bytes);
            ex += " bytes";
            throw std::runtime_error(ex);
        }

        m_BytesRead += bytes;
    }

    void peek_n(size_t bytes, uint8_t* to)
    {
        if (!ok())
            throw std::runtime_error("Failed to get a char from file");

        for (size_t i = 0; i < bytes; i++)
        {
            uint8_t val = fgetc(m_File);
            if (val == EOF)
                throw std::runtime_error("Failed to get a char from file");
           to[i] = val;
        }

        for (size_t i = 0; i < bytes; i++)
        {
            if (ungetc(to[bytes - i - 1], m_File) == EOF)
                throw std::runtime_error("Failed to unget a char from file");
        }
    }

    size_t bytes_read()
    {
        return m_BytesRead;
    }

    ~FileController()
    {
        if (m_File) fclose(m_File);
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
    static XImage load(const std::string& path, bool flip = false)
    {
        XImage image;

        FileController file(path);

        if (!file)
            return image;

        switch (deduce_file_format(file))
        {
        case FileFormat::BMP:
            BMP::load(file, image, flip);
            break;
        }

        return image;
    }
private:
    static FileFormat deduce_file_format(FileController& file)
    {
        uint8_t magic[2];
        file.peek_n(2, magic);

        if (magic[0] == 'B' && magic[1] == 'M')
            return FileFormat::BMP;
        else
            return FileFormat::UNKNOWN;
    }

    class BMP
    {
        struct RGBA_MASK
        {
            uint32_t r;
            uint32_t g;
            uint32_t b;
            uint32_t a;

            bool has_alpha() { return a; }
        };

        struct BMP_DATA {
            BMP_DATA() = default;

            uint32_t pao;
            uint32_t dib_size;
            bool flipped;
            uint8_t* palette;
            uint32_t compression_method;
            uint32_t colors;

            // bytes per color
            // can be 3 or 4 (99% of the time its 4)
            uint16_t bpc;
            uint16_t bpp;
            uint8_t channels;
            uint16_t width;
            uint16_t height;
            RGBA_MASK masks;

            bool has_palette() { return colors; }
            bool has_rgba_mask() { return masks.a | masks.r | masks.g | masks.b; }

            ~BMP_DATA() { delete[] palette; }
        };
    public:
        static void load(FileController& file, XImage& image, bool force_flip)
        {
            BMP_DATA idata{};

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
                    idata.dib_size == 16)
                {
                    idata.width  = file.get_u16();
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
                if (idata.compression_method &&
                   ((idata.compression_method != 3) &&
                    (idata.compression_method != 6)
                   ))
                    return;

                // OS22XBITMAPHEADER: Huffman 1D
                if ((idata.compression_method == 3) && ((idata.dib_size == 16) || (idata.dib_size == 64)))
                    return;

                if (idata.has_palette())
                {
                    if (idata.dib_size > 12)
                    {
                        idata.palette = new uint8_t[idata.colors * sizeof(uint32_t)];
                        idata.bpc = sizeof(uint32_t);
                    }
                    else
                    {
                        idata.palette = new uint8_t[idata.colors * 3ull];
                        idata.bpc = 3;
                    }
                }

                // BITMAPINFOHEADER stores this after the dib
                if ((idata.compression_method == 3) || (idata.compression_method == 6))
                {
                    idata.masks.r = file.get_u32();
                    idata.masks.g = file.get_u32();
                    idata.masks.b = file.get_u32();

                    if ((idata.compression_method == 6) || (idata.dib_size >= 56))
                        idata.masks.a = file.get_u32();
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
                        file.get_n(idata.colors * sizeof(uint32_t), idata.palette);
                    else // OS21X stores colors as 24-bit RGB
                        file.get_n(idata.colors * 3ull, idata.palette);

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
                else if (idata.bpp == 32)
                {
                    // if RGBA mask is specified we have 4 channels
                    // STBI automatically assumes that all 32bpp BMPS are RGBA
                    // so maybe do the same?
                    if (idata.has_rgba_mask() && idata.masks.has_alpha())
                        idata.channels = 4;
                    else
                        idata.channels = 3;
                }

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
        static bool load_pixel_array(FileController& file, BMP_DATA& image_data, XImageData::Container& to)
        {
            if (image_data.has_palette())
                return load_indexed(file, image_data, to);
            else if (image_data.has_rgba_mask())
                return load_sampled(file, image_data, to);
            else
                return load_raw(file, image_data, to);
        }

        static bool load_indexed(FileController& file, BMP_DATA& idata, XImageData::Container& to)
        {
            bool result = true;
            uint8_t* row_buffer = nullptr;

            try
            {
                uint32_t row_padded = (int)(ceil(idata.width / (8.0 / idata.bpp)) + 3) & (~3);
                to.resize(3ull * idata.width * idata.height);

                row_buffer = new uint8_t[row_padded];

                // current Y of the image
                for (size_t i = 1; i < idata.height + 1ull; i++)
                {
                    file.get_n(row_padded, row_buffer);

                    size_t pixel_count = 0;
                    // Perhaps theres a prettier way to do this:
                    // e.g get rid of the 2 loops and calculate
                    // the pixel using the % operator and pixel_count.

                    // current byte
                    for (size_t j = 0;; j++)
                    {
                        // current bit of the byte
                        for (int8_t pixel = 8 - idata.bpp; pixel >= 0; pixel -= idata.bpp)
                        {
                            pixel_count++;

                            uint8_t RGB[3];

                            uint8_t palette_index = (row_buffer[j] >> pixel) & (BIT(idata.bpp) - 1);

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

        static bool load_sampled(FileController& file, BMP_DATA& idata, XImageData::Container& to)
        {
            return true;
        }

        static bool load_raw(FileController& file, BMP_DATA& idata, XImageData::Container& to)
        {
            bool result = true;
            uint8_t* row_buffer = nullptr;

            try {
                uint8_t bytes_per_pixel = idata.bpp / 8;
                uint32_t row_padded = (idata.width * bytes_per_pixel + 3) & (~3);
                to.resize(static_cast<size_t>(idata.channels) * idata.width * idata.height);

                uint8_t tmp;
                row_buffer = new uint8_t[row_padded];

                for (size_t i = 1; i < idata.height + 1ull; i++)
                {
                    file.get_n(row_padded, row_buffer);

                    for (size_t j = 0; j < static_cast<size_t>(idata.width) * bytes_per_pixel; j += bytes_per_pixel)
                    {
                        // assert here to get rid of the warning
                        assert(j + 2 < row_padded);

                        tmp = row_buffer[j];
                        row_buffer[j] = row_buffer[j + 2];
                        row_buffer[j + 2] = tmp;

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
                            row_buffer + j, idata.channels
                        );
                    }
                }

                delete[] row_buffer;
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

    XILoader() = default;
};
