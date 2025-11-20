#pragma once

namespace ignite::net
{
    class StreamWriter;
    class StreamReader;

    class ISerializable
    {
    public:
        virtual ~ISerializable() = default;
        virtual void Serialize(StreamWriter* writer) const = 0;
        virtual void Deserialize(StreamReader* reader) = 0;
    };

}

// Helper macro for implementing static Serialize/Deserialize methods
// that work with StreamWriter::WriteObject and StreamReader::ReadObject
#define IGNITE_NET_IMPL_SERIALIZABLE(ClassName) \
    static void Serialize(ignite::net::StreamWriter* writer, const ClassName& obj) { \
        obj.Serialize(writer); \
    } \
    static ClassName Deserialize(ignite::net::StreamReader* reader) { \
        ClassName obj; \
        obj.Deserialize(reader); \
        return obj; \
    }