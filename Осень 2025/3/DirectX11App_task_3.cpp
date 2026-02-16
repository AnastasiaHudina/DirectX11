#include <algorithm>
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <assert.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <cstring>

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
ID3D11Buffer* m_pGeomBuffer = nullptr;
ID3D11Buffer* m_pSceneBuffer = nullptr;

// === ПЕРЕМЕННЫЕ ДЛЯ БУФЕРА ГЛУБИНЫ ===
ID3D11Texture2D* m_pDepthBuffer = nullptr;
ID3D11DepthStencilView* m_pDepthStencilView = nullptr;
ID3D11DepthStencilState* m_pDepthStencilState = nullptr;
ID3D11RasterizerState* m_pRasterizerState = nullptr;

UINT m_width = 1280;
UINT m_height = 720;

// === ПЕРЕМЕННЫЕ ДЛЯ УПРАВЛЕНИЯ КАМЕРОЙ ===
struct Camera
{
    DirectX::XMFLOAT3 poi = { 0.0f, 0.0f, 0.0f }; // Point of interest
    float r = 3.0f; // Distance from POI
    float theta = 0.0f; // Vertical angle
    float phi = 0.0f; // Horizontal angle
} m_camera;

bool m_rbPressed = false;
int m_prevMouseX = 0, m_prevMouseY = 0;

static const float CameraRotationSpeed = DirectX::XM_PI * 2.0f;

#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = nullptr; } }

// === СТРУКТУРА ВЕРШИНЫ ===
struct Vertex
{
    float x, y, z;
    float r, g, b, a;
};

// === СТРУКТУРЫ ДЛЯ КОНСТАНТНЫХ БУФЕРОВ ===
struct GeomBuffer
{
    DirectX::XMFLOAT4X4 m;
};

struct SceneBuffer
{
    DirectX::XMFLOAT4X4 vp;
};

// Прототипы функций
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
bool InitWindow(HINSTANCE hInstance, int nCmdShow);
bool InitDirectX();
bool InitCube();
bool InitBuffers();
void Render();
void Cleanup();
void ResizeSwapChain(UINT width, UINT height);
void UpdateCamera();
bool SetupBackBuffer();

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
        L"DirectX 11 - Исправленный кубик",
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

            float dx = -(float)(currentX - m_prevMouseX) / (float)std::max<UINT>(1, m_width) * CameraRotationSpeed;
            float dy = (float)(currentY - m_prevMouseY) / (float)std::max<UINT>(1, m_height) * CameraRotationSpeed;

            m_camera.phi += dx;
            m_camera.theta += dy;

            // Ограничиваем вертикальный угол
            m_camera.theta = std::min<float>(std::max<float>(m_camera.theta, -(float)DirectX::XM_PIDIV2 + 0.001f), (float)DirectX::XM_PIDIV2 - 0.001f);

            m_prevMouseX = currentX;
            m_prevMouseY = currentY;
        }
        break;

    case WM_MOUSEWHEEL:
    {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        m_camera.r -= delta / 1200.0f;
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

    // Создаем устройство и swap chain
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
        return false;

    // Настраиваем back buffer
    if (!SetupBackBuffer())
        return false;

    // Создаем растеризатор
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.CullMode = D3D11_CULL_BACK;
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

    return true;
}

bool SetupBackBuffer()
{
    HRESULT result = S_OK;

    SAFE_RELEASE(m_pBackBufferRTV);
    SAFE_RELEASE(m_pDepthStencilView);
    SAFE_RELEASE(m_pDepthBuffer);
    SAFE_RELEASE(m_pDepthStencilState);

    // Получаем back buffer
    ID3D11Texture2D* pBackBuffer = nullptr;
    result = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    if (FAILED(result)) return false;

    result = m_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pBackBufferRTV);
    SAFE_RELEASE(pBackBuffer);
    if (FAILED(result)) return false;

    // Создаем буфер глубины
    D3D11_TEXTURE2D_DESC depthDesc = {};
    depthDesc.Width = m_width;
    depthDesc.Height = m_height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
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

    // Создаем состояние глубины
    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = true;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
    depthStencilDesc.StencilEnable = false;

    result = m_pDevice->CreateDepthStencilState(&depthStencilDesc, &m_pDepthStencilState);
    if (FAILED(result)) return false;

    return true;
}

bool InitBuffers()
{
    HRESULT result = S_OK;

    // Создаем буфер для матрицы модели
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(GeomBuffer);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    result = m_pDevice->CreateBuffer(&desc, nullptr, &m_pGeomBuffer);
    if (FAILED(result)) return false;

    // Создаем буфер для матрицы вида-проекции
    desc = {};
    desc.ByteWidth = sizeof(SceneBuffer);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    result = m_pDevice->CreateBuffer(&desc, nullptr, &m_pSceneBuffer);
    return SUCCEEDED(result);
}

bool InitCube()
{
    HRESULT result = S_OK;

    // Вершины куба (8 уникальных вершин)
    static const Vertex Vertices[] = {
        // Нижняя грань
        { -0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f }, // 0
        {  0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f }, // 1
        {  0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 1.0f }, // 2
        { -0.5f, -0.5f,  0.5f, 1.0f, 0.0f, 0.0f, 1.0f }, // 3

        // Верхняя грань
        { -0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f }, // 4
        {  0.5f,  0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f }, // 5
        {  0.5f,  0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f }, // 6
        { -0.5f,  0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f }, // 7
    };

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(Vertices);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = sizeof(Vertex);

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = Vertices;
    data.SysMemPitch = 0;
    data.SysMemSlicePitch = 0;

    result = m_pDevice->CreateBuffer(&desc, &data, &m_pVertexBuffer);
    if (FAILED(result)) return false;

    // Индексы для куба
    static const USHORT Indices[] = {
        // Нижняя грань
        0, 1, 2, 0, 2, 3,
        // Верхняя грань
        4, 6, 5, 4, 7, 6,
        // Передняя грань
        3, 2, 6, 3, 6, 7,
        // Задняя грань
        4, 5, 1, 4, 1, 0,
        // Левая грань
        4, 0, 3, 4, 3, 7,
        // Правая грань
        1, 5, 6, 1, 6, 2
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

    // Шейдеры
    const char* vsSource = R"(
        cbuffer GeomBuffer : register(b0)
        {
            float4x4 m;
        };
        
        cbuffer SceneBuffer : register(b1)
        {
            float4x4 vp;
        };

        struct VSInput
        {
            float3 pos : POSITION;
            float4 color : COLOR;
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
            result.color = vertex.color;
            return result;
        }
    )";

    const char* psSource = R"(
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

    ID3DBlob* pVSBlob = nullptr;
    ID3DBlob* pPSBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    // Компилируем вершинный шейдер
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

    // Компилируем пиксельный шейдер
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

    // Создаем input layout
    static const D3D11_INPUT_ELEMENT_DESC InputDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    result = m_pDevice->CreateInputLayout(InputDesc, 2,
        pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pInputLayout);

    pVSBlob->Release();
    pPSBlob->Release();
    if (pErrorBlob) pErrorBlob->Release();

    return SUCCEEDED(result);
}

void UpdateCamera()
{
    // Вычисляем позицию камеры в сферических координатах
    DirectX::XMFLOAT3 pos;
    pos.x = m_camera.poi.x + m_camera.r * cosf(m_camera.theta) * cosf(m_camera.phi);
    pos.y = m_camera.poi.y + m_camera.r * sinf(m_camera.theta);
    pos.z = m_camera.poi.z + m_camera.r * cosf(m_camera.theta) * sinf(m_camera.phi);

    DirectX::XMVECTOR eye = DirectX::XMVectorSet(pos.x, pos.y, pos.z, 1.0f);
    DirectX::XMVECTOR at = DirectX::XMVectorSet(m_camera.poi.x, m_camera.poi.y, m_camera.poi.z, 1.0f);
    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(eye, at, up);

    float fov = DirectX::XM_PI / 3.0f;
    float aspectRatio = (float)m_width / (float)m_height;
    float nearZ = 0.1f;
    float farZ = 100.0f;

    DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(fov, aspectRatio, nearZ, farZ);

    // Матрица вида-проекции
    DirectX::XMMATRIX vp = view * proj;

    // Записываем в буфер (транспонируем для HLSL)
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT result = m_pDeviceContext->Map(m_pSceneBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(result))
    {
        SceneBuffer* sceneBuffer = (SceneBuffer*)mappedResource.pData;
        DirectX::XMMATRIX vpT = DirectX::XMMatrixTranspose(vp);
        DirectX::XMStoreFloat4x4(&sceneBuffer->vp, vpT);
        m_pDeviceContext->Unmap(m_pSceneBuffer, 0);
    }
}

void ResizeSwapChain(UINT width, UINT height)
{
    if (width == 0 || height == 0)
        return;

    m_width = width;
    m_height = height;

    // Убираем привязанные рендер-таргеты
    if (m_pDeviceContext)
    {
        ID3D11RenderTargetView* nullRTV[1] = { nullptr };
        m_pDeviceContext->OMSetRenderTargets(1, nullRTV, nullptr);
    }

    SAFE_RELEASE(m_pBackBufferRTV);
    SAFE_RELEASE(m_pDepthStencilView);
    SAFE_RELEASE(m_pDepthBuffer);
    SAFE_RELEASE(m_pDepthStencilState);

    // ResizeBuffers
    if (m_pSwapChain)
    {
        HRESULT hr = m_pSwapChain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr))
        {
            return;
        }
    }

    // Восстанавливаем backbuffer и depth
    SetupBackBuffer();
}

void Render()
{
    if (!m_pDeviceContext || !m_pBackBufferRTV)
        return;

    // Устанавливаем рендер-таргеты
    ID3D11RenderTargetView* views[] = { m_pBackBufferRTV };
    m_pDeviceContext->OMSetRenderTargets(1, views, m_pDepthStencilView);

    // Устанавливаем состояния
    m_pDeviceContext->OMSetDepthStencilState(m_pDepthStencilState, 0);
    m_pDeviceContext->RSSetState(m_pRasterizerState);

    // Очищаем буферы
    static const FLOAT BackColor[4] = { 0.25f, 0.25f, 0.5f, 1.0f }; // ← ЭТО ЦВЕТ ФОНА
    m_pDeviceContext->ClearRenderTargetView(m_pBackBufferRTV, BackColor);
    if (m_pDepthStencilView)
        m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // Устанавливаем viewport
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (FLOAT)m_width;
    viewport.Height = (FLOAT)m_height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_pDeviceContext->RSSetViewports(1, &viewport);

    // Обновляем камеру
    UpdateCamera();

    // Модельная матрица (статичный куб)
    DirectX::XMMATRIX model = DirectX::XMMatrixIdentity();
    GeomBuffer geomBufCpu;
    DirectX::XMMATRIX modelT = DirectX::XMMatrixTranspose(model);
    DirectX::XMStoreFloat4x4(&geomBufCpu.m, modelT);

    // Обновляем ресурс
    m_pDeviceContext->UpdateSubresource(m_pGeomBuffer, 0, nullptr, &geomBufCpu, 0, 0);

    // Устанавливаем вершинный и индексный буферы
    ID3D11Buffer* vertexBuffers[] = { m_pVertexBuffer };
    UINT strides[] = { sizeof(Vertex) };
    UINT offsets[] = { 0 };
    m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);

    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetInputLayout(m_pInputLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Устанавливаем шейдеры
    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

    // Устанавливаем константные буферы
    ID3D11Buffer* constantBuffers[] = { m_pGeomBuffer, m_pSceneBuffer };
    m_pDeviceContext->VSSetConstantBuffers(0, 2, constantBuffers);

    // Отрисовываем куб (36 индексов)
    m_pDeviceContext->DrawIndexed(36, 0, 0);

    // Показываем результат
    HRESULT result = m_pSwapChain->Present(1, 0);
    assert(SUCCEEDED(result));
}

void Cleanup()
{
    SAFE_RELEASE(m_pRasterizerState);
    SAFE_RELEASE(m_pDepthStencilState);
    SAFE_RELEASE(m_pDepthStencilView);
    SAFE_RELEASE(m_pDepthBuffer);

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