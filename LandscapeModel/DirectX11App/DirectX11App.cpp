// === ПОЯСНЕНИЕ К УПРАВЛЕНИЮ ===
//На мышь - приближать/отдалять
//Правая кнопка мыши - вращение камерой
//Клавиши wasd - положение камеры относительно направления взгляда
//Клавиши qe - положение камеры по мировой оси Y (вверх/вниз)

// Standard Windows Headers
#include <windows.h>
#include <windowsx.h>

// DirectX Headers
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <DirectXMath.h>

// ImGui Headers - добавляем файлы из папки "imgui"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

// C++ Standard Library
#include <algorithm>
#include <assert.h>
#include <cstring>
#include <vector>
#include <string>
#include <malloc.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// Включаем заголовочные файл для работы с DDS и освещением
#include "DDS.h"
#include "Light.h"

#define TINYEXR_IMPLEMENTATION
#include "tinyexr/tinyexr.h"

// Явное объявление для ImGui функции
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

enum PostProcessEffect {
    PP_NONE = 0,
    PP_SEPIA = 1,
    PP_COLD_TINT = 2,
    PP_NIGHT_VISION = 3
};

// === СТРУКТУРЫ ДЛЯ КОНСТАНТНЫХ БУФЕРОВ ===
struct GeomBuffer
{
    DirectX::XMFLOAT4X4 m;
    DirectX::XMFLOAT4X4 normalM;
    DirectX::XMFLOAT4 shineSpeedTexIdNM;
    DirectX::XMFLOAT4 posAngle;
};

struct SceneBuffer
{
    DirectX::XMFLOAT4X4 vp;
    DirectX::XMFLOAT4 cameraPos;
    DirectX::XMFLOAT4 lightInfo;
    Light lights[10];
    DirectX::XMFLOAT4 ambientColor;

};

struct PostProcessBuffer
{
    int effectType;    // 0 - нет, 1 - сепия, 2 - холодный тон, 3 - ночной режим
    int padding[3];    // Выравнивание
};

struct SmallSphereGeomBuffer
{
    DirectX::XMFLOAT4X4 m;
    DirectX::XMFLOAT4 color;
};

// === ПРОТОТИПЫ ФУНКЦИЙ ===
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
bool InitWindow(HINSTANCE hInstance, int nCmdShow);
bool InitDirectX();
bool InitTerrain();
bool InitShaders();
bool InitBuffers();
bool LoadTextureArray();
bool LoadNormalMap();
bool InitSmallSpheres();
void Render();
void Cleanup();
void ResizeSwapChain(UINT width, UINT height);
void UpdateCamera();
bool SetupBackBuffer();
bool InitColorBuffer();
void RenderSmallSpheres();
void CreateSphere(size_t latCells, size_t lonCells, UINT16* pIndices, DirectX::XMFLOAT3* pPos);

// Глобальные переменные
HWND g_hWnd = NULL;
ID3D11Device* m_pDevice = nullptr;
ID3D11DeviceContext* m_pDeviceContext = nullptr;
IDXGISwapChain* m_pSwapChain = nullptr;
ID3D11RenderTargetView* m_pBackBufferRTV = nullptr;

// === ПЕРЕМЕННЫЕ ДЛЯ ОСВЕЩЕНИЯ ===
Light m_lights[10]; // Массив источников света
DirectX::XMFLOAT4 m_ambientColor = { 0.1f, 0.1f, 0.2f, 1.0f };
int m_lightCount = 1; // Количество активных источников света
bool m_useNormalMaps = true;
bool m_showNormals = false;
bool m_showLightBulbs = true;

// === ПЕРЕМЕННЫЕ ДЛЯ КАРТЫ НОРМАЛЕЙ ===
ID3D11Texture2D* m_pTextureNM = nullptr;
ID3D11ShaderResourceView* m_pTextureViewNM = nullptr;

// === ПЕРЕМЕННЫЕ ДЛЯ ПОСТПРОЦЕССИНГА ===
ID3D11Texture2D* m_pColorBuffer = nullptr;
ID3D11RenderTargetView* m_pColorBufferRTV = nullptr;
ID3D11ShaderResourceView* m_pColorBufferSRV = nullptr;
ID3D11PixelShader* m_pPostProcessPixelShader = nullptr;
ID3D11VertexShader* m_pPostProcessVertexShader = nullptr;
ID3D11Buffer* m_pPostProcessBuffer = nullptr;
int m_postProcessEffect = 0;            // 0 - нет эффекта

// === ПЕРЕМЕННЫЕ ДЛЯ МАЛЕНЬКИХ СФЕР (ВИЗУАЛИЗАЦИЯ ИСТОЧНИКОВ) ===
ID3D11Buffer* m_pSmallSphereVertexBuffer = nullptr;
ID3D11Buffer* m_pSmallSphereIndexBuffer = nullptr;
ID3D11VertexShader* m_pSmallSphereVertexShader = nullptr;
ID3D11PixelShader* m_pSmallSpherePixelShader = nullptr;
ID3D11InputLayout* m_pSmallSphereInputLayout = nullptr;
ID3D11Buffer* m_pSmallSphereGeomBuffers[10]; // Константные буферы для каждой сферы
UINT m_smallSphereIndexCount = 0;

// === ПЕРЕМЕННЫЕ ДЛЯ ImGui ===
bool m_showImGui = true;

// === ПЕРЕМЕННЫЕ ДЛЯ ПЕРЕМЕЩЕНИЯ КАМЕРЫ ===
bool m_keyW = false;
bool m_keyA = false;
bool m_keyS = false;
bool m_keyD = false;
bool m_keyQ = false;
bool m_keyE = false;

// === ПЕРЕМЕННЫЕ ДЛЯ КУБИКА === (???)
ID3D11Buffer* m_pVertexBuffer = nullptr;
ID3D11Buffer* m_pIndexBuffer = nullptr;
ID3D11VertexShader* m_pVertexShader = nullptr;
ID3D11PixelShader* m_pPixelShader = nullptr;
ID3D11InputLayout* m_pInputLayout = nullptr;

// === ПЕРЕМЕННЫЕ ДЛЯ ЛАНДШАФТА ===
ID3D11Buffer* m_pTerrainVertexBuffer = nullptr;
ID3D11Buffer* m_pTerrainIndexBuffer = nullptr;
ID3D11Buffer* m_pTerrainGeomBuffer = nullptr;
UINT m_terrainIndexCount = 0;
UINT m_terrainGridSizeX = 256;      // кол-во вершин по X
UINT m_terrainGridSizeZ = 256;      // кол-во вершин по Z
float m_terrainWidth = 20.0f;       // ширина ландшафта по X
float m_terrainDepth = 20.0f;       // глубина ландшафта по Z
float m_terrainHeightScale = 2.0f;  // масштаб высоты


// === ПЕРЕМЕННЫЕ ДЛЯ МАТРИЦ И УПРАВЛЕНИЯ ===
ID3D11Buffer* m_pSceneBuffer = nullptr;

// === ПЕРЕМЕННЫЕ ДЛЯ БУФЕРА ГЛУБИНЫ (D32_FLOAT) ===
ID3D11Texture2D* m_pDepthBuffer = nullptr;
ID3D11DepthStencilView* m_pDepthStencilView = nullptr;
ID3D11RasterizerState* m_pRasterizerState = nullptr;

// === ПЕРЕМЕННЫЕ ДЛЯ СОСТОЯНИЙ ГЛУБИНЫ ===
ID3D11DepthStencilState* m_pNormalDepthState = nullptr;     // Для непрозрачных объектов

// === ПЕРЕМЕННЫЕ ДЛЯ BLEND STATES ===
ID3D11BlendState* m_pOpaqueBlendState = nullptr;    // Для непрозрачных объектов

// === ПЕРЕМЕННЫЕ ДЛЯ ТЕКСТУР ===
ID3D11Texture2D* m_pTexture = nullptr;
ID3D11ShaderResourceView* m_pTextureView = nullptr;
ID3D11SamplerState* m_pSampler = nullptr;

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

// === СТРУКТУРА ВЕРШИНЫ С НОРМАЛЯМИ И КАСАТЕЛЬНЫМИ ===
struct TextureTangentVertex
{
    float x, y, z;           // Позиция
    float tx, ty, tz;        // Касательный вектор (tangent)
    float nx, ny, nz;        // Нормаль
    float u, v;              // Текстурные координаты
};

bool InitShaders()
{
    HRESULT result = S_OK;

    // Вершинный шейдер (тот же, что был для куба)
    const char* vsSource = R"(
    cbuffer GeomBuffer : register(b0)
    {
        float4x4 m;
        float4x4 normalM;
        float4 shineSpeedTexIdNM; // x - shine, z - texId, w - hasNormalMap
        float4 posAngle; // не используется
    };
    
    cbuffer SceneBuffer : register(b1)
    {
        float4x4 vp;
        float4 cameraPos;
        float4 lightInfo; // x - light count, y - use normal maps, z - show normals, w - unused
        struct Light { float4 pos; float4 color; };
        Light lights[10];
        float4 ambientColor;
    };

    struct VSInput
    {
        float3 pos : POSITION;
        float3 tangent : TANGENT;
        float3 norm : NORMAL;
        float2 uv : TEXCOORD;
    };

    struct VSOutput
    {
        float4 pos : SV_POSITION;
        float3 worldPos : POSITION;
        float3 tangent : TANGENT;
        float3 norm : NORMAL;
        float2 uv : TEXCOORD;
        nointerpolation float4 geomData : GEOM_DATA; // передадим shine и texId
    };

    VSOutput main(VSInput vertex)
    {
        VSOutput result;
        float4 worldPos = mul(float4(vertex.pos, 1.0), m);
        result.pos = mul(worldPos, vp);
        result.worldPos = worldPos.xyz;
        
        result.tangent = mul(float4(vertex.tangent, 0.0f), normalM).xyz;
        result.norm = mul(float4(vertex.norm, 0.0f), normalM).xyz;
        result.uv = vertex.uv;
        result.geomData = shineSpeedTexIdNM;
        return result;
    }
)";

    const char* psSource = R"(
    Texture2DArray colorTexture : register(t0);
    Texture2D normalMapTexture : register(t1);
    SamplerState colorSampler : register(s0);

    cbuffer SceneBuffer : register(b1)
    {
        float4x4 vp;
        float4 cameraPos;
        float4 lightInfo; // x - light count, y - use normal maps, z - show normals
        struct Light { float4 pos; float4 color; };
        Light lights[10];
        float4 ambientColor;
    };

    struct VSOutput
    {
        float4 pos : SV_POSITION;
        float3 worldPos : POSITION;
        float3 tangent : TANGENT;
        float3 norm : NORMAL;
        float2 uv : TEXCOORD;
        nointerpolation float4 geomData : GEOM_DATA; // x=shine, z=texId, w=hasNormalMap
    };

    float3 CalculateLighting(float3 objColor, float3 objNormal, float3 pos, float shine)
    {
        float3 finalColor = float3(0, 0, 0);
        
        if (lightInfo.z > 0.5)
        {
            return objNormal * 0.5 + float3(0.5, 0.5, 0.5);
        }
        
        finalColor = objColor * ambientColor.rgb;
        
        for (int i = 0; i < (int)lightInfo.x; i++)
        {
            float3 lightDir = lights[i].pos.xyz - pos;
            float lightDist = length(lightDir);
            lightDir /= lightDist;
            
            float atten = 1.0 / (lightDist * lightDist);
            atten = clamp(atten, 0.0, 1.0);
            
            float diffuse = max(dot(lightDir, objNormal), 0.0);
            finalColor += objColor * diffuse * atten * lights[i].color.rgb;
            
            if (shine > 0.0)
            {
                float3 viewDir = normalize(cameraPos.xyz - pos);
                float3 reflectDir = reflect(-lightDir, objNormal);
                float specular = pow(max(dot(viewDir, reflectDir), 0.0), shine);
                finalColor += objColor * 0.5 * specular * atten * lights[i].color.rgb;
            }
        }
        
        return finalColor;
    }

    float4 main(VSOutput pixel) : SV_Target0
    {
        int texId = (int)pixel.geomData.z;
        float3 color = colorTexture.Sample(colorSampler, float3(pixel.uv, texId)).rgb;

        float3 normal = normalize(pixel.norm);
        
        if (lightInfo.y > 0.5 && pixel.geomData.w > 0.5)
        {
            float3 texNormal = normalMapTexture.Sample(colorSampler, pixel.uv).rgb;
            texNormal = texNormal * 2.0 - 1.0;
            
            float3 bitangent = normalize(cross(pixel.tangent, pixel.norm));
            float3x3 TBN = float3x3(pixel.tangent, bitangent, pixel.norm);
            normal = normalize(mul(texNormal, TBN));
        }
        
        float3 finalColor = CalculateLighting(color, normal, pixel.worldPos, pixel.geomData.x);
        return float4(finalColor, 1.0);
    }
)";

    ID3DBlob* pVSBlob = nullptr;
    ID3DBlob* pPSBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    result = D3DCompile(vsSource, strlen(vsSource), "VS", nullptr, nullptr, "main", "vs_5_0", flags, 0, &pVSBlob, &pErrorBlob);
    if (FAILED(result))
    {
        if (pErrorBlob) { OutputDebugStringA((const char*)pErrorBlob->GetBufferPointer()); pErrorBlob->Release(); }
        return false;
    }

    result = m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), nullptr, &m_pVertexShader);
    if (FAILED(result))
    {
        pVSBlob->Release();
        return false;
    }

    result = D3DCompile(psSource, strlen(psSource), "PS", nullptr, nullptr, "main", "ps_5_0", flags, 0, &pPSBlob, &pErrorBlob);
    if (FAILED(result))
    {
        if (pErrorBlob) { OutputDebugStringA((const char*)pErrorBlob->GetBufferPointer()); pErrorBlob->Release(); }
        pVSBlob->Release();
        return false;
    }

    result = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), nullptr, &m_pPixelShader);
    if (FAILED(result))
    {
        pVSBlob->Release();
        pPSBlob->Release();
        return false;
    }

    static const D3D11_INPUT_ELEMENT_DESC InputDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    result = m_pDevice->CreateInputLayout(InputDesc, 4,
        pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &m_pInputLayout);

    pVSBlob->Release();
    pPSBlob->Release();
    if (pErrorBlob) pErrorBlob->Release();

    return SUCCEEDED(result);
}

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
bool LoadTextureArray()
{
    HRESULT result = S_OK;

    // Загружаем две текстуры: Brick.dds и Kitty.dds
    TextureDesc textureDesc[2];
    //bool ddsRes = LoadDDS(L"Brick.dds", textureDesc[0]);
    bool ddsRes = LoadDDS(L"Ice.dds", textureDesc[0]);
    if (ddsRes)
        ddsRes = LoadDDS(L"Ice.dds", textureDesc[1]);

    // Создаем массив текстур
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Format = textureDesc[0].fmt;
    desc.ArraySize = 2;  // Две текстуры в массиве
    desc.MipLevels = textureDesc[0].mipmapsCount;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Height = textureDesc[0].height;
    desc.Width = textureDesc[0].width;

    // Подготавливаем данные для массива
    std::vector<D3D11_SUBRESOURCE_DATA> data;
    data.resize(desc.MipLevels * 2);

    for (UINT32 j = 0; j < 2; j++)
    {
        UINT32 blockWidth = DivUp(desc.Width, 4u);
        UINT32 blockHeight = DivUp(desc.Height, 4u);
        UINT32 pitch = blockWidth * GetBytesPerBlock(desc.Format);
        const char* pSrcData = reinterpret_cast<const char*>(textureDesc[j].pData);

        for (UINT32 i = 0; i < desc.MipLevels; i++)
        {
            data[j * desc.MipLevels + i].pSysMem = pSrcData;
            data[j * desc.MipLevels + i].SysMemPitch = pitch;
            data[j * desc.MipLevels + i].SysMemSlicePitch = 0;

            pSrcData += pitch * blockHeight;
            blockHeight = std::max<UINT32>(1u, blockHeight / 2);
            blockWidth = std::max<UINT32>(1u, blockWidth / 2);
            pitch = blockWidth * GetBytesPerBlock(desc.Format);
        }
    }

    result = m_pDevice->CreateTexture2D(&desc, data.data(), &m_pTexture);

    // Освобождаем данные
    for (UINT32 j = 0; j < 2; j++)
        if (textureDesc[j].pData)
            free(textureDesc[j].pData);

    if (FAILED(result))
        return false;

    // Создаем SRV для массива текстур
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.ArraySize = 2;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.MipLevels = desc.MipLevels;
    srvDesc.Texture2DArray.MostDetailedMip = 0;

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

bool LoadNormalMap()
{
    HRESULT result = S_OK;

    TextureDesc textureDesc;
    //if (!LoadDDS(L"BrickNM.dds", textureDesc))
    if (!LoadDDS(L"landscape/Terrain003_4K_NM2.dds", textureDesc))
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

    result = m_pDevice->CreateTexture2D(&desc, data.data(), &m_pTextureNM);
    free(textureDesc.pData);

    if (FAILED(result))
        return false;

    // Создаем view для текстуры
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = desc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;

    result = m_pDevice->CreateShaderResourceView(m_pTextureNM, &srvDesc, &m_pTextureViewNM);
    return SUCCEEDED(result);
}

bool InitSmallSpheres()
{
    HRESULT result = S_OK;

    // Параметры сферы (меньше, чем skybox)
    static const size_t SphereSteps = 8;
    std::vector<DirectX::XMFLOAT3> sphereVertices;
    std::vector<UINT16> indices;

    size_t vertexCount = (SphereSteps + 1) * (SphereSteps + 1);
    size_t indexCount = SphereSteps * SphereSteps * 6;
    m_smallSphereIndexCount = (UINT)indexCount;

    sphereVertices.resize(vertexCount);
    indices.resize(indexCount);

    CreateSphere(SphereSteps, SphereSteps, indices.data(), sphereVertices.data());

    // Уменьшаем размер сферы (источники света маленькие)
    for (auto& v : sphereVertices)
    {
        v.x *= 0.125f;
        v.y *= 0.125f;
        v.z *= 0.125f;
    }

    // Создаем vertex buffer
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = (UINT)(sphereVertices.size() * sizeof(DirectX::XMFLOAT3));
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;
    vbDesc.MiscFlags = 0;
    vbDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA vbData;
    vbData.pSysMem = sphereVertices.data();
    vbData.SysMemPitch = 0;
    vbData.SysMemSlicePitch = 0;

    result = m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pSmallSphereVertexBuffer);
    if (FAILED(result)) return false;

    // Создаем index buffer
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = (UINT)(indices.size() * sizeof(UINT16));
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibDesc.CPUAccessFlags = 0;
    ibDesc.MiscFlags = 0;
    ibDesc.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();
    ibData.SysMemPitch = 0;
    ibData.SysMemSlicePitch = 0;

    result = m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pSmallSphereIndexBuffer);
    if (FAILED(result)) return false;

    // Шейдеры для маленьких сфер
    const char* smallSphereVSSource = R"(
        cbuffer GeomBuffer : register(b0)
        {
            float4x4 m;
            float4 color;
        };
        
        cbuffer SceneBuffer : register(b1)
        {
            float4x4 vp;
            float4 cameraPos;
            float4 lightInfo;
            struct Light { float4 pos; float4 color; };
            Light lights[10];
            float4 ambientColor;
        };

        struct VSInput
        {
            float3 pos : POSITION;
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
            result.color = color;
            return result;
        }
    )";

    const char* smallSpherePSSource = R"(
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
    ID3DBlob* pSmallSphereVSBlob = nullptr;
    ID3DBlob* pSmallSpherePSBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    result = D3DCompile(smallSphereVSSource, strlen(smallSphereVSSource),
        "SmallSphereVS", nullptr, nullptr, "main", "vs_5_0", flags, 0, &pSmallSphereVSBlob, &pErrorBlob);
    if (FAILED(result)) {
        if (pErrorBlob) {
            OutputDebugStringA((const char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        }
        return false;
    }

    result = m_pDevice->CreateVertexShader(pSmallSphereVSBlob->GetBufferPointer(),
        pSmallSphereVSBlob->GetBufferSize(), nullptr, &m_pSmallSphereVertexShader);
    if (FAILED(result)) {
        pSmallSphereVSBlob->Release();
        return false;
    }

    result = D3DCompile(smallSpherePSSource, strlen(smallSpherePSSource),
        "SmallSpherePS", nullptr, nullptr, "main", "ps_5_0", flags, 0, &pSmallSpherePSBlob, &pErrorBlob);
    if (FAILED(result)) {
        if (pErrorBlob) {
            OutputDebugStringA((const char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        }
        pSmallSphereVSBlob->Release();
        return false;
    }

    result = m_pDevice->CreatePixelShader(pSmallSpherePSBlob->GetBufferPointer(),
        pSmallSpherePSBlob->GetBufferSize(), nullptr, &m_pSmallSpherePixelShader);
    if (FAILED(result)) {
        pSmallSphereVSBlob->Release();
        pSmallSpherePSBlob->Release();
        return false;
    }

    // Создаем input layout для маленьких сфер
    D3D11_INPUT_ELEMENT_DESC smallSphereLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    result = m_pDevice->CreateInputLayout(smallSphereLayout, 1,
        pSmallSphereVSBlob->GetBufferPointer(),
        pSmallSphereVSBlob->GetBufferSize(),
        &m_pSmallSphereInputLayout);

    pSmallSphereVSBlob->Release();
    pSmallSpherePSBlob->Release();
    if (pErrorBlob) pErrorBlob->Release();

    if (FAILED(result)) return false;

    // Создаем константные буферы для каждой маленькой сферы
    D3D11_BUFFER_DESC smallSphereGeomBufferDesc = {};
    smallSphereGeomBufferDesc.ByteWidth = sizeof(SmallSphereGeomBuffer);
    smallSphereGeomBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    smallSphereGeomBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    smallSphereGeomBufferDesc.CPUAccessFlags = 0;
    smallSphereGeomBufferDesc.MiscFlags = 0;
    smallSphereGeomBufferDesc.StructureByteStride = 0;

    // Инициализируем все буферы
    for (int i = 0; i < 10; i++)
    {
        SmallSphereGeomBuffer geomData;
        DirectX::XMMATRIX model = DirectX::XMMatrixIdentity();
        DirectX::XMMATRIX modelT = DirectX::XMMatrixTranspose(model);
        DirectX::XMStoreFloat4x4(&geomData.m, modelT);
        geomData.color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

        D3D11_SUBRESOURCE_DATA initData = { &geomData, 0, 0 };
        result = m_pDevice->CreateBuffer(&smallSphereGeomBufferDesc, &initData, &m_pSmallSphereGeomBuffers[i]);
        if (FAILED(result)) return false;
    }

    return true;
}


// === ФУНКЦИЯ ИНИЦИАЛИЗАЦИИ ПОСТПРОЦЕССИНГА ===
bool InitPostProcess()
{
    HRESULT result = S_OK;

    // Шейдеры для постпроцессинга (мульти-эффект)
    const char* postProcessVSSource = R"(
        struct VSInput
        {
            uint vertexId : SV_VertexID;
        };
        
        struct VSOutput
        {
            float4 pos : SV_POSITION;
            float2 uv : TEXCOORD;
        };
        
        VSOutput main(VSInput vertex)
        {
            VSOutput result;
            
            float4 pos = float4(0, 0, 0, 0);
            
            // Один треугольник вместо квадрата
            switch (vertex.vertexId)
            {
                case 0:
                    pos = float4(-1, 1, 0, 1);
                    break;
                case 1:
                    pos = float4(3, 1, 0, 1);
                    break;
                case 2:
                    pos = float4(-1, -3, 0, 1);
                    break;
            }
            
            result.pos = pos;
            result.uv = float2(pos.x * 0.5 + 0.5, 0.5 - pos.y * 0.5);
            
            return result;
        }
    )";

    const char* postProcessPSSource = R"(
    struct VSOutput
    {
        float4 pos : SV_POSITION;
        float2 uv : TEXCOORD;
    };
    
    Texture2D colorTexture : register(t0);
    SamplerState colorSampler : register(s0);
    
    // Константный буфер для постпроцессинга
    cbuffer PostProcessBuffer : register(b0)
    {
        int effectType;    // 0 - нет, 1 - сепия, 2 - холодный тон, 3 - ночной режим
        int padding[3];
    };
    
    float3 ApplySepia(float3 color)
    {
        // Коэффициенты для фильтра сепии
        float rr = 0.393f;
        float rg = 0.769f;
        float rb = 0.189f;
        
        float gr = 0.349f;
        float gg = 0.686f;
        float gb = 0.168f;
        
        float br = 0.272f;
        float bg = 0.534f;
        float bb = 0.131f;
        
        float red = (rr * color.r) + (rg * color.g) + (rb * color.b);
        float green = (gr * color.r) + (gg * color.g) + (gb * color.b);
        float blue = (br * color.r) + (bg * color.g) + (bb * color.b);
        
        return float3(red, green, blue);
    }
    
    float3 ApplyColdTint(float3 color)
    {
        // Холодный тон: усиливаем синий и зеленый, уменьшаем красный
        // Фиксированная интенсивность (без настройки)
        float3 coldTint = float3(0.6f, 0.8f, 1.0f);
        return color * coldTint;
    }
    
    float3 ApplyNightVision(float3 color)
    {
        // Ночное видение (зелено-синее)
        float gray = dot(color, float3(0.299f, 0.587f, 0.114f));
        return float3(0.1f, gray, gray);
    }
    
    float4 main(VSOutput pixel) : SV_Target0
    {
        float3 color = colorTexture.Sample(colorSampler, pixel.uv).rgb;
        float3 finalColor = color;
        
        // Применяем эффекты в зависимости от типа
        if (effectType == 1) // Сепия
        {
            finalColor = ApplySepia(color);
        }
        else if (effectType == 2) // Холодный тон
        {
            finalColor = ApplyColdTint(color);
        }
        else if (effectType == 3) // Ночной режим
        {
            finalColor = ApplyNightVision(color);
        }
        
        // Ограничиваем значения от 0 до 1
        finalColor = clamp(finalColor, 0.0f, 1.0f);
        
        return float4(finalColor, 1.0f);
    }
)";


    ID3DBlob* pPostProcessVSBlob = nullptr;
    ID3DBlob* pPostProcessPSBlob = nullptr;
    ID3DBlob* pErrorBlob = nullptr;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    // Компилируем вершинный шейдер
    result = D3DCompile(
        postProcessVSSource,
        strlen(postProcessVSSource),
        "PostProcessVS",
        nullptr,
        nullptr,
        "main",
        "vs_5_0",
        flags,
        0,
        &pPostProcessVSBlob,
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

    // Создаем вершинный шейдер
    result = m_pDevice->CreateVertexShader(
        pPostProcessVSBlob->GetBufferPointer(),
        pPostProcessVSBlob->GetBufferSize(),
        nullptr,
        &m_pPostProcessVertexShader
    );

    if (FAILED(result))
    {
        pPostProcessVSBlob->Release();
        return false;
    }

    // Компилируем пиксельный шейдер
    result = D3DCompile(
        postProcessPSSource,
        strlen(postProcessPSSource),
        "PostProcessPS",
        nullptr,
        nullptr,
        "main",
        "ps_5_0",
        flags,
        0,
        &pPostProcessPSBlob,
        &pErrorBlob
    );

    if (FAILED(result))
    {
        if (pErrorBlob)
        {
            OutputDebugStringA((const char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        }
        pPostProcessVSBlob->Release();
        return false;
    }

    // Создаем пиксельный шейдер
    result = m_pDevice->CreatePixelShader(
        pPostProcessPSBlob->GetBufferPointer(),
        pPostProcessPSBlob->GetBufferSize(),
        nullptr,
        &m_pPostProcessPixelShader
    );

    // Освобождаем blob-объекты
    if (pPostProcessVSBlob) pPostProcessVSBlob->Release();
    if (pPostProcessPSBlob) pPostProcessPSBlob->Release();
    if (pErrorBlob) pErrorBlob->Release();

    // Создаем константный буфер для постпроцессинга
    D3D11_BUFFER_DESC postProcessBufferDesc = {};
    postProcessBufferDesc.ByteWidth = sizeof(PostProcessBuffer);
    postProcessBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    postProcessBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    postProcessBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    postProcessBufferDesc.MiscFlags = 0;
    postProcessBufferDesc.StructureByteStride = 0;

    PostProcessBuffer initPostProcessData;
    initPostProcessData.effectType = 0;
    initPostProcessData.padding[0] = initPostProcessData.padding[1] = initPostProcessData.padding[2] = 0;

    D3D11_SUBRESOURCE_DATA postProcessInitData = { &initPostProcessData, 0, 0 };

    result = m_pDevice->CreateBuffer(&postProcessBufferDesc, &postProcessInitData, &m_pPostProcessBuffer);
    if (FAILED(result)) return false;

    return SUCCEEDED(result);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Для детерминированной случайности
    srand(12345);

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

    // Инициализируем ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    ImGui::StyleColorsDark();

    // Инициализируем ImGui для Win32 и DirectX11
    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX11_Init(m_pDevice, m_pDeviceContext);

    if (!InitShaders())
    {
        MessageBox(NULL, L"Не удалось создать шейдеры!", L"Ошибка", MB_OK);
        Cleanup();
        return -1;
    }

    if (!InitTerrain())
    {
        MessageBox(NULL, L"Не удалось инициализировать ландшафт!", L"Ошибка", MB_OK);
        Cleanup();
        return -1;
    }

    if (!InitBuffers())
    {
        MessageBox(NULL, L"Не удалось инициализировать буферы!", L"Ошибка", MB_OK);
        Cleanup();
        return -1;
    }

    // Загружаем массив текстур 
    if (!LoadTextureArray())
    {
        MessageBox(NULL, L"Не удалось загрузить текстуры!", L"Ошибка", MB_OK);
        Cleanup();
        return -1;
    }

    // Загружаем карту нормалей
    if (!LoadNormalMap())
    {
        MessageBox(NULL, L"Не удалось загрузить карту нормалей!", L"Ошибка", MB_OK);
        Cleanup();
        return -1;
    }

    // Инициализация маленьких сфер для визуализации источников
    if (!InitSmallSpheres())
    {
        MessageBox(NULL, L"Не удалось инициализировать маленькие сферы!", L"Ошибка", MB_OK);
        Cleanup();
        return -1;
    }

    // Инициализация постпроцессинга
    if (!InitPostProcess())
    {
        MessageBox(NULL, L"Не удалось инициализировать постпроцессинг!", L"Ошибка", MB_OK);
        Cleanup();
        return -1;
    }

    // Инициализация источников света
    m_lightCount = 1;
    m_lights[0].pos = DirectX::XMFLOAT4(1.0f, 3.0f, 0.0f, 1.0f);
    m_lights[0].color = DirectX::XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f);
    m_ambientColor = DirectX::XMFLOAT4(0.0f, 0.0f, 0.2f, 1.0f); //темный эмбиент, чтобы видеть источники
    //m_ambientColor = DirectX::XMFLOAT4(1.0f, 1.0f, 1.1f, 1.0f); //светлый эмбиент

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
        L"DirectX 11 - Landscape",
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
    // Передаем сообщения в ImGui
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

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
        m_prevMouseX = GET_X_LPARAM(lParam);  // Используем GET_X_LPARAM
        m_prevMouseY = GET_Y_LPARAM(lParam);  // Используем GET_Y_LPARAM
        SetCapture(hWnd);
        break;

    case WM_RBUTTONUP:
        m_rbPressed = false;
        ReleaseCapture();
        break;

    case WM_MOUSEMOVE:
        if (m_rbPressed)
        {
            int currentX = GET_X_LPARAM(lParam);  // Используем GET_X_LPARAM
            int currentY = GET_Y_LPARAM(lParam);  // Используем GET_Y_LPARAM

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

    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_TAB:
            m_showImGui = !m_showImGui;
            break;
        case 'W': m_keyW = true; break;
        case 'A': m_keyA = true; break;
        case 'S': m_keyS = true; break;
        case 'D': m_keyD = true; break;
        case 'Q': m_keyQ = true; break;
        case 'E': m_keyE = true; break;
        default: break;
        }
        break;

    case WM_KEYUP:
        switch (wParam)
        {
        case 'W': m_keyW = false; break;
        case 'A': m_keyA = false; break;
        case 'S': m_keyS = false; break;
        case 'D': m_keyD = false; break;
        case 'Q': m_keyQ = false; break;
        case 'E': m_keyE = false; break;
        default: break;
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

    if (!InitColorBuffer())
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


    // === СОЗДАНИЕ BLEND STATES ===
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;          // отключаем смешивание
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    result = m_pDevice->CreateBlendState(&blendDesc, &m_pOpaqueBlendState);
    if (FAILED(result)) return false;

    return SUCCEEDED(result);
}

bool SetupBackBuffer()
{
    HRESULT result = S_OK;

    SAFE_RELEASE(m_pBackBufferRTV);
    SAFE_RELEASE(m_pDepthStencilView);
    SAFE_RELEASE(m_pDepthBuffer);

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

bool InitColorBuffer()
{
    HRESULT result = S_OK;

    SAFE_RELEASE(m_pColorBuffer);
    SAFE_RELEASE(m_pColorBufferRTV);
    SAFE_RELEASE(m_pColorBufferSRV);

    D3D11_TEXTURE2D_DESC colorBufferDesc = {};
    colorBufferDesc.Width = m_width;
    colorBufferDesc.Height = m_height;
    colorBufferDesc.MipLevels = 1;
    colorBufferDesc.ArraySize = 1;
    colorBufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    colorBufferDesc.SampleDesc.Count = 1;
    colorBufferDesc.SampleDesc.Quality = 0;
    colorBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    colorBufferDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    colorBufferDesc.CPUAccessFlags = 0;
    colorBufferDesc.MiscFlags = 0;

    result = m_pDevice->CreateTexture2D(&colorBufferDesc, nullptr, &m_pColorBuffer);
    if (FAILED(result)) return false;

    result = m_pDevice->CreateRenderTargetView(m_pColorBuffer, nullptr, &m_pColorBufferRTV);
    if (FAILED(result)) return false;

    result = m_pDevice->CreateShaderResourceView(m_pColorBuffer, nullptr, &m_pColorBufferSRV);
    return SUCCEEDED(result);
}

bool InitBuffers()
{
    HRESULT result = S_OK;

    // Константный буфер сцены
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

bool InitTerrain()
{
    HRESULT result = S_OK;

    // 1. Загрузка карты высот из EXR
    const char* exrFilename = "landscape/Terrain003_2K.exr";
    float* out_rgba = nullptr;
    int width, height;
    const char* err = nullptr;

    int ret = LoadEXR(&out_rgba, &width, &height, exrFilename, &err);
    if (ret != TINYEXR_SUCCESS)
    {
        if (err)
        {
            OutputDebugStringA(err);
            FreeEXRErrorMessage(err);
        }
        MessageBox(NULL, L"Не удалось загрузить карту высот EXR!", L"Ошибка", MB_OK);
        return false;
    }

    // 2. Определяем размер сетки (уменьшаем шаг для производительности)
    const int step = 8; // берём каждый 8-й пиксель
    m_terrainGridSizeX = (width + step - 1) / step;
    m_terrainGridSizeZ = (height + step - 1) / step;

    // Ограничиваем, чтобы индексы влезли в UINT16 (макс 65535)
    m_terrainGridSizeX = std::min(m_terrainGridSizeX, 256u);
    m_terrainGridSizeZ = std::min(m_terrainGridSizeZ, 256u);

    UINT vertexCount = m_terrainGridSizeX * m_terrainGridSizeZ;
    std::vector<TextureTangentVertex> vertices(vertexCount);

    // Диапазон координат
    float xMin = -m_terrainWidth / 2.0f;
    float xMax = m_terrainWidth / 2.0f;
    float zMin = -m_terrainDepth / 2.0f;
    float zMax = m_terrainDepth / 2.0f;
    float stepX = (xMax - xMin) / (m_terrainGridSizeX - 1);
    float stepZ = (zMax - zMin) / (m_terrainGridSizeZ - 1);

    // Заполняем позиции и текстурные координаты
    for (UINT j = 0; j < m_terrainGridSizeZ; j++)
    {
        for (UINT i = 0; i < m_terrainGridSizeX; i++)
        {
            UINT index = j * m_terrainGridSizeX + i;

            int mapX = i * step;
            int mapY = j * step;
            if (mapX >= width) mapX = width - 1;
            if (mapY >= height) mapY = height - 1;

            float heightVal = out_rgba[(mapY * width + mapX) * 4]; // красный канал

            float x = xMin + i * stepX;
            float z = zMin + j * stepZ;
            float y = heightVal * m_terrainHeightScale;

            vertices[index].x = x;
            vertices[index].y = y;
            vertices[index].z = z;

            vertices[index].u = (float)i / (m_terrainGridSizeX - 1);
            vertices[index].v = (float)j / (m_terrainGridSizeZ - 1);

            // Пока нули, позже вычислим
            vertices[index].nx = vertices[index].ny = vertices[index].nz = 0.0f;
            vertices[index].tx = vertices[index].ty = vertices[index].tz = 0.0f;
        }
    }

    free(out_rgba); // данные EXR больше не нужны

    // 3. Создание индексов (два треугольника на ячейку)
    std::vector<UINT16> indices;
    indices.reserve((m_terrainGridSizeX - 1) * (m_terrainGridSizeZ - 1) * 6);

    for (UINT j = 0; j < m_terrainGridSizeZ - 1; j++)
    {
        for (UINT i = 0; i < m_terrainGridSizeX - 1; i++)
        {
            UINT topLeft = j * m_terrainGridSizeX + i;
            UINT topRight = j * m_terrainGridSizeX + i + 1;
            UINT bottomLeft = (j + 1) * m_terrainGridSizeX + i;
            UINT bottomRight = (j + 1) * m_terrainGridSizeX + i + 1;

            // Первый треугольник
            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            // Второй треугольник
            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }
    m_terrainIndexCount = (UINT)indices.size();

    // 4. Вычисление нормалей и касательных
    // Сначала обнулим
    for (auto& v : vertices)
    {
        v.nx = v.ny = v.nz = 0.0f;
        v.tx = v.ty = v.tz = 0.0f;
    }

    for (size_t i = 0; i < indices.size(); i += 3)
    {
        UINT16 i0 = indices[i];
        UINT16 i1 = indices[i + 1];
        UINT16 i2 = indices[i + 2];

        auto& v0 = vertices[i0];
        auto& v1 = vertices[i1];
        auto& v2 = vertices[i2];

        // Позиции
        DirectX::XMFLOAT3 p0(v0.x, v0.y, v0.z);
        DirectX::XMFLOAT3 p1(v1.x, v1.y, v1.z);
        DirectX::XMFLOAT3 p2(v2.x, v2.y, v2.z);

        // Текстурные координаты
        DirectX::XMFLOAT2 uv0(v0.u, v0.v);
        DirectX::XMFLOAT2 uv1(v1.u, v1.v);
        DirectX::XMFLOAT2 uv2(v2.u, v2.v);

        using namespace DirectX;

        XMVECTOR p0v = XMLoadFloat3(&p0);
        XMVECTOR p1v = XMLoadFloat3(&p1);
        XMVECTOR p2v = XMLoadFloat3(&p2);
        XMVECTOR e1 = XMVectorSubtract(p1v, p0v);
        XMVECTOR e2 = XMVectorSubtract(p2v, p0v);

        // Нормаль треугольника
        XMVECTOR normal = XMVector3Cross(e1, e2);
        normal = XMVector3Normalize(normal);

        // Касательная
        float deltaU1 = uv1.x - uv0.x;
        float deltaV1 = uv1.y - uv0.y;
        float deltaU2 = uv2.x - uv0.x;
        float deltaV2 = uv2.y - uv0.y;
        float f = 1.0f / (deltaU1 * deltaV2 - deltaU2 * deltaV1);
        XMVECTOR tangent;
        if (isfinite(f))
        {
            tangent = XMVectorScale(XMVectorSubtract(XMVectorScale(e1, deltaV2), XMVectorScale(e2, deltaV1)), f);
            tangent = XMVector3Normalize(tangent);
        }
        else
        {
            tangent = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        }

        // Прибавляем к вершинам
        XMStoreFloat3((XMFLOAT3*)&v0.nx, XMVectorAdd(XMLoadFloat3((XMFLOAT3*)&v0.nx), normal));
        XMStoreFloat3((XMFLOAT3*)&v1.nx, XMVectorAdd(XMLoadFloat3((XMFLOAT3*)&v1.nx), normal));
        XMStoreFloat3((XMFLOAT3*)&v2.nx, XMVectorAdd(XMLoadFloat3((XMFLOAT3*)&v2.nx), normal));

        XMStoreFloat3((XMFLOAT3*)&v0.tx, XMVectorAdd(XMLoadFloat3((XMFLOAT3*)&v0.tx), tangent));
        XMStoreFloat3((XMFLOAT3*)&v1.tx, XMVectorAdd(XMLoadFloat3((XMFLOAT3*)&v1.tx), tangent));
        XMStoreFloat3((XMFLOAT3*)&v2.tx, XMVectorAdd(XMLoadFloat3((XMFLOAT3*)&v2.tx), tangent));
    }

    // Нормализуем нормали и касательные
    for (auto& v : vertices)
    {
        DirectX::XMStoreFloat3((DirectX::XMFLOAT3*)&v.nx, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&v.nx)));
        DirectX::XMStoreFloat3((DirectX::XMFLOAT3*)&v.tx, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&v.tx)));
    }

    // 5. Создание вершинного буфера
    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.ByteWidth = (UINT)(vertices.size() * sizeof(TextureTangentVertex));
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbDesc.CPUAccessFlags = 0;
    vbDesc.StructureByteStride = sizeof(TextureTangentVertex);

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();

    result = m_pDevice->CreateBuffer(&vbDesc, &vbData, &m_pTerrainVertexBuffer);
    if (FAILED(result)) return false;

    // 6. Создание индексного буфера
    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.ByteWidth = (UINT)(indices.size() * sizeof(UINT16));
    ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices.data();

    result = m_pDevice->CreateBuffer(&ibDesc, &ibData, &m_pTerrainIndexBuffer);
    if (FAILED(result)) return false;

    // 7. Константный буфер для геометрии ландшафта
    D3D11_BUFFER_DESC geomBufferDesc = {};
    geomBufferDesc.ByteWidth = sizeof(GeomBuffer);
    geomBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    geomBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

    GeomBuffer geomData;
    DirectX::XMMATRIX model = DirectX::XMMatrixIdentity();
    DirectX::XMStoreFloat4x4(&geomData.m, DirectX::XMMatrixTranspose(model));
    DirectX::XMStoreFloat4x4(&geomData.normalM, DirectX::XMMatrixIdentity()); // обратная = единичная
    geomData.shineSpeedTexIdNM = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f); // shine=0, texId=0
    geomData.posAngle = DirectX::XMFLOAT4(0, 0, 0, 0);

    D3D11_SUBRESOURCE_DATA geomInitData = { &geomData, 0, 0 };
    result = m_pDevice->CreateBuffer(&geomBufferDesc, &geomInitData, &m_pTerrainGeomBuffer);
    if (FAILED(result)) return false;

    return true;
}

void UpdateCamera()
{
    // Позиция камеры
    DirectX::XMFLOAT3 pos;
    pos.x = m_camera.poi.x + m_camera.r * cosf(m_camera.theta) * cosf(m_camera.phi);
    pos.y = m_camera.poi.y + m_camera.r * sinf(m_camera.theta);
    pos.z = m_camera.poi.z + m_camera.r * cosf(m_camera.theta) * sinf(m_camera.phi);

    // === ИСПРАВЛЕННОЕ ВЫЧИСЛЕНИЕ ВЕКТОРА UP ===
    // Вектор up должен вращаться вместе с камерой, а не быть фиксированным
    float upTheta = m_camera.theta + DirectX::XM_PIDIV2;
    DirectX::XMVECTOR up = DirectX::XMVectorSet(
        cosf(upTheta) * cosf(m_camera.phi),
        sinf(upTheta),
        cosf(upTheta) * sinf(m_camera.phi),
        0.0f
    );

    DirectX::XMVECTOR eye = DirectX::XMVectorSet(pos.x, pos.y, pos.z, 0.0f);
    DirectX::XMVECTOR at = DirectX::XMVectorSet(m_camera.poi.x, m_camera.poi.y, m_camera.poi.z, 0.0f);

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

    // Создаем константный буфер сцены с освещением
    SceneBuffer sceneBuffer;
    DirectX::XMMATRIX vpT = DirectX::XMMatrixTranspose(vp);
    DirectX::XMStoreFloat4x4(&sceneBuffer.vp, vpT);
    sceneBuffer.cameraPos = DirectX::XMFLOAT4(pos.x, pos.y, pos.z, 1.0f);

    // Заполняем информацию об освещении
    sceneBuffer.lightInfo = DirectX::XMFLOAT4(
        (float)m_lightCount,           // Количество источников
        m_useNormalMaps ? 1.0f : 0.0f, // Использовать карты нормалей
        m_showNormals ? 1.0f : 0.0f,   // Показывать нормали
        0.0f         // frustum culling не используется!
    );

    // Копируем источники света
    for (int i = 0; i < 10; i++)
    {
        sceneBuffer.lights[i] = m_lights[i];
    }
    sceneBuffer.ambientColor = m_ambientColor;

    // Обновляем константный буфер
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT result = m_pDeviceContext->Map(m_pSceneBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(result))
    {
        memcpy(mappedResource.pData, &sceneBuffer, sizeof(SceneBuffer));
        m_pDeviceContext->Unmap(m_pSceneBuffer, 0);
    }
}

void UpdatePostProcessBuffer()
{
    if (!m_pPostProcessBuffer) return;

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT result = m_pDeviceContext->Map(m_pPostProcessBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(result))
    {
        PostProcessBuffer* postProcessData = (PostProcessBuffer*)mappedResource.pData;
        postProcessData->effectType = m_postProcessEffect;
        postProcessData->padding[0] = postProcessData->padding[1] = postProcessData->padding[2] = 0;
        m_pDeviceContext->Unmap(m_pPostProcessBuffer, 0);
    }
}

// === ФУНКЦИЯ РЕНДЕРИНГА ПОСТПРОЦЕССИНГА ===
void RenderPostProcess()
{
    // Сбрасываем шейдерный ресурс, который может быть ещё привязан
    ID3D11ShaderResourceView* nullSRV = nullptr;
    m_pDeviceContext->PSSetShaderResources(0, 1, &nullSRV);

    // Переключаемся на back buffer как рендер-таргет
    ID3D11RenderTargetView* views[] = { m_pBackBufferRTV };
    m_pDeviceContext->OMSetRenderTargets(1, views, nullptr);

    // Устанавливаем состояния
    m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);
    m_pDeviceContext->RSSetState(nullptr);
    m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

    // Устанавливаем сэмплер
    ID3D11SamplerState* samplers[] = { m_pSampler };
    m_pDeviceContext->PSSetSamplers(0, 1, samplers);

    // Устанавливаем текстуру как шейдерный ресурс
    ID3D11ShaderResourceView* resources[] = { m_pColorBufferSRV };
    m_pDeviceContext->PSSetShaderResources(0, 1, resources);

    // Настраиваем пайплайн для постпроцессинга
    m_pDeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
    m_pDeviceContext->IASetInputLayout(nullptr);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->VSSetShader(m_pPostProcessVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPostProcessPixelShader, nullptr, 0);

    // Устанавливаем константный буфер постпроцессинга
    ID3D11Buffer* postProcessConstantBuffers[] = { m_pPostProcessBuffer };
    m_pDeviceContext->PSSetConstantBuffers(0, 1, postProcessConstantBuffers);

    // Рисуем 3 вершины (один треугольник)
    m_pDeviceContext->Draw(3, 0);
}

void ResizeSwapChain(UINT width, UINT height)
{
    if (width == 0 || height == 0)
        return;

    m_width = width;
    m_height = height;

    // Отвязываем все render target'ы
    if (m_pDeviceContext)
    {
        ID3D11RenderTargetView* nullRTV[1] = { nullptr };
        m_pDeviceContext->OMSetRenderTargets(1, nullRTV, nullptr);
    }

    // Освобождаем все ресурсы, зависящие от размера окна
    SAFE_RELEASE(m_pBackBufferRTV);
    SAFE_RELEASE(m_pDepthStencilView);
    SAFE_RELEASE(m_pDepthBuffer);
    SAFE_RELEASE(m_pColorBufferRTV);
    SAFE_RELEASE(m_pColorBufferSRV);
    SAFE_RELEASE(m_pColorBuffer);

    // Изменяем размер цепочки обмена
    if (m_pSwapChain)
    {
        HRESULT hr = m_pSwapChain->ResizeBuffers(0, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(hr))
        {
            // Здесь можно добавить логирование ошибки
            return;
        }
    }

    // Пересоздаём back buffer RTV и depth buffer
    if (!SetupBackBuffer())
    {
        // Обработка ошибки
        return;
    }

    // Пересоздаём color buffer для постпроцессинга
    if (!InitColorBuffer())
    {
        // Обработка ошибки
        return;
    }
}

void RenderSmallSpheres()
{
    if (!m_showLightBulbs || m_lightCount == 0)
        return;

    // Устанавливаем состояние для непрозрачных объектов
    m_pDeviceContext->OMSetDepthStencilState(m_pNormalDepthState, 0);
    m_pDeviceContext->OMSetBlendState(m_pOpaqueBlendState, nullptr, 0xFFFFFFFF);

    // Настраиваем пайплайн для маленьких сфер
    m_pDeviceContext->IASetIndexBuffer(m_pSmallSphereIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    ID3D11Buffer* vertexBuffers[] = { m_pSmallSphereVertexBuffer };
    UINT strides[] = { sizeof(DirectX::XMFLOAT3) };
    UINT offsets[] = { 0 };
    m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    m_pDeviceContext->IASetInputLayout(m_pSmallSphereInputLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->VSSetShader(m_pSmallSphereVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pSmallSpherePixelShader, nullptr, 0);

    // Рисуем каждую маленькую сферу (источник света)
    for (int i = 0; i < m_lightCount; i++)
    {
        // Обновляем матрицу трансформации для этой сферы
        SmallSphereGeomBuffer geomData;
        DirectX::XMMATRIX model = DirectX::XMMatrixTranslation(
            m_lights[i].pos.x,
            m_lights[i].pos.y,
            m_lights[i].pos.z
        );
        DirectX::XMMATRIX modelT = DirectX::XMMatrixTranspose(model);
        DirectX::XMStoreFloat4x4(&geomData.m, modelT);
        geomData.color = m_lights[i].color;

        m_pDeviceContext->UpdateSubresource(m_pSmallSphereGeomBuffers[i], 0, nullptr, &geomData, 0, 0);

        // Устанавливаем константные буферы
        ID3D11Buffer* constantBuffers[] = { m_pSmallSphereGeomBuffers[i], m_pSceneBuffer };
        m_pDeviceContext->VSSetConstantBuffers(0, 2, constantBuffers);
        m_pDeviceContext->PSSetConstantBuffers(0, 2, constantBuffers);

        // Рисуем сферу
        m_pDeviceContext->DrawIndexed(m_smallSphereIndexCount, 0, 0);
    }
}

void Render()
{
    if (!m_pDeviceContext || !m_pBackBufferRTV)
        return;

    ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
    m_pDeviceContext->PSSetShaderResources(0, 2, nullSRVs);

    // Рендерим сцену в текстуру для постпроцессинга
    ID3D11RenderTargetView* views[] = { m_pColorBufferRTV };
    m_pDeviceContext->OMSetRenderTargets(1, views, m_pDepthStencilView);

    // === REVERSED DEPTH: очистка чёрным цветом ===
    static const FLOAT BackColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // чёрный
    m_pDeviceContext->ClearRenderTargetView(m_pColorBufferRTV, BackColor);
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

    // Обработка перемещения камеры клавишами WASD + QE
    if (m_keyW || m_keyA || m_keyS || m_keyD || m_keyQ || m_keyE)
    {
        float theta = m_camera.theta;
        float phi = m_camera.phi;

        // Направление взгляда от камеры к poi
        float forwardX = -cosf(theta) * cosf(phi);
        float forwardY = -sinf(theta);
        float forwardZ = -cosf(theta) * sinf(phi);

        // Вектор up (как в UpdateCamera)
        float upTheta = theta + DirectX::XM_PIDIV2;
        float upX = cosf(upTheta) * cosf(phi);
        float upY = sinf(upTheta);
        float upZ = cosf(upTheta) * sinf(phi);

        // Вектор right = cross(up, forward)
        float rightX = upY * forwardZ - upZ * forwardY;
        float rightY = upZ * forwardX - upX * forwardZ;
        float rightZ = upX * forwardY - upY * forwardX;

        // Нормализация right
        float len = sqrtf(rightX * rightX + rightY * rightY + rightZ * rightZ);
        if (len > 0.0001f)
        {
            rightX /= len; rightY /= len; rightZ /= len;
        }

        const float speed = 0.1f;

        // Перемещение по горизонтали (WASD)
        if (m_keyW)
        {
            m_camera.poi.x += forwardX * speed;
            m_camera.poi.y += forwardY * speed;
            m_camera.poi.z += forwardZ * speed;
        }
        if (m_keyS)
        {
            m_camera.poi.x -= forwardX * speed;
            m_camera.poi.y -= forwardY * speed;
            m_camera.poi.z -= forwardZ * speed;
        }
        if (m_keyD)
        {
            m_camera.poi.x += rightX * speed;
            m_camera.poi.y += rightY * speed;
            m_camera.poi.z += rightZ * speed;
        }
        if (m_keyA)
        {
            m_camera.poi.x -= rightX * speed;
            m_camera.poi.y -= rightY * speed;
            m_camera.poi.z -= rightZ * speed;
        }

        // Перемещение по вертикали (фиксированная ось Y)
        if (m_keyQ)
        {
            m_camera.poi.y += speed; // вверх
        }
        if (m_keyE)
        {
            m_camera.poi.y -= speed; // вниз
        }
    }

    UpdateCamera();
    UpdatePostProcessBuffer();

    // 1. РЕНДЕРИМ ЛАНДШАФТ
    m_pDeviceContext->OMSetDepthStencilState(m_pNormalDepthState, 0);
    m_pDeviceContext->OMSetBlendState(m_pOpaqueBlendState, nullptr, 0xFFFFFFFF);

    // Устанавливаем текстуры (пока используем старые)
    ID3D11ShaderResourceView* terrainResources[] = { m_pTextureView, m_pTextureViewNM };
    m_pDeviceContext->PSSetShaderResources(0, 2, terrainResources);

    // Настраиваем пайплайн
    ID3D11Buffer* vertexBuffers[] = { m_pTerrainVertexBuffer };
    UINT strides[] = { sizeof(TextureTangentVertex) };
    UINT offsets[] = { 0 };
    m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    m_pDeviceContext->IASetIndexBuffer(m_pTerrainIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetInputLayout(m_pInputLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

    ID3D11SamplerState* samplers[] = { m_pSampler };
    m_pDeviceContext->PSSetSamplers(0, 1, samplers);

    // Константные буферы
    ID3D11Buffer* constantBuffers[] = { m_pTerrainGeomBuffer, m_pSceneBuffer };
    m_pDeviceContext->VSSetConstantBuffers(0, 2, constantBuffers);
    m_pDeviceContext->PSSetConstantBuffers(0, 2, constantBuffers);

    // Рисуем ландшафт
    m_pDeviceContext->DrawIndexed(m_terrainIndexCount, 0, 0);

    // 2. РЕНДЕРИМ МАЛЕНЬКИЕ СФЕРЫ (ИСТОЧНИКИ СВЕТА)
    RenderSmallSpheres();

    // Применяем постпроцессинг
    RenderPostProcess();

    // 5. РЕНДЕРИМ ImGui
    if (m_showImGui)
    {
        // Начало нового кадра ImGui
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Окно управления освещением
        ImGui::Begin("Lights control", &m_showImGui, ImGuiWindowFlags_AlwaysAutoResize);

        ImGui::Checkbox("Show bulbs", &m_showLightBulbs); //Показывать источники света
        ImGui::Checkbox("Use normal maps", &m_useNormalMaps); //Использовать карты нормалей
        ImGui::Checkbox("Show normals", &m_showNormals); //Показывать нормали

        // Кнопки для добавления/удаления источников света
        if (ImGui::Button("Add bulb") && m_lightCount < 10) //Добавить источник
        {
            m_lightCount++;
            // Инициализируем новый источник
            m_lights[m_lightCount - 1].pos = DirectX::XMFLOAT4(0.0f, 3.0f, 0.0f, 1.0f);
            m_lights[m_lightCount - 1].color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete bulb") && m_lightCount > 0) //Удалить источник
        {
            m_lightCount--;
        }

        // Параметры окружающего освещения
        ImGui::ColorEdit3("Ambient light", &m_ambientColor.x); //Окружающий цвет

        // Параметры для каждого источника света
        for (int i = 0; i < m_lightCount; i++)
        {
            ImGui::PushID(i);
            ImGui::Text("Light %d", i); //Источник
            char posLabel[32];
            sprintf_s(posLabel, "Pos %d", i); //Позиция
            ImGui::DragFloat3(posLabel, &m_lights[i].pos.x, 0.1f, -10.0f, 10.0f);
            char colorLabel[32];
            sprintf_s(colorLabel, "Color %d", i); //Цвет
            ImGui::ColorEdit3(colorLabel, &m_lights[i].color.x);
            ImGui::PopID();
        }

        ImGui::End();


        // Окно постпроцессинга
        ImGui::Begin("Post Processing", &m_showImGui, ImGuiWindowFlags_AlwaysAutoResize);

        const char* effectNames[] = {
            "No Effect",     // 0
            "Sepia",         // 1
            "Cold Tint",     // 2
            "Night Vision"   // 3
        };

        ImGui::Text("Select Post-Process Effect:");
        ImGui::Combo("Effect", &m_postProcessEffect, effectNames, IM_ARRAYSIZE(effectNames));

        // Отображение текущего выбранного эффекта
        ImGui::Text("Current Effect:");
        ImGui::SameLine();
        switch (m_postProcessEffect)
        {
        case 0: ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "None"); break;
        case 1: ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.2f, 1.0f), "Sepia"); break;
        case 2: ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "Cold Tint"); break;
        case 3: ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.3f, 1.0f), "Night Vision"); break;
        }

        ImGui::End();


        // Рендеринг ImGui
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    HRESULT result = m_pSwapChain->Present(1, 0);
    assert(SUCCEEDED(result));
}

void Cleanup()
{
    // Освобождаем ресурсы маленьких сфер
    for (int i = 0; i < 10; i++)
    {
        SAFE_RELEASE(m_pSmallSphereGeomBuffers[i]);
    }
    SAFE_RELEASE(m_pSmallSphereInputLayout);
    SAFE_RELEASE(m_pSmallSpherePixelShader);
    SAFE_RELEASE(m_pSmallSphereVertexShader);
    SAFE_RELEASE(m_pSmallSphereIndexBuffer);
    SAFE_RELEASE(m_pSmallSphereVertexBuffer);

    // Освобождаем карту нормалей
    SAFE_RELEASE(m_pTextureViewNM);
    SAFE_RELEASE(m_pTextureNM);

    // Освобождаем ресурсы постпроцессинга
    SAFE_RELEASE(m_pPostProcessBuffer);
    SAFE_RELEASE(m_pPostProcessPixelShader);
    SAFE_RELEASE(m_pPostProcessVertexShader);
    SAFE_RELEASE(m_pColorBufferSRV);
    SAFE_RELEASE(m_pColorBufferRTV);
    SAFE_RELEASE(m_pColorBuffer);

    // Завершаем работу ImGui
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // Освобождаем blend states
    SAFE_RELEASE(m_pOpaqueBlendState);

    // Освобождаем состояния глубины
    SAFE_RELEASE(m_pNormalDepthState);


    SAFE_RELEASE(m_pSampler);
    SAFE_RELEASE(m_pTextureView);
    SAFE_RELEASE(m_pTexture);

    SAFE_RELEASE(m_pRasterizerState);
    SAFE_RELEASE(m_pDepthStencilView);
    SAFE_RELEASE(m_pDepthBuffer);

    SAFE_RELEASE(m_pTerrainVertexBuffer);
    SAFE_RELEASE(m_pTerrainIndexBuffer);
    SAFE_RELEASE(m_pTerrainGeomBuffer);

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