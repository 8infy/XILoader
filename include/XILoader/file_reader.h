#pragma once

#include <string>
#include <vector>

#include <assert.h>

#include "utils.h"

namespace XIL {

    class DataReader
    {
        friend class ChunkedBitReader;
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

        DataReader(void* data, size_t size, bool grant_ownership = false) noexcept
            : m_Data(data),
            m_Size(size),
            m_BytesRead(0),
            m_Owner(grant_ownership)
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

            return *this;
        }

        void init_with(void* data, size_t size, bool grant_ownership = false)
        {
            delete_if_owner();

            m_Data = data;
            m_Size = size;
            m_Owner = grant_ownership;
        }

        bool has_atleast(size_t bytes) const
        {
            return m_Size - m_BytesRead >= bytes;
        }

        size_t bytes_left() const
        {
            return m_Size - m_BytesRead;
        }

        bool has_ownership() const
        {
            return m_Owner;
        }

        void grant_ownership()
        {
            m_Owner = true;
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

            if XIL_CONSTEXPR (host_endiannes() == byte_order::BIG)
                out = XIL_U16_SWAP(out);

            return out;
        }

        uint32_t get_u32()
        {
            uint32_t out;

            get_n(sizeof(uint32_t), &out);

            if XIL_CONSTEXPR (host_endiannes() == byte_order::BIG)
                out = XIL_U32_SWAP(out);

            return out;
        }

        uint32_t get_u32_big()
        {
            uint32_t out;

            get_n(sizeof(uint32_t), &out);

            if XIL_CONSTEXPR (host_endiannes() == byte_order::LITTLE)
                out = XIL_U32_SWAP(out);

            return out;
        }

        int32_t get_i32()
        {
            uint32_t out;

            get_n(sizeof(uint32_t), &out);

            if XIL_CONSTEXPR (host_endiannes() == byte_order::BIG)
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

        DataReader get_subset(size_t bytes)
        {
            if (bytes > bytes_left())
                throw std::runtime_error("Buffer overflow");

            auto cur = cursor();
            m_BytesRead += bytes;

            return DataReader(cur, bytes);
        }

        void rewind_n(size_t bytes)
        {
            if (m_Size - bytes_left() < bytes)
                throw std::runtime_error("Buffer overflow");

            m_BytesRead -= bytes;
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

    static inline bool read_file(const std::string& path, DataReader& into)
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

    class ChunkedBitReader
    {
    private:
        struct DataChunk
        {
            uint8_t* data;
            size_t size;
            bool should_be_deleted;
        };
    private:
        std::vector<DataChunk> m_ChunkedData;
        size_t m_ActiveChunk;
        size_t m_ActiveByte;
        uint8_t m_CurrentBit;
    public:
        ChunkedBitReader() noexcept
            : m_ActiveChunk(0),
            m_ActiveByte(0),
            m_CurrentBit(0)
        {
        }

        ChunkedBitReader(void* data, size_t size, bool grant_ownership = false)
            : ChunkedBitReader()
        {
            append_chunk(data, size, grant_ownership);
        }

        ChunkedBitReader(const DataReader& data)
            : ChunkedBitReader()
        {
            append_chunk(data);
        }

        ChunkedBitReader(DataReader&& data)
            : ChunkedBitReader()
        {
            append_chunk(std::move(data));
        }

        // This function should potentially allow to preserve the current offset of DataReader.
        // However, that would require to keep the byte offset for each chunk
        // and would overall complicate how we do things. Is it really something that we want?
        void append_chunk(DataReader&& data)
        {
            append_chunk(data.m_Data, data.m_Size, data.has_ownership());
        }

        void append_chunk(const DataReader& data)
        {
            append_chunk(data.m_Data, data.m_Size);
        }

        void append_chunk(void* data, size_t size, bool grant_ownership = false)
        {
            m_ChunkedData.push_back({ static_cast<uint8_t*>(data), size, grant_ownership });
        }

        size_t bytes_left() const
        {
            size_t total = bytes_left_for_current_chunk();

            for (size_t i = m_ActiveChunk + 1; i < m_ChunkedData.size(); i++)
                total += m_ChunkedData[i].size;

            return total;
        }

        void skip_bytes(size_t count)
        {
            for (;;)
            {
                size_t this_chunk = bytes_left_for_current_chunk();

                if (count < this_chunk)
                {
                    m_ActiveByte += count;
                }
                else if (count > this_chunk)
                {
                    count -= this_chunk;

                    m_ActiveChunk++;

                    if (m_ActiveChunk > m_ChunkedData.size() - 1)
                        throw std::runtime_error("Buffer overflow");

                    m_ActiveByte = 0;
                }

                m_CurrentBit = 0;
            }
        }

        uint32_t get_bits(uint8_t count)
        {
            if (count < bits_left_for_current_byte())
                return (current_byte() >> m_CurrentBit) & XIL_BITS(count);
            else
            {
                // use a for loop here?
                //uint32_t bits = (current_byte() >> m_CurrentBit) & XIL_BITS(count);
            }
        }

        uint32_t get_bits_reversed(uint8_t count)
        {
            // implement me
            return 0;
        }

        void skip_bits(size_t count)
        {
            // refactor
            // something like count / 8

            if (bits_left_for_current_byte() > count)
            {
                m_CurrentBit += static_cast<uint8_t>(count);
            }
            else
            {
                count -= bits_left_for_current_byte();

                for (;;)
                {
                    if (count > 8)
                    {
                        flush_byte();
                        count -= 8;
                    }
                    else
                    {
                        m_CurrentBit = static_cast<uint8_t>(count);
                        break;
                    }
                }
            }
        }

        void flush_byte()
        {
            if (bytes_left_for_current_chunk())
            {
                m_ActiveByte++;
                m_CurrentBit = 0;
            }
            else
                next_chunk();
        }

        ~ChunkedBitReader()
        {
            for (const auto& chnk : m_ChunkedData)
            {
                if (chnk.should_be_deleted)
                    delete[] chnk.data;
            }
        }
    private:
        size_t bytes_left_for_current_chunk() const noexcept
        {
            auto bytes_left = current_chunk().size - m_ActiveByte;

            if (m_CurrentBit)
                return bytes_left ? bytes_left - 1 : 0;
            else
                return bytes_left;
        }

        uint8_t bits_left_for_current_byte() const noexcept
        {
            assert(m_CurrentBit < 8);

            return 8 - m_CurrentBit;
        }

        const DataChunk& current_chunk() const noexcept
        {
            return m_ChunkedData[m_ActiveChunk];
        }

        void next_chunk()
        {
            if (m_ChunkedData.size() - 1 == m_ActiveChunk)
                throw std::runtime_error("Buffer overflow");
            else
            {
                m_ActiveChunk++;
                m_ActiveByte = 0;
                m_CurrentBit = 0;
            }
        }

        uint8_t current_byte() const
        {
            return current_chunk().data[m_ActiveByte];
        }
    };
}
