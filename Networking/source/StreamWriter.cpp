#include "StreamWriter.hpp"

namespace ignite {
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
    size_t size = string.size();
    WriteData(reinterpret_cast<char *>(&size), sizeof(size_t));
    WriteData(string.data(), sizeof(char) * string.size());
}

void StreamWriter::WriteString(const std::string_view string)
{
    size_t size = string.size();
    WriteData(reinterpret_cast<char *>(&size), sizeof(size_t));
    WriteData(string.data(), sizeof(char) * string.size());
}

}