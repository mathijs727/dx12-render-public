#include "pch.h"
#include <Engine/Util/BinaryReader.h>
#include <Engine/Util/BinaryWriter.h>
#include <array>
#include <filesystem>
#include <optional>
#include <variant>
#include <vector>

using namespace Catch::literals;

TEST_CASE("Util::BinaryReaderWriter::Serialize and deserialize", "[Util]")
{
    SECTION("basic data types - explicit template")
    {
        const std::filesystem::path testFilePath = "test_basic_data_types1.bin";
        {
            Util::BinaryWriter writer { testFilePath };
            writer.write<int>(-10);
            writer.write<float>(3.14f);
            writer.write<size_t>(1000);
        }

        {
            Util::BinaryReader reader { testFilePath };
            REQUIRE(reader.read<int>() == -10);
            REQUIRE(reader.read<float>() == 3.14f);
            REQUIRE(reader.read<size_t>() == 1000);
        }
    }

    SECTION("basic data types - deduce template")
    {
        const std::filesystem::path testFilePath = "test_basic_data_types2.bin";
        {
            Util::BinaryWriter writer { testFilePath };
            int a = -10;
            float b = 3.14f;
            size_t c = 1000;
            writer.write(a);
            writer.write(b);
            writer.write(c);
        }

        {
            Util::BinaryReader reader { testFilePath };
            int a;
            float b;
            size_t c;
            reader.read(a);
            reader.read(b);
            reader.read(c);
            REQUIRE(a == -10);
            REQUIRE(b == 3.14f);
            REQUIRE(c == 1000);
        }
    }

    SECTION("std::filesystem::path")
    {
        const std::filesystem::path testFilePath = "test_filesystem_path.bin";
        {
            Util::BinaryWriter writer { testFilePath };
            const std::filesystem::path path = "example/path/to/file.txt";
            writer.write(path);
        }
        {
            Util::BinaryReader reader { testFilePath };
            const auto readPath = reader.read<std::filesystem::path>();
            REQUIRE(readPath == "example/path/to/file.txt");
        }
    }

    SECTION("std::string")
    {
        const std::filesystem::path testFilePath = "test_std_string.bin";
        {
            Util::BinaryWriter writer { testFilePath };
            const std::string str = "Hello, World!";
            writer.write(str);
        }
        {
            Util::BinaryReader reader { testFilePath };
            const auto readStr = reader.read<std::string>();
            REQUIRE(readStr == "Hello, World!");
        }
    }

    SECTION("std::optional (filled)")
    {
        const std::filesystem::path testFilePath = "test_std_optional.bin";
        {
            Util::BinaryWriter writer { testFilePath };
            const std::optional<int> optionalValue = 42;
            writer.write(optionalValue);
        }
        {
            Util::BinaryReader reader { testFilePath };
            const auto readOptional = reader.read<std::optional<int>>();
            REQUIRE(readOptional.has_value());
            REQUIRE(*readOptional == 42);
        }
    }

    SECTION("std::optional (empty)")
    {
        const std::filesystem::path testFilePath = "test_std_optional.bin";
        {
            Util::BinaryWriter writer { testFilePath };
            const std::optional<int> optionalValue {};
            writer.write(optionalValue);
            writer.write<int>(123); // Write a value after the optional to test reading
        }
        {
            Util::BinaryReader reader { testFilePath };
            const auto readOptional = reader.read<std::optional<int>>();
            REQUIRE(!readOptional.has_value());
            REQUIRE(reader.read<int>() == 123);
        }
    }

    SECTION("std::variant")
    {
        const std::filesystem::path testFilePath = "test_std_variant.bin";
        {
            Util::BinaryWriter writer { testFilePath };
            const std::variant<int, float, std::string> variantValue = 42;
            writer.write(variantValue);
        }
        {
            Util::BinaryReader reader { testFilePath };
            const auto readVariant = reader.read<std::variant<int, float, std::string>>();
            REQUIRE(std::holds_alternative<int>(readVariant));
            REQUIRE(std::get<int>(readVariant) == 42);
        }
    }

    struct TriviallyCopyableType {
        int a;
        float b;
    };
    SECTION("TriviallyCopyableType")
    {
        const std::filesystem::path testFilePath = "test_trivially_copyable_type.bin";
        TriviallyCopyableType value { 42, 3.14f };
        {
            Util::BinaryWriter writer { testFilePath };
            writer.write(value);
        }
        {
            Util::BinaryReader reader { testFilePath };
            const auto readValue = reader.read<TriviallyCopyableType>();
            REQUIRE(readValue.a == value.a);
            REQUIRE(readValue.b == value.b);
        }
    }

    SECTION("glm::vec2")
    {
        const std::filesystem::path testFilePath = "test_glm_vec2.bin";
        {
            Util::BinaryWriter writer { testFilePath };
            glm::vec2 vec(1.0f, 2.0f);
            writer.write(vec);
        }
        {
            Util::BinaryReader reader { testFilePath };
            const auto readVec = reader.read<glm::vec2>();
            REQUIRE(readVec == glm::vec2(1.0f, 2.0f));
        }
    }

    SECTION("glm::vec3")
    {
        const std::filesystem::path testFilePath = "test_glm_vec3.bin";
        {
            Util::BinaryWriter writer { testFilePath };
            glm::vec3 vec(1.0f, 2.0f, 3.0f);
            writer.write(vec);
        }
        {
            Util::BinaryReader reader { testFilePath };
            const auto readVec = reader.read<glm::vec3>();
            REQUIRE(readVec == glm::vec3(1.0f, 2.0f, 3.0f));
        }
    }

    SECTION("glm::vec4")
    {
        const std::filesystem::path testFilePath = "test_glm_vec4.bin";
        {
            Util::BinaryWriter writer { testFilePath };
            glm::vec4 vec(1.0f, 2.0f, 3.0f, 4.0f);
            writer.write(vec);
        }
        {
            Util::BinaryReader reader { testFilePath };
            const auto readVec = reader.read<glm::vec4>();
            REQUIRE(readVec == glm::vec4(1.0f, 2.0f, 3.0f, 4.0f));
        }
    }

    struct ComplexCopyableType {
    public:
        int a;
        float b;
        std::string c;
        void writeTo(Util::BinaryWriter& writer) const
        {
            writer.write(a);
            writer.write(b);
            writer.write(c);
        }
        void readFrom(Util::BinaryReader& reader)
        {
            reader.read(a);
            reader.read(b);
            reader.read(c);
        }
    };
    SECTION("ComplexCopyableType")
    {
        const std::filesystem::path testFilePath = "test_complex_copyable_type.bin";
        ComplexCopyableType value { 42, 3.14f, "Hello" };
        {
            Util::BinaryWriter writer { testFilePath };
            writer.write(value);
        }
        {
            Util::BinaryReader reader { testFilePath };
            const auto readValue = reader.read<ComplexCopyableType>();
            REQUIRE(readValue.a == value.a);
            REQUIRE(readValue.b == value.b);
            REQUIRE(readValue.c == value.c);
        }
    }

    SECTION("std::vector<int>")
    {
        const std::filesystem::path testFilePath = "test_std_vector_int.bin";
        {
            Util::BinaryWriter writer { testFilePath };
            std::vector<int> vec = { 1, 2, 3, 4, 5 };
            writer.write(vec);
        }
        {
            Util::BinaryReader reader { testFilePath };
            const auto readVec = reader.read<std::vector<int>>();
            REQUIRE(readVec.size() == 5);
            REQUIRE(readVec[0] == 1);
            REQUIRE(readVec[1] == 2);
            REQUIRE(readVec[2] == 3);
            REQUIRE(readVec[3] == 4);
            REQUIRE(readVec[4] == 5);
        }
    }

    SECTION("std::vector<ComplexCopyableType>")
    {
        const std::filesystem::path testFilePath = "test_std_vector_complex_copyable_type.bin";
        {
            Util::BinaryWriter writer { testFilePath };
            std::vector<ComplexCopyableType> vec = { { 1, 2.0f, "One" }, { 3, 4.0f, "Two" } };
            writer.write(vec);
        }
        {
            Util::BinaryReader reader { testFilePath };
            const auto readVec = reader.read<std::vector<ComplexCopyableType>>();
            REQUIRE(readVec.size() == 2);
            REQUIRE(readVec[0].a == 1);
            REQUIRE(readVec[0].b == 2.0f);
            REQUIRE(readVec[0].c == "One");
            REQUIRE(readVec[1].a == 3);
            REQUIRE(readVec[1].b == 4.0f);
            REQUIRE(readVec[1].c == "Two");
        }
    }

    SECTION("std::array<int, 4>")
    {
        const std::filesystem::path testFilePath = "test_std_array.bin";
        {
            Util::BinaryWriter writer { testFilePath };
            std::array<int, 4> arr = { 1, 2, 3, 4 };
            writer.write(arr);
        }
        {
            Util::BinaryReader reader { testFilePath };
            const auto readArr = reader.read<std::array<int, 4>>();
            REQUIRE(readArr.size() == 4);
            REQUIRE(readArr[0] == 1);
            REQUIRE(readArr[1] == 2);
            REQUIRE(readArr[2] == 3);
            REQUIRE(readArr[3] == 4);
        }
    }

    SECTION("std::array<ComplexCopyableType, 2>")
    {
        const std::filesystem::path testFilePath = "test_std_array_complex_copyable_type.bin";
        {
            Util::BinaryWriter writer { testFilePath };
            std::array<ComplexCopyableType, 2> arr { ComplexCopyableType { 1, 2.0f, "One" }, ComplexCopyableType { 3, 4.0f, "Two" } };
            writer.write(arr);
        }
        {
            Util::BinaryReader reader { testFilePath };
            const auto readArr = reader.read<std::array<ComplexCopyableType, 2>>();
            REQUIRE(readArr.size() == 2);
            REQUIRE(readArr[0].a == 1);
            REQUIRE(readArr[0].b == 2.0f);
            REQUIRE(readArr[0].c == "One");
            REQUIRE(readArr[1].a == 3);
            REQUIRE(readArr[1].b == 4.0f);
            REQUIRE(readArr[1].c == "Two");
        }
    }
}