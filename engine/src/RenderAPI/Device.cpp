#include "Engine/RenderAPI/Device.h"
#include "Engine/RenderAPI/Internal/D3D12Includes.h"
#include <tbx/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <dxgidebug.h>
#include <spdlog/spdlog.h>
DISABLE_WARNINGS_POP()
#include <array>

#define D3D12_USE_WARP_DEVICE 0
#define D3D12_ENABLE_GPU_VALIDATION 0
constexpr D3D_FEATURE_LEVEL desiredFeatureLevel = D3D_FEATURE_LEVEL_12_0;

namespace RenderAPI {

static void enableDebugLayer();
static bool checkRayTracingSupport(ID3D12Device5* pDevice);

WRL::ComPtr<IDXGIAdapter4> createAdapter()
{
#if defined(D3D12_ENABLE_VALIDATION) && D3D12_ENABLE_VALIDATION
    enableDebugLayer();
#endif

    WRL::ComPtr<IDXGIFactory5> pFactory;
    UINT createFactoryFlags = 0;
#if defined(D3D12_ENABLE_VALIDATION) && D3D12_ENABLE_VALIDATION
    createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&pFactory)));

    WRL::ComPtr<IDXGIAdapter1> pDxgiAdapter1;
    WRL::ComPtr<IDXGIAdapter4> pDxgiAdapter4;
#if defined(D3D12_USE_WARP_DEVICE) && D3D12_USE_WARP_DEVICE
    ThrowIfFailed(pFactory->EnumWarpAdapter(IID_PPV_ARGS(&pDxgiAdapter4)));
#else
    size_t maxDedicatedVideoMemory = 0;
    for (UINT i = 0; pFactory->EnumAdapters1(i, &pDxgiAdapter1) != DXGI_ERROR_NOT_FOUND; i++) {
        DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
        pDxgiAdapter1->GetDesc1(&dxgiAdapterDesc1);

        // Check to see if the adapter can create a D3D12 device without actually creating it. The adapter
        // with the largest dedicated video memory is favored.
        // clang-format off
        if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
            SUCCEEDED(D3D12CreateDevice(pDxgiAdapter1.Get(), desiredFeatureLevel, __uuidof(ID3D12Device), nullptr)) &&
            dxgiAdapterDesc1.DedicatedVideoMemory > maxDedicatedVideoMemory)
        {
            maxDedicatedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
            ThrowIfFailed(pDxgiAdapter1.As(&pDxgiAdapter4));
        }
        // clang-format on
    }
#endif

    return pDxgiAdapter4;
}

WRL::ComPtr<ID3D12Device5> createDevice(IDXGIAdapter4* pAdapter)
{
    WRL::ComPtr<ID3D12Device5> pDevice;

    // https://www.3dgep.com/learning-directx12-1/#Enable_the_Direct3D_12_Debug_Layer
    ThrowIfFailed(D3D12CreateDevice(
        pAdapter,
        desiredFeatureLevel,
        IID_PPV_ARGS(&pDevice)));
#if defined(D3D12_ENABLE_VALIDATION) && D3D12_ENABLE_VALIDATION
    WRL::ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(pDevice.As(&pInfoQueue))) {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

        // Suppress messages based on their severity level
        D3D12_MESSAGE_SEVERITY severities[] = {
            D3D12_MESSAGE_SEVERITY_INFO
        };

        // Suppress individual messages by their ID
        auto denyIds = std::array {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE, // I'm really not sure how to avoid this message.
            D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE, // I'm really not sure how to avoid this message.
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE, // This warning occurs when using capture frame while graphics debugging.
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE, // This warning occurs when using capture frame while graphics debugging.
        };

        D3D12_INFO_QUEUE_FILTER newFilter = {};
        // newFilter.DenyList.NumCategories = _countof(Categories);
        // newFilter.DenyList.pCategoryList = Categories;
        newFilter.DenyList.NumSeverities = _countof(severities);
        newFilter.DenyList.pSeverityList = severities;
        newFilter.DenyList.NumIDs = static_cast<unsigned>(denyIds.size());
        newFilter.DenyList.pIDList = denyIds.data();

        ThrowIfFailed(pInfoQueue->PushStorageFilter(&newFilter));
    } else {
        spdlog::warn("Could not create info queue: D3D12 warning messages may not be generated");
    }
#endif

    if (!checkRayTracingSupport(pDevice.Get()))
        spdlog::warn("Ray tracing is not supported on this device");

    return pDevice;
}

WRL::ComPtr<ID3D12DebugDevice1> createDebugDevice(ID3D12Device5* pDevice)
{
    WRL::ComPtr<ID3D12DebugDevice1> pDebugDevice;
    ThrowIfFailed(pDevice->QueryInterface(IID_PPV_ARGS(&pDebugDevice)));
    return pDebugDevice;
}

void reportLiveObjects(ID3D12Device5* pDevice)
{
    WRL::ComPtr<ID3D12DebugDevice1> pDebugDevice;
    ThrowIfFailed(pDevice->QueryInterface(IID_PPV_ARGS(&pDebugDevice)));
    pDebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL);
}

[[maybe_unused]] static void enableDebugLayer()
{
    // https://docs.microsoft.com/en-us/windows/desktop/direct3d12/creating-a-basic-direct3d-12-component
    // https://www.3dgep.com/learning-directx12-1/#Enable_the_Direct3D_12_Debug_Layer
    WRL::ComPtr<ID3D12Debug> debugController0;
    WRL::ComPtr<ID3D12Debug3> debugController3;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController0)))) {
        ThrowIfFailed(debugController0->QueryInterface(IID_PPV_ARGS(&debugController3)));
        debugController3->EnableDebugLayer();
#if defined(D3D12_ENABLE_GPU_VALIDATION) && D3D12_ENABLE_GPU_VALIDATION
        debugController3->SetEnableGPUBasedValidation(true);
#endif
        spdlog::debug("Debug layer enabled");
    } else {
        spdlog::error("Failed to create D3D12 debug controller");
    }

    WRL::ComPtr<IDXGIInfoQueue> pDxgiInfoQueue;
    if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(pDxgiInfoQueue.GetAddressOf())))) {
        pDxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
        pDxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
        pDxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, true);
        pDxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_MESSAGE, false);
        pDxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_INFO, false);
    }
}

static bool checkRayTracingSupport(ID3D12Device5* pDevice)
{
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options {};
    ThrowIfFailed(pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options, sizeof(options)));
    return options.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
}

}
