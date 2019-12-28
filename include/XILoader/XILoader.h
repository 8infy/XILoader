#include <string>
#include <vector>

struct XRGBPixel
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;

    operator uint8_t*()
    {
        return &r;
    }
};

struct XRGBAPixel
{
    uint8_t r;
    uint8_t g;
    uint8_t b;

    operator uint8_t*()
    {
        return &r;
    }
};

struct XImageViewer
{
    std::vector<uint8_t>& m_Data;
    uint16_t m_RequestedX;

    XImageViewer(std::vector<uint8_t>& data, uint16_t xcoord)
        : m_Data(data), m_RequestedX(xcoord)
    {
    }

    XRGBPixel y(uint16_t ycoord)
    {
        return { m_Data[0], m_Data[1], m_Data[3] };
    }

    XRGBPixel operator[](uint16_t ycoord)
    {
        return y(ycoord);
    }

    operator uint8_t*()
    {
        return &m_Data[0];
    }
};

struct XImage
{
public:
    enum Format
    {
        UNKNOWN   = 0,
        RGB       = 3,
        RGBA      = 4
    };

    friend class XILoader;
private:
    XImage() = default;
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

    XImageViewer operator[](uint16_t xcoord)
    {
        return XImageViewer(m_Data, xcoord);
    }
};

class XILoader
{
private:
    enum class ImageFormat
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
        case ImageFormat::BMP:
            BMP::load(file, image);
            break;
        }

        fclose(file);

        return image;
    }
private:
    static ImageFormat deduce_file_format(FILE* file)
    {
        auto h1 = fgetc(file);
        auto h2 = fgetc(file);
        ungetc(h2, file);
        ungetc(h1, file);

        if (h1 == 'B' && h2 == 'M')
            return ImageFormat::BMP;
        else
            return ImageFormat::UNKNOWN;
    }

    class BMP
    {
    public:
        static void load(FILE* file, XImage& image)
        {
            image.m_Format = XImage::RGB;

            uint8_t info[54];
            fread(info, sizeof(uint8_t), 54, file);

            image.m_Width = *reinterpret_cast<int32_t*>(&info[18]);
            image.m_Height = abs(*reinterpret_cast<int32_t*>(&info[22]));

            int32_t row_padded = (image.m_Width * 3 + 3) & (~3);
            image.m_Data.resize(3ull * image.m_Width * image.m_Height);

            uint8_t tmp;
            uint8_t* row_buffer = new uint8_t[row_padded];

            for (size_t i = 0; i < image.m_Height; i++)
            {
                fread(row_buffer, sizeof(uint8_t), row_padded, file);
                for (int j = 0; j < image.m_Width * 3; j += 3)
                {
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
        }
    };

    XILoader() = default;
};