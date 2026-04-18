#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <assert.h>
#include <tchar.h>
#include <string>
#include <vector>
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
ID3D11SamplerState* m_pSampler = nullptr;

ID3D11Buffer* m_pSkyboxVertexBuffer = nullptr;
ID3D11Buffer* m_pSkyboxIndexBuffer = nullptr;
UINT m_SkyboxIndexCount = 0;

ID3D11VertexShader* m_pSkyboxVertexShader = nullptr;
ID3D11PixelShader* m_pSkyboxPixelShader = nullptr;
ID3D11ShaderResourceView* m_pSkyboxTextureView = nullptr;

UINT WindowWidth = 1280;
UINT WindowHeight = 720;

float m_camRotX = 0.0f;
float m_camRotY = 0.0f;

struct GeomBuffer
{
    DirectX::XMMATRIX m;
    DirectX::XMFLOAT4 size;
};

struct SceneBuffer
{
    DirectX::XMMATRIX vp;
    DirectX::XMFLOAT4 cameraPos;
};

struct Vertex
{
    float x, y, z;
    float u, v;
};

inline HRESULT SetResourceName(ID3D11DeviceChild* pResource, const std::string& name)
{
    return pResource->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)name.length(), name.c_str());
}

HRESULT InitDirectX(HWND hWnd);
HRESULT InitScene();

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

    return result;
}

HRESULT InitScene()
{
    HRESULT result;

    static const Vertex Vertices[24] = {
        // Front face
            {-0.5f, -0.5f, -0.5f, 0.0f, 1.0f}, { 0.5f, -0.5f, -0.5f, 1.0f, 1.0f},
            { 0.5f,  0.5f, -0.5f, 1.0f, 0.0f}, {-0.5f,  0.5f, -0.5f, 0.0f, 0.0f},
            // Back face
            { 0.5f, -0.5f,  0.5f, 0.0f, 1.0f}, {-0.5f, -0.5f,  0.5f, 1.0f, 1.0f},
            {-0.5f,  0.5f,  0.5f, 1.0f, 0.0f}, { 0.5f,  0.5f,  0.5f, 0.0f, 0.0f},
            // Left face
            {-0.5f, -0.5f,  0.5f, 0.0f, 1.0f}, {-0.5f, -0.5f, -0.5f, 1.0f, 1.0f},
            {-0.5f,  0.5f, -0.5f, 1.0f, 0.0f}, {-0.5f,  0.5f,  0.5f, 0.0f, 0.0f},
            // Right face
            { 0.5f, -0.5f, -0.5f, 0.0f, 1.0f}, { 0.5f, -0.5f,  0.5f, 1.0f, 1.0f},
            { 0.5f,  0.5f,  0.5f, 1.0f, 0.0f}, { 0.5f,  0.5f, -0.5f, 0.0f, 0.0f},
            // Top face
            {-0.5f,  0.5f, -0.5f, 0.0f, 1.0f}, { 0.5f,  0.5f, -0.5f, 1.0f, 1.0f},
            { 0.5f,  0.5f,  0.5f, 1.0f, 0.0f}, {-0.5f,  0.5f,  0.5f, 0.0f, 0.0f},
            // Bottom face
            {-0.5f, -0.5f,  0.5f, 0.0f, 1.0f}, { 0.5f, -0.5f,  0.5f, 1.0f, 1.0f},
            { 0.5f, -0.5f, -0.5f, 1.0f, 0.0f}, {-0.5f, -0.5f, -0.5f, 0.0f, 0.0f}
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
    if (SUCCEEDED(result))
    {
        static const D3D11_INPUT_ELEMENT_DESC InputDesc[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0}
        };

        result = m_pDevice->CreateInputLayout(InputDesc, 2, pVertexShaderCode->GetBufferPointer(), pVertexShaderCode->GetBufferSize(), &m_pInputLayout);
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
            descGeom.MiscFlags = 0;
            descGeom.StructureByteStride = 0;

            result = m_pDevice->CreateBuffer(&descScene, nullptr, &m_pSceneBuffer);
            assert(SUCCEEDED(result));
            if (SUCCEEDED(result))
            {
                result = SetResourceName(m_pSceneBuffer, "SceneBuffer");
            }
        }
    }

    if (SUCCEEDED(result))
    {
        result = DirectX::CreateDDSTextureFromFile(
            m_pDevice,
            L"a.dds",
            nullptr,
            &m_pTextureView
        );
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
    CompileShaderFromFile(L"skybox.vs", &pBlob);
    m_pDevice->CreateVertexShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &m_pSkyboxVertexShader);
    SAFE_RELEASE(pBlob);

    CompileShaderFromFile(L"skybox.ps", &pBlob);
    m_pDevice->CreatePixelShader(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), nullptr, &m_pSkyboxPixelShader);
    SAFE_RELEASE(pBlob);

    DirectX::CreateDDSTextureFromFile(m_pDevice, L"sun.dds", nullptr, &m_pSkyboxTextureView);

    return result;
}

void Render()
{
    if (!m_pDeviceContext || !m_pSwapChain) return;

    if (GetAsyncKeyState(VK_LEFT) & 0x8000) m_camRotY -= 0.02f;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) m_camRotY += 0.02f;
    if (GetAsyncKeyState(VK_UP) & 0x8000) m_camRotX -= 0.02f;
    if (GetAsyncKeyState(VK_DOWN) & 0x8000) m_camRotX += 0.02f;

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

    DirectX::XMMATRIX m = DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f), -(float)angle);

    DirectX::XMMATRIX camRotation = DirectX::XMMatrixRotationRollPitchYaw(m_camRotX, m_camRotY, 0.0f);

    DirectX::XMMATRIX v = DirectX::XMMatrixInverse(nullptr, DirectX::XMMatrixMultiply(camRotation, DirectX::XMMatrixTranslation(0, 0, -5.5f)));

    float viewWidth = tanf(fov / 2) * 2 * n;
    float viewHeight = viewWidth * aspectRatio;
    float radius = sqrtf(n * n + (viewWidth / 2) * (viewWidth / 2) + (viewHeight / 2) * (viewHeight / 2)) * 1.1f;

    DirectX::XMMATRIX camWorld = DirectX::XMMatrixInverse(nullptr, v);
    DirectX::XMVECTOR camPosVec = DirectX::XMVector3TransformCoord(DirectX::XMVectorSet(0, 0, 0, 1.0f), camWorld);
    DirectX::XMFLOAT4 camPos;
    DirectX::XMStoreFloat4(&camPos, camPosVec);

    DirectX::XMMATRIX p = DirectX::XMMatrixPerspectiveLH(viewWidth, viewHeight, n, f);

    D3D11_MAPPED_SUBRESOURCE subresource;
    HRESULT result = m_pDeviceContext->Map(m_pSceneBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &subresource);
    assert(SUCCEEDED(result));
    if (SUCCEEDED(result))
    {
        SceneBuffer& sceneBuffer = *reinterpret_cast<SceneBuffer*>(subresource.pData);
        sceneBuffer.vp = DirectX::XMMatrixMultiply(v, p);
        sceneBuffer.cameraPos = camPos;
        m_pDeviceContext->Unmap(m_pSceneBuffer, 0);
    }

    m_pDeviceContext->ClearState();

    ID3D11RenderTargetView* views[] = { m_pBackBufferRTV };
    m_pDeviceContext->OMSetRenderTargets(1, views, nullptr);

    static const FLOAT BackColor[4] = { 1.0f, 0.125f, 0.25f, 1.0f };
    m_pDeviceContext->ClearRenderTargetView(m_pBackBufferRTV, BackColor);

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

    ID3D11Buffer* constBuffers[] = { m_pGeomBuffer, m_pSceneBuffer };
    m_pDeviceContext->VSSetConstantBuffers(0, 2, constBuffers);

    ID3D11SamplerState* samplers[] = { m_pSampler };
    m_pDeviceContext->PSSetSamplers(0, 1, samplers);

    UINT strides[] = { 20 };
    UINT offsets[] = { 0 };

    GeomBuffer skyboxGeom;
    skyboxGeom.m = DirectX::XMMatrixIdentity();
    skyboxGeom.size = DirectX::XMFLOAT4(radius, 0.0f, 0.0f, 0.0f);
    m_pDeviceContext->UpdateSubresource(m_pGeomBuffer, 0, nullptr, &skyboxGeom, 0, 0);

    m_pDeviceContext->IASetIndexBuffer(m_pSkyboxIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetVertexBuffers(0, 1, &m_pSkyboxVertexBuffer, strides, offsets);

    m_pDeviceContext->VSSetShader(m_pSkyboxVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pSkyboxPixelShader, nullptr, 0);
    m_pDeviceContext->PSSetShaderResources(0, 1, &m_pSkyboxTextureView);
    m_pDeviceContext->DrawIndexed(m_SkyboxIndexCount, 0, 0);


    GeomBuffer cubeGeom;
    cubeGeom.m = m;
    m_pDeviceContext->UpdateSubresource(m_pGeomBuffer, 0, nullptr, &cubeGeom, 0, 0);


    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = { m_pVertexBuffer };
    m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);

    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

    ID3D11ShaderResourceView* resources[] = { m_pTextureView };
    m_pDeviceContext->PSSetShaderResources(0, 1, resources);

    m_pDeviceContext->DrawIndexed(36, 0, 0);

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

    SAFE_RELEASE(m_pBackBufferRTV);
    SAFE_RELEASE(m_pSwapChain);
    SAFE_RELEASE(m_pDeviceContext);

    SAFE_RELEASE(m_pGeomBuffer);
    SAFE_RELEASE(m_pSceneBuffer);

    SAFE_RELEASE(m_pSampler);
    SAFE_RELEASE(m_pTextureView);

    SAFE_RELEASE(m_pSkyboxVertexBuffer);
    SAFE_RELEASE(m_pSkyboxIndexBuffer);
    SAFE_RELEASE(m_pSkyboxVertexShader);
    SAFE_RELEASE(m_pSkyboxPixelShader);
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