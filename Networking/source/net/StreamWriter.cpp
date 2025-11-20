#include "StreamWriter.hpp"

namespace ignite::net
{
    void StreamWriter::WriteBuffer(Buffer buffer, const bool writeSize)
    {
        if (writeSize) {
            WriteData(reinterpret_cast<char *>(&buffer.Size), sizeof(uint32_t));
        }

        WriteData(static_cast<char *>(buffer.Data), buffer.Size);
    }

    void StreamWriter::WriteZero(const uint64_t size)
    {
        char zero = 0;
        for (uint64_t i = 0; i < size; i++)
            WriteData(&zero, 1);
    }

    void StreamWriter::WriteString(const std::string &string)
    {
        uint32_t size = static_cast<uint32_t>(string.size());
        WriteData(reinterpret_cast<char *>(&size), sizeof(uint32_t));
        WriteData(string.data(), size);
    }

    void StreamWriter::WriteString(const std::string_view string)
    {
        uint32_t size = static_cast<uint32_t>(string.size());
        WriteData(reinterpret_cast<char *>(&size), sizeof(uint32_t));
        WriteData(string.data(), size);
    }
}