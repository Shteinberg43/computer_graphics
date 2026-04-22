#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <assert.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include "DDSTextureLoader11.h"


#define SAFE_RELEASE(p) if (p) { (p)->Release(); (p) = nullptr; }


ID3D11Device* m_pDevice = nullptr;
ID3D11DeviceContext* m_pDeviceContext = nullptr;
IDXGISwapChain* m_pSwapChain = nullptr;
ID3D11RenderTargetView* m_pBackBufferRTV = nullptr;

ID3D11Buffer* m_pVertexBuffer = nullptr;
ID3D11Buffer* m_pIndexBuffer = nullptr;
ID3D11VertexShader* m_pVertexShader = nullptr;
ID3D11PixelShader* m_pPixelShader = nullptr;
ID3D11InputLayout* m_pInputLayout = nullptr;

ID3D11Buffer* m_pGeomBuffer = nullptr;
ID3D11Buffer* m_pSceneBuffer = nullptr;

ID3D11ShaderResourceView* m_pTextureView = nullptr;
ID3D11ShaderResourceView* m_pNormalTextureView = nullptr;
ID3D11SamplerState* m_pSampler = nullptr;
ID3D11Texture2D* m_pColorTextureArray = nullptr;
ID3D11ShaderResourceView* m_pColorTextureArraySRV = nullptr;

ID3D11Buffer* m_pGeomBufferInst = nullptr;
ID3D11Buffer* m_pGeomBufferInstVis = nullptr;
ID3D11VertexShader* m_pInstancedVertexShader = nullptr;
ID3D11PixelShader* m_pInstancedPixelShader = nullptr;

ID3D11Buffer* m_pSkyboxVertexBuffer = nullptr;
ID3D11Buffer* m_pSkyboxIndexBuffer = nullptr;
UINT m_SkyboxIndexCount = 0;

ID3D11VertexShader* m_pSkyboxVertexShader = nullptr;
ID3D11PixelShader* m_pSkyboxPixelShader = nullptr;
ID3D11InputLayout* m_pSkyboxInputLayout = nullptr;
ID3D11ShaderResourceView* m_pSkyboxTextureView = nullptr;
ID3D11RasterizerState* m_pSkyboxRasterizerState = nullptr;

ID3D11Texture2D* m_pDepthBuffer = nullptr;
ID3D11DepthStencilView* m_pDepthBufferDSV = nullptr;
ID3D11DepthStencilState* m_pOpaqueDepthState = nullptr;
ID3D11DepthStencilState* m_pSkyboxDepthState = nullptr;
ID3D11DepthStencilState* m_pTransDepthState = nullptr;
ID3D11BlendState* m_pTransBlendState = nullptr;
ID3D11Texture2D* m_pColorBufferTex = nullptr;
ID3D11RenderTargetView* m_pColorBufferRTV = nullptr;
ID3D11ShaderResourceView* m_pColorBufferSRV = nullptr;
ID3D11VertexShader* m_pPostVertexShader = nullptr;
ID3D11PixelShader* m_pPostPixelShader = nullptr;
ID3D11Buffer* m_pPostProcessBuffer = nullptr;

UINT WindowWidth = 1280;
UINT WindowHeight = 720;

float m_camRotX = 0.3f;
float m_camRotY = 0.0f;
int m_postProcessMode = 1;

struct GeomBuffer
{
    DirectX::XMMATRIX m;
    DirectX::XMFLOAT4 size;
    DirectX::XMMATRIX normalM;
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT4 material;
};

struct Light
{
    DirectX::XMFLOAT4 pos;
    DirectX::XMFLOAT4 color;
};

struct SceneBuffer
{
    DirectX::XMMATRIX vp;
    DirectX::XMFLOAT4 cameraPos;
    DirectX::XMINT4 lightCount;
    Light lights[10];
    DirectX::XMFLOAT4 ambientColor;
};

struct Vertex
{
    float x, y, z;
    float nx, ny, nz;
    float tx, ty, tz;
    float u, v;
};

struct TransparentDrawItem
{
    DirectX::XMMATRIX world;
    DirectX::XMFLOAT4 color;
    float distanceSq;
};

constexpr UINT MaxInst = 100;
constexpr UINT InstanceCount = 10;

struct GeomBufferInstItem
{
    DirectX::XMMATRIX model;
    DirectX::XMMATRIX norm;
    DirectX::XMFLOAT4 shineSpeedTexIdNM;
    DirectX::XMFLOAT4 posAngle;
};

struct GeomBufferInstVis
{
    DirectX::XMUINT4 ids[MaxInst];
};

struct PostProcessBuffer
{
    DirectX::XMINT4 mode;
};

inline HRESULT SetResourceName(ID3D11DeviceChild* pResource, const std::string& name)
{
    return pResource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)name.length(), name.c_str());
}

HRESULT InitDirectX(HWND hWnd);
HRESULT InitScene();
HRESULT CreateDepthResources();
HRESULT CreateColorBufferResources();

DirectX::XMFLOAT4 BuildPlane(const DirectX::XMVECTOR& p0, const DirectX::XMVECTOR& p1, const DirectX::XMVECTOR& p2, const DirectX::XMVECTOR& p3);
void BuildFrustumPlanes(
    const DirectX::XMVECTOR& camPos,
    const DirectX::XMVECTOR& camRight,
    const DirectX::XMVECTOR& camUp,
    const DirectX::XMVECTOR& camForward,
    float nearDist,
    float farDist,
    float fov,
    float aspectRatio,
    DirectX::XMFLOAT4 outPlanes[6]);
void ComputeWorldAABB(const DirectX::XMMATRIX& model, const DirectX::XMFLOAT3& localMin, const DirectX::XMFLOAT3& localMax, DirectX::XMFLOAT3& outMin, DirectX::XMFLOAT3& outMax);
bool IsBoxInside(const DirectX::XMFLOAT4 planes[6], const DirectX::XMFLOAT3& bbMin, const DirectX::XMFLOAT3& bbMax);

void Cleanup();
void Render();
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

HRESULT CompileShaderFromFile(const std::wstring& path, ID3DBlob** ppCode)
{
    FILE* pFile = nullptr;
    _wfopen_s(&pFile, path.c_str(), L"rb");
    if (!pFile) return E_FAIL;

    fseek(pFile, 0, SEEK_END);
    long size = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    std::vector<char> data(size);
    fread(data.data(), 1, size, pFile);
    fclose(pFile);

    std::string entryPoint = "";
    std::string platform = "";

    if (path.find(L".vs") != std::wstring::npos) 
    { 
        entryPoint = "vs"; 
        platform = "vs_5_0"; 
    }
    else if (path.find(L".ps") != std::wstring::npos) 
    { 
        entryPoint = "ps"; 
        platform = "ps_5_0"; 
    }

    UINT flags1 = 0;
#ifdef _DEBUG
    flags1 |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ID3DBlob* pErrMsg = nullptr;

    std::string pathA(path.begin(), path.end());
    HRESULT result = D3DCompile(data.data(), data.size(), pathA.c_str(), nullptr, nullptr, entryPoint.c_str(), platform.c_str(), flags1, 0, ppCode, &pErrMsg);

    if (!SUCCEEDED(result) && pErrMsg != nullptr)
    {
        OutputDebugStringA((const char*)pErrMsg->GetBufferPointer());
    }
    SAFE_RELEASE(pErrMsg);
    return result;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0, 0, hInstance, nullptr, LoadCursor(nullptr, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1), nullptr, L"lab1", nullptr };
    RegisterClassEx(&wcex);


    RECT rc;
    rc.left = 0;
    rc.right = WindowWidth; // 1280
    rc.top = 0;
    rc.bottom = WindowHeight; // 720

    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, TRUE);

    HWND hWnd = CreateWindow(L"lab1", L"lab1", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd) return 0;
    ShowWindow(hWnd, nCmdShow);

    // Инициализация DirectX
    if (FAILED(InitDirectX(hWnd)))
    {
        Cleanup();
        return 0;
    }

    if (FAILED(InitScene())) 
    { 
        Cleanup(); 
        return 0; 
    }

    MSG msg = { 0 };
    bool exit = false;

    while (!exit)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (!TranslateAccelerator(msg.hwnd, nullptr, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            if (msg.message == WM_QUIT)
                exit = true;
        }
        Render();
    }

    Cleanup();
    return (int)msg.wParam;
}


HRESULT InitDirectX(HWND hWnd)
{
    HRESULT result;

    IDXGIFactory* pFactory = nullptr;
    result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory);

    IDXGIAdapter* pSelectedAdapter = NULL;
    if (SUCCEEDED(result))
    {
        IDXGIAdapter* pAdapter = NULL;
        UINT adapterIdx = 0;
        while (SUCCEEDED(pFactory->EnumAdapters(adapterIdx, &pAdapter)))
        {
            DXGI_ADAPTER_DESC desc;
            pAdapter->GetDesc(&desc);
            if (wcscmp(desc.Description, L"Microsoft Basic Render Driver") != 0)
            {
                pSelectedAdapter = pAdapter;
                break;
            }
            SAFE_RELEASE(pAdapter);
            adapterIdx++;
        }
    }
    assert(pSelectedAdapter != NULL);

    D3D_FEATURE_LEVEL level;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
    if (SUCCEEDED(result))
    {
        UINT flags = 0;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        result = D3D11CreateDevice(pSelectedAdapter, D3D_DRIVER_TYPE_UNKNOWN, NULL,
            flags, levels, 1, D3D11_SDK_VERSION, &m_pDevice, &level, &m_pDeviceContext);

        assert(level == D3D_FEATURE_LEVEL_11_0);
        assert(SUCCEEDED(result));
    }

    if (SUCCEEDED(result))
    {
        DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
        swapChainDesc.BufferCount = 2;
        swapChainDesc.BufferDesc.Width = WindowWidth;
        swapChainDesc.BufferDesc.Height = WindowHeight;
        swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;
        swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
        swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.OutputWindow = hWnd;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.Windowed = true;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.Flags = 0;

        result = pFactory->CreateSwapChain(m_pDevice, &swapChainDesc, &m_pSwapChain);
        assert(SUCCEEDED(result));
    }

    SAFE_RELEASE(pSelectedAdapter);
    SAFE_RELEASE(pFactory);

    ID3D11Texture2D* pBackBuffer = NULL;
    result = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    assert(SUCCEEDED(result));

    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pBackBufferRTV);
        assert(SUCCEEDED(result));
        SAFE_RELEASE(pBackBuffer);
    }

    if (SUCCEEDED(result))
    {
        result = CreateDepthResources();
        assert(SUCCEEDED(result));
    }
    if (SUCCEEDED(result))
    {
        result = CreateColorBufferResources();
        assert(SUCCEEDED(result));
    }

    return result;
}

HRESULT CreateDepthResources()
{
    SAFE_RELEASE(m_pDepthBufferDSV);
    SAFE_RELEASE(m_pDepthBuffer);

    if (!m_pDevice || WindowWidth == 0 || WindowHeight == 0)
        return S_OK;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Format = DXGI_FORMAT_D32_FLOAT;
    desc.ArraySize = 1;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Height = WindowHeight;
    desc.Width = WindowWidth;
    desc.MipLevels = 1;

    HRESULT result = m_pDevice->CreateTexture2D(&desc, nullptr, &m_pDepthBuffer);
    if (SUCCEEDED(result))
    {
        result = SetResourceName(m_pDepthBuffer, "DepthBuffer");
    }

    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreateDepthStencilView(m_pDepthBuffer, nullptr, &m_pDepthBufferDSV);
    }
    if (SUCCEEDED(result))
    {
        result = SetResourceName(m_pDepthBufferDSV, "DepthBufferDSV");
    }

    return result;
}

HRESULT CreateColorBufferResources()
{
    SAFE_RELEASE(m_pColorBufferSRV);
    SAFE_RELEASE(m_pColorBufferRTV);
    SAFE_RELEASE(m_pColorBufferTex);

    if (!m_pDevice || WindowWidth == 0 || WindowHeight == 0)
        return S_OK;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.ArraySize = 1;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Height = WindowHeight;
    desc.Width = WindowWidth;
    desc.MipLevels = 1;

    HRESULT result = m_pDevice->CreateTexture2D(&desc, nullptr, &m_pColorBufferTex);
    if (SUCCEEDED(result))
    {
        result = SetResourceName(m_pColorBufferTex, "ColorBufferTex");
    }
    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreateRenderTargetView(m_pColorBufferTex, nullptr, &m_pColorBufferRTV);
    }
    if (SUCCEEDED(result))
    {
        result = SetResourceName(m_pColorBufferRTV, "ColorBufferRTV");
    }
    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreateShaderResourceView(m_pColorBufferTex, nullptr, &m_pColorBufferSRV);
    }
    if (SUCCEEDED(result))
    {
        result = SetResourceName(m_pColorBufferSRV, "ColorBufferSRV");
    }

    return result;
}

DirectX::XMFLOAT4 BuildPlane(const DirectX::XMVECTOR& p0, const DirectX::XMVECTOR& p1, const DirectX::XMVECTOR& p2, const DirectX::XMVECTOR& p3)
{
    DirectX::XMVECTOR norm = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSubtract(p1, p0), DirectX::XMVectorSubtract(p3, p0)));
    DirectX::XMVECTOR pos = DirectX::XMVectorScale(DirectX::XMVectorAdd(DirectX::XMVectorAdd(p0, p1), DirectX::XMVectorAdd(p2, p3)), 0.25f);
    float d = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(pos, norm));
    DirectX::XMVECTOR plane = DirectX::XMVectorSet(
        DirectX::XMVectorGetX(norm),
        DirectX::XMVectorGetY(norm),
        DirectX::XMVectorGetZ(norm),
        d);

    DirectX::XMFLOAT4 result;
    DirectX::XMStoreFloat4(&result, plane);
    return result;
}

void BuildFrustumPlanes(
    const DirectX::XMVECTOR& camPos,
    const DirectX::XMVECTOR& camRight,
    const DirectX::XMVECTOR& camUp,
    const DirectX::XMVECTOR& camForward,
    float nearDist,
    float farDist,
    float fov,
    float aspectRatio,
    DirectX::XMFLOAT4 outPlanes[6])
{
    const float nearHalfWidth = tanf(fov * 0.5f) * nearDist;
    const float nearHalfHeight = nearHalfWidth * aspectRatio;
    const float farHalfWidth = tanf(fov * 0.5f) * farDist;
    const float farHalfHeight = farHalfWidth * aspectRatio;

    const DirectX::XMVECTOR nearCenter = DirectX::XMVectorAdd(camPos, DirectX::XMVectorScale(camForward, nearDist));
    const DirectX::XMVECTOR farCenter = DirectX::XMVectorAdd(camPos, DirectX::XMVectorScale(camForward, farDist));

    const DirectX::XMVECTOR nearRight = DirectX::XMVectorScale(camRight, nearHalfWidth);
    const DirectX::XMVECTOR nearUp = DirectX::XMVectorScale(camUp, nearHalfHeight);
    const DirectX::XMVECTOR farRight = DirectX::XMVectorScale(camRight, farHalfWidth);
    const DirectX::XMVECTOR farUp = DirectX::XMVectorScale(camUp, farHalfHeight);

    const DirectX::XMVECTOR ntl = DirectX::XMVectorAdd(DirectX::XMVectorSubtract(nearCenter, nearRight), nearUp);
    const DirectX::XMVECTOR ntr = DirectX::XMVectorAdd(DirectX::XMVectorAdd(nearCenter, nearRight), nearUp);
    const DirectX::XMVECTOR nbl = DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(nearCenter, nearRight), nearUp);
    const DirectX::XMVECTOR nbr = DirectX::XMVectorSubtract(DirectX::XMVectorAdd(nearCenter, nearRight), nearUp);

    const DirectX::XMVECTOR ftl = DirectX::XMVectorAdd(DirectX::XMVectorSubtract(farCenter, farRight), farUp);
    const DirectX::XMVECTOR ftr = DirectX::XMVectorAdd(DirectX::XMVectorAdd(farCenter, farRight), farUp);
    const DirectX::XMVECTOR fbl = DirectX::XMVectorSubtract(DirectX::XMVectorSubtract(farCenter, farRight), farUp);
    const DirectX::XMVECTOR fbr = DirectX::XMVectorSubtract(DirectX::XMVectorAdd(farCenter, farRight), farUp);

    outPlanes[0] = BuildPlane(nbl, nbr, ntr, ntl); // near
    outPlanes[1] = BuildPlane(fbr, fbl, ftl, ftr); // far
    outPlanes[2] = BuildPlane(fbl, nbl, ntl, ftl); // left
    outPlanes[3] = BuildPlane(nbr, fbr, ftr, ntr); // right
    outPlanes[4] = BuildPlane(ntl, ntr, ftr, ftl); // top
    outPlanes[5] = BuildPlane(nbr, nbl, fbl, fbr); // bottom
}

void ComputeWorldAABB(const DirectX::XMMATRIX& model, const DirectX::XMFLOAT3& localMin, const DirectX::XMFLOAT3& localMax, DirectX::XMFLOAT3& outMin, DirectX::XMFLOAT3& outMax)
{
    const DirectX::XMVECTOR corners[8] = {
        DirectX::XMVectorSet(localMin.x, localMin.y, localMin.z, 1.0f),
        DirectX::XMVectorSet(localMax.x, localMin.y, localMin.z, 1.0f),
        DirectX::XMVectorSet(localMin.x, localMax.y, localMin.z, 1.0f),
        DirectX::XMVectorSet(localMax.x, localMax.y, localMin.z, 1.0f),
        DirectX::XMVectorSet(localMin.x, localMin.y, localMax.z, 1.0f),
        DirectX::XMVectorSet(localMax.x, localMin.y, localMax.z, 1.0f),
        DirectX::XMVectorSet(localMin.x, localMax.y, localMax.z, 1.0f),
        DirectX::XMVectorSet(localMax.x, localMax.y, localMax.z, 1.0f)
    };

    DirectX::XMVECTOR minV = DirectX::XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0);
    DirectX::XMVECTOR maxV = DirectX::XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0);

    for (const DirectX::XMVECTOR& corner : corners)
    {
        DirectX::XMVECTOR worldCorner = DirectX::XMVector3TransformCoord(corner, model);
        minV = DirectX::XMVectorMin(minV, worldCorner);
        maxV = DirectX::XMVectorMax(maxV, worldCorner);
    }

    DirectX::XMStoreFloat3(&outMin, minV);
    DirectX::XMStoreFloat3(&outMax, maxV);
}

bool IsBoxInside(const DirectX::XMFLOAT4 planes[6], const DirectX::XMFLOAT3& bbMin, const DirectX::XMFLOAT3& bbMax)
{
    for (UINT i = 0; i < 6; ++i)
    {
        const DirectX::XMFLOAT4& plane = planes[i];
        DirectX::XMFLOAT4 p(
            (plane.x < 0.0f) ? bbMin.x : bbMax.x,
            (plane.y < 0.0f) ? bbMin.y : bbMax.y,
            (plane.z < 0.0f) ? bbMin.z : bbMax.z,
            1.0f);

        float s = p.x * plane.x + p.y * plane.y + p.z * plane.z + plane.w;
        if (s < 0.0f)
            return false;
    }
    return true;
}

HRESULT InitScene()
{
    HRESULT result;

    static const Vertex Vertices[24] = {
        // Front face
        {-0.5f, -0.5f, -0.5f, 0, 0, -1, 1, 0, 0, 0.0f, 1.0f},
        { 0.5f, -0.5f, -0.5f, 0, 0, -1, 1, 0, 0, 1.0f, 1.0f},
        { 0.5f,  0.5f, -0.5f, 0, 0, -1, 1, 0, 0, 1.0f, 0.0f},
        {-0.5f,  0.5f, -0.5f, 0, 0, -1, 1, 0, 0, 0.0f, 0.0f},
        // Back face
        { 0.5f, -0.5f,  0.5f, 0, 0, 1, -1, 0, 0, 0.0f, 1.0f},
        {-0.5f, -0.5f,  0.5f, 0, 0, 1, -1, 0, 0, 1.0f, 1.0f},
        {-0.5f,  0.5f,  0.5f, 0, 0, 1, -1, 0, 0, 1.0f, 0.0f},
        { 0.5f,  0.5f,  0.5f, 0, 0, 1, -1, 0, 0, 0.0f, 0.0f},
        // Left face
        {-0.5f, -0.5f,  0.5f, -1, 0, 0, 0, 0, -1, 0.0f, 1.0f},
        {-0.5f, -0.5f, -0.5f, -1, 0, 0, 0, 0, -1, 1.0f, 1.0f},
        {-0.5f,  0.5f, -0.5f, -1, 0, 0, 0, 0, -1, 1.0f, 0.0f},
        {-0.5f,  0.5f,  0.5f, -1, 0, 0, 0, 0, -1, 0.0f, 0.0f},
        // Right face
        { 0.5f, -0.5f, -0.5f, 1, 0, 0, 0, 0, 1, 0.0f, 1.0f},
        { 0.5f, -0.5f,  0.5f, 1, 0, 0, 0, 0, 1, 1.0f, 1.0f},
        { 0.5f,  0.5f,  0.5f, 1, 0, 0, 0, 0, 1, 1.0f, 0.0f},
        { 0.5f,  0.5f, -0.5f, 1, 0, 0, 0, 0, 1, 0.0f, 0.0f},
        // Top face
        {-0.5f,  0.5f, -0.5f, 0, 1, 0, 0, 0, 1, 0.0f, 1.0f},
        { 0.5f,  0.5f, -0.5f, 0, 1, 0, 0, 0, 1, 1.0f, 1.0f},
        { 0.5f,  0.5f,  0.5f, 0, 1, 0, 0, 0, 1, 1.0f, 0.0f},
        {-0.5f,  0.5f,  0.5f, 0, 1, 0, 0, 0, 1, 0.0f, 0.0f},
        // Bottom face
        {-0.5f, -0.5f,  0.5f, 0, -1, 0, 0, 0, -1, 0.0f, 1.0f},
        { 0.5f, -0.5f,  0.5f, 0, -1, 0, 0, 0, -1, 1.0f, 1.0f},
        { 0.5f, -0.5f, -0.5f, 0, -1, 0, 0, 0, -1, 1.0f, 0.0f},
        {-0.5f, -0.5f, -0.5f, 0, -1, 0, 0, 0, -1, 0.0f, 0.0f}
    };

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(Vertices);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = &Vertices;
    data.SysMemPitch = sizeof(Vertices);
    data.SysMemSlicePitch = 0;

    result = m_pDevice->CreateBuffer(&desc, &data, &m_pVertexBuffer);
    if (SUCCEEDED(result))
    {
        result = SetResourceName(m_pVertexBuffer, "VertexBuffer");
    }
    
    if (SUCCEEDED(result))
    {
        static const USHORT Indices[36] = { 
            0, 2, 1, 0, 3, 2, // Передняя
            4, 6, 5, 4, 7, 6, // Задняя
            8, 10, 9, 8, 11, 10, // Левая
            12, 14, 13, 12, 15, 14, // Правая
            16, 18, 17, 16, 19, 18, // Верхняя
            20, 22, 21, 20, 23, 22  // Нижняя
        };

        desc = {};
        desc.ByteWidth = sizeof(Indices);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        desc.StructureByteStride = 0;

        data.pSysMem = &Indices;
        data.SysMemPitch = sizeof(Indices);
        data.SysMemSlicePitch = 0;

        result = m_pDevice->CreateBuffer(&desc, &data, &m_pIndexBuffer);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pIndexBuffer, "IndexBuffer");
        }
    }

    ID3DBlob* pVertexShaderCode = nullptr;
    if (SUCCEEDED(result))
    {
        if (SUCCEEDED(CompileShaderFromFile(L"triangle.vs", &pVertexShaderCode)))
        {
            result = m_pDevice->CreateVertexShader(pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), nullptr, &m_pVertexShader);
            if (SUCCEEDED(result))
            {
                result = SetResourceName(m_pVertexShader, "triangle.vs");
            }
        }
        else
        {
            return E_FAIL;
        }
    }

    ID3DBlob* pPixelShaderCode = nullptr;
    if (SUCCEEDED(result))
    {
        if (SUCCEEDED(CompileShaderFromFile(L"triangle.ps", &pPixelShaderCode)))
        {
            result = m_pDevice->CreatePixelShader(pPixelShaderCode->GetBufferPointer(), pPixelShaderCode->GetBufferSize(), nullptr, &m_pPixelShader);
            if (SUCCEEDED(result))
            {
                result = SetResourceName(m_pPixelShader, "triangle.ps");
            }
            SAFE_RELEASE(pPixelShaderCode);
        }
        else
        {
            return E_FAIL;
        }
    }

    ID3DBlob* pInstancedVertexShaderCode = nullptr;
    if (SUCCEEDED(result))
    {
        if (SUCCEEDED(CompileShaderFromFile(L"instanced.vs", &pInstancedVertexShaderCode)))
        {
            result = m_pDevice->CreateVertexShader(
                pInstancedVertexShaderCode->GetBufferPointer(),
                pInstancedVertexShaderCode->GetBufferSize(),
                nullptr,
                &m_pInstancedVertexShader);
            if (SUCCEEDED(result))
            {
                result = SetResourceName(m_pInstancedVertexShader, "instanced.vs");
            }
        }
        else
        {
            return E_FAIL;
        }
    }

    ID3DBlob* pInstancedPixelShaderCode = nullptr;
    if (SUCCEEDED(result))
    {
        if (SUCCEEDED(CompileShaderFromFile(L"instanced.ps", &pInstancedPixelShaderCode)))
        {
            result = m_pDevice->CreatePixelShader(
                pInstancedPixelShaderCode->GetBufferPointer(),
                pInstancedPixelShaderCode->GetBufferSize(),
                nullptr,
                &m_pInstancedPixelShader);
            if (SUCCEEDED(result))
            {
                result = SetResourceName(m_pInstancedPixelShader, "instanced.ps");
            }
            SAFE_RELEASE(pInstancedPixelShaderCode);
        }
        else
        {
            return E_FAIL;
        }
    }
    SAFE_RELEASE(pInstancedVertexShaderCode);

    ID3DBlob* pPostVertexShaderCode = nullptr;
    if (SUCCEEDED(result))
    {
        if (SUCCEEDED(CompileShaderFromFile(L"postprocess.vs", &pPostVertexShaderCode)))
        {
            result = m_pDevice->CreateVertexShader(
                pPostVertexShaderCode->GetBufferPointer(),
                pPostVertexShaderCode->GetBufferSize(),
                nullptr,
                &m_pPostVertexShader);
            if (SUCCEEDED(result))
            {
                result = SetResourceName(m_pPostVertexShader, "postprocess.vs");
            }
        }
        else
        {
            return E_FAIL;
        }
    }
    SAFE_RELEASE(pPostVertexShaderCode);

    ID3DBlob* pPostPixelShaderCode = nullptr;
    if (SUCCEEDED(result))
    {
        if (SUCCEEDED(CompileShaderFromFile(L"postprocess.ps", &pPostPixelShaderCode)))
        {
            result = m_pDevice->CreatePixelShader(
                pPostPixelShaderCode->GetBufferPointer(),
                pPostPixelShaderCode->GetBufferSize(),
                nullptr,
                &m_pPostPixelShader);
            if (SUCCEEDED(result))
            {
                result = SetResourceName(m_pPostPixelShader, "postprocess.ps");
            }
        }
        else
        {
            return E_FAIL;
        }
    }
    SAFE_RELEASE(pPostPixelShaderCode);

    if (SUCCEEDED(result))
    {
        static const D3D11_INPUT_ELEMENT_DESC InputDesc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };

        result = m_pDevice->CreateInputLayout(InputDesc, 4, pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), &m_pInputLayout);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pInputLayout, "InputLayout");
        }
    }
    SAFE_RELEASE(pVertexShaderCode);

    if (SUCCEEDED(result))
    {
        D3D11_BUFFER_DESC descGeom = {};
        descGeom.ByteWidth = sizeof(GeomBuffer);
        descGeom.Usage = D3D11_USAGE_DEFAULT;
        descGeom.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        descGeom.CPUAccessFlags = 0;
        descGeom.MiscFlags = 0;
        descGeom.StructureByteStride = 0;

        result = m_pDevice->CreateBuffer(&descGeom, nullptr, &m_pGeomBuffer);
        assert(SUCCEEDED(result));
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pGeomBuffer, "GeomBuffer");
        }
        if (SUCCEEDED(result))
        {
            D3D11_BUFFER_DESC descScene = {};
            descScene.ByteWidth = sizeof(SceneBuffer);
            descScene.Usage = D3D11_USAGE_DYNAMIC;
            descScene.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            descScene.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            descScene.MiscFlags = 0;
            descScene.StructureByteStride = 0;

            result = m_pDevice->CreateBuffer(&descScene, nullptr, &m_pSceneBuffer);
            assert(SUCCEEDED(result));
            if (SUCCEEDED(result))
            {
                result = SetResourceName(m_pSceneBuffer, "SceneBuffer");
            }
        }
        if (SUCCEEDED(result))
        {
            D3D11_BUFFER_DESC descInst = {};
            descInst.ByteWidth = sizeof(GeomBufferInstItem) * MaxInst;
            descInst.Usage = D3D11_USAGE_DEFAULT;
            descInst.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            descInst.CPUAccessFlags = 0;
            descInst.MiscFlags = 0;
            descInst.StructureByteStride = 0;

            result = m_pDevice->CreateBuffer(&descInst, nullptr, &m_pGeomBufferInst);
            if (SUCCEEDED(result))
            {
                result = SetResourceName(m_pGeomBufferInst, "GeomBufferInst");
            }
        }
        if (SUCCEEDED(result))
        {
            D3D11_BUFFER_DESC descInstVis = {};
            descInstVis.ByteWidth = sizeof(GeomBufferInstVis);
            descInstVis.Usage = D3D11_USAGE_DEFAULT;
            descInstVis.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            descInstVis.CPUAccessFlags = 0;
            descInstVis.MiscFlags = 0;
            descInstVis.StructureByteStride = 0;

            result = m_pDevice->CreateBuffer(&descInstVis, nullptr, &m_pGeomBufferInstVis);
            if (SUCCEEDED(result))
            {
                result = SetResourceName(m_pGeomBufferInstVis, "GeomBufferInstVis");
            }
        }
        if (SUCCEEDED(result))
        {
            D3D11_BUFFER_DESC descPost = {};
            descPost.ByteWidth = sizeof(PostProcessBuffer);
            descPost.Usage = D3D11_USAGE_DEFAULT;
            descPost.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            descPost.CPUAccessFlags = 0;
            descPost.MiscFlags = 0;
            descPost.StructureByteStride = 0;

            result = m_pDevice->CreateBuffer(&descPost, nullptr, &m_pPostProcessBuffer);
            if (SUCCEEDED(result))
            {
                result = SetResourceName(m_pPostProcessBuffer, "PostProcessBuffer");
            }
        }
    }

    if (SUCCEEDED(result))
    {
        result = DirectX::CreateDDSTextureFromFile(
            m_pDevice,
            L"Brick.dds",
            nullptr,
            &m_pTextureView
        );
    }

    if (SUCCEEDED(result))
    {
        result = DirectX::CreateDDSTextureFromFile(
            m_pDevice,
            L"BrickNM.dds",
            nullptr,
            &m_pNormalTextureView
        );
    }

    if (SUCCEEDED(result))
    {
        ID3D11Resource* colorRes0 = nullptr;
        ID3D11Resource* colorRes1 = nullptr;
        ID3D11Texture2D* colorTex0 = nullptr;
        ID3D11Texture2D* colorTex1 = nullptr;

        result = DirectX::CreateDDSTextureFromFile(m_pDevice, L"Brick.dds", &colorRes0, nullptr);
        if (SUCCEEDED(result))
        {
            result = DirectX::CreateDDSTextureFromFile(m_pDevice, L"Kitty.dds", &colorRes1, nullptr);
        }

        if (SUCCEEDED(result))
        {
            result = colorRes0->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&colorTex0);
        }
        if (SUCCEEDED(result))
        {
            result = colorRes1->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&colorTex1);
        }

        D3D11_TEXTURE2D_DESC desc0 = {};
        D3D11_TEXTURE2D_DESC desc1 = {};
        if (SUCCEEDED(result))
        {
            colorTex0->GetDesc(&desc0);
            colorTex1->GetDesc(&desc1);
            if (desc0.Width != desc1.Width ||
                desc0.Height != desc1.Height ||
                desc0.Format != desc1.Format ||
                desc0.MipLevels != desc1.MipLevels ||
                desc0.SampleDesc.Count != desc1.SampleDesc.Count)
            {
                result = E_FAIL;
            }
        }

        if (SUCCEEDED(result))
        {
            D3D11_TEXTURE2D_DESC arrayDesc = desc0;
            arrayDesc.ArraySize = 2;
            arrayDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            arrayDesc.Usage = D3D11_USAGE_DEFAULT;
            arrayDesc.CPUAccessFlags = 0;
            arrayDesc.MiscFlags = 0;

            result = m_pDevice->CreateTexture2D(&arrayDesc, nullptr, &m_pColorTextureArray);
            if (SUCCEEDED(result))
            {
                result = SetResourceName(m_pColorTextureArray, "ColorTextureArray");
            }

            if (SUCCEEDED(result))
            {
                for (UINT mip = 0; mip < arrayDesc.MipLevels; ++mip)
                {
                    UINT srcSubres0 = D3D11CalcSubresource(mip, 0, desc0.MipLevels);
                    UINT srcSubres1 = D3D11CalcSubresource(mip, 0, desc1.MipLevels);
                    UINT dstSubres0 = D3D11CalcSubresource(mip, 0, arrayDesc.MipLevels);
                    UINT dstSubres1 = D3D11CalcSubresource(mip, 1, arrayDesc.MipLevels);

                    m_pDeviceContext->CopySubresourceRegion(m_pColorTextureArray, dstSubres0, 0, 0, 0, colorTex0, srcSubres0, nullptr);
                    m_pDeviceContext->CopySubresourceRegion(m_pColorTextureArray, dstSubres1, 0, 0, 0, colorTex1, srcSubres1, nullptr);
                }
            }

            if (SUCCEEDED(result))
            {
                D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.Format = arrayDesc.Format;
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                srvDesc.Texture2DArray.MostDetailedMip = 0;
                srvDesc.Texture2DArray.MipLevels = arrayDesc.MipLevels;
                srvDesc.Texture2DArray.FirstArraySlice = 0;
                srvDesc.Texture2DArray.ArraySize = 2;
                result = m_pDevice->CreateShaderResourceView(m_pColorTextureArray, &srvDesc, &m_pColorTextureArraySRV);
                if (SUCCEEDED(result))
                {
                    result = SetResourceName(m_pColorTextureArraySRV, "ColorTextureArraySRV");
                }
            }
        }

        SAFE_RELEASE(colorTex0);
        SAFE_RELEASE(colorTex1);
        SAFE_RELEASE(colorRes0);
        SAFE_RELEASE(colorRes1);
    }

    if (SUCCEEDED(result))
    {
        D3D11_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D11_FILTER_ANISOTROPIC;
        sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
        sampDesc.MinLOD = -FLT_MAX;
        sampDesc.MaxLOD = FLT_MAX;
        sampDesc.MipLODBias = 0.0f;
        sampDesc.MaxAnisotropy = 16;
        sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampDesc.BorderColor[0] = sampDesc.BorderColor[1] = sampDesc.BorderColor[2] = sampDesc.BorderColor[3] = 1.0f;

        result = m_pDevice->CreateSamplerState(&sampDesc, &m_pSampler);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pSampler, "Sampler");
        }
    }

    if (SUCCEEDED(result))
    {
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable = TRUE;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        desc.DepthFunc = D3D11_COMPARISON_GREATER;
        desc.StencilEnable = FALSE;
        result = m_pDevice->CreateDepthStencilState(&desc, &m_pOpaqueDepthState);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pOpaqueDepthState, "OpaqueDepthState");
        }
    }

    if (SUCCEEDED(result))
    {
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable = TRUE;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
        desc.StencilEnable = FALSE;
        result = m_pDevice->CreateDepthStencilState(&desc, &m_pSkyboxDepthState);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pSkyboxDepthState, "SkyboxDepthState");
        }
    }

    if (SUCCEEDED(result))
    {
        D3D11_DEPTH_STENCIL_DESC desc = {};
        desc.DepthEnable = TRUE;
        desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        desc.DepthFunc = D3D11_COMPARISON_GREATER;
        desc.StencilEnable = FALSE;
        result = m_pDevice->CreateDepthStencilState(&desc, &m_pTransDepthState);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pTransDepthState, "TransDepthState");
        }
    }

    if (SUCCEEDED(result))
    {
        D3D11_BLEND_DESC desc = {};
        desc.AlphaToCoverageEnable = FALSE;
        desc.IndependentBlendEnable = FALSE;
        desc.RenderTarget[0].BlendEnable = TRUE;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        desc.RenderTarget[0].RenderTargetWriteMask =
            D3D11_COLOR_WRITE_ENABLE_RED |
            D3D11_COLOR_WRITE_ENABLE_GREEN |
            D3D11_COLOR_WRITE_ENABLE_BLUE;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        result = m_pDevice->CreateBlendState(&desc, &m_pTransBlendState);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pTransBlendState, "TransBlendState");
        }
    }

    if (SUCCEEDED(result))
    {
        D3D11_RASTERIZER_DESC desc = {};
        desc.FillMode = D3D11_FILL_SOLID;
        desc.CullMode = D3D11_CULL_NONE;
        desc.FrontCounterClockwise = FALSE;
        desc.DepthClipEnable = TRUE;
        result = m_pDevice->CreateRasterizerState(&desc, &m_pSkyboxRasterizerState);
        if (SUCCEEDED(result))
        {
            result = SetResourceName(m_pSkyboxRasterizerState, "SkyboxRasterizerState");
        }
    }

    std::vector<Vertex> sphereVertices;
    std::vector<USHORT> sphereIndices;
    int latLines = 20, longLines = 20;

    for (int i = 0; i <= latLines; ++i) {
        float theta = i * DirectX::XM_PI / latLines;
        for (int j = 0; j <= longLines; ++j) {
            float phi = j * 2 * DirectX::XM_PI / longLines;
            Vertex v;
            v.x = sinf(theta) * cosf(phi);
            v.y = cosf(theta);
            v.z = sinf(theta) * sinf(phi);
            v.nx = 0.0f; v.ny = 0.0f; v.nz = 0.0f;
            v.tx = 0.0f; v.ty = 0.0f; v.tz = 0.0f;
            v.u = 0.0f; v.v = 0.0f;
            sphereVertices.push_back(v);
        }
    }
    for (int i = 0; i < latLines; ++i) {
        for (int j = 0; j < longLines; ++j) {
            int first = (i * (longLines + 1)) + j;
            int second = first + longLines + 1;

            sphereIndices.push_back(first);
            sphereIndices.push_back(second);
            sphereIndices.push_back(first + 1);

            sphereIndices.push_back(second);
            sphereIndices.push_back(second + 1);
            sphereIndices.push_back(first + 1);
        }
    }
    m_SkyboxIndexCount = (UINT)sphereIndices.size();

    D3D11_BUFFER_DESC sbDesc = {};
    sbDesc.ByteWidth = sizeof(Vertex) * (UINT)sphereVertices.size();
    sbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    sbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sbData = {};
    sbData.pSysMem = sphereVertices.data();
    m_pDevice->CreateBuffer(&sbDesc, &sbData, &m_pSkyboxVertexBuffer);

    sbDesc.ByteWidth = sizeof(USHORT) * m_SkyboxIndexCount;
    sbDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    sbData.pSysMem = sphereIndices.data();
    m_pDevice->CreateBuffer(&sbDesc, &sbData, &m_pSkyboxIndexBuffer);
    
    ID3DBlob* pBlob = nullptr;
    result = CompileShaderFromFile(L"skybox.vs", &pBlob);
    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreateVertexShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &m_pSkyboxVertexShader);
        if (SUCCEEDED(result))
        {
            static const D3D11_INPUT_ELEMENT_DESC skyboxInputDesc[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0}
            };
            result = m_pDevice->CreateInputLayout(
                skyboxInputDesc,
                2,
                pBlob->GetBufferPointer(),
                pBlob->GetBufferSize(),
                &m_pSkyboxInputLayout);
            if (SUCCEEDED(result))
            {
                result = SetResourceName(m_pSkyboxInputLayout, "SkyboxInputLayout");
            }
        }
    }
    SAFE_RELEASE(pBlob);

    result = CompileShaderFromFile(L"skybox.ps", &pBlob);
    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &m_pSkyboxPixelShader);
    }
    SAFE_RELEASE(pBlob);

    DirectX::CreateDDSTextureFromFile(m_pDevice, L"skybox.dds", nullptr, &m_pSkyboxTextureView);

    return result;
}

void Render()
{
    if (!m_pDeviceContext || !m_pSwapChain) return;
    if (WindowWidth == 0 || WindowHeight == 0 || !m_pDepthBufferDSV || !m_pColorBufferRTV || !m_pColorBufferSRV) return;
    if (!m_pPostVertexShader || !m_pPostPixelShader || !m_pPostProcessBuffer) return;

    if (GetAsyncKeyState(VK_LEFT) & 0x8000) m_camRotY -= 0.01f;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) m_camRotY += 0.01f;
    if (GetAsyncKeyState(VK_UP) & 0x8000) m_camRotX -= 0.01f;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) m_camRotX += 0.01f;
    if ((GetAsyncKeyState('0') & 0x1) || (GetAsyncKeyState(VK_NUMPAD0) & 0x1)) m_postProcessMode = 0;
    if ((GetAsyncKeyState('1') & 0x1) || (GetAsyncKeyState(VK_NUMPAD1) & 0x1)) m_postProcessMode = 1;

    if (m_camRotX > 1.5f) m_camRotX = 1.5f;
    if (m_camRotX < -1.5f) m_camRotX = -1.5f;

    static ULONGLONG timeStart = GetTickCount64();
    ULONGLONG timeCur = GetTickCount64();
    float time = (timeCur - timeStart) / 1000.0f;
    double angle = time * DirectX::XM_PI * 0.5;

    float f = 100.0f;
    float n = 0.1f;
    float fov = (float)DirectX::XM_PI / 3;
    float aspectRatio = (float)WindowHeight / (float)WindowWidth;

    DirectX::XMMATRIX camRotation = DirectX::XMMatrixRotationRollPitchYaw(m_camRotX, m_camRotY, 0.0f);
    DirectX::XMMATRIX camTransform = DirectX::XMMatrixMultiply(camRotation, DirectX::XMMatrixTranslation(0, 3.0f, -5.5f));
    DirectX::XMMATRIX v = DirectX::XMMatrixInverse(nullptr, camTransform);

    float projWidth = tanf(fov / 2) * 2 * f;
    float projHeight = projWidth * aspectRatio;
    DirectX::XMMATRIX p = DirectX::XMMatrixPerspectiveLH(projWidth, projHeight, f, n);

    float nearViewWidth = tanf(fov / 2) * 2 * n;
    float nearViewHeight = nearViewWidth * aspectRatio;
    float radius = sqrtf(n * n + (nearViewWidth / 2) * (nearViewWidth / 2) + (nearViewHeight / 2) * (nearViewHeight / 2)) * 1.1f;

    DirectX::XMMATRIX camWorld = DirectX::XMMatrixInverse(nullptr, v);
    DirectX::XMVECTOR camPosVec = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0, 0, 0, 1.0f), camWorld);
    DirectX::XMFLOAT4 camPos;
    DirectX::XMStoreFloat4(&camPos, camPosVec);

    const UINT instanceCount = (InstanceCount > MaxInst) ? MaxInst : InstanceCount;
    std::array<GeomBufferInstItem, MaxInst> instData = {};
    GeomBufferInstVis visibleInst = {};

    const UINT gridCols = (instanceCount > 0) ? (UINT)ceilf(sqrtf((float)instanceCount)) : 1;
    const UINT gridRows = (instanceCount + gridCols - 1) / gridCols;
    const float spacing = 2.4f;

    for (UINT i = 0; i < instanceCount; ++i)
    {
        UINT row = i / gridCols;
        UINT col = i % gridCols;
        float x = ((float)col - ((float)gridCols - 1.0f) * 0.5f) * spacing;
        float z = ((float)row - ((float)gridRows - 1.0f) * 0.5f) * spacing;
        float y = 0.15f * sinf(time * 0.9f + i * 0.35f);
        float yaw = (float)angle * (0.45f + 0.07f * (i % 5)) + i * 0.31f;
        float pitch = 0.2f * sinf(time * 0.8f + i * 0.41f);
        float roll = 0.15f * cosf(time * 0.6f + i * 0.27f);
        float scale = (i % 3 == 1) ? 0.85f : 1.0f;

        DirectX::XMMATRIX model = DirectX::XMMatrixMultiply(
            DirectX::XMMatrixMultiply(
                DirectX::XMMatrixScaling(scale, scale, scale),
                DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, roll)),
            DirectX::XMMatrixTranslation(x, y, z));

        float texId = (float)(i % 2); // 0 = Brick, 1 = Kitty
        float useNormalMap = (texId < 0.5f) ? 1.0f : 0.0f;

        instData[i].model = model;
        instData[i].norm = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, model));
        instData[i].shineSpeedTexIdNM = DirectX::XMFLOAT4(36.0f, 1.0f, texId, useNormalMap);
        instData[i].posAngle = DirectX::XMFLOAT4(x, y, z, yaw);
    }

    DirectX::XMVECTOR cube1Center = DirectX::XMVectorSet(0, 0, 0, 1);
    DirectX::XMVECTOR cube2Center = DirectX::XMVectorSet(0, 0, 0, 1);
    if (instanceCount > 0)
    {
        UINT centerIdx = instanceCount / 2;
        UINT neighborIdx = (instanceCount > 1) ? ((centerIdx + 1) % instanceCount) : centerIdx;
        cube1Center = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0, 0, 0, 1.0f), instData[centerIdx].model);
        cube2Center = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0, 0, 0, 1.0f), instData[neighborIdx].model);
    }

    DirectX::XMFLOAT3 cube1CenterPos = {};
    DirectX::XMFLOAT3 cube2CenterPos = {};
    DirectX::XMStoreFloat3(&cube1CenterPos, cube1Center);
    DirectX::XMStoreFloat3(&cube2CenterPos, cube2Center);

    const float lightHeight = 2.0f;
    const DirectX::XMFLOAT4 light0Pos = DirectX::XMFLOAT4(cube1CenterPos.x, cube1CenterPos.y + lightHeight, cube1CenterPos.z, 1.0f);
    const DirectX::XMFLOAT4 light1Pos = DirectX::XMFLOAT4(cube2CenterPos.x, cube2CenterPos.y + lightHeight, cube2CenterPos.z, 1.0f);
    const DirectX::XMFLOAT4 light0Color = DirectX::XMFLOAT4(1.0f, 0.9f, 0.8f, 4.0f);
    const DirectX::XMFLOAT4 light1Color = DirectX::XMFLOAT4(0.70f, 0.85f, 1.0f, 4.5f);

    const DirectX::XMFLOAT3 cubeLocalMin(-0.5f, -0.5f, -0.5f);
    const DirectX::XMFLOAT3 cubeLocalMax(0.5f, 0.5f, 0.5f);
    DirectX::XMFLOAT4 frustumPlanes[6] = {};

    DirectX::XMVECTOR camRight = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(1, 0, 0, 0), camRotation));
    DirectX::XMVECTOR camUp = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0, 1, 0, 0), camRotation));
    DirectX::XMVECTOR camForward = DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0, 0, 1, 0), camRotation));
    BuildFrustumPlanes(camPosVec, camRight, camUp, camForward, n, f, fov, aspectRatio, frustumPlanes);

    UINT visibleCount = 0;
    for (UINT i = 0; i < instanceCount; ++i)
    {
        DirectX::XMFLOAT3 bbMin = {};
        DirectX::XMFLOAT3 bbMax = {};
        ComputeWorldAABB(instData[i].model, cubeLocalMin, cubeLocalMax, bbMin, bbMax);
        if (IsBoxInside(frustumPlanes, bbMin, bbMax))
        {
            visibleInst.ids[visibleCount] = DirectX::XMUINT4(i, 0, 0, 0);
            ++visibleCount;
        }
    }

    D3D11_MAPPED_SUBRESOURCE subresource;
    HRESULT result = m_pDeviceContext->Map(m_pSceneBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &subresource);
    assert(SUCCEEDED(result));
    if (SUCCEEDED(result))
    {
        SceneBuffer& sceneBuffer = *reinterpret_cast<SceneBuffer*>(subresource.pData);
        sceneBuffer.vp = DirectX::XMMatrixMultiply(v, p);
        sceneBuffer.cameraPos = camPos;
        sceneBuffer.lightCount.x = 2;
        sceneBuffer.lightCount.y = 0;
        sceneBuffer.lightCount.z = 0;
        sceneBuffer.lightCount.w = 0;
        sceneBuffer.ambientColor = DirectX::XMFLOAT4(0.08f, 0.08f, 0.10f, 1.0f);

        for (int i = 0; i < 10; ++i)
        {
            sceneBuffer.lights[i].pos = DirectX::XMFLOAT4(0, 0, 0, 1);
            sceneBuffer.lights[i].color = DirectX::XMFLOAT4(0, 0, 0, 0);
        }

        sceneBuffer.lights[0].pos = light0Pos;
        sceneBuffer.lights[0].color = light0Color;
        sceneBuffer.lights[1].pos = light1Pos;
        sceneBuffer.lights[1].color = light1Color;
        m_pDeviceContext->Unmap(m_pSceneBuffer, 0);
    }

    m_pDeviceContext->UpdateSubresource(m_pGeomBufferInst, 0, nullptr, instData.data(), 0, 0);
    m_pDeviceContext->UpdateSubresource(m_pGeomBufferInstVis, 0, nullptr, &visibleInst, 0, 0);
    PostProcessBuffer postProcess = {};
    postProcess.mode = DirectX::XMINT4(m_postProcessMode, 0, 0, 0);
    m_pDeviceContext->UpdateSubresource(m_pPostProcessBuffer, 0, nullptr, &postProcess, 0, 0);
    m_pDeviceContext->ClearState();

    ID3D11RenderTargetView* views[] = { m_pColorBufferRTV };
    m_pDeviceContext->OMSetRenderTargets(1, views, m_pDepthBufferDSV);

    static const FLOAT BackColor[4] = { 1.0f, 0.125f, 0.25f, 1.0f };
    m_pDeviceContext->ClearRenderTargetView(m_pColorBufferRTV, BackColor);
    m_pDeviceContext->ClearDepthStencilView(m_pDepthBufferDSV, D3D11_CLEAR_DEPTH, 0.0f, 0);

    D3D11_VIEWPORT viewport;
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (FLOAT)WindowWidth;
    viewport.Height = (FLOAT)WindowHeight;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_pDeviceContext->RSSetViewports(1, &viewport);

    D3D11_RECT rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = WindowWidth;
    rect.bottom = WindowHeight;
    m_pDeviceContext->RSSetScissorRects(1, &rect);

    m_pDeviceContext->IASetInputLayout(m_pInputLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D11SamplerState* samplers[] = { m_pSampler };
    m_pDeviceContext->PSSetSamplers(0, 1, samplers);

    UINT strides[] = { sizeof(Vertex) };
    UINT offsets[] = { 0 };

    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = { m_pVertexBuffer };
    m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);

    m_pDeviceContext->OMSetDepthStencilState(m_pOpaqueDepthState, 0);
    m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

    m_pDeviceContext->VSSetShader(m_pInstancedVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pInstancedPixelShader, nullptr, 0);

    ID3D11Buffer* instVsBuffers[] = { nullptr, m_pSceneBuffer, m_pGeomBufferInst, m_pGeomBufferInstVis };
    ID3D11Buffer* instPsBuffers[] = { nullptr, m_pSceneBuffer, m_pGeomBufferInst };
    m_pDeviceContext->VSSetConstantBuffers(0, 4, instVsBuffers);
    m_pDeviceContext->PSSetConstantBuffers(0, 3, instPsBuffers);

    ID3D11ShaderResourceView* instancedResources[] = { m_pColorTextureArraySRV, m_pNormalTextureView };
    m_pDeviceContext->PSSetShaderResources(0, 2, instancedResources);

    if (visibleCount > 0)
    {
        m_pDeviceContext->DrawIndexedInstanced(36, visibleCount, 0, 0, 0);
    }

    ID3D11Buffer* constBuffers[] = { m_pGeomBuffer, m_pSceneBuffer };
    m_pDeviceContext->VSSetConstantBuffers(0, 2, constBuffers);
    m_pDeviceContext->PSSetConstantBuffers(0, 2, constBuffers);

    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

    ID3D11ShaderResourceView* resources[] = { m_pTextureView, m_pNormalTextureView };
    m_pDeviceContext->PSSetShaderResources(0, 2, resources);

    GeomBuffer lightMarkerGeom = {};
    lightMarkerGeom.size = DirectX::XMFLOAT4(0, 0, 0, 0);
    lightMarkerGeom.material = DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, -1.0f);
    const float markerRadius = 0.07f;

    lightMarkerGeom.m = DirectX::XMMatrixMultiply(
        DirectX::XMMatrixScaling(markerRadius, markerRadius, markerRadius),
        DirectX::XMMatrixTranslation(light0Pos.x, light0Pos.y, light0Pos.z));
    lightMarkerGeom.normalM = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, lightMarkerGeom.m));
    lightMarkerGeom.color = DirectX::XMFLOAT4(light0Color.x, light0Color.y, light0Color.z, 1.0f);
    m_pDeviceContext->UpdateSubresource(m_pGeomBuffer, 0, nullptr, &lightMarkerGeom, 0, 0);
    m_pDeviceContext->IASetIndexBuffer(m_pSkyboxIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pSkyboxVertexBuffer, strides, offsets);
    m_pDeviceContext->DrawIndexed(m_SkyboxIndexCount, 0, 0);

    lightMarkerGeom.m = DirectX::XMMatrixMultiply(
        DirectX::XMMatrixScaling(markerRadius, markerRadius, markerRadius),
        DirectX::XMMatrixTranslation(light1Pos.x, light1Pos.y, light1Pos.z));
    lightMarkerGeom.normalM = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, lightMarkerGeom.m));
    lightMarkerGeom.color = DirectX::XMFLOAT4(light1Color.x, light1Color.y, light1Color.z, 1.0f);
    m_pDeviceContext->UpdateSubresource(m_pGeomBuffer, 0, nullptr, &lightMarkerGeom, 0, 0);
    m_pDeviceContext->DrawIndexed(m_SkyboxIndexCount, 0, 0);

    m_pDeviceContext->OMSetDepthStencilState(m_pSkyboxDepthState, 0);

    GeomBuffer skyboxGeom = {};
    skyboxGeom.m = DirectX::XMMatrixIdentity();
    skyboxGeom.size = DirectX::XMFLOAT4(radius, 0.0f, 0.0f, 0.0f);
    skyboxGeom.normalM = DirectX::XMMatrixIdentity();
    skyboxGeom.color = DirectX::XMFLOAT4(1, 1, 1, 1);
    skyboxGeom.material = DirectX::XMFLOAT4(0, 1, 0, 0);
    m_pDeviceContext->UpdateSubresource(m_pGeomBuffer, 0, nullptr, &skyboxGeom, 0, 0);

    m_pDeviceContext->IASetInputLayout(m_pSkyboxInputLayout);
    m_pDeviceContext->IASetIndexBuffer(m_pSkyboxIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pSkyboxVertexBuffer, strides, offsets);
    m_pDeviceContext->VSSetShader(m_pSkyboxVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pSkyboxPixelShader, nullptr, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &m_pSkyboxTextureView);
    m_pDeviceContext->RSSetState(m_pSkyboxRasterizerState);
    m_pDeviceContext->DrawIndexed(m_SkyboxIndexCount, 0, 0);
    m_pDeviceContext->RSSetState(nullptr);

    m_pDeviceContext->IASetInputLayout(m_pInputLayout);
    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);
    m_pDeviceContext->PSSetShaderResources(0, 2, resources);
    m_pDeviceContext->OMSetDepthStencilState(m_pTransDepthState, 0);
    float blendFactor[4] = { 0, 0, 0, 0 };
    m_pDeviceContext->OMSetBlendState(m_pTransBlendState, blendFactor, 0xFFFFFFFF);

    std::array<TransparentDrawItem, 2> transparentItems = {};
    auto buildOrbitPanel = [&](float orbitRadius, float phase, float speed, float orbitHeight, const DirectX::XMFLOAT3& panelScale)
        {
            float orbitAngle = phase + (float)angle * speed;
            DirectX::XMVECTOR orbitOffset = DirectX::XMVectorSet(cosf(orbitAngle) * orbitRadius, orbitHeight, sinf(orbitAngle) * orbitRadius, 0.0f);
            DirectX::XMVECTOR panelPos = DirectX::XMVectorAdd(cube1Center, orbitOffset);

            DirectX::XMVECTOR normal = DirectX::XMVector3Normalize(orbitOffset);
            DirectX::XMVECTOR upReference = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            float absDot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(normal, upReference));
            if (absDot < 0.0f) absDot = -absDot;
            if (absDot > 0.98f)
            {
                upReference = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
            }

            DirectX::XMVECTOR tangentX = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(upReference, normal));
            DirectX::XMVECTOR tangentY = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(normal, tangentX));

            DirectX::XMMATRIX orientation = DirectX::XMMATRIX(tangentX, tangentY, normal, DirectX::XMVectorSet(0, 0, 0, 1));
            DirectX::XMMATRIX scaling = DirectX::XMMatrixScaling(panelScale.x, panelScale.y, panelScale.z);
            DirectX::XMMATRIX translation = DirectX::XMMatrixTranslationFromVector(panelPos);
            return DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(scaling, orientation), translation);
        };

    transparentItems[0].world = buildOrbitPanel(0.8f, 0.0f, 1.1f, 0.00f, DirectX::XMFLOAT3(1.2f, 1.2f, 0.01f));
    transparentItems[0].color = DirectX::XMFLOAT4(0.2f, 0.8f, 1.0f, 0.45f);

    transparentItems[1].world = buildOrbitPanel(1.1f, 0.0f, 1.1f, 0.35f, DirectX::XMFLOAT3(1.1f, 1.1f, 0.01f));
    transparentItems[1].color = DirectX::XMFLOAT4(1.0f, 0.5f, 0.2f, 0.55f);

    DirectX::XMVECTOR cameraPos3 = DirectX::XMVectorSet(camPos.x, camPos.y, camPos.z, 1.0f);
    for (auto& item : transparentItems)
    {
        DirectX::XMVECTOR center = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0, 0, 0, 1.0f), item.world);
        DirectX::XMVECTOR delta = DirectX::XMVectorSubtract(center, cameraPos3);
        item.distanceSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(delta));
    }

    std::sort(transparentItems.begin(), transparentItems.end(), [](const TransparentDrawItem& a, const TransparentDrawItem& b)
        {
            return a.distanceSq > b.distanceSq;
        });

    for (const auto& item : transparentItems)
    {
        GeomBuffer transGeom = {};
        transGeom.m = item.world;
        transGeom.size = DirectX::XMFLOAT4(0, 0, 0, 0);
        transGeom.normalM = DirectX::XMMatrixTranspose(DirectX::XMMatrixInverse(nullptr, item.world));
        transGeom.color = item.color;
        transGeom.material = DirectX::XMFLOAT4(26.0f, 1.0f, 0.0f, 0.0f);
        m_pDeviceContext->UpdateSubresource(m_pGeomBuffer, 0, nullptr, &transGeom, 0, 0);
        m_pDeviceContext->DrawIndexed(36, 0, 0);
    }

    m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);

    ID3D11RenderTargetView* backbufferViews[] = { m_pBackBufferRTV };
    m_pDeviceContext->OMSetRenderTargets(1, backbufferViews, nullptr);
    m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);
    m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
    m_pDeviceContext->RSSetState(nullptr);
    m_pDeviceContext->IASetInputLayout(nullptr);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    m_pDeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
    m_pDeviceContext->VSSetShader(m_pPostVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPostPixelShader, nullptr, 0);
    m_pDeviceContext->PSSetConstantBuffers(0, 1, &m_pPostProcessBuffer);
    ID3D11SamplerState* postSamplers[] = { m_pSampler };
    m_pDeviceContext->PSSetSamplers(0, 1, postSamplers);
    ID3D11ShaderResourceView* postResources[] = { m_pColorBufferSRV };
    m_pDeviceContext->PSSetShaderResources(0, 1, postResources);
    m_pDeviceContext->Draw(3, 0);
    ID3D11ShaderResourceView* nullSrv[] = { nullptr };
    m_pDeviceContext->PSSetShaderResources(0, 1, nullSrv);

    result = m_pSwapChain->Present(0, 0);
    assert(SUCCEEDED(result));
}

void Cleanup()
{
    SAFE_RELEASE(m_pInputLayout);
    SAFE_RELEASE(m_pVertexShader);
    SAFE_RELEASE(m_pPixelShader);
    SAFE_RELEASE(m_pIndexBuffer);
    SAFE_RELEASE(m_pVertexBuffer);

    SAFE_RELEASE(m_pDepthBufferDSV);
    SAFE_RELEASE(m_pDepthBuffer);
    SAFE_RELEASE(m_pColorBufferSRV);
    SAFE_RELEASE(m_pColorBufferRTV);
    SAFE_RELEASE(m_pColorBufferTex);
    SAFE_RELEASE(m_pOpaqueDepthState);
    SAFE_RELEASE(m_pSkyboxDepthState);
    SAFE_RELEASE(m_pTransDepthState);
    SAFE_RELEASE(m_pTransBlendState);
    SAFE_RELEASE(m_pSkyboxRasterizerState);

    SAFE_RELEASE(m_pBackBufferRTV);
    SAFE_RELEASE(m_pSwapChain);
    SAFE_RELEASE(m_pDeviceContext);

    SAFE_RELEASE(m_pGeomBuffer);
    SAFE_RELEASE(m_pSceneBuffer);
    SAFE_RELEASE(m_pGeomBufferInst);
    SAFE_RELEASE(m_pGeomBufferInstVis);

    SAFE_RELEASE(m_pSampler);
    SAFE_RELEASE(m_pTextureView);
    SAFE_RELEASE(m_pNormalTextureView);
    SAFE_RELEASE(m_pColorTextureArraySRV);
    SAFE_RELEASE(m_pColorTextureArray);

    SAFE_RELEASE(m_pInstancedVertexShader);
    SAFE_RELEASE(m_pInstancedPixelShader);
    SAFE_RELEASE(m_pPostVertexShader);
    SAFE_RELEASE(m_pPostPixelShader);
    SAFE_RELEASE(m_pPostProcessBuffer);

    SAFE_RELEASE(m_pSkyboxVertexBuffer);
    SAFE_RELEASE(m_pSkyboxIndexBuffer);
    SAFE_RELEASE(m_pSkyboxVertexShader);
    SAFE_RELEASE(m_pSkyboxPixelShader);
    SAFE_RELEASE(m_pSkyboxInputLayout);
    SAFE_RELEASE(m_pSkyboxTextureView);

    if (m_pDevice)
    {
        ID3D11Debug* d3dDebug = nullptr;
        m_pDevice->QueryInterface(IID_PPV_ARGS(&d3dDebug));

        UINT references = m_pDevice->Release();
        m_pDevice = nullptr;

        if (references > 1)
        {
            d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
        }
        SAFE_RELEASE(d3dDebug);
    }
}

//обработка сообщений
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        if (m_pDevice && m_pSwapChain && m_pDeviceContext)
        {
            m_pDeviceContext->OMSetRenderTargets(0, 0, 0);
            SAFE_RELEASE(m_pBackBufferRTV);
            SAFE_RELEASE(m_pDepthBufferDSV);
            SAFE_RELEASE(m_pDepthBuffer);
            SAFE_RELEASE(m_pColorBufferSRV);
            SAFE_RELEASE(m_pColorBufferRTV);
            SAFE_RELEASE(m_pColorBufferTex);


            WindowWidth = LOWORD(lParam);
            WindowHeight = HIWORD(lParam);
            m_pSwapChain->ResizeBuffers(0, WindowWidth, WindowHeight, DXGI_FORMAT_UNKNOWN, 0);

            ID3D11Texture2D* pBackBuffer = NULL;
            m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
            if (pBackBuffer)
            {
                m_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pBackBufferRTV);
                SAFE_RELEASE(pBackBuffer);
            }
            CreateDepthResources();
            CreateColorBufferResources();
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
