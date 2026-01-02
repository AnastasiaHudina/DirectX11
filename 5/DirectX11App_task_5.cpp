#include <algorithm>
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <assert.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <cstring>
#include <vector>
#include <string>
#include <malloc.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// Глобальные переменные
HWND g_hWnd = NULL;
ID3D11Device* m_pDevice = nullptr;
ID3D11DeviceContext* m_pDeviceContext = nullptr;
IDXGISwapChain* m_pSwapChain = nullptr;
ID3D11RenderTargetView* m_pBackBufferRTV = nullptr;

// === ПЕРЕМЕННЫЕ ДЛЯ КУБИКА ===
ID3D11Buffer* m_pVertexBuffer = nullptr;
ID3D11Buffer* m_pIndexBuffer = nullptr;
ID3D11VertexShader* m_pVertexShader = nullptr;
ID3D11PixelShader* m_pPixelShader = nullptr;
ID3D11InputLayout* m_pInputLayout = nullptr;

// === НОВЫЕ ПЕРЕМЕННЫЕ ДЛЯ МАТРИЦ И УПРАВЛЕНИЯ ===
ID3D11Buffer* m_pGeomBuffer = nullptr;        // Для первого куба
ID3D11Buffer* m_pGeomBuffer2 = nullptr;       // Для второго куба
ID3D11Buffer* m_pSceneBuffer = nullptr;

// === ПЕРЕМЕННЫЕ ДЛЯ БУФЕРА ГЛУБИНЫ (D32_FLOAT) ===
ID3D11Texture2D* m_pDepthBuffer = nullptr;
ID3D11DepthStencilView* m_pDepthStencilView = nullptr;
ID3D11DepthStencilState* m_pDepthStencilState = nullptr;
ID3D11RasterizerState* m_pRasterizerState = nullptr;

// === ПЕРЕМЕННЫЕ ДЛЯ СОСТОЯНИЙ ГЛУБИНЫ ===
ID3D11DepthStencilState* m_pSkyboxDepthState = nullptr;     // Для skybox
ID3D11DepthStencilState* m_pNormalDepthState = nullptr;     // Для непрозрачных объектов
ID3D11DepthStencilState* m_pTransDepthState = nullptr;      // Для прозрачных объектов (без записи глубины)

// === ПЕРЕМЕННЫЕ ДЛЯ BLEND STATES ===
ID3D11BlendState* m_pTransBlendState = nullptr;     // Для прозрачных объектов
ID3D11BlendState* m_pOpaqueBlendState = nullptr;    // Для непрозрачных объектов

// === ПЕРЕМЕННЫЕ ДЛЯ ТЕКСТУР ===
ID3D11Texture2D* m_pTexture = nullptr;
ID3D11ShaderResourceView* m_pTextureView = nullptr;
ID3D11SamplerState* m_pSampler = nullptr;

// === ПЕРЕМЕННЫЕ ДЛЯ SKYBOX ===
ID3D11Buffer* m_pSphereVertexBuffer = nullptr;
ID3D11Buffer* m_pSphereIndexBuffer = nullptr;
ID3D11VertexShader* m_pSphereVertexShader = nullptr;
ID3D11PixelShader* m_pSpherePixelShader = nullptr;
ID3D11InputLayout* m_pSphereInputLayout = nullptr;
ID3D11Buffer* m_pSphereGeomBuffer = nullptr;
ID3D11Texture2D* m_pCubemapTexture = nullptr;
ID3D11ShaderResourceView* m_pCubemapView = nullptr;
UINT m_sphereIndexCount = 0;

// === ПЕРЕМЕННЫЕ ДЛЯ ПРОЗРАЧНЫХ ОБЪЕКТОВ ===
struct ColorVertex
{
    float x, y, z;
    DWORD color;
};

ID3D11Buffer* m_pRectVertexBuffer = nullptr;
ID3D11Buffer* m_pRectIndexBuffer = nullptr;
ID3D11VertexShader* m_pRectVertexShader = nullptr;
ID3D11PixelShader* m_pRectPixelShader = nullptr;
ID3D11InputLayout* m_pRectInputLayout = nullptr;
ID3D11Buffer* m_pRectGeomBuffer1 = nullptr;  // Для первого прозрачного прямоугольника
ID3D11Buffer* m_pRectGeomBuffer2 = nullptr;  // Для второго прозрачного прямоугольника

// Позиции прозрачных прямоугольников
static const DirectX::XMFLOAT3 Rect0Pos = { 0.0f, 0.0f, 0.0f };
static const DirectX::XMFLOAT3 Rect1Pos = { 0.2f, 0.0f, 0.0f };

// Bounding box для сортировки
struct BoundingRect
{
    DirectX::XMFLOAT3 v[4];
};
BoundingRect m_boundingRects[2];

UINT m_width = 1280;
UINT m_height = 720;

// === ПЕРЕМЕННЫЕ ДЛЯ УПРАВЛЕНИЯ КАМЕРОЙ ===
struct Camera
{
    DirectX::XMFLOAT3 poi = { 0.0f, 0.0f, 0.0f };
    float r = 5.0f;
    float theta = DirectX::XM_PIDIV4;
    float phi = -DirectX::XM_PIDIV4;
} m_camera;

bool m_rbPressed = false;
int m_prevMouseX = 0, m_prevMouseY = 0;

static const float CameraRotationSpeed = DirectX::XM_PI * 2.0f;

#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = nullptr; } }

// === СТРУКТУРА ВЕРШИНЫ С ТЕКСТУРНЫМИ КООРДИНАТАМИ ===
struct TextureVertex
{
    float x, y, z;
    float u, v;
};

// === СТРУКТУРЫ ДЛЯ КОНСТАНТНЫХ БУФЕРОВ ===
struct GeomBuffer
{
    DirectX::XMFLOAT4X4 m;
};

struct SceneBuffer
{
    DirectX::XMFLOAT4X4 vp;
    DirectX::XMFLOAT4 cameraPos;
};

struct SphereGeomBuffer
{
    DirectX::XMFLOAT4X4 m;
    DirectX::XMFLOAT4 size;
};

struct RectGeomBuffer
{
    DirectX::XMFLOAT4X4 m;
    DirectX::XMFLOAT4 color;
};

// === СТРУКТУРА ДЛЯ ЗАГРУЗКИ DDS ===
struct TextureDesc
{
    UINT32 pitch = 0;
    UINT32 mipmapsCount = 0;
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
    UINT32 width = 0;
    UINT32 height = 0;
    void* pData = nullptr;
};

// Прототипы функций
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
bool InitWindow(HINSTANCE hInstance, int nCmdShow);
bool InitDirectX();
bool InitCube();
bool InitBuffers();
bool LoadTexture(const std::wstring& filepath);
bool InitSkybox();
bool InitTransparentObjects();
void Render();
void Cleanup();
void ResizeSwapChain(UINT width, UINT height);
void UpdateCamera();
bool SetupBackBuffer();
void RenderTransparentObjects();

// === ФУНКЦИИ ДЛЯ РАБОТЫ С DDS ===
bool LoadDDS(const std::wstring& filepath, TextureDesc& desc, bool singleMip = false);
UINT32 GetBytesPerBlock(const DXGI_FORMAT& fmt);
UINT32 DivUp(UINT32 a, UINT32 b);
std::string WCSToMBS(const std::wstring& wstr);

// === ФУНКЦИЯ СОЗДАНИЯ СФЕРЫ ===
void CreateSphere(size_t latCells, size_t lonCells, UINT16* pIndices, DirectX::XMFLOAT3* pPos)
{
    for (size_t lat = 0; lat < latCells + 1; lat++)
    {
        for (size_t lon = 0; lon < lonCells + 1; lon++)
        {
            int index = (int)(lat * (lonCells + 1) + lon);
            float lonAngle = 2.0f * (float)DirectX::XM_PI * lon / lonCells + (float)DirectX::XM_PI;
            float latAngle = -(float)DirectX::XM_PI / 2 + (float)DirectX::XM_PI * lat / latCells;

            DirectX::XMFLOAT3 r;
            r.x = sinf(lonAngle) * cosf(latAngle);
            r.y = sinf(latAngle);
            r.z = cosf(lonAngle) * cosf(latAngle);

            pPos[index] = r;
        }
    }

    for (size_t lat = 0; lat < latCells; lat++)
    {
        for (size_t lon = 0; lon < lonCells; lon++)
        {
            size_t index = lat * lonCells * 6 + lon * 6;
            pIndices[index + 0] = (UINT16)(lat * (lonCells + 1) + lon + 0);
            pIndices[index + 2] = (UINT16)(lat * (lonCells + 1) + lon + 1);
            pIndices[index + 1] = (UINT16)((lat + 1) * (lonCells + 1) + lon);
            pIndices[index + 3] = (UINT16)(lat * (lonCells + 1) + lon + 1);
            pIndices[index + 5] = (UINT16)((lat + 1) * (lonCells + 1) + lon + 1);
            pIndices[index + 4] = (UINT16)((lat + 1) * (lonCells + 1) + lon);
        }
    }
}

// === ФУНКЦИЯ ЗАГРУЗКИ ТЕКСТУРЫ ИЗ DDS ===
bool LoadTexture(const std::wstring& filepath)
{
    HRESULT result = S_OK;

    TextureDesc textureDesc;
    if (!LoadDDS(filepath, textureDesc))
    {
        return false;
    }

    // Проверяем поддержку формата
    UINT formatSupport = 0;
    if (FAILED(m_pDevice->CheckFormatSupport(textureDesc.fmt, &formatSupport)) ||
        !(formatSupport & D3D11_FORMAT_SUPPORT_TEXTURE2D))
    {
        free(textureDesc.pData);
        return false;
    }

    // Создаем текстуру
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Format = textureDesc.fmt;
    desc.ArraySize = 1;
    desc.MipLevels = textureDesc.mipmapsCount;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Height = textureDesc.height;
    desc.Width = textureDesc.width;

    UINT32 blockWidth = DivUp(desc.Width, 4u);
    UINT32 blockHeight = DivUp(desc.Height, 4u);
    UINT32 pitch = blockWidth * GetBytesPerBlock(desc.Format);
    const char* pSrcData = reinterpret_cast<const char*>(textureDesc.pData);

    std::vector<D3D11_SUBRESOURCE_DATA> data;
    data.resize(desc.MipLevels);
    for (UINT32 i = 0; i < desc.MipLevels; i++)
    {
        data[i].pSysMem = pSrcData;
        data[i].SysMemPitch = pitch;
        data[i].SysMemSlicePitch = 0;

        pSrcData += pitch * blockHeight;
        blockHeight = std::max<UINT32>(1u, blockHeight / 2);
        blockWidth = std::max<UINT32>(1u, blockWidth / 2);
        pitch = blockWidth * GetBytesPerBlock(desc.Format);
    }

    result = m_pDevice->CreateTexture2D(&desc, data.data(), &m_pTexture);
    free(textureDesc.pData);

    if (FAILED(result))
        return false;

    // Создаем view для текстуры
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    result = m_pDevice->CreateShaderResourceView(m_pTexture, &srvDesc, &m_pTextureView);
    if (FAILED(result)) return false;

    // Создаем семплер
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_ANISOTROPIC;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.MinLOD = -FLT_MAX;
    samplerDesc.MaxLOD = FLT_MAX;
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = 16;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.BorderColor[0] = samplerDesc.BorderColor[1] = samplerDesc.BorderColor[2] = samplerDesc.BorderColor[3] = 1.0f;

    result = m_pDevice->CreateSamplerState(&samplerDesc, &m_pSampler);
    return SUCCEEDED(result);
}

// === НОВАЯ ФУНКЦИЯ ДЛЯ ИНИЦИАЛИЗАЦИИ ПРОЗРАЧНЫХ ОБЪЕКТОВ ===
bool InitTransparentObjects()
{
    HRESULT result = S_OK;

    // Вершины для прозрачного прямоугольника
    static const ColorVertex RectVertices[] =
    {
        // Белый цвет (0xFFFFFFFF), альфа = 1.0
        // Фактический цвет будет задаваться в константном буфере
      {0.0f, -0.75f, -0.75f, 0xFFFFFFFF},
      {0.0f,  0.75f, -0.75f, 0xFFFFFFFF},
      {0.0f,  0.75f,  0.75f, 0xFFFFFFFF},
      {0.0f, -0.75f,  0.75f, 0xFFFFFFFF}
    };

    // Индексы для прямоугольника (два треугольника)
    static const USHORT RectIndices[] = { 0, 1, 2, 0, 2, 3 };

    // Сохраняем bounding box для сортировки
    for (int i = 0; i < 4; i++)
    {
        m_boundingRects[0].v[i] = { RectVertices[i].x + Rect0Pos.x,
                                   RectVertices[i].y + Rect0Pos.y,
                                   RectVertices[i].z + Rect0Pos.z };
        m_boundingRects[1].v[i] = { RectVertices[i].x + Rect1Pos.x,
                                   RectVertices[i].y + Rect1Pos.y,
                                   RectVertices[i].z + Rect1Pos.z };
    }

    // Создаем vertex buffer для прямоугольника
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = sizeof(RectVertices);
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;
    vbDesc.MiscFlags = 0;
    vbDesc.StructureByteStride = sizeof(ColorVertex);

    D3D11_SUBRESOURCE_DATA vbData;
    vbData.pSysMem = RectVertices;
    vbData.SysMemPitch = 0;
    vbData.SysMemSlicePitch = 0;

    result = m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pRectVertexBuffer);
    if (FAILED(result)) return false;

    // Создаем index buffer для прямоугольника
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = sizeof(RectIndices);
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = 0;
    ibDesc.MiscFlags = 0;
    ibDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = RectIndices;
    ibData.SysMemPitch = 0;
    ibData.SysMemSlicePitch = 0;

    result = m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pRectIndexBuffer);
    if (FAILED(result)) return false;

    // Шейдеры для прозрачных объектов
    const char* rectVSSource = R"(
        cbuffer GeomBuffer : register(b0)
        {
            float4x4 m;
            float4 color;
        };
        
        cbuffer SceneBuffer : register(b1)
        {
            float4x4 vp;
            float4 cameraPos;
        };

        struct VSInput
        {
            float3 pos : POSITION;
            uint color : COLOR;
        };

        struct VSOutput
        {
            float4 pos : SV_POSITION;
            float4 color : COLOR;
        };

        VSOutput main(VSInput vertex)
        {
            VSOutput result;
            float4 worldPos = mul(float4(vertex.pos, 1.0), m);
            result.pos = mul(worldPos, vp);
            
            // Распаковываем цвет из DWORD в float4
            float4 unpackedColor;
            unpackedColor.a = ((vertex.color >> 24) & 0xFF) / 255.0;
            unpackedColor.r = ((vertex.color >> 16) & 0xFF) / 255.0;
            unpackedColor.g = ((vertex.color >> 8) & 0xFF) / 255.0;
            unpackedColor.b = (vertex.color & 0xFF) / 255.0;
            
            result.color = unpackedColor * color;
            return result;
        }
    )";

    const char* rectPSSource = R"(
        struct VSOutput
        {
            float4 pos : SV_POSITION;
            float4 color : COLOR;
        };

        float4 main(VSOutput pixel) : SV_Target0
        {
            return pixel.color;
        }
    )";

    // Компилируем и создаем шейдеры
    ID3DBlob* pRectVSBlob = nullptr;
    ID3DBlob* pRectPSBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    result = D3DCompile(rectVSSource, strlen(rectVSSource), "RectVS", nullptr, nullptr, "main", "vs_5_0", flags, 0, &pRectVSBlob, &pErrorBlob);
    if (FAILED(result)) {
        if (pErrorBlob) {
            OutputDebugStringA((const char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        }
        return false;
    }

    result = m_pDevice->CreateVertexShader(pRectVSBlob->GetBufferPointer(), pRectVSBlob->GetBufferSize(), nullptr, &m_pRectVertexShader);
    if (FAILED(result)) {
        pRectVSBlob->Release();
        return false;
    }

    result = D3DCompile(rectPSSource, strlen(rectPSSource), "RectPS", nullptr, nullptr, "main", "ps_5_0", flags, 0, &pRectPSBlob, &pErrorBlob);
    if (FAILED(result)) {
        if (pErrorBlob) {
            OutputDebugStringA((const char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        }
        pRectVSBlob->Release();
        return false;
    }

    result = m_pDevice->CreatePixelShader(pRectPSBlob->GetBufferPointer(), pRectPSBlob->GetBufferSize(), nullptr, &m_pRectPixelShader);
    if (FAILED(result)) {
        pRectVSBlob->Release();
        pRectPSBlob->Release();
        return false;
    }

    // Создаем input layout для прямоугольника
    D3D11_INPUT_ELEMENT_DESC rectLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    result = m_pDevice->CreateInputLayout(rectLayout, 2, pRectVSBlob->GetBufferPointer(), pRectVSBlob->GetBufferSize(), &m_pRectInputLayout);

    pRectVSBlob->Release();
    pRectPSBlob->Release();
    if (pErrorBlob) pErrorBlob->Release();

    if (FAILED(result)) return false;

    // Создаем константные буферы для прямоугольников
    D3D11_BUFFER_DESC rectGeomBufferDesc = {};
    rectGeomBufferDesc.ByteWidth = sizeof(RectGeomBuffer);
    rectGeomBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    rectGeomBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    rectGeomBufferDesc.CPUAccessFlags = 0;
    rectGeomBufferDesc.MiscFlags = 0;
    rectGeomBufferDesc.StructureByteStride = 0;

    // Первый прямоугольник
    RectGeomBuffer rectGeomData1;
    DirectX::XMMATRIX model1 = DirectX::XMMatrixTranslation(Rect0Pos.x, Rect0Pos.y, Rect0Pos.z);
    DirectX::XMMATRIX modelT1 = DirectX::XMMatrixTranspose(model1);
    DirectX::XMStoreFloat4x4(&rectGeomData1.m, modelT1);
    rectGeomData1.color = DirectX::XMFLOAT4(1.0f, 1.0f, 0.0f, 0.7f); // Альфа = 0.7 (менее прозрачный)

    D3D11_SUBRESOURCE_DATA rectGeomInitData1 = { &rectGeomData1, 0, 0 };
    result = m_pDevice->CreateBuffer(&rectGeomBufferDesc, &rectGeomInitData1, &m_pRectGeomBuffer1);
    if (FAILED(result)) return false;

    // Второй прямоугольник 
    RectGeomBuffer rectGeomData2;
    DirectX::XMMATRIX model2 = DirectX::XMMatrixTranslation(Rect1Pos.x, Rect1Pos.y, Rect1Pos.z);
    DirectX::XMMATRIX modelT2 = DirectX::XMMatrixTranspose(model2);
    DirectX::XMStoreFloat4x4(&rectGeomData2.m, modelT2);
    rectGeomData2.color = DirectX::XMFLOAT4(0.0f, 0.0f, 1.0f, 0.3f); // Альфа = 0.3 (более прозрачный)

    D3D11_SUBRESOURCE_DATA rectGeomInitData2 = { &rectGeomData2, 0, 0 };
    result = m_pDevice->CreateBuffer(&rectGeomBufferDesc, &rectGeomInitData2, &m_pRectGeomBuffer2);

    return SUCCEEDED(result);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    if (!InitWindow(hInstance, nCmdShow))
    {
        MessageBox(NULL, L"Не удалось создать окно!", L"Ошибка", MB_OK);
        return -1;
    }

    if (!InitDirectX())
    {
        MessageBox(NULL, L"Не удалось инициализировать DirectX!", L"Ошибка", MB_OK);
        Cleanup();
        return -1;
    }

    if (!InitCube())
    {
        MessageBox(NULL, L"Не удалось инициализировать кубик!", L"Ошибка", MB_OK);
        Cleanup();
        return -1;
    }

    if (!InitBuffers())
    {
        MessageBox(NULL, L"Не удалось инициализировать буферы!", L"Ошибка", MB_OK);
        Cleanup();
        return -1;
    }

    // Загружаем текстуру для куба
    if (!LoadTexture(L"Kitty.dds")) // формат dxt3
    {
        MessageBox(NULL, L"Не удалось загрузить текстуру cube!", L"Ошибка", MB_OK);
        Cleanup();
        return -1;
    }

    if (!InitSkybox())
    {
        MessageBox(NULL, L"Не удалось инициализировать skybox!", L"Ошибка", MB_OK);
        Cleanup();
        return -1;
    }

    // Инициализируем прозрачные объекты
    if (!InitTransparentObjects())
    {
        MessageBox(NULL, L"Не удалось инициализировать прозрачные объекты!", L"Ошибка", MB_OK);
        Cleanup();
        return -1;
    }

    MSG msg = {};
    bool exit = false;

    while (!exit)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                exit = true;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Render();
        }
    }

    Cleanup();
    return (int)msg.wParam;
}

bool InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"DirectX11Window";

    if (!RegisterClassEx(&wc))
        return false;

    RECT rc = {};
    rc.left = 0;
    rc.right = m_width;
    rc.top = 0;
    rc.bottom = m_height;

    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, TRUE);

    g_hWnd = CreateWindow(
        L"DirectX11Window",
        L"DirectX 11 - Два текстурированных кубика, Skybox и прозрачные объекты",
        WS_OVERLAPPEDWINDOW,
        100, 100,
        rc.right - rc.left,
        rc.bottom - rc.top,
        NULL, NULL, hInstance, NULL
    );

    if (!g_hWnd)
        return false;

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    return true;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
    {
        UINT newWidth = LOWORD(lParam);
        UINT newHeight = HIWORD(lParam);

        if (m_pSwapChain && newWidth > 0 && newHeight > 0)
        {
            ResizeSwapChain(newWidth, newHeight);
        }
    }
    break;

    case WM_RBUTTONDOWN:
        m_rbPressed = true;
        m_prevMouseX = LOWORD(lParam);
        m_prevMouseY = HIWORD(lParam);
        SetCapture(hWnd);
        break;

    case WM_RBUTTONUP:
        m_rbPressed = false;
        ReleaseCapture();
        break;

    case WM_MOUSEMOVE:
        if (m_rbPressed)
        {
            int currentX = LOWORD(lParam);
            int currentY = HIWORD(lParam);

            float dx = -(float)(currentX - m_prevMouseX) / m_width * CameraRotationSpeed;
            float dy = (float)(currentY - m_prevMouseY) / m_width * CameraRotationSpeed;

            m_camera.phi += dx;
            m_camera.theta += dy;

            m_camera.theta = std::min<float>(std::max<float>(m_camera.theta, -(float)DirectX::XM_PIDIV2 + 0.001f), (float)DirectX::XM_PIDIV2 - 0.001f);

            m_prevMouseX = currentX;
            m_prevMouseY = currentY;
        }
        break;

    case WM_MOUSEWHEEL:
    {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        m_camera.r -= delta / 100.0f;
        if (m_camera.r < 1.0f)
            m_camera.r = 1.0f;
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

bool InitDirectX()
{
    HRESULT result = S_OK;

    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Width = m_width;
    swapChainDesc.BufferDesc.Height = m_height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = g_hWnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

    UINT createFlags = 0;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createFlags,
        featureLevels,
        1,
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &m_pSwapChain,
        &m_pDevice,
        nullptr,
        &m_pDeviceContext
    );

    if (FAILED(result))
    {
        result = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            createFlags,
            featureLevels,
            1,
            D3D11_SDK_VERSION,
            &swapChainDesc,
            &m_pSwapChain,
            &m_pDevice,
            nullptr,
            &m_pDeviceContext
        );
    }

    if (FAILED(result))
        return false;

    if (!SetupBackBuffer())
        return false;

    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_NONE;
    rasterDesc.FrontCounterClockwise = FALSE;
    rasterDesc.DepthBias = 0;
    rasterDesc.SlopeScaledDepthBias = 0.0f;
    rasterDesc.DepthBiasClamp = 0.0f;
    rasterDesc.DepthClipEnable = TRUE;
    rasterDesc.ScissorEnable = FALSE;
    rasterDesc.MultisampleEnable = FALSE;
    rasterDesc.AntialiasedLineEnable = FALSE;

    result = m_pDevice->CreateRasterizerState(&rasterDesc, &m_pRasterizerState);
    if (FAILED(result)) return false;

    // === СОЗДАНИЕ СОСТОЯНИЙ ГЛУБИНЫ ДЛЯ REVERSED DEPTH ===

    // Для непрозрачных объектов (кубы) - reversed depth
    D3D11_DEPTH_STENCIL_DESC opaqueDepthDesc = {};
    opaqueDepthDesc.DepthEnable = TRUE;
    opaqueDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    opaqueDepthDesc.DepthFunc = D3D11_COMPARISON_GREATER;  // REVERSED DEPTH: GREATER
    opaqueDepthDesc.StencilEnable = FALSE;

    result = m_pDevice->CreateDepthStencilState(&opaqueDepthDesc, &m_pNormalDepthState);
    if (FAILED(result)) return false;

    // Для skybox - reversed depth, без записи, GREATER_EQUAL
    D3D11_DEPTH_STENCIL_DESC skyboxDepthDesc = {};
    skyboxDepthDesc.DepthEnable = TRUE;
    skyboxDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    skyboxDepthDesc.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;  // REVERSED DEPTH: GREATER_EQUAL
    skyboxDepthDesc.StencilEnable = FALSE;

    result = m_pDevice->CreateDepthStencilState(&skyboxDepthDesc, &m_pSkyboxDepthState);
    if (FAILED(result)) return false;

    // Для прозрачных объектов - reversed depth, без записи
    D3D11_DEPTH_STENCIL_DESC transDepthDesc = {};
    transDepthDesc.DepthEnable = TRUE;
    transDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    transDepthDesc.DepthFunc = D3D11_COMPARISON_GREATER;  // REVERSED DEPTH: GREATER
    transDepthDesc.StencilEnable = FALSE;

    result = m_pDevice->CreateDepthStencilState(&transDepthDesc, &m_pTransDepthState);
    if (FAILED(result)) return false;

    // === СОЗДАНИЕ BLEND STATES ===

    // Для прозрачных объектов
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    result = m_pDevice->CreateBlendState(&blendDesc, &m_pTransBlendState);
    if (FAILED(result)) return false;

    // Для непрозрачных объектов (отключено смешивание)
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    result = m_pDevice->CreateBlendState(&blendDesc, &m_pOpaqueBlendState);

    return SUCCEEDED(result);
}

bool SetupBackBuffer()
{
    HRESULT result = S_OK;

    SAFE_RELEASE(m_pBackBufferRTV);
    SAFE_RELEASE(m_pDepthStencilView);
    SAFE_RELEASE(m_pDepthBuffer);
    SAFE_RELEASE(m_pDepthStencilState);

    ID3D11Texture2D* pBackBuffer = nullptr;
    result = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(result)) return false;

    result = m_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pBackBufferRTV);
    SAFE_RELEASE(pBackBuffer);
    if (FAILED(result)) return false;

    // === СОЗДАНИЕ БУФЕРА ГЛУБИНЫ D32_FLOAT ===
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = m_width;
    depthDesc.Height = m_height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthDesc.CPUAccessFlags = 0;
    depthDesc.MiscFlags = 0;

    result = m_pDevice->CreateTexture2D(&depthDesc, nullptr, &m_pDepthBuffer);
    if (FAILED(result)) return false;

    D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc = {};
    depthViewDesc.Format = depthDesc.Format;
    depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthViewDesc.Texture2D.MipSlice = 0;

    result = m_pDevice->CreateDepthStencilView(m_pDepthBuffer, &depthViewDesc, &m_pDepthStencilView);
    if (FAILED(result)) return false;

    return true;
}

bool InitBuffers()
{
    HRESULT result = S_OK;

    D3D11_BUFFER_DESC geomBufferDesc = {};
    geomBufferDesc.ByteWidth = sizeof(GeomBuffer);
    geomBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    geomBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    geomBufferDesc.CPUAccessFlags = 0;
    geomBufferDesc.MiscFlags = 0;
    geomBufferDesc.StructureByteStride = 0;

    GeomBuffer geomBuffer;
    DirectX::XMStoreFloat4x4(&geomBuffer.m, DirectX::XMMatrixIdentity());

    D3D11_SUBRESOURCE_DATA geomInitData;
    geomInitData.pSysMem = &geomBuffer;
    geomInitData.SysMemPitch = 0;
    geomInitData.SysMemSlicePitch = 0;

    result = m_pDevice->CreateBuffer(&geomBufferDesc, &geomInitData, &m_pGeomBuffer);
    if (FAILED(result)) return false;

    // Создаем второй буфер для второго куба
    result = m_pDevice->CreateBuffer(&geomBufferDesc, &geomInitData, &m_pGeomBuffer2);
    if (FAILED(result)) return false;

    D3D11_BUFFER_DESC sceneBufferDesc = {};
    sceneBufferDesc.ByteWidth = sizeof(SceneBuffer);
    sceneBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    sceneBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    sceneBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    sceneBufferDesc.MiscFlags = 0;
    sceneBufferDesc.StructureByteStride = 0;

    result = m_pDevice->CreateBuffer(&sceneBufferDesc, nullptr, &m_pSceneBuffer);
    return SUCCEEDED(result);
}

bool InitCube()
{
    HRESULT result = S_OK;

    static const TextureVertex Vertices[] = {
        {-0.5f, -0.5f,  0.5f, 0.0f, 1.0f},
        { 0.5f, -0.5f,  0.5f, 1.0f, 1.0f},
        { 0.5f, -0.5f, -0.5f, 1.0f, 0.0f},
        {-0.5f, -0.5f, -0.5f, 0.0f, 0.0f},

        {-0.5f,  0.5f, -0.5f, 0.0f, 1.0f},
        { 0.5f,  0.5f, -0.5f, 1.0f, 1.0f},
        { 0.5f,  0.5f,  0.5f, 1.0f, 0.0f},
        {-0.5f,  0.5f,  0.5f, 0.0f, 0.0f},

        { 0.5f, -0.5f, -0.5f, 0.0f, 1.0f},
        { 0.5f, -0.5f,  0.5f, 1.0f, 1.0f},
        { 0.5f,  0.5f,  0.5f, 1.0f, 0.0f},
        { 0.5f,  0.5f, -0.5f, 0.0f, 0.0f},

        {-0.5f, -0.5f,  0.5f, 0.0f, 1.0f},
        {-0.5f, -0.5f, -0.5f, 1.0f, 1.0f},
        {-0.5f,  0.5f, -0.5f, 1.0f, 0.0f},
        {-0.5f,  0.5f,  0.5f, 0.0f, 0.0f},

        { 0.5f, -0.5f,  0.5f, 0.0f, 1.0f},
        {-0.5f, -0.5f,  0.5f, 1.0f, 1.0f},
        {-0.5f,  0.5f,  0.5f, 1.0f, 0.0f},
        { 0.5f,  0.5f,  0.5f, 0.0f, 0.0f},

        {-0.5f, -0.5f, -0.5f, 0.0f, 1.0f},
        { 0.5f, -0.5f, -0.5f, 1.0f, 1.0f},
        { 0.5f,  0.5f, -0.5f, 1.0f, 0.0f},
        {-0.5f,  0.5f, -0.5f, 0.0f, 0.0f}
    };

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(Vertices);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = sizeof(TextureVertex);

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = Vertices;
    data.SysMemPitch = 0;
    data.SysMemSlicePitch = 0;

    result = m_pDevice->CreateBuffer(&desc, &data, &m_pVertexBuffer);
    if (FAILED(result)) return false;

    static const USHORT Indices[] = {
        0, 2, 1, 0, 3, 2,
        4, 6, 5, 4, 7, 6,
        8, 10, 9, 8, 11, 10,
        12, 14, 13, 12, 15, 14,
        16, 18, 17, 16, 19, 18,
        20, 22, 21, 20, 23, 22
    };

    desc = {};
    desc.ByteWidth = sizeof(Indices);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    data = {};
    data.pSysMem = Indices;
    data.SysMemPitch = 0;
    data.SysMemSlicePitch = 0;

    result = m_pDevice->CreateBuffer(&desc, &data, &m_pIndexBuffer);
    if (FAILED(result)) return false;

    const char* vsSource = R"(
        cbuffer GeomBuffer : register(b0)
        {
            float4x4 m;
        };
        
        cbuffer SceneBuffer : register(b1)
        {
            float4x4 vp;
            float4 cameraPos;
        };

        struct VSInput
        {
            float3 pos : POSITION;
            float2 uv : TEXCOORD;
        };

        struct VSOutput
        {
            float4 pos : SV_POSITION;
            float2 uv : TEXCOORD;
        };

        VSOutput main(VSInput vertex)
        {
            VSOutput result;
            float4 worldPos = mul(float4(vertex.pos, 1.0), m);
            result.pos = mul(worldPos, vp);
            result.uv = vertex.uv;
            return result;
        }
    )";

    const char* psSource = R"(
        Texture2D colorTexture : register(t0);
        SamplerState colorSampler : register(s0);

        struct VSOutput
        {
            float4 pos : SV_POSITION;
            float2 uv : TEXCOORD;
        };

        float4 main(VSOutput pixel) : SV_Target0
        {
            return colorTexture.Sample(colorSampler, pixel.uv);
        }
    )";

    ID3DBlob* pVSBlob = nullptr;
    ID3DBlob* pPSBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    result = D3DCompile(
        vsSource,
        strlen(vsSource),
        "VS",
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        flags,
        0,
        &pVSBlob,
        &pErrorBlob
    );

    if (FAILED(result))
    {
        if (pErrorBlob)
        {
            OutputDebugStringA((const char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        }
        return false;
    }

    result = m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),
        pVSBlob->GetBufferSize(), nullptr, &m_pVertexShader);
    if (FAILED(result))
    {
        pVSBlob->Release();
        return false;
    }

    result = D3DCompile(
        psSource,
        strlen(psSource),
        "PS",
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        flags,
        0,
        &pPSBlob,
        &pErrorBlob
    );

    if (FAILED(result))
    {
        if (pErrorBlob)
        {
            OutputDebugStringA((const char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        }
        pVSBlob->Release();
        return false;
    }

    result = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(),
        pPSBlob->GetBufferSize(), nullptr, &m_pPixelShader);
    if (FAILED(result))
    {
        pVSBlob->Release();
        pPSBlob->Release();
        return false;
    }

    static const D3D11_INPUT_ELEMENT_DESC InputDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    result = m_pDevice->CreateInputLayout(InputDesc, 2,
        pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pInputLayout);

    pVSBlob->Release();
    pPSBlob->Release();
    if (pErrorBlob) pErrorBlob->Release();

    return SUCCEEDED(result);
}

bool InitSkybox()
{
    HRESULT result = S_OK;

    static const size_t SphereSteps = 32;
    std::vector<DirectX::XMFLOAT3> sphereVertices;
    std::vector<UINT16> indices;

    size_t vertexCount = (SphereSteps + 1) * (SphereSteps + 1);
    size_t indexCount = SphereSteps * SphereSteps * 6;
    m_sphereIndexCount = (UINT)indexCount;

    sphereVertices.resize(vertexCount);
    indices.resize(indexCount);

    CreateSphere(SphereSteps, SphereSteps, indices.data(), sphereVertices.data());

    D3D11_BUFFER_DESC sphereVbDesc = {};
    sphereVbDesc.ByteWidth = (UINT)(sphereVertices.size() * sizeof(DirectX::XMFLOAT3));
    sphereVbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    sphereVbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    sphereVbDesc.CPUAccessFlags = 0;
    sphereVbDesc.MiscFlags = 0;
    sphereVbDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA sphereVbData;
    sphereVbData.pSysMem = sphereVertices.data();
    sphereVbData.SysMemPitch = 0;
    sphereVbData.SysMemSlicePitch = 0;

    result = m_pDevice->CreateBuffer(&sphereVbDesc, &sphereVbData, &m_pSphereVertexBuffer);
    if (FAILED(result)) return false;

    D3D11_BUFFER_DESC sphereIbDesc = {};
    sphereIbDesc.ByteWidth = (UINT)(indices.size() * sizeof(UINT16));
    sphereIbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    sphereIbDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    sphereIbDesc.CPUAccessFlags = 0;
    sphereIbDesc.MiscFlags = 0;
    sphereIbDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA sphereIbData = {};
    sphereIbData.pSysMem = indices.data();
    sphereIbData.SysMemPitch = 0;
    sphereIbData.SysMemSlicePitch = 0;

    result = m_pDevice->CreateBuffer(&sphereIbDesc, &sphereIbData, &m_pSphereIndexBuffer);
    if (FAILED(result)) return false;

    const char* sphereVSSource = R"(
        cbuffer SceneBuffer : register(b0)
        {
            float4x4 vp;
            float4 cameraPos;
        };

        cbuffer GeomBuffer : register(b1)
        {
            float4x4 m;
            float4 size;
        };

        struct VSInput
        {
            float3 pos : POSITION;
        };

        struct VSOutput
        {
            float4 pos : SV_POSITION;
            float3 localPos : TEXCOORD0;
        };

        VSOutput main(VSInput input)
        {
            VSOutput output;
            float4 worldPos = mul(float4(input.pos * size.x, 1.0), m);
            worldPos.xyz += cameraPos.xyz;
            output.pos = mul(worldPos, vp);
            // КЛЮЧЕВОЕ ИСПРАВЛЕНИЕ: выставляем z = 0 для skybox (reversed depth)
            output.pos.z = 0.0;
            output.localPos = input.pos;
            return output;
        }
    )";

    const char* spherePSSource = R"(
        TextureCube colorTexture : register(t0);
        SamplerState colorSampler : register(s0);

        struct VSOutput
        {
            float4 pos : SV_POSITION;
            float3 localPos : TEXCOORD0;
        };

        float4 main(VSOutput input) : SV_Target0
        {
            return colorTexture.Sample(colorSampler, input.localPos);
        }
    )";

    ID3DBlob* pSphereVSBlob = nullptr;
    ID3DBlob* pSpherePSBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    result = D3DCompile(sphereVSSource, strlen(sphereVSSource), "SphereVS", nullptr, nullptr, "main", "vs_5_0", flags, 0, &pSphereVSBlob, &pErrorBlob);
    if (FAILED(result)) {
        if (pErrorBlob) {
            OutputDebugStringA((const char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        }
        return false;
    }

    result = m_pDevice->CreateVertexShader(pSphereVSBlob->GetBufferPointer(), pSphereVSBlob->GetBufferSize(), nullptr, &m_pSphereVertexShader);
    if (FAILED(result)) {
        pSphereVSBlob->Release();
        return false;
    }

    result = D3DCompile(spherePSSource, strlen(spherePSSource), "SpherePS", nullptr, nullptr, "main", "ps_5_0", flags, 0, &pSpherePSBlob, &pErrorBlob);
    if (FAILED(result)) {
        if (pErrorBlob) {
            OutputDebugStringA((const char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        }
        pSphereVSBlob->Release();
        return false;
    }

    result = m_pDevice->CreatePixelShader(pSpherePSBlob->GetBufferPointer(), pSpherePSBlob->GetBufferSize(), nullptr, &m_pSpherePixelShader);
    if (FAILED(result)) {
        pSphereVSBlob->Release();
        pSpherePSBlob->Release();
        return false;
    }

    D3D11_INPUT_ELEMENT_DESC sphereInputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    result = m_pDevice->CreateInputLayout(sphereInputLayout, 1, pSphereVSBlob->GetBufferPointer(), pSphereVSBlob->GetBufferSize(), &m_pSphereInputLayout);
    pSphereVSBlob->Release();
    pSpherePSBlob->Release();
    if (FAILED(result)) return false;

    D3D11_BUFFER_DESC sphereGeomBufferDesc = {};
    sphereGeomBufferDesc.ByteWidth = sizeof(SphereGeomBuffer);
    sphereGeomBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    sphereGeomBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    sphereGeomBufferDesc.CPUAccessFlags = 0;
    sphereGeomBufferDesc.MiscFlags = 0;

    SphereGeomBuffer sphereGeomData;
    DirectX::XMStoreFloat4x4(&sphereGeomData.m, DirectX::XMMatrixIdentity());

    float n = 0.1f;
    float fov = DirectX::XM_PI / 3;
    float halfW = tanf(fov / 2) * n;
    float halfH = (float)m_height / m_width * halfW;
    float r = sqrtf(n * n + halfH * halfH + halfW * halfW) * 1.1f * 2.0f;

    sphereGeomData.size = DirectX::XMFLOAT4(r, 0.0f, 0.0f, 0.0f);

    D3D11_SUBRESOURCE_DATA sphereGeomInitData = { &sphereGeomData, 0, 0 };
    result = m_pDevice->CreateBuffer(&sphereGeomBufferDesc, &sphereGeomInitData, &m_pSphereGeomBuffer);
    if (FAILED(result)) return false;

    // Загрузка cubemap текстур
    const std::wstring TextureNames[6] = {
        //L"background_posx.dds", L"background_negx.dds",
        //L"background_posy.dds", L"background_negy.dds",
        //L"background_posz.dds", L"background_negz.dds"
        L"posx.dds", L"negx.dds",
        L"posy.dds", L"negy.dds",
        L"posz.dds", L"negz.dds"
    };

    TextureDesc texDescs[6];
    bool ddsRes = true;
    for (int i = 0; i < 6 && ddsRes; i++)
    {
        ddsRes = LoadDDS(TextureNames[i].c_str(), texDescs[i], true);
    }

    if (!ddsRes)
    {
        for (int i = 0; i < 6; i++)
        {
            if (texDescs[i].pData)
                free(texDescs[i].pData);
        }
        return false;
    }

    DXGI_FORMAT textureFmt = texDescs[0].fmt;

    // Проверяем поддержку формата cubemap
    UINT cubemapFormatSupport = 0;
    if (FAILED(m_pDevice->CheckFormatSupport(textureFmt, &cubemapFormatSupport)) ||
        !(cubemapFormatSupport & D3D11_FORMAT_SUPPORT_TEXTURECUBE))
    {
        for (int i = 0; i < 6; i++)
        {
            free(texDescs[i].pData);
        }
        return false;
    }

    // Создаем cubemap текстуру
    D3D11_TEXTURE2D_DESC cubemapDesc = {};
    cubemapDesc.Format = textureFmt;
    cubemapDesc.ArraySize = 6;
    cubemapDesc.MipLevels = 1;
    cubemapDesc.Usage = D3D11_USAGE_IMMUTABLE;
    cubemapDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    cubemapDesc.CPUAccessFlags = 0;
    cubemapDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
    cubemapDesc.SampleDesc.Count = 1;
    cubemapDesc.SampleDesc.Quality = 0;
    cubemapDesc.Height = texDescs[0].height;
    cubemapDesc.Width = texDescs[0].width;

    UINT32 blockWidth = DivUp(cubemapDesc.Width, 4u);
    UINT32 blockHeight = DivUp(cubemapDesc.Height, 4u);
    UINT32 pitch = blockWidth * GetBytesPerBlock(cubemapDesc.Format);

    D3D11_SUBRESOURCE_DATA cubeData[6];
    for (int i = 0; i < 6; i++)
    {
        cubeData[i].pSysMem = texDescs[i].pData;
        cubeData[i].SysMemPitch = pitch;
        cubeData[i].SysMemSlicePitch = 0;
    }

    result = m_pDevice->CreateTexture2D(&cubemapDesc, cubeData, &m_pCubemapTexture);
    if (FAILED(result))
    {
        for (int i = 0; i < 6; i++)
        {
            free(texDescs[i].pData);
        }
        return false;
    }

    for (int i = 0; i < 6; i++)
    {
        free(texDescs[i].pData);
    }

    // Создаем view для cubemap
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = textureFmt;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels = 1;
    srvDesc.TextureCube.MostDetailedMip = 0;

    result = m_pDevice->CreateShaderResourceView(m_pCubemapTexture, &srvDesc, &m_pCubemapView);
    return SUCCEEDED(result);
}

void UpdateCamera()
{
    DirectX::XMFLOAT3 pos;
    pos.x = m_camera.poi.x + m_camera.r * cosf(m_camera.theta) * cosf(m_camera.phi);
    pos.y = m_camera.poi.y + m_camera.r * sinf(m_camera.theta);
    pos.z = m_camera.poi.z + m_camera.r * cosf(m_camera.theta) * sinf(m_camera.phi);

    DirectX::XMVECTOR eye = DirectX::XMVectorSet(pos.x, pos.y, pos.z, 0.0f);
    DirectX::XMVECTOR at = DirectX::XMVectorSet(m_camera.poi.x, m_camera.poi.y, m_camera.poi.z, 0.0f);
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eye, at, up);

    // === REVERSED DEPTH: матрица проекции с far и near в обратном порядке ===
    float f = 100.0f;
    float n = 0.1f;
    float fov = (float)DirectX::XM_PI / 3;
    float aspectRatio = (float)m_width / m_height;

    // Используем перспективную проекцию с reversed depth
    float halfW = tanf(fov / 2) * f;
    float halfH = halfW / aspectRatio;

    DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveLH(
        halfW * 2.0f,
        halfH * 2.0f,
        f,  // far plane передается как near
        n   // near plane передается как far
    );

    DirectX::XMMATRIX vp = DirectX::XMMatrixMultiply(view, proj);

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT result = m_pDeviceContext->Map(m_pSceneBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(result))
    {
        SceneBuffer* sceneBuffer = (SceneBuffer*)mappedResource.pData;
        DirectX::XMMATRIX vpT = DirectX::XMMatrixTranspose(vp);
        DirectX::XMStoreFloat4x4(&sceneBuffer->vp, vpT);
        sceneBuffer->cameraPos = DirectX::XMFLOAT4(pos.x, pos.y, pos.z, 1.0f);
        m_pDeviceContext->Unmap(m_pSceneBuffer, 0);
    }
}

// === НОВАЯ ФУНКЦИЯ ДЛЯ РЕНДЕРИНГА ПРОЗРАЧНЫХ ОБЪЕКТОВ ===
void RenderTransparentObjects()
{
    // Устанавливаем состояние для прозрачных объектов
    m_pDeviceContext->OMSetDepthStencilState(m_pTransDepthState, 0);
    m_pDeviceContext->OMSetBlendState(m_pTransBlendState, nullptr, 0xFFFFFFFF);

    // Настраиваем пайплайн для прямоугольников
    m_pDeviceContext->IASetIndexBuffer(m_pRectIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* rectVertexBuffers[] = { m_pRectVertexBuffer };
    UINT rectStrides[] = { sizeof(ColorVertex) };
    UINT rectOffsets[] = { 0 };
    m_pDeviceContext->IASetVertexBuffers(0, 1, rectVertexBuffers, rectStrides, rectOffsets);
    m_pDeviceContext->IASetInputLayout(m_pRectInputLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->VSSetShader(m_pRectVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pRectPixelShader, nullptr, 0);

    // Получаем позицию камеры из константного буфера
    DirectX::XMFLOAT4 cameraPos;
    {
        D3D11_MAPPED_SUBRESOURCE mapped;
        if (SUCCEEDED(m_pDeviceContext->Map(m_pSceneBuffer, 0, D3D11_MAP_READ, 0, &mapped)))
        {
            SceneBuffer* sceneBuffer = (SceneBuffer*)mapped.pData;
            cameraPos = sceneBuffer->cameraPos;
            m_pDeviceContext->Unmap(m_pSceneBuffer, 0);
        }
    }

    // Вычисляем расстояния от камеры до каждого прямоугольника (от дальнего к ближнему)
    float distances[2] = { 0.0f, 0.0f };
    DirectX::XMVECTOR cameraPosVec = DirectX::XMLoadFloat4(&cameraPos);

    for (int i = 0; i < 4; i++)
    {
        DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&m_boundingRects[0].v[i]);
        DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&m_boundingRects[1].v[i]);

        DirectX::XMVECTOR diff0 = DirectX::XMVectorSubtract(v0, cameraPosVec);
        DirectX::XMVECTOR diff1 = DirectX::XMVectorSubtract(v1, cameraPosVec);

        float dist0 = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(diff0));
        float dist1 = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(diff1));

        distances[0] = std::max<float>(distances[0], dist0);
        distances[1] = std::max<float>(distances[1], dist1);
    }

    // Сортируем индексы по убыванию расстояния (дальние рисуем первыми) - КРИТИЧНО
    int indices[2] = { 0, 1 };
    if (distances[0] < distances[1])
    {
        indices[0] = 1;
        indices[1] = 0;
    }

    // Рисуем прямоугольники в порядке от дальнего к ближнему
    for (int i = 0; i < 2; i++)
    {
        int idx = indices[i];
        ID3D11Buffer* rectGeomBuffer = (idx == 0) ? m_pRectGeomBuffer1 : m_pRectGeomBuffer2;

        ID3D11Buffer* constantBuffers[] = { rectGeomBuffer, m_pSceneBuffer };
        m_pDeviceContext->VSSetConstantBuffers(0, 2, constantBuffers);

        m_pDeviceContext->DrawIndexed(6, 0, 0);
    }
}

void ResizeSwapChain(UINT width, UINT height)
{
    if (width == 0 || height == 0)
        return;

    m_width = width;
    m_height = height;

    if (m_pDeviceContext)
    {
        ID3D11RenderTargetView* nullRTV[1] = { nullptr };
        m_pDeviceContext->OMSetRenderTargets(1, nullRTV, nullptr);
    }

    SAFE_RELEASE(m_pBackBufferRTV);
    SAFE_RELEASE(m_pDepthStencilView);
    SAFE_RELEASE(m_pDepthBuffer);
    SAFE_RELEASE(m_pDepthStencilState);

    if (m_pSwapChain)
    {
        HRESULT hr = m_pSwapChain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr))
        {
            return;
        }
    }

    SetupBackBuffer();

    // Обновляем константный буфер сферы (skybox) при изменении размера
    if (m_pSphereGeomBuffer)
    {
        float n = 0.1f;
        float fov = (float)DirectX::XM_PI / 3;
        float halfW = tanf(fov / 2) * n;
        float halfH = (float)m_height / m_width * halfW;
        float r = sqrtf(n * n + halfH * halfH + halfW * halfW) * 1.1f * 2.0f;

        SphereGeomBuffer geomBuffer;
        DirectX::XMStoreFloat4x4(&geomBuffer.m, DirectX::XMMatrixIdentity());
        geomBuffer.size = DirectX::XMFLOAT4(r, 0.0f, 0.0f, 0.0f);

        m_pDeviceContext->UpdateSubresource(m_pSphereGeomBuffer, 0, nullptr, &geomBuffer, 0, 0);
    }
}

void Render()
{
    if (!m_pDeviceContext || !m_pBackBufferRTV)
        return;

    ID3D11RenderTargetView* views[] = { m_pBackBufferRTV };
    m_pDeviceContext->OMSetRenderTargets(1, views, m_pDepthStencilView);

    // === REVERSED DEPTH: очистка в 0.0f ===
    static const FLOAT BackColor[4] = { 0.25f, 0.25f, 0.5f, 1.0f };
    m_pDeviceContext->ClearRenderTargetView(m_pBackBufferRTV, BackColor);
    if (m_pDepthStencilView)
        m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 0.0f, 0);

    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (FLOAT)m_width;
    viewport.Height = (FLOAT)m_height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_pDeviceContext->RSSetViewports(1, &viewport);

    m_pDeviceContext->RSSetState(m_pRasterizerState);

    UpdateCamera();

    // 1. РЕНДЕРИМ НЕПРОЗРАЧНЫЕ ОБЪЕКТЫ (КУБЫ)
    m_pDeviceContext->OMSetDepthStencilState(m_pNormalDepthState, 0);
    m_pDeviceContext->OMSetBlendState(m_pOpaqueBlendState, nullptr, 0xFFFFFFFF);

    // Рендерим первый куб
    DirectX::XMMATRIX model1 = DirectX::XMMatrixTranslation(-1.0f, 0.0f, 0.0f);
    GeomBuffer geomBufCpu1;
    DirectX::XMMATRIX modelT1 = DirectX::XMMatrixTranspose(model1);
    DirectX::XMStoreFloat4x4(&geomBufCpu1.m, modelT1);
    m_pDeviceContext->UpdateSubresource(m_pGeomBuffer, 0, nullptr, &geomBufCpu1, 0, 0);

    ID3D11Buffer* vertexBuffers[] = { m_pVertexBuffer };
    UINT strides[] = { sizeof(TextureVertex) };
    UINT offsets[] = { 0 };
    m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetInputLayout(m_pInputLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

    ID3D11ShaderResourceView* cubeResources[] = { m_pTextureView };
    m_pDeviceContext->PSSetShaderResources(0, 1, cubeResources);

    ID3D11SamplerState* samplers[] = { m_pSampler };
    m_pDeviceContext->PSSetSamplers(0, 1, samplers);

    ID3D11Buffer* constantBuffers1[] = { m_pGeomBuffer, m_pSceneBuffer };
    m_pDeviceContext->VSSetConstantBuffers(0, 2, constantBuffers1);
    m_pDeviceContext->DrawIndexed(36, 0, 0);

    // Рендерим второй куб
    DirectX::XMMATRIX model2 = DirectX::XMMatrixTranslation(1.0f, 0.0f, 0.0f);
    GeomBuffer geomBufCpu2;
    DirectX::XMMATRIX modelT2 = DirectX::XMMatrixTranspose(model2);
    DirectX::XMStoreFloat4x4(&geomBufCpu2.m, modelT2);
    m_pDeviceContext->UpdateSubresource(m_pGeomBuffer2, 0, nullptr, &geomBufCpu2, 0, 0);

    ID3D11Buffer* constantBuffers2[] = { m_pGeomBuffer2, m_pSceneBuffer };
    m_pDeviceContext->VSSetConstantBuffers(0, 2, constantBuffers2);
    m_pDeviceContext->DrawIndexed(36, 0, 0);

    // 2. РЕНДЕРИМ SKYBOX
    m_pDeviceContext->OMSetDepthStencilState(m_pSkyboxDepthState, 0);
    m_pDeviceContext->OMSetBlendState(m_pOpaqueBlendState, nullptr, 0xFFFFFFFF);

    ID3D11ShaderResourceView* skyboxResources[] = { m_pCubemapView };
    m_pDeviceContext->PSSetShaderResources(0, 1, skyboxResources);

    m_pDeviceContext->IASetIndexBuffer(m_pSphereIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* sphereVertexBuffers[] = { m_pSphereVertexBuffer };
    UINT sphereStrides[] = { sizeof(DirectX::XMFLOAT3) };
    UINT sphereOffsets[] = { 0 };
    m_pDeviceContext->IASetVertexBuffers(0, 1, sphereVertexBuffers, sphereStrides, sphereOffsets);
    m_pDeviceContext->IASetInputLayout(m_pSphereInputLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->VSSetShader(m_pSphereVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pSpherePixelShader, nullptr, 0);

    ID3D11Buffer* sphereConstantBuffers[] = { m_pSceneBuffer, m_pSphereGeomBuffer };
    m_pDeviceContext->VSSetConstantBuffers(0, 2, sphereConstantBuffers);

    m_pDeviceContext->DrawIndexed(m_sphereIndexCount, 0, 0);

    // 3. РЕНДЕРИМ ПРОЗРАЧНЫЕ ОБЪЕКТЫ
    RenderTransparentObjects();

    HRESULT result = m_pSwapChain->Present(1, 0);
    assert(SUCCEEDED(result));
}

void Cleanup()
{
    // Освобождаем ресурсы прозрачных объектов
    SAFE_RELEASE(m_pRectGeomBuffer2);
    SAFE_RELEASE(m_pRectGeomBuffer1);
    SAFE_RELEASE(m_pRectInputLayout);
    SAFE_RELEASE(m_pRectPixelShader);
    SAFE_RELEASE(m_pRectVertexShader);
    SAFE_RELEASE(m_pRectIndexBuffer);
    SAFE_RELEASE(m_pRectVertexBuffer);

    // Освобождаем blend states
    SAFE_RELEASE(m_pOpaqueBlendState);
    SAFE_RELEASE(m_pTransBlendState);

    // Освобождаем состояния глубины
    SAFE_RELEASE(m_pTransDepthState);
    SAFE_RELEASE(m_pSkyboxDepthState);
    SAFE_RELEASE(m_pNormalDepthState);

    SAFE_RELEASE(m_pCubemapView);
    SAFE_RELEASE(m_pCubemapTexture);
    SAFE_RELEASE(m_pSphereGeomBuffer);
    SAFE_RELEASE(m_pSphereInputLayout);
    SAFE_RELEASE(m_pSpherePixelShader);
    SAFE_RELEASE(m_pSphereVertexShader);
    SAFE_RELEASE(m_pSphereIndexBuffer);
    SAFE_RELEASE(m_pSphereVertexBuffer);

    SAFE_RELEASE(m_pSampler);
    SAFE_RELEASE(m_pTextureView);
    SAFE_RELEASE(m_pTexture);

    SAFE_RELEASE(m_pRasterizerState);
    SAFE_RELEASE(m_pDepthStencilState);
    SAFE_RELEASE(m_pDepthStencilView);
    SAFE_RELEASE(m_pDepthBuffer);

    SAFE_RELEASE(m_pGeomBuffer2);
    SAFE_RELEASE(m_pGeomBuffer);
    SAFE_RELEASE(m_pSceneBuffer);
    SAFE_RELEASE(m_pInputLayout);
    SAFE_RELEASE(m_pVertexShader);
    SAFE_RELEASE(m_pPixelShader);
    SAFE_RELEASE(m_pIndexBuffer);
    SAFE_RELEASE(m_pVertexBuffer);
    SAFE_RELEASE(m_pBackBufferRTV);
    SAFE_RELEASE(m_pSwapChain);
    SAFE_RELEASE(m_pDeviceContext);
    SAFE_RELEASE(m_pDevice);
}

// === РЕАЛИЗАЦИЯ ФУНКЦИЙ ДЛЯ РАБОТЫ С DDS ===

UINT32 DivUp(UINT32 a, UINT32 b)
{
    return (a + b - (UINT32)1) / b;
}

UINT32 GetBytesPerBlock(const DXGI_FORMAT& fmt)
{
    switch (fmt)
    {
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        return 8;
        break;

    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return 16;
        break;
    }
    assert(0);
    return 0;
}

std::string WCSToMBS(const std::wstring& wstr)
{
    size_t len = wstr.length();
    std::vector<char> res;
    res.resize(len + 1);
    size_t resLen = 0;
    wcstombs_s(&resLen, res.data(), res.size(), wstr.c_str(), len);
    return res.data();
}

bool LoadDDS(const std::wstring& filepath, TextureDesc& desc, bool singleMip)
{
    FILE* pFile = nullptr;
    _wfopen_s(&pFile, filepath.c_str(), L"rb");
    if (pFile == nullptr)
    {
        return false;
    }

    const UINT32 DDSSignature = 0x20534444;

#pragma pack(push, 1)
    struct PixelFormat
    {
        UINT32 size;
        UINT32 flags;
        UINT32 fourCC;
        UINT32 bitCount;
        UINT32 RMask, GMask, BMask, AMask;
    };

    struct DDSHeader
    {
        UINT32 size;
        UINT32 flags;
        UINT32 height;
        UINT32 width;
        UINT32 pitchOrLinearSize;
        UINT32 depth;
        UINT32 mipMapCount;
        UINT32 reserved[11];
        PixelFormat pixelFormat;
        UINT32 caps, caps2, caps3, caps4;
        UINT32 reserved2;
    };
#pragma pack(pop)

    const UINT32 DDPF_FOURCC = 0x4;
    const UINT32 DDPF_ALPHAPIXELS = 0x1;
    const UINT32 DDSD_CAPS = 0x1;
    const UINT32 DDSD_HEIGHT = 0x2;
    const UINT32 DDSD_WIDTH = 0x4;
    const UINT32 DDSD_PITCH = 0x8;
    const UINT32 DDSD_PIXELFORMAT = 0x1000;
    const UINT32 DDSD_MIPMAPCOUNT = 0x20000;
    const UINT32 DDSD_LINEARSIZE = 0x80000;
    const UINT32 DDSD_DEPTH = 0x800000;

    // Читаем сигнатуру
    UINT32 signature = 0;
    if (fread(&signature, 1, sizeof(UINT32), pFile) != sizeof(UINT32) || signature != DDSSignature)
    {
        fclose(pFile);
        return false;
    }

    // Читаем заголовок DDS
    DDSHeader header;
    memset(&header, 0, sizeof(DDSHeader));
    size_t readSize = fread(&header, 1, sizeof(header), pFile);
    if (readSize != sizeof(DDSHeader) || readSize != header.size)
    {
        fclose(pFile);
        return false;
    }

    // Проверяем флаги
    if (!((header.flags & DDSD_CAPS) != 0 && (header.flags & DDSD_HEIGHT) != 0 &&
        (header.flags & DDSD_WIDTH) != 0 && (header.flags & DDSD_PIXELFORMAT) != 0))
    {
        fclose(pFile);
        return false;
    }

    // Получаем формат текстуры
    char fourCC[5] = { 0 };
    memcpy(fourCC, &header.pixelFormat.fourCC, 4);

    if (strcmp(fourCC, "DXT1") == 0)
    {
        desc.fmt = DXGI_FORMAT_BC1_UNORM;
    }
    else if (strcmp(fourCC, "DXT3") == 0)
    {
        desc.fmt = DXGI_FORMAT_BC2_UNORM;
    }
    else if (strcmp(fourCC, "DXT5") == 0)
    {
        desc.fmt = DXGI_FORMAT_BC3_UNORM;
    }
    else
    {
        fclose(pFile);
        return false;
    }

    // Читаем pitch
    desc.pitch = (header.flags & DDSD_PITCH) != 0 ? (UINT32)header.pitchOrLinearSize : 0;

    // Читаем количество mip уровней
    desc.mipmapsCount = (header.flags & DDSD_MIPMAPCOUNT) != 0 ? (UINT32)header.mipMapCount : 1;

    if (singleMip)
    {
        desc.mipmapsCount = 1;
    }

    // Устанавливаем размер изображения
    desc.width = header.width;
    desc.height = header.height;

    // Получаем размер данных
    UINT32 dataSize = (header.flags & DDSD_LINEARSIZE) != 0 ? (UINT32)header.pitchOrLinearSize : 0;
    if (dataSize == 0)
    {
        long long curPos = _ftelli64(pFile);
        fseek(pFile, 0, SEEK_END);
        dataSize = (UINT32)(_ftelli64(pFile) - curPos);
        fseek(pFile, (int)curPos, SEEK_SET);
    }
    else
    {
        UINT32 levelSize = dataSize / 4;
        for (UINT32 i = 1; i < desc.mipmapsCount; i++)
        {
            dataSize += levelSize;
            levelSize = std::max<UINT32>(16u, levelSize / 4);
        }
    }

    desc.pData = malloc(dataSize);
    readSize = fread(desc.pData, 1, dataSize, pFile);
    if (readSize != dataSize)
    {
        free(desc.pData);
        desc.pData = nullptr;
        fclose(pFile);
        return false;
    }

    fclose(pFile);
    return true;
}