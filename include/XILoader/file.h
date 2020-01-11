#pragma once

#include <string>

#include "utils.h"

namespace XIL {

    class File
    {
    private:
        FILE* m_File;
        size_t m_BytesRead;
    public:
        File(const std::string& path)
            : m_File(nullptr), m_BytesRead(0)
        {
            XIL_OPEN_FILE(m_File, path);
        }

        File(File&& other) noexcept
        {
            std::swap(m_File, other.m_File);
            std::swap(m_BytesRead, other.m_BytesRead);
        }

        File& operator=(File&& other) noexcept
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

            if constexpr (host_endiannes() == byte_order::BIG)
                out = XIL_U16_SWAP(out);

            return out;
        }

        uint32_t get_u32()
        {
            uint32_t out;
            if (!ok() || !XIL_READ_EXACTLY(sizeof(uint32_t), &out, sizeof(out), m_File))
                throw std::runtime_error("Failed to extract uint32 from file");

            m_BytesRead += sizeof(uint32_t);

            if constexpr (host_endiannes() == byte_order::BIG)
                out = XIL_U32_SWAP(out);

            return out;
        }

        uint32_t get_u32_big()
        {
            uint32_t out;
            if (!ok() || !XIL_READ_EXACTLY(sizeof(uint32_t), &out, sizeof(out), m_File))
                throw std::runtime_error("Failed to extract uint32 from file");

            m_BytesRead += sizeof(uint32_t);

            if constexpr (host_endiannes() == byte_order::LITTLE)
                out = XIL_U32_SWAP(out);

            return out;
        }

        int32_t get_i32()
        {
            uint32_t data;

            if (!ok() || !XIL_READ_EXACTLY(sizeof(uint32_t), &data, sizeof(data), m_File))
                throw std::runtime_error("Failed to extract int32 from file");

            m_BytesRead += sizeof(uint32_t);

            if constexpr (host_endiannes() == byte_order::BIG)
                data = XIL_U32_SWAP(data);

            return static_cast<int32_t>(data);
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

        uint8_t get_bit(uint8_t index)
        {
            if (index > 7)
                throw std::runtime_error("A byte is 8 bits wide [0...7] range (got a larger value)");

            uint8_t byte;
            peek_n(sizeof(uint8_t), &byte);

            return (byte >> index) & XIL_BIT(0);
        }

        uint8_t get_bits(uint8_t offset, uint8_t count)
        {
            if ((offset + count) > 8)
                throw std::runtime_error("A byte is 8 bits wide [0...7] range (got a larger value)");

            uint8_t byte;
            peek_n(sizeof(uint8_t), &byte);

            return (byte >> offset) & XIL_BITS(count);
        }

        void next_byte()
        {
            skip_n(1);
        }

        ~File()
        {
            if (m_File) fclose(m_File);
        }
    };
}
