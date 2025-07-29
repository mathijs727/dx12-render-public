#pragma once
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace ast {

struct Output {
    bool shouldExport;
    std::filesystem::path cppFolder, shaderFolder;
};

// ===============================================================================
// ===============================================================================
// =====    Definition of all variable types (including groups & structs)    =====
// ===============================================================================
// ===============================================================================
struct UnresolvedType {
    std::string typeName;
};

struct Variable;
struct Struct {
    std::string name;
    std::vector<Variable> variables;
};

// All types that directly map to HLSL.
struct BasicType {
    std::string hlslType;
};
struct Texture2D {
    std::string textureType;
};
struct RWTexture2D {
    std::string textureType;
};
struct ByteAddressBuffer {
    std::string structType;
};
struct RWByteAddressBuffer {
    std::string structType;
};
struct StructInstance {
    uint32_t structIndex;
};
using StructuredType = std::variant<BasicType, StructInstance, ast::UnresolvedType>;
struct StructuredBuffer {
    StructuredType dataType;
};
struct RWStructuredBuffer {
    StructuredType dataType;
};
struct RaytracingAccelerationStructure {
};
struct GroupInstance {
    uint32_t groupIndex;
};
// May be used by code generation back-end to store internal data.
struct CustomType {
    virtual ~CustomType() = default;
};
// clang-format off
using VariableType = std::variant<
    UnresolvedType,
    std::shared_ptr<CustomType>,
    StructInstance,
    GroupInstance,
    BasicType,
    Texture2D,
    RWTexture2D,
    ByteAddressBuffer,
    RWByteAddressBuffer,
    StructuredBuffer,
    RWStructuredBuffer,
    RaytracingAccelerationStructure
>;
// clang-format on

enum class ShaderStage {
    Vertex,
    Geometry,
    Pixel,
    Compute,
    RayTracing
};

// ====================================
// ====================================
// =====    Actual definitions    =====
// ====================================
// ====================================
struct BindPoint {
    std::string name;
    std::vector<uint32_t> shaderInputGroups;
};

struct BindPointReference {
    std::string bindPointName;
    std::string name;
    std::vector<ShaderStage> shaderStages;
    uint32_t bindPointIndex = (uint32_t)-1;
};
struct RootConstant {
    std::string name;
    std::vector<ShaderStage> shaderStages;
    uint32_t num32Bitvalues;
};
struct RootConstantBufferView {
    std::string name;
    std::vector<ShaderStage> shaderStages;
};
struct StaticSampler {
    std::string name;
    std::unordered_map<std::string, std::string> options;
};
struct ShaderInputLayoutOptions {
    bool localRootSignature = false; // Ray tracing related.
};
struct ShaderInputLayout {
    std::string name;
    ShaderInputLayoutOptions options;

    std::vector<BindPointReference> bindPoints;
    std::vector<RootConstant> rootConstants;
    std::vector<RootConstantBufferView> rootConstantBufferViews;
    std::vector<StaticSampler> staticSamplers;
};

struct Variable {
    std::string name;
    VariableType type;
    uint32_t arrayCount; // 0 if not an array, >= 1 otherwise

    static constexpr uint32_t Unbounded = std::numeric_limits<uint32_t>::max();
};
struct Group {
    std::string name;
    std::vector<Variable> variables;
};

struct ShaderInputGroupBindTo {
    std::string inputLayoutName;
    std::string bindPointName;
};
struct ShaderInputGroup {
    std::string name;
    std::string bindPointName;
    uint32_t bindPointIndex;

    std::vector<Variable> variables;
};

struct Constant {
    std::string name;
    int64_t value;
};

class AbstractSyntaxTree {
public:
    using Metadata = Output;

    template <typename T>
    struct ItemWithMetadata {
        Metadata metadata;
        T item;

        const T* operator->() const { return &item; }
        T* operator->() { return &item; }
        operator T&() { return item; }
        operator const T&() const { return item; }

        T& operator*() { return item; }
        const T& operator*() const { return item; }
    };

    std::vector<ItemWithMetadata<ShaderInputLayout>> shaderInputLayouts;
    std::vector<ItemWithMetadata<Struct>> structs;
    std::vector<ItemWithMetadata<Group>> groups;
    std::vector<ItemWithMetadata<ShaderInputGroup>> shaderInputGroups;
    std::vector<ItemWithMetadata<BindPoint>> bindPoints;

    std::vector<ItemWithMetadata<Constant>> constants;
};

}