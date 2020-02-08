#pragma once

#include <string>
#include <vector>

#include <assert.h>

#include "utils.h"

namespace XIL {

    class DataStream
    {
        friend class ChunkedBitReader;
    private:
        void* m_Data;
        size_t m_Size;
        size_t m_BytesRead;
        bool m_Owner;
    public:
        DataStream() noexcept
            : m_Data(nullptr),
            m_Size(0),
            m_BytesRead(0),
            m_Owner(false)
        {
        }

        DataStream(void* data, size_t size, bool grant_ownership = false) noexcept
            : m_Data(data),
            m_Size(size),
            m_BytesRead(0),
            m_Owner(grant_ownership)
        {
        }

        DataStream(const DataStream& other) = delete;
        DataStream& operator=(const DataStream& other) = delete;

        DataStream(DataStream&& other) noexcept
            : DataStream()
        {
            std::swap(m_Data, other.m_Data);
            std::swap(m_Size, other.m_Size);
            std::swap(m_BytesRead, other.m_BytesRead);
            std::swap(m_Owner, other.m_Owner);
        }

        DataStream& operator=(DataStream&& other) noexcept
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

        DataStream get_subset(size_t bytes)
        {
            if (bytes > bytes_left())
                throw std::runtime_error("Buffer overflow");

            auto cur = cursor();
            m_BytesRead += bytes;

            return DataStream(cur, bytes);
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

        ~DataStream()
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

    static inline void read_file(const std::string& path, DataStream& into)
    {
        FILE* file;
        XIL_OPEN_FILE(file, path);

        if (!file) throw std::runtime_error("Couldn't open the file");

        fseek(file, 0, SEEK_END);
        size_t fsize = ftell(file);
        rewind(file);

        uint8_t* data = new uint8_t[fsize];
        if (!XIL_READ_EXACTLY(fsize, data, fsize, file))
        {
            delete[] data;
            throw std::runtime_error("Couldn't read the file");
        }

        into.init_with(data, fsize, true);
    }

    class ChunkedBitReader
    {
    private:
        struct DataChunk
        {
            uint8_t* data;
            size_t size;
            size_t active_byte;
            bool should_be_deleted;
        };
    private:
        std::vector<DataChunk> m_ChunkedData;
        size_t m_ActiveChunk;
        uint8_t m_CurrentBit;
    public:
        ChunkedBitReader() noexcept
            : m_ActiveChunk(0),
            m_CurrentBit(0)
        {
        }

        ChunkedBitReader(void* data, size_t size, bool grant_ownership = false)
            : ChunkedBitReader()
        {
            append_chunk(data, size, grant_ownership);
        }

        ChunkedBitReader(const DataStream& data)
            : ChunkedBitReader()
        {
            append_chunk(data);
        }

        ChunkedBitReader(DataStream&& data)
            : ChunkedBitReader()
        {
            append_chunk(std::move(data));
        }

        void append_chunk(DataStream&& data, bool preserve_offset = true)
        {
            append_chunk(
                data.m_Data,
                data.m_Size,
                preserve_offset ? data.m_BytesRead : 0,
                data.has_ownership()
            );

            data.revoke_ownership();
        }

        void append_chunk(const DataStream& data, bool preserve_offset = true)
        {
            append_chunk(data.m_Data, data.m_Size, preserve_offset ? data.m_BytesRead : 0, false);
        }

        void append_chunk(void* data, size_t size, bool grant_ownership = false)
        {
            append_chunk(data, size, 0, grant_ownership);
        }

        void append_chunk(void* data, size_t size, size_t offset, bool grant_ownership = false)
        {
            m_ChunkedData.push_back({ static_cast<uint8_t*>(data), size, offset, grant_ownership });
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
            auto current_chunk_bytes = bytes_left_for_current_chunk();

            for (;;)
            {
                if (current_chunk_bytes < count)
                {
                    current_chunk().active_byte += count;
                    break;
                }
                else
                {
                    count -= current_chunk_bytes;
                    next_chunk();
                }
            }

            m_CurrentBit = 0;
        }

        void skip_bits(size_t count)
        {
            size_t full_bytes = count / 8;

            // count is now within a single byte range
            count -= full_bytes * 8;

            if (bits_left_for_current_byte() < count)
            {
                count -= 8 - m_CurrentBit; // 1. skip current byte bits
                skip_bytes(full_bytes);    // 2. skip full bytes
                m_CurrentBit = count;      // 3. add whats left in count
            }
            else
                m_CurrentBit += count;
        }

        uint32_t get_bits(uint8_t count)
        {
            if (count > 32)
                throw std::runtime_error("Maximum bit count is 32, got a larger value");

            uint8_t bit_offset = 0;
            uint32_t value = 0;

            for (;;)
            {
                if (!bits_left_for_current_byte())
                    flush_byte();

                if (bits_left_for_current_byte() >= count)
                {
                    value |= ((current_byte() & XIL_BITS(count)) << bit_offset);
                    m_CurrentBit += count;
                    break;
                }
                else
                {
                    auto bit_limiter = count > 8 ? XIL_BITS(8) : XIL_BITS(count);
                    value |= ((current_byte() & bit_limiter) << bit_offset);
                    bit_offset += bits_left_for_current_byte();
                    count -= bits_left_for_current_byte();
                    flush_byte();
                }
            }

            return value;
        }

        // skip_if_unused - skip the byte even if it hasn't been read from
        void flush_byte(bool skip_if_unused = true)
        {
            if (!skip_if_unused)
                if (bits_left_for_current_byte() == 8)
                    return;

            if (bytes_left_for_current_chunk())
            {
                current_chunk().active_byte++;
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
           return current_chunk().size - (current_chunk().active_byte + 1);
        }

        uint8_t bits_left_for_current_byte() const noexcept
        {
            return 8 - m_CurrentBit;
        }

        const DataChunk& current_chunk() const noexcept
        {
            return m_ChunkedData[m_ActiveChunk];
        }

        DataChunk& current_chunk() noexcept
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
                m_CurrentBit = 0;
            }
        }

        uint8_t current_byte() const
        {
            return current_chunk().data[current_chunk().active_byte] >> m_CurrentBit;
        }
    };
}
