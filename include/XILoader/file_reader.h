#pragma once

#include <string>

#include "utils.h"

namespace XIL {

    class DataReader
    {
    private:
        void* m_Data;
        size_t m_Size;
        size_t m_BytesRead;
        bool m_Owner;
    public:
        DataReader() noexcept
            : m_Data(nullptr),
            m_Size(0),
            m_BytesRead(0),
            m_Owner(false)
        {
        }

        DataReader(void* data, size_t size, bool delete_after = false) noexcept
            : m_Data(data),
            m_Size(size),
            m_BytesRead(0),
            m_Owner(delete_after)
        {
        }

        DataReader(const DataReader& other) = delete;
        DataReader& operator=(const DataReader& other) = delete;

        DataReader(DataReader&& other) noexcept
            : DataReader()
        {
            std::swap(m_Data, other.m_Data);
            std::swap(m_Size, other.m_Size);
            std::swap(m_BytesRead, other.m_BytesRead);
            std::swap(m_Owner, other.m_Owner);
        }

        DataReader& operator=(DataReader&& other) noexcept
        {
            std::swap(m_Data, other.m_Data);
            std::swap(m_Size, other.m_Size);
            std::swap(m_BytesRead, other.m_BytesRead);
            std::swap(m_Owner, other.m_Owner);
        }

        void init_with(void* data, size_t size, bool delete_after = false)
        {
            delete_if_owner();

            m_Data = data;
            m_Size = size;
            m_Owner = delete_after;
        }

        bool has_atleast(size_t desired) const
        {
            return m_Size - m_BytesRead >= desired;
        }

        bool has_ownership() const
        {
            return m_Owner;
        }

        size_t bytes_left() const
        {
            return m_Size - m_BytesRead;
        }

        void revoke_ownership()
        {
            m_Owner = false;
        }

        uint8_t get_u8()
        {
            uint8_t out;

            get_n(sizeof(uint8_t), &out);

            return out;
        }

        uint16_t get_u16()
        {
            uint16_t out;

            get_n(sizeof(uint16_t), &out);

            if constexpr (host_endiannes() == byte_order::BIG)
                out = XIL_U16_SWAP(out);

            return out;
        }

        uint32_t get_u32()
        {
            uint32_t out;

            get_n(sizeof(uint32_t), &out);

            if constexpr (host_endiannes() == byte_order::BIG)
                out = XIL_U32_SWAP(out);

            return out;
        }

        uint32_t get_u32_big()
        {
            uint32_t out;

            get_n(sizeof(uint32_t), &out);

            if constexpr (host_endiannes() == byte_order::LITTLE)
                out = XIL_U32_SWAP(out);

            return out;
        }

        int32_t get_i32()
        {
            uint32_t out;

            get_n(sizeof(uint32_t), &out);

            if constexpr (host_endiannes() == byte_order::BIG)
                out = XIL_U32_SWAP(out);

            // This can potentially break on some machines?
            return static_cast<int32_t>(out);
        }

        void get_n(size_t bytes, void* to)
        {
            if (!has_atleast(bytes))
                throw std::runtime_error("Buffer overflow");

            XIL_MEMCPY(to, bytes, cursor(), bytes);

            m_BytesRead += bytes;
        }

        void skip_n(size_t bytes)
        {
            if (!has_atleast(bytes))
                throw std::runtime_error("Buffer overflow");

            m_BytesRead += bytes;
        }

        void peek_n(size_t bytes, uint8_t* to) const
        {
            XIL_MEMCPY(to, bytes, cursor(), bytes);
        }

        size_t bytes_read() const
        {
            return m_BytesRead;
        }

        uint8_t get_bit(uint8_t index) const
        {
            if (index > 7)
                throw std::runtime_error("A byte is 8 bits wide [0...7] range (got a larger value)");

            uint8_t byte;
            peek_n(sizeof(uint8_t), &byte);

            return (byte >> index) & XIL_BIT(0);
        }

        uint8_t get_bits(uint8_t offset, uint8_t count) const
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

        ~DataReader()
        {
            delete_if_owner();
        }

    private:
        void delete_if_owner()
        {
            if (m_Owner)
                delete[] m_Data;
        }

        void* cursor() const
        {
            return static_cast<uint8_t*>(m_Data) + m_BytesRead;
        }
    };

    inline bool read_file(const std::string& path, DataReader& into)
    {
        FILE* file;
        XIL_OPEN_FILE(file, path);

        if (!file) return false;

        fseek(file, 0, SEEK_END);
        size_t fsize = ftell(file);
        rewind(file);

        uint8_t* data = new uint8_t[fsize];
        if (!XIL_READ_EXACTLY(fsize, data, fsize, file))
        {
            delete[] data;
            return false;
        }

        into.init_with(data, fsize, true);

        return true;
    }
}
