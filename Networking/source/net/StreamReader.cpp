#include "StreamReader.hpp"

namespace ignite::net
{
    bool StreamReader::ReadBuffer(Buffer &buffer, uint32_t size)
    {
        buffer.Size = size;
        if (size == 0)
        {
            uint32_t wireSize = 0;
            if (!ReadData((char *)&wireSize, sizeof(uint32_t)))
                return false;
            buffer.Size = wireSize;
        }

        // Guard against hostile/corrupt sizes coming off the wire.
        if (buffer.Size > MaxReadSize)
            return false;

        buffer.Allocate(buffer.Size);
        return ReadData((char *)buffer.Data, buffer.Size);
    }

    bool StreamReader::ReadString(std::string &string)
    {
        uint32_t size;
        if (!ReadData((char *)&size, sizeof(uint32_t)))
            return false;

        if (size > MaxReadSize)
            return false;

        string.resize(size);
        return ReadData((char *)string.data(), size);
    }
}