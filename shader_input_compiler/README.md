# Shader Input Compiler
Resource bindings in DirectX12 are a big departure from older APIs.
Rather than binding individual resources at runtime (`glUniform...` and `glBindTexture`), one now needs to carefully design DescriptorTableLayouts and fill DescriptorTables accordingly.

Various solutions have been proposed to simplify resource binding through automation:
 * ["Rendering Hitman with DirectX 12" by IO Interactive](https://ubm-twvideo01.s3.amazonaws.com/o1/vault/gdc2016/Presentations/meyer_jonas_rendering_hitman_with.pdf)
 * ["Halcyon Architecture" by EA Seed](https://media.contentapi.ea.com/content/dam/ea/seed/presentations/wihlidal-halcyonarchitecture-notes.pdf)
 * ["Destiny Shader Pipeline" by Bungie](https://advances.realtimerendering.com/destiny/gdc_2017/Destiny_shader_system_GDC_2017_v.4.0.pdf)
 * ["Moving to DirectX 12: Lessons Learned" by Ubisoft Montreal](https://gpuopen.com/wp-content/uploads/2017/07/GDC2017-Moving-To-DX12-Lessons-Learned.pdf)

This subproject is heavily inspired by [Ubisoft Montreal's solution](https://gpuopen.com/wp-content/uploads/2017/07/GDC2017-Moving-To-DX12-Lessons-Learned.pdf).
The core idea is to specify groups of resources that are always bound at the same time; for example diffuse textures & normal textures.
These are specified in a Domain Specific Language (DSL) from which we generate both C++ & HLSL code which can be used by the consuming program.

## Language Specification
The syntax of the DSL is designed to mimic that of C++.
The top-level input file should start with an output statement `#output "path/to/output/folder" "namespace"`. The first argument is the folder where to output the generated C++ & HLSL code. The second argument is the C++ namespace used in the generated code.

To encourage re-use of resource binding specifications, we support include statemnents like in C++ `#include "file_to_include.si"`.

The rest of the input file specification consists of one or more declarations of `struct`, `Group`, `ShaderInputGroup`, `BindPoint`, and `ShaderInputLayout`.

`struct` are used to store basic data types, and use the exact same syntax as in `C` or `C++`.
```c++
struct ExampleStruct {
	int exampleInteger;
	float3 exampleFloat3;
};
```

`Groups` are similar to structs, but can store both basic data types as well as HLSL resources, such as `Texture2D<float>` or `RayTracingAccelerationStructure`.
```c++
Group ExampleGroup<BindTo=ComputeMain>
{
    Texture2D<float4> texture;
    int basicDataType;
    ExampleStruct structInstance;
};
```

Like a `struct`, a `Group` **cannot** be bound to the graphics pipeline. This is handled by a `ShaderInputGroup`, which combines HLSL resources, `struct`'s, and `Group`'s into a single bindable unit.
```c++
ShaderInputGroup ExampleGroup<BindTo=ComputeMain>
{
    RWTexture2D<float4> outputTexture;
    Texture2D<float4> boundedTextureArray[2];
    Texture2D<float4> unboundedTextureArray[];
    RaytracingAccelerationStructure rayTracingStructure;
    int basicDataType;
    ExampleStruct structInstance;
};
```

The `<BindTo=...>` indicates to which `BindPoint` the `ShaderInputGroup` should bind.
One may consider a `BindPoint` to be analogous of a **root parameters(s)** in DirectX12.
Under the hood, a `BindPoint` maps to one or more **root parameters** depending on the requirements. (Multiple unbounded resources arrays in a `ShaderInputGroup` bound to the `BindPoint` will require multiple `DescriptorTable` root parameters.)

A `BindPoint` is declared as follows:
```c++
BindPoint ExampleBindPoint {};
```

Finally, a `ShaderInputLayout` is an abstraction for the `RootSignature`.
The `ShaderInputLayout` may consist of `BindPoint`s, `RootConstant`'s, `RootCBV`'s (root constant buffer views), and static texture samplers (`StaticSample`).
(TODO: add `RootShaderResourceView` and `RootUnorderedAccessView`).
```c++
ShaderInputLayout ExampleLayout {
    ExampleBindPoint exampleBindPoint {
        .shaderStages = [vertex, fragment]
    };

    RootConstant rootConstant {
        .shaderStages = [vertex, fragment],
        .num32BitValues = 2
    };
    RootCBV rootConstantBufferView {
        .shaderStages = [vertex, fragment]
    };

    StaticSampler materialSampler {
        .Filter = "D3D12_FILTER_MIN_MAG_MIP_LINEAR",
        .AddressU = "D3D12_TEXTURE_ADDRESS_MODE_WRAP",
        .AddressV = "D3D12_TEXTURE_ADDRESS_MODE_WRAP"
    };
};
```

The `BindPoints` represent root parameters of the `RootSignature`.
One must specify in which shader stages the bind point will be used; failing to do so will lead to incorrect generated code.

The ray tracing pipeline requires creating "local" root signatures.
This is supported by passing the optional `<Local>` flag:
```c++
ShaderInputLayout ExampleLayout<Local> {
    // ...
};
```
