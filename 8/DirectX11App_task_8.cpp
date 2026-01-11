//Этот скрипт + файлы DDS.cpp, DDS.h, Light.h

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

struct AABB
{
    DirectX::XMFLOAT3 vmin;
    DirectX::XMFLOAT3 vmax;
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

// === ПРОТОТИПЫ ФУНКЦИЙ ===
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
bool InitWindow(HINSTANCE hInstance, int nCmdShow);
bool InitDirectX();
bool InitCube();
bool InitBuffers();
bool LoadTextureArray();
bool LoadNormalMap();
bool InitSkybox();
bool InitTransparentObjects();
bool InitSmallSpheres();
void Render();
void Cleanup();
void ResizeSwapChain(UINT width, UINT height);
void UpdateCamera();
bool SetupBackBuffer();
void RenderTransparentObjects();
void RenderSmallSpheres();
void InitGeomInstance(int index, float x, float y, float z, float shine, int texId, bool hasNormalMap);
void CullBoxes();
DirectX::XMFLOAT4 BuildPlane(const DirectX::XMFLOAT3& p0, const DirectX::XMFLOAT3& p1, const DirectX::XMFLOAT3& p2, const DirectX::XMFLOAT3& p3);
bool IsBoxInside(const DirectX::XMFLOAT4 frustum[6], const DirectX::XMFLOAT3& bbMin, const DirectX::XMFLOAT3& bbMax);
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

// === ПЕРЕМЕННЫЕ ДЛЯ КУБИКА ===
ID3D11Buffer* m_pVertexBuffer = nullptr;
ID3D11Buffer* m_pIndexBuffer = nullptr;
ID3D11VertexShader* m_pVertexShader = nullptr;
ID3D11PixelShader* m_pPixelShader = nullptr;
ID3D11InputLayout* m_pInputLayout = nullptr;

// === ПЕРЕМЕННЫЕ ДЛЯ INSTANCING ===
static const int MaxInst = 100;  // Максимальное количество инстансов
ID3D11Buffer* m_pGeomBufferInst = nullptr;       // Буфер для ВСЕХ инстансов
ID3D11Buffer* m_pGeomBufferInstVis = nullptr;    // Буфер для видимых индексов
std::vector<GeomBuffer> m_geomBuffers;           // Данные инстансов на CPU
std::vector<AABB> m_geomBBs;                     // Bounding boxes для culling
UINT m_instCount = 2;                            // Текущее количество инстансов (начинаем с 2)
UINT m_visibleInstances = 0;                     // Видимые инстансы после culling
bool m_doCull = true;                            // Включить/выключить culling

// === ПЕРЕМЕННЫЕ ДЛЯ МАТРИЦ И УПРАВЛЕНИЯ ===
ID3D11Buffer* m_pSceneBuffer = nullptr;
// m_pGeomBuffer и m_pGeomBuffer2 удалены - заменены на m_pGeomBufferInst

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

// === СТРУКТУРА ВЕРШИНЫ С НОРМАЛЯМИ И КАСАТЕЛЬНЫМИ ===
struct TextureTangentVertex
{
    float x, y, z;           // Позиция
    float tx, ty, tz;        // Касательный вектор (tangent)
    float nx, ny, nz;        // Нормаль
    float u, v;              // Текстурные координаты
};

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

// === ФУНКЦИИ ДЛЯ INSTANCING И FRUSTUM CULLING ===

// Инициализация параметров инстанса
void InitGeomInstance(int index, float x, float y, float z, float shine, int texId, bool hasNormalMap)
{
    m_geomBuffers[index].posAngle = DirectX::XMFLOAT4(x, y, z, 0.0f);
    m_geomBuffers[index].shineSpeedTexIdNM = DirectX::XMFLOAT4(shine, 0.0f, (float)texId, hasNormalMap ? 1.0f : 0.0f);

    // Матрица трансформации
    DirectX::XMMATRIX model = DirectX::XMMatrixTranslation(x, y, z);
    DirectX::XMStoreFloat4x4(&m_geomBuffers[index].m, DirectX::XMMatrixTranspose(model));

    // Матрица нормалей (обратная транспонированная)
    DirectX::XMMATRIX normalM = DirectX::XMMatrixInverse(nullptr, model);
    normalM = DirectX::XMMatrixTranspose(normalM);
    DirectX::XMStoreFloat4x4(&m_geomBuffers[index].normalM, normalM);

    // AABB для frustum culling (куб 1x1x1)
    m_geomBBs[index].vmin = DirectX::XMFLOAT3(x - 0.5f, y - 0.5f, z - 0.5f);
    m_geomBBs[index].vmax = DirectX::XMFLOAT3(x + 0.5f, y + 0.5f, z + 0.5f);
}

// Построение плоскости по 4 точкам
DirectX::XMFLOAT4 BuildPlane(const DirectX::XMFLOAT3& p0, const DirectX::XMFLOAT3& p1, const DirectX::XMFLOAT3& p2, const DirectX::XMFLOAT3& p3)
{
    DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&p0);
    DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&p1);
    DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&p2);
    DirectX::XMVECTOR v3 = DirectX::XMLoadFloat3(&p3);

    DirectX::XMVECTOR edge1 = DirectX::XMVectorSubtract(v1, v0);
    DirectX::XMVECTOR edge2 = DirectX::XMVectorSubtract(v3, v0);
    DirectX::XMVECTOR normal = DirectX::XMVector3Cross(edge1, edge2);
    normal = DirectX::XMVector3Normalize(normal);

    DirectX::XMVECTOR pos = DirectX::XMVectorScale(DirectX::XMVectorAdd(DirectX::XMVectorAdd(DirectX::XMVectorAdd(v0, v1), v2), v3), 0.25f);
    float d = -DirectX::XMVectorGetX(DirectX::XMVector3Dot(normal, pos));

    DirectX::XMFLOAT4 plane;
    DirectX::XMStoreFloat3(&reinterpret_cast<DirectX::XMFLOAT3&>(plane), normal);
    plane.w = d;

    return plane;
}

// Проверка AABB на попадание во frustum
bool IsBoxInside(const DirectX::XMFLOAT4 frustum[6], const DirectX::XMFLOAT3& bbMin, const DirectX::XMFLOAT3& bbMax)
{
    for (int i = 0; i < 6; i++)
    {
        DirectX::XMFLOAT3 norm = reinterpret_cast<const DirectX::XMFLOAT3&>(frustum[i]);

        // Выбираем ближайшую точку AABB к плоскости
        DirectX::XMFLOAT3 testPoint;
        testPoint.x = (norm.x >= 0) ? bbMax.x : bbMin.x;
        testPoint.y = (norm.y >= 0) ? bbMax.y : bbMin.y;
        testPoint.z = (norm.z >= 0) ? bbMax.z : bbMin.z;

        // Проверяем знак расстояния
        float distance = norm.x * testPoint.x + norm.y * testPoint.y + norm.z * testPoint.z + frustum[i].w;
        if (distance < 0.0f)
            return false;
    }
    return true;
}

// Основная функция frustum culling
void CullBoxes()
{
    if (!m_doCull)
    {
        m_visibleInstances = m_instCount;
        return;
    }

    // === ИСПРАВЛЕННОЕ ВЫЧИСЛЕНИЕ ВЕКТОРОВ КАМЕРЫ ===

    // Направление камеры (взгляд) - отрицательное, так как камера смотрит по -Z
    DirectX::XMFLOAT3 dir;
    dir.x = -cosf(m_camera.theta) * cosf(m_camera.phi);
    dir.y = -sinf(m_camera.theta);
    dir.z = -cosf(m_camera.theta) * sinf(m_camera.phi);

    // Вектор up зависит от угла камеры, а не фиксированный (как в проекте 1)
    float upTheta = m_camera.theta + DirectX::XM_PIDIV2;
    DirectX::XMFLOAT3 up;
    up.x = cosf(upTheta) * cosf(m_camera.phi);
    up.y = sinf(upTheta);
    up.z = cosf(upTheta) * sinf(m_camera.phi);

    // Вектор right = cross(up, dir) и нормализуем
    DirectX::XMVECTOR upVec = DirectX::XMLoadFloat3(&up);
    DirectX::XMVECTOR dirVec = DirectX::XMLoadFloat3(&dir);
    DirectX::XMVECTOR rightVec = DirectX::XMVector3Cross(upVec, dirVec);
    rightVec = DirectX::XMVector3Normalize(rightVec);
    DirectX::XMFLOAT3 right;
    DirectX::XMStoreFloat3(&right, rightVec);

    // Позиция камеры
    DirectX::XMFLOAT3 cameraPos;
    cameraPos.x = m_camera.poi.x + m_camera.r * cosf(m_camera.theta) * cosf(m_camera.phi);
    cameraPos.y = m_camera.poi.y + m_camera.r * sinf(m_camera.theta);
    cameraPos.z = m_camera.poi.z + m_camera.r * cosf(m_camera.theta) * sinf(m_camera.phi);

    // Параметры пирамиды видимости
    float f = 100.0f;
    float n = 0.1f;
    float fov = DirectX::XM_PI / 3.0f;

    // Вершины ближней плоскости
    float xNear = tanf(fov * 0.5f) * n;
    float yNear = xNear * ((float)m_height / m_width);

    DirectX::XMFLOAT3 nearVertices[4];
    nearVertices[0] = DirectX::XMFLOAT3(
        cameraPos.x + dir.x * n - up.x * yNear - right.x * xNear,
        cameraPos.y + dir.y * n - up.y * yNear - right.y * xNear,
        cameraPos.z + dir.z * n - up.z * yNear - right.z * xNear
    );
    nearVertices[1] = DirectX::XMFLOAT3(
        cameraPos.x + dir.x * n - up.x * yNear + right.x * xNear,
        cameraPos.y + dir.y * n - up.y * yNear + right.y * xNear,
        cameraPos.z + dir.z * n - up.z * yNear + right.z * xNear
    );
    nearVertices[2] = DirectX::XMFLOAT3(
        cameraPos.x + dir.x * n + up.x * yNear + right.x * xNear,
        cameraPos.y + dir.y * n + up.y * yNear + right.y * xNear,
        cameraPos.z + dir.z * n + up.z * yNear + right.z * xNear
    );
    nearVertices[3] = DirectX::XMFLOAT3(
        cameraPos.x + dir.x * n + up.x * yNear - right.x * xNear,
        cameraPos.y + dir.y * n + up.y * yNear - right.y * xNear,
        cameraPos.z + dir.z * n + up.z * yNear - right.z * xNear
    );

    // Вершины дальней плоскости
    float xFar = tanf(fov * 0.5f) * f;
    float yFar = xFar * ((float)m_height / m_width);

    DirectX::XMFLOAT3 farVertices[4];
    farVertices[0] = DirectX::XMFLOAT3(
        cameraPos.x + dir.x * f - up.x * yFar - right.x * xFar,
        cameraPos.y + dir.y * f - up.y * yFar - right.y * xFar,
        cameraPos.z + dir.z * f - up.z * yFar - right.z * xFar
    );
    farVertices[1] = DirectX::XMFLOAT3(
        cameraPos.x + dir.x * f - up.x * yFar + right.x * xFar,
        cameraPos.y + dir.y * f - up.y * yFar + right.y * xFar,
        cameraPos.z + dir.z * f - up.z * yFar + right.z * xFar
    );
    farVertices[2] = DirectX::XMFLOAT3(
        cameraPos.x + dir.x * f + up.x * yFar + right.x * xFar,
        cameraPos.y + dir.y * f + up.y * yFar + right.y * xFar,
        cameraPos.z + dir.z * f + up.z * yFar + right.z * xFar
    );
    farVertices[3] = DirectX::XMFLOAT3(
        cameraPos.x + dir.x * f + up.x * yFar - right.x * xFar,
        cameraPos.y + dir.y * f + up.y * yFar - right.y * xFar,
        cameraPos.z + dir.z * f + up.z * yFar - right.z * xFar
    );

    // Строим 6 плоскостей как в лекции (по 4 точкам)
    DirectX::XMFLOAT4 frustum[6];
    frustum[0] = BuildPlane(nearVertices[0], nearVertices[1], nearVertices[2], nearVertices[3]); // Ближняя
    frustum[1] = BuildPlane(nearVertices[0], farVertices[0], farVertices[1], nearVertices[1]);   // Левая
    frustum[2] = BuildPlane(nearVertices[1], farVertices[1], farVertices[2], nearVertices[2]);   // Верхняя
    frustum[3] = BuildPlane(nearVertices[2], farVertices[2], farVertices[3], nearVertices[3]);   // Правая
    frustum[4] = BuildPlane(nearVertices[3], farVertices[3], farVertices[0], nearVertices[0]);   // Нижняя
    frustum[5] = BuildPlane(farVertices[1], farVertices[0], farVertices[3], farVertices[2]);     // Дальняя

    // Проверяем каждый инстанс
    std::vector<DirectX::XMFLOAT4> visibleIds(MaxInst);
    m_visibleInstances = 0;

    for (UINT i = 0; i < m_instCount; i++)
    {
        if (IsBoxInside(frustum, m_geomBBs[i].vmin, m_geomBBs[i].vmax))
        {
            visibleIds[m_visibleInstances] = DirectX::XMFLOAT4((float)i, 0.0f, 0.0f, 0.0f);
            m_visibleInstances++;
        }
    }

    // Обновляем буфер видимых индексов
    D3D11_MAPPED_SUBRESOURCE mapped;
    if (SUCCEEDED(m_pDeviceContext->Map(m_pGeomBufferInstVis, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        memcpy(mapped.pData, visibleIds.data(), sizeof(DirectX::XMFLOAT4) * m_visibleInstances);
        m_pDeviceContext->Unmap(m_pGeomBufferInstVis, 0);
    }
}

// === ФУНКЦИЯ ЗАГРУЗКИ ТЕКСТУРЫ ИЗ DDS ===
bool LoadTextureArray()
{
    HRESULT result = S_OK;

    // Загружаем две текстуры: Brick.dds и Kitty.dds
    TextureDesc textureDesc[2];
    bool ddsRes = LoadDDS(L"Brick.dds", textureDesc[0]);
    //bool ddsRes = LoadDDS(L"Ice.dds", textureDesc[0]);
    if (ddsRes)
        ddsRes = LoadDDS(L"Kitty.dds", textureDesc[1]);

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
    if (!LoadDDS(L"BrickNM.dds", textureDesc))
        //if (!LoadDDS(L"IceNM.dds", textureDesc))
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

    // Загружаем массив текстур для кубов (Brick.dds и Kitty.dds)
    if (!LoadTextureArray())
    {
        MessageBox(NULL, L"Не удалось загрузить текстуры кубов!", L"Ошибка", MB_OK);
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
    m_lights[0].pos = DirectX::XMFLOAT4(1.0f, 0.70f, 0.0f, 1.0f);
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
        if (wParam == VK_TAB) // TAB для переключения ImGui
        {
            m_showImGui = !m_showImGui;
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

    // Создание текстуры для постпроцессинга
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

    // Создание Render Target View
    result = m_pDevice->CreateRenderTargetView(m_pColorBuffer, nullptr, &m_pColorBufferRTV);
    if (FAILED(result)) return false;

    // Создание Shader Resource View
    result = m_pDevice->CreateShaderResourceView(m_pColorBuffer, nullptr, &m_pColorBufferSRV);
    if (FAILED(result)) return false;

    return true;
}

bool InitBuffers()
{
    HRESULT result = S_OK;

    // Инициализируем векторы для инстансов
    m_geomBuffers.resize(MaxInst);
    m_geomBBs.resize(MaxInst);

    // 1. Константный буфер для ВСЕХ инстансов
    D3D11_BUFFER_DESC geomBufferDesc = {};
    geomBufferDesc.ByteWidth = sizeof(GeomBuffer) * MaxInst;
    geomBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    geomBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    geomBufferDesc.CPUAccessFlags = 0;
    geomBufferDesc.MiscFlags = 0;
    geomBufferDesc.StructureByteStride = 0;

    result = m_pDevice->CreateBuffer(&geomBufferDesc, nullptr, &m_pGeomBufferInst);
    if (FAILED(result)) return false;

    // 2. Буфер для видимых индексов (используется при culling)
    D3D11_BUFFER_DESC visBufferDesc = {};
    visBufferDesc.ByteWidth = sizeof(DirectX::XMFLOAT4) * MaxInst;
    visBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    visBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    visBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    visBufferDesc.MiscFlags = 0;
    visBufferDesc.StructureByteStride = 0;

    result = m_pDevice->CreateBuffer(&visBufferDesc, nullptr, &m_pGeomBufferInstVis);
    if (FAILED(result)) return false;

    // 3. Инициализация первых двух инстансов (как были кубы)
    // Первый куб (Brick текстура, с normal map, блеск 0)
    InitGeomInstance(0, -1.0f, 0.0f, 0.0f, 0.0f, 0, true);

    // Второй куб (Brick текстура, с normal map, блеск 64)
    InitGeomInstance(1, 1.0f, 0.0f, 0.0f, 64.0f, 0, true);

    // Инициализируем еще 8 случайных инстансов для демонстрации

    m_instCount = 2; // Всего 2 инстансов в начале

    // Обновляем GPU буфер
    m_pDeviceContext->UpdateSubresource(m_pGeomBufferInst, 0, nullptr, m_geomBuffers.data(), 0, 0);

    // 4. Константный буфер сцены
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

    static const TextureTangentVertex Vertices[] = {
        // Bottom face
        { -0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f },
        { 0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f },
        { 0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f },
        { -0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f },

        // Top face
        { -0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f },
        { 0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f },
        { 0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f },
        { -0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f },

        // Front face (правая грань)
        { 0.5f, -0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f },
        { 0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f },
        { 0.5f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f },
        { 0.5f, 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },

        // Back face (левая грань)
        { -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f },
        { -0.5f, -0.5f, -0.5f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f },
        { -0.5f, 0.5f, -0.5f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f },
        { -0.5f, 0.5f, 0.5f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f },

        // Left face (передняя грань)
        { 0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f },
        { -0.5f, -0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f },
        { -0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f },
        { 0.5f, 0.5f, 0.5f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f },

        // Right face (задняя грань)
        { -0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f },
        { 0.5f, -0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f },
        { 0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f },
        { -0.5f, 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f }
    };

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(Vertices);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = sizeof(TextureTangentVertex);

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
    cbuffer GeomBufferInst : register(b1)
    {
        struct InstanceData {
            float4x4 m;
            float4x4 normalM;
            float4 shineSpeedTexIdNM;
            float4 posAngle;
        };
        InstanceData geomBuffer[100];
    };
    
    cbuffer GeomBufferInstVis : register(b2)
    {
        float4 ids[100];
    };
    
    cbuffer SceneBuffer : register(b0)
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
        float3 tangent : TANGENT;
        float3 norm : NORMAL;
        float2 uv : TEXCOORD;
        uint instanceId : SV_InstanceID;
    };

    struct VSOutput
    {
        float4 pos : SV_POSITION;
        float3 worldPos : POSITION;
        float3 tangent : TANGENT;
        float3 norm : NORMAL;
        float2 uv : TEXCOORD;
        nointerpolation uint instanceId : INST_ID;
    };

    VSOutput main(VSInput vertex)
    {
        VSOutput result;
    
    // Получаем индекс видимого инстанса из буфера видимых
    uint visibleIdx = vertex.instanceId;
    uint realIdx = (uint)ids[visibleIdx].x;
    
    // Преобразуем вершину
    float4 worldPos = mul(float4(vertex.pos, 1.0), geomBuffer[realIdx].m);
    result.pos = mul(worldPos, vp);
    result.worldPos = worldPos.xyz;
    
    // Преобразуем касательный вектор и нормаль
    result.tangent = mul(float4(vertex.tangent, 0.0f), geomBuffer[realIdx].normalM).xyz;
    result.norm = mul(float4(vertex.norm, 0.0f), geomBuffer[realIdx].normalM).xyz;
    result.uv = vertex.uv;
    result.instanceId = realIdx; // Сохраняем реальный индекс для пиксельного шейдера
    
    return result;
}
)";

    const char* psSource = R"(
    Texture2DArray colorTexture : register(t0);  // ИЗМЕНЕНО: теперь массив текстур
    Texture2D normalMapTexture : register(t1);
    SamplerState colorSampler : register(s0);

    cbuffer SceneBuffer : register(b0)
    {
        float4x4 vp;
        float4 cameraPos;
        float4 lightInfo;
        struct Light { float4 pos; float4 color; };
        Light lights[10];
        float4 ambientColor;
    };

    cbuffer GeomBufferInst : register(b1)
    {
        struct InstanceData {
            float4x4 m;
            float4x4 normalM;
            float4 shineSpeedTexIdNM;
            float4 posAngle;
        };
        InstanceData geomBuffer[100];
    };

    struct VSOutput
    {
        float4 pos : SV_POSITION;
        float3 worldPos : POSITION;
        float3 tangent : TANGENT;
        float3 norm : NORMAL;
        float2 uv : TEXCOORD;
        nointerpolation uint instanceId : INST_ID;
    };

    // Функция вычисления освещения (оставить без изменений)
    float3 CalculateLighting(float3 objColor, float3 objNormal, float3 pos, float shine)
    {
        float3 finalColor = float3(0, 0, 0);
        
        // Отображение нормалей для отладки
        if (lightInfo.z > 0.5)
        {
            return objNormal * 0.5 + float3(0.5, 0.5, 0.5);
        }
        
        // Окружающее освещение
        finalColor = objColor * ambientColor.rgb;
        
        // Вклад от каждого источника света
        for (int i = 0; i < (int)lightInfo.x; i++)
        {
            float3 lightDir = lights[i].pos.xyz - pos;
            float lightDist = length(lightDir);
            lightDir /= lightDist;
            
            // Квадратичное затухание
            float atten = 1.0 / (lightDist * lightDist);
            atten = clamp(atten, 0.0, 1.0);
            
            // Рассеянная составляющая
            float diffuse = max(dot(lightDir, objNormal), 0.0);
            finalColor += objColor * diffuse * atten * lights[i].color.rgb;
            
            // Зеркальная составляющая (если есть блеск)
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
        uint idx = pixel.instanceId; // Берем индекс, переданный из вершинного шейдера
        
        // ВАЖНО: выборка из массива текстур по индексу из geomBuffer
        // geomBuffer[idx].shineSpeedTexIdNM.z содержит индекс текстуры (0 = Brick, 1 = Kitty)
        float3 color = colorTexture.Sample(colorSampler, float3(pixel.uv, geomBuffer[idx].shineSpeedTexIdNM.z)).rgb;

        float3 normal = normalize(pixel.norm);
        
        // Используем карту нормалей, если включено и она есть у этого инстанса
        if (lightInfo.y > 0.5 && geomBuffer[idx].shineSpeedTexIdNM.w > 0.5)
        {
            // Получаем нормаль из карты нормалей
            float3 texNormal = normalMapTexture.Sample(colorSampler, pixel.uv).rgb;
            texNormal = texNormal * 2.0 - 1.0; // Из [0,1] в [-1,1]
            
            // Преобразуем из касательного пространства в мировое
            float3 bitangent = normalize(cross(pixel.tangent, pixel.norm));
            float3x3 TBN = float3x3(pixel.tangent, bitangent, pixel.norm);
            normal = normalize(mul(texNormal, TBN));
        }
        
        float3 finalColor = CalculateLighting(color, normal, pixel.worldPos, geomBuffer[idx].shineSpeedTexIdNM.x);
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
        m_doCull ? 1.0f : 0.0f         // Использовать frustum culling
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

// === ФУНКЦИЯ РЕНДЕРИНГА ПОСТПРОЦЕССИНГА ===
void RenderPostProcess()
{
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

    // Пересоздаем текстуру для постпроцессинга
    if (m_pColorBuffer)
    {
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

        HRESULT result = m_pDevice->CreateTexture2D(&colorBufferDesc, nullptr, &m_pColorBuffer);
        if (SUCCEEDED(result))
        {
            result = m_pDevice->CreateRenderTargetView(m_pColorBuffer, nullptr, &m_pColorBufferRTV);
        }
        if (SUCCEEDED(result))
        {
            result = m_pDevice->CreateShaderResourceView(m_pColorBuffer, nullptr, &m_pColorBufferSRV);
        }
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

    // Рендерим сцену в текстуру для постпроцессинга
    ID3D11RenderTargetView* views[] = { m_pColorBufferRTV };
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
    UpdatePostProcessBuffer();

    // 1. РЕНДЕРИМ НЕПРОЗРАЧНЫЕ ОБЪЕКТЫ (КУБЫ) С ИНСТАНСИНГОМ
    m_pDeviceContext->OMSetDepthStencilState(m_pNormalDepthState, 0);
    m_pDeviceContext->OMSetBlendState(m_pOpaqueBlendState, nullptr, 0xFFFFFFFF);

    // Выполняем frustum culling
    CullBoxes();

    // Устанавливаем массив текстур (Brick + Kitty)
    ID3D11ShaderResourceView* cubeResources[] = { m_pTextureView, m_pTextureViewNM };
    m_pDeviceContext->PSSetShaderResources(0, 2, cubeResources);

    // Настраиваем пайплайн для инстансинга
    ID3D11Buffer* vertexBuffers[] = { m_pVertexBuffer };
    UINT strides[] = { sizeof(TextureTangentVertex) };
    UINT offsets[] = { 0 };
    m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);
    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetInputLayout(m_pInputLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

    ID3D11SamplerState* samplers[] = { m_pSampler };
    m_pDeviceContext->PSSetSamplers(0, 1, samplers);

    // Устанавливаем ВСЕ три константных буфера
    ID3D11Buffer* constantBuffers[] = { m_pSceneBuffer, m_pGeomBufferInst, m_pGeomBufferInstVis };
    m_pDeviceContext->VSSetConstantBuffers(0, 3, constantBuffers);
    m_pDeviceContext->PSSetConstantBuffers(0, 3, constantBuffers);

    // ВАЖНО: Используем DrawIndexedInstanced вместо отдельных вызовов DrawIndexed
    if (m_doCull)
    {
        m_pDeviceContext->DrawIndexedInstanced(36, m_visibleInstances, 0, 0, 0);
    }
    else
    {
        m_pDeviceContext->DrawIndexedInstanced(36, m_instCount, 0, 0, 0);
    }

    // 2. РЕНДЕРИМ МАЛЕНЬКИЕ СФЕРЫ (ИСТОЧНИКИ СВЕТА)
    RenderSmallSpheres();

    // 3. РЕНДЕРИМ SKYBOX
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

    // 4. РЕНДЕРИМ ПРОЗРАЧНЫЕ ОБЪЕКТЫ
    RenderTransparentObjects();

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
            m_lights[m_lightCount - 1].pos = DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
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

        // Окно управления инстансами
        ImGui::Begin("Instances control", &m_showImGui, ImGuiWindowFlags_AlwaysAutoResize); //Управление инстансами (экземплярами)

        ImGui::Checkbox("Frustum Culling", &m_doCull);
        ImGui::Text("Total instances: %d", m_instCount); //Всего экземпляров
        ImGui::Text("Visible instances: %d", m_visibleInstances); //Видимые экземпляры

        // Кнопки добавления/удаления инстансов
        if (ImGui::Button("Add Instance") && m_instCount < MaxInst) //Добавить экземпляр
        {
            float x = (rand() / (float)RAND_MAX) * 10.0f - 5.0f;
            float y = (rand() / (float)RAND_MAX) * 4.0f - 2.0f;
            float z = (rand() / (float)RAND_MAX) * 10.0f - 5.0f;
            float shine = (rand() / (float)RAND_MAX) > 0.5f ? 64.0f : 0.0f;
            int texId = (rand() / (float)RAND_MAX) > 0.5f ? 1 : 0;
            bool hasNormalMap = (texId == 0);

            InitGeomInstance(m_instCount, x, y, z, shine, texId, hasNormalMap);
            m_instCount++;

            // Обновляем GPU буфер
            m_pDeviceContext->UpdateSubresource(m_pGeomBufferInst, 0, nullptr, m_geomBuffers.data(), 0, 0);
        }

        ImGui::SameLine();

        if (ImGui::Button("Remove Instance") && m_instCount > 2) //Удалить экземпляр
        {
            m_instCount--;
        }

        ImGui::SameLine();

        if (ImGui::Button("Reset to 2"))
        {
            m_instCount = 2; // Оставляем первые два

            // Удаляем остальные
            for (int i = 2; i < MaxInst; i++)
            {
                m_geomBuffers[i] = GeomBuffer();
                m_geomBBs[i] = AABB();
            }


            // Обновляем GPU буфер
            m_pDeviceContext->UpdateSubresource(m_pGeomBufferInst, 0, nullptr, m_geomBuffers.data(), 0, 0);
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

    // Освобождаем ресурсы инстансинга
    SAFE_RELEASE(m_pGeomBufferInstVis);
    SAFE_RELEASE(m_pGeomBufferInst);

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