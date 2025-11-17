#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <assert.h>
#include <dxgi.h>

//#pragma comment(lib, "d3d11.lib")
//#pragma comment(lib, "dxgi.lib")
//#pragma comment(lib, "d3dcompiler.lib")

// Глобальные переменные (как в лекции)
HWND g_hWnd = NULL;
ID3D11Device* m_pDevice = nullptr;
ID3D11DeviceContext* m_pDeviceContext = nullptr;
IDXGISwapChain* m_pSwapChain = nullptr;
ID3D11RenderTargetView* m_pBackBufferRTV = nullptr;

// === НОВЫЕ ПЕРЕМЕННЫЕ ДЛЯ ТРЕУГОЛЬНИКА ===
ID3D11Buffer* m_pVertexBuffer = nullptr;        // Буфер вершин (слайд 8)
ID3D11Buffer* m_pIndexBuffer = nullptr;         // Буфер индексов (слайд 11)
ID3D11VertexShader* m_pVertexShader = nullptr;  // Вершинный шейдер (слайд 23)
ID3D11PixelShader* m_pPixelShader = nullptr;    // Пиксельный шейдер (слайд 24)
ID3D11InputLayout* m_pInputLayout = nullptr;    // Разметка вершин (слайд 28)

UINT m_width = 1280;  // Начальная ширина
UINT m_height = 720;  // Начальная высота

// Макрос для безопасного освобождения памяти (как в лекции)
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = nullptr; } }

// === СТРУКТУРА ВЕРШИНЫ (слайд 7) ===
struct Vertex
{
    float x, y, z;      // Позиция в пространстве
    COLORREF color;     // Цвет вершины (как в Windows - 0x00BBGGRR)
};

// Прототипы функций
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
bool InitWindow(HINSTANCE hInstance, int nCmdShow);
bool InitDirectX();
bool InitTriangle();    // === НОВАЯ ФУНКЦИЯ ===
void Render();
void Cleanup();
void ResizeSwapChain(UINT width, UINT height);

// Главная функция программы (точка входа)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 1. Инициализируем окно
    if (!InitWindow(hInstance, nCmdShow))
    {
        MessageBox(NULL, L"Не удалось создать окно!", L"Ошибка", MB_OK);
        return -1;
    }

    // 2. Инициализируем DirectX (как в лекции)
    if (!InitDirectX())
    {
        MessageBox(NULL, L"Не удалось инициализировать DirectX!", L"Ошибка", MB_OK);
        Cleanup();
        return -1;
    }

    // === ВЫЗОВ ИНИЦИАЛИЗАЦИИ ТРЕУГОЛЬНИКА ===
    if (!InitTriangle())
    {
        MessageBox(NULL, L"Не удалось инициализировать треугольник!", L"Ошибка", MB_OK);
        Cleanup();
        return -1;
    }

    // 3. Главный цикл приложения (как на слайде 4)
    MSG msg = {};
    bool exit = false;

    while (!exit)
    {
        // Проверяем сообщения от Windows
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                exit = true;

            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            // Если нет сообщений - рисуем кадр
            Render();
        }
    }

    // 4. Очистка перед выходом
    Cleanup();
    return (int)msg.wParam;
}

// Функция создания окна
bool InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    // 1. Регистрируем класс окна
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;           // Указываем нашу функцию обработки сообщений
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"DirectX11Window";

    if (!RegisterClassEx(&wc))
        return false;

    // 2. Рассчитываем размер окна (как на слайде 6)
    RECT rc = {};
    rc.left = 0;
    rc.right = m_width;    // 1280
    rc.top = 0;
    rc.bottom = m_height;  // 720

    // AdjustWindowRect корректирует размер окна под рамки и заголовок
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, TRUE);

    // 3. Создаем окно
    g_hWnd = CreateWindow(
        L"DirectX11Window",
        L"DirectX 11 Application Треугольник в NDC",  // Изменили заголовок
        WS_OVERLAPPEDWINDOW,
        100, 100,                          // Позиция окна
        rc.right - rc.left,                // Ширина окна
        rc.bottom - rc.top,                // Высота окна  
        NULL, NULL, hInstance, NULL
    );

    if (!g_hWnd)
        return false;

    // 4. Показываем окно
    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    return true;
}

// Функция обработки сообщений окна
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:  // Обработка изменения размера окна (как в задании)
    {
        UINT newWidth = LOWORD(lParam);
        UINT newHeight = HIWORD(lParam);

        if (m_pSwapChain && newWidth > 0 && newHeight > 0)
        {
            // Меняем размер буферов swap chain
            ResizeSwapChain(newWidth, newHeight);
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

// Функция инициализации DirectX (основная логика из лекции)
bool InitDirectX()
{
    HRESULT result = S_OK;

    // === Шаг 1: Создаем фабрику DXGI (как на слайдах 11-12) ===
    IDXGIFactory* pFactory = nullptr;
    result = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&pFactory);

    if (FAILED(result))
        return false;

    // === Шаг 2: Выбираем видеокарту (адаптер) ===
    IDXGIAdapter* pSelectedAdapter = NULL;

    IDXGIAdapter* pAdapter = NULL;
    UINT adapterIdx = 0;

    // Перебираем все доступные видеокарты
    while (SUCCEEDED(pFactory->EnumAdapters(adapterIdx, &pAdapter)))
    {
        DXGI_ADAPTER_DESC desc;
        pAdapter->GetDesc(&desc);

        // Пропускаем "Microsoft Basic Render Driver" (программную эмуляцию)
        if (wcscmp(desc.Description, L"Microsoft Basic Render Driver") != 0)
        {
            pSelectedAdapter = pAdapter;
            break;  // Нашли подходящую видеокарту
        }

        pAdapter->Release();
        adapterIdx++;
    }

    if (pSelectedAdapter == NULL)
    {
        pFactory->Release();
        return false;
    }

    // === Шаг 3: Создаем устройство и контекст (как на слайде 15) ===
    D3D_FEATURE_LEVEL level;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;  // Включаем отладочный слой в Debug версии
#endif

    result = D3D11CreateDevice(
        pSelectedAdapter,           // Выбранная видеокарта
        D3D_DRIVER_TYPE_UNKNOWN,   // Тип драйвера
        NULL,                      // Не используем программный драйвер
        flags,                     // Флаги создания
        levels,                    // Требуемый уровень функциональности
        1,                         // Количество уровней
        D3D11_SDK_VERSION,         // Версия SDK
        &m_pDevice,                // Возвращаемое устройство
        &level,                    // Полученный уровень
        &m_pDeviceContext          // Возвращаемый контекст
    );

    // Проверяем успешность создания (как в лекции)
    assert(level == D3D_FEATURE_LEVEL_11_0);
    assert(SUCCEEDED(result));

    if (FAILED(result))
    {
        pSelectedAdapter->Release();
        pFactory->Release();
        return false;
    }

    // === Шаг 4: Создаем swap chain (как на слайдах 26-27) ===
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 2;  // Два буфера (передний и задний)
    swapChainDesc.BufferDesc.Width = m_width;
    swapChainDesc.BufferDesc.Height = m_height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // Стандартный формат RGBA
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 0;    // Автоматическая частота
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;  // Будем использовать для вывода
    swapChainDesc.OutputWindow = g_hWnd;  // Наше окно
    swapChainDesc.SampleDesc.Count = 1;    // Без сглаживания
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = true;         // Оконный режим
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;  // Современный метод
    swapChainDesc.Flags = 0;

    result = pFactory->CreateSwapChain(m_pDevice, &swapChainDesc, &m_pSwapChain);
    assert(SUCCEEDED(result));

    // === Шаг 5: Создаем Render Target View (как на слайде 29) ===
    ID3D11Texture2D* pBackBuffer = NULL;
    result = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
    assert(SUCCEEDED(result));

    if (SUCCEEDED(result))
    {
        result = m_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pBackBufferRTV);
        assert(SUCCEEDED(result));
        SAFE_RELEASE(pBackBuffer);
    }

    // === Шаг 6: Очищаем временные объекты ===
    pSelectedAdapter->Release();
    pFactory->Release();

    return SUCCEEDED(result);
}

// === НОВАЯ ФУНКЦИЯ ПОСЛЕ InitDirectX ===
bool InitTriangle()
{
    HRESULT result = S_OK;

    // 1. СОЗДАНИЕ ВЕРШИН (слайды 7-9)
    // Создаем массив из 3 вершин треугольника в координатах NDC (Normalized Device Coordinates)
    // NDC - это система координат, где экран идет от -1 до 1 по X и Y
    // Центр экрана - это (0,0), левый нижний угол (-1,-1), правый верхний (1,1)
    
    struct Vertex
    {
        float x, y, z;
        COLORREF color;  // Оригинальный тип из лекций
    };

    static const Vertex Vertices[] = {
        // Координаты + цвет
        {-0.5f, -0.5f, 0.0f, RGB(55, 0, 105)},    // Левая нижняя вершина - индиго
        { 0.5f, -0.5f, 0.0f, RGB(160, 0, 255)},    // Правая нижняя вершина - фиолетовый  
        { 0.0f, 0.5f, 0.0f, RGB(255, 0, 255)}     // Верхняя вершина - пурпурный

        // {-0.5f, -0.5f, 0.0f, RGB(255, 0, 0)},    // Левая нижняя вершина - красный цвет
        // { 0.5f, -0.5f, 0.0f, RGB(0, 255, 0)},    // Правая нижняя вершина - зеленый цвет  
        // { 0.0f,  0.5f, 0.0f, RGB(0, 0, 255)}     // Верхняя вершина - синий цвет
    };

    // Описываем свойства буфера вершин (куда мы положим наши вершины)
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(Vertices);           // Размер буфера = размер всего массива вершин
    desc.Usage = D3D11_USAGE_IMMUTABLE;          // Буфер неизменяемый (только для чтения видеокартой)
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;   // Это буфер вершин (а не индексный или другой)
    desc.CPUAccessFlags = 0;                     // Процессор не может изменять этот буфер
    desc.MiscFlags = 0;                          // Дополнительные флаги не нужны
    desc.StructureByteStride = 0;                // Размер одного элемента (0 - автоматически)

    // Подготавливаем данные для загрузки в буфер
    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = Vertices;                     // Указатель на наш массив вершин в оперативной памяти
    data.SysMemPitch = 0;                        // Размер одной строки  // ИСПРАВЛЕНО: 0 для вершинных буферов 
    data.SysMemSlicePitch = 0;                   // Размер среза (для 3D текстур, здесь не используется)

    // СОЗДАЕМ БУФЕР ВЕРШИН в видеопамяти видеокарты
    // Видеокарта получает копию наших вершин и работает с ней напрямую (это быстро!)
    result = m_pDevice->CreateBuffer(&desc, &data, &m_pVertexBuffer);
    if (FAILED(result)) return false;

    // 2. СОЗДАНИЕ ИНДЕКСОВ (слайды 11-12)
    // Индексы говорят видеокарте, в каком порядке соединять вершины для создания треугольников
    // Вместо того чтобы передавать вершины несколько раз, мы передаем их один раз и указываем порядок
    static const USHORT Indices[] = { 0, 2, 1 };  // Соединяем вершины 0→1→2 чтобы получить треугольник // ИСПРАВЛЕНО: по часовой стрелке

    // Описываем свойства буфера индексов (аналогично буферу вершин)
    desc = {};
    desc.ByteWidth = sizeof(Indices);            // Размер = размер массива индексов
    desc.Usage = D3D11_USAGE_IMMUTABLE;          // Тоже неизменяемый
    desc.BindFlags = D3D11_BIND_INDEX_BUFFER;    // Это буфер ИНДЕКСОВ (а не вершин)
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    // Подготавливаем данные индексов
    data = {};
    data.pSysMem = Indices;                      // Указатель на массив индексов
    data.SysMemPitch = 0;  // ИСПРАВЛЕНО для COLORREF
    data.SysMemSlicePitch = 0;

    // СОЗДАЕМ БУФЕР ИНДЕКСОВ в видеопамяти
    result = m_pDevice->CreateBuffer(&desc, &data, &m_pIndexBuffer);
    if (FAILED(result)) return false;

    // 3. КОМПИЛЯЦИЯ ШЕЙДЕРОВ (слайды 20-24, 49)
    // Шейдеры - это маленькие программы, которые работают на видеокарте
    // Они говорят видеокарте КАК обрабатывать вершины и пиксели

    // ВЕРШИННЫЙ ШЕЙДЕР - работает с каждой вершиной отдельно
    // Получает позицию и цвет вершины, возвращает конечную позицию и цвет
    const char* vsSource = R"(
        // Входные данные для вершинного шейдера (то что приходит из нашего буфера вершин)
        struct VSInput
        {
            float3 pos : POSITION;   // POSITION - системная семантика для позиции
            float4 color : COLOR;    // COLOR - семантика для цвета
        };

        // Выходные данные вершинного шейдера (то что передается дальше в конвейер)
        struct VSOutput
        {
            float4 pos : SV_Position;  // SV_Position - ОБЯЗАТЕЛЬНО для конечной позиции
            float4 color : COLOR;      // Цвет, который пойдет в пиксельный шейдер
        };

        // Главная функция вершинного шейдера - вызывается для КАЖДОЙ вершины
        VSOutput vs(VSInput vertex)
        {
            VSOutput result;
            // Преобразуем позицию из float3 в float4, добавляя 1.0 как w-компоненту
            // Это нужно для работы с однородными координатами
            result.pos = float4(vertex.pos, 1.0);
            // Просто передаем цвет дальше без изменений
            result.color = vertex.color.bgra;
            return result;
        }
    )";

    // ПИКСЕЛЬНЫЙ ШЕЙДЕР - работает с каждым пикселем отдельно
    // Получает интерполированные данные от вершинного шейдера, возвращает конечный цвет пикселя
    const char* psSource = R"(
        // Входные данные для пиксельного шейдера (то что пришло из вершинного шейдера)
        struct VSOutput
        {
            float4 pos : SV_Position;  // Позиция пикселя
            float4 color : COLOR;      // Цвет, интерполированный между вершинами
        };

        // Главная функция пиксельного шейдера - вызывается для КАЖДОГО пикселя
        // SV_Target0 - означает что результат записывается в первый render target
        float4 ps(VSOutput pixel) : SV_Target0
        {
            // Просто возвращаем цвет который пришел из вершинного шейдера
            // Видеокарта автоматически интерполирует цвета между вершинами треугольника
            return pixel.color;
        }
    )";

    // Переменные для хранения скомпилированных шейдеров и ошибок
    ID3DBlob* pVSBlob = nullptr;    // Скомпилированный вершинный шейдер
    ID3DBlob* pPSBlob = nullptr;    // Скомпилированный пиксельный шейдер  
    ID3DBlob* pErrorBlob = nullptr; // Сообщения об ошибках компиляции

    // Флаги компиляции - как именно компилировать шейдеры
    UINT flags = 0;
#ifdef _DEBUG
    // В отладочном режиме включаем отладочную информацию и отключаем оптимизации
    // чтобы было проще искать ошибки в шейдерах
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    // КОМПИЛИРУЕМ ВЕРШИННЫЙ ШЕЙДЕР:
    // Берем исходный код → компилируем в байткод для видеокарты → создаем шейдер
    result = D3DCompile(vsSource,        // Исходный код шейдера
        strlen(vsSource), // Длина исходного кода
        "VS",             // Имя файла (для отладки)
        nullptr,          // Макросы (не используем)
        nullptr,          // Include обработчик (не используем)
        "vs",             // Имя входной функции
        "vs_5_0",         // Версия шейдерной модели (Vertex Shader 5.0)
        flags,            // Флаги компиляции
        0,                // Дополнительные флаги
        &pVSBlob,         // Результат - скомпилированный шейдер
        &pErrorBlob);     // Результат - сообщения об ошибках

    if (FAILED(result))
    {
        // Если компиляция не удалась - выводим ошибки в отладочную консоль
        if (pErrorBlob)
        {
            OutputDebugStringA((const char*)pErrorBlob->GetBufferPointer());
            pErrorBlob->Release();
        }
        return false;
    }

    // СОЗДАЕМ ВЕРШИННЫЙ ШЕЙДЕР из скомпилированного байткода
    result = m_pDevice->CreateVertexShader(pVSBlob->GetBufferPointer(),  // Указатель на байткод
        pVSBlob->GetBufferSize(),      // Размер байткода
        nullptr,                       // Указатель на класс линковки
        &m_pVertexShader);             // Результат - наш шейдер
    if (FAILED(result))
    {
        pVSBlob->Release();
        return false;
    }

    // КОМПИЛИРУЕМ ПИКСЕЛЬНЫЙ ШЕЙДЕР (аналогично вершинному)
    result = D3DCompile(psSource, strlen(psSource), "PS", nullptr, nullptr,
        "ps", "ps_5_0", flags, 0, &pPSBlob, &pErrorBlob);
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

    // СОЗДАЕМ ПИКСЕЛЬНЫЙ ШЕЙДЕР
    result = m_pDevice->CreatePixelShader(pPSBlob->GetBufferPointer(),
        pPSBlob->GetBufferSize(),
        nullptr, &m_pPixelShader);
    if (FAILED(result))
    {
        pVSBlob->Release();
        pPSBlob->Release();
        return false;
    }

    // 4. СОЗДАНИЕ РАЗМЕТКИ ВЕРШИН (слайд 28)
    // Input Layout - это "инструкция" для DirectX о том, как устроены данные в нашем буфере вершин
    // Он связывает поля в нашем буфере с входными параметрами вершинного шейдера

    // Описываем КАЖДОЕ поле в нашей структуре Vertex:
    static const D3D11_INPUT_ELEMENT_DESC InputDesc[] = {
        // Первое поле - ПОЗИЦИЯ (3 числа float)
        { "POSITION",                          // Имя семантики (должно совпадать с шейдером)
          0,                                   // Индекс семантики
          DXGI_FORMAT_R32G32B32_FLOAT,         // Формат данных: 3 float числа (R32G32B32 = X,Y,Z)
          0,                                   // Номер входного слота
          0,                                   // Смещение от начала вершины (в байтах)
          D3D11_INPUT_PER_VERTEX_DATA,         // Это данные ВЕРШИНЫ (а не экземпляра)
          0 },                                 // Дополнительные флаги

        // Второе поле - ЦВЕТ (4 байта в формате BGRA)
        { "COLOR",                             // Имя семантики
          0,                                   // Индекс семантики  
          DXGI_FORMAT_B8G8R8A8_UNORM,          // Формат: 4 байта Blue,Green,Red,Alpha (0-255 → 0.0-1.0)
          0,                                   // Номер входного слота
          12,                                  // Смещение: позиция занимает 12 байт (3 float × 4 байта)
          D3D11_INPUT_PER_VERTEX_DATA,         // Это данные вершины
          0 }
    };

    // СОЗДАЕМ INPUT LAYOUT
    // Нужно передать скомпилированный вершинный шейдер, чтобы DirectX проверил совместимость
    result = m_pDevice->CreateInputLayout(InputDesc,          // Наш массив описаний полей
        2,                   // Количество полей в массиве
        pVSBlob->GetBufferPointer(),  // Скомпилированный шейдер
        pVSBlob->GetBufferSize(),     // Размер шейдера
        &m_pInputLayout);    // Результат - наш input layout

// Освобождаем временные объекты (они больше не нужны)
    pVSBlob->Release();    // Скомпилированный шейдер
    pPSBlob->Release();    // Скомпилированный шейдер
    if (pErrorBlob) pErrorBlob->Release();  // Сообщения об ошибках

    return SUCCEEDED(result);  // Возвращаем true если все успешно, false если была ошибка
}

// Функция изменения размера swap chain (для обработки WM_SIZE)
void ResizeSwapChain(UINT width, UINT height)
{
    if (width == 0 || height == 0)
        return;

    // 1. Освобождаем старый render target view
    SAFE_RELEASE(m_pBackBufferRTV);

    // 2. Меняем размер буферов swap chain
    HRESULT result = m_pSwapChain->ResizeBuffers(
        0,          // Сохраняем текущее количество буферов
        width,      // Новая ширина
        height,     // Новая высота
        DXGI_FORMAT_UNKNOWN,  // Сохраняем текущий формат
        0           // Флаги
    );

    if (SUCCEEDED(result))
    {
        m_width = width;
        m_height = height;

        // 3. Создаем новый render target view
        ID3D11Texture2D* pBackBuffer = NULL;
        result = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);

        if (SUCCEEDED(result))
        {
            result = m_pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pBackBufferRTV);
            SAFE_RELEASE(pBackBuffer);
        }
    }
}

// === ЗАМЕНИЛИ ВЕСЬ КОД ЭТОЙ ФУНКЦИИ ===
void Render()
{
    // 1. Очищаем состояние конвейера
    m_pDeviceContext->ClearState();

    // 2. Устанавливаем render target (слайд 52)
    ID3D11RenderTargetView* views[] = { m_pBackBufferRTV };
    m_pDeviceContext->OMSetRenderTargets(1, views, nullptr);

    // 3. Очищаем задний буфер серым цветом
    static const FLOAT BackColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; //черный фон
    m_pDeviceContext->ClearRenderTargetView(m_pBackBufferRTV, BackColor);
    
    //Чтобы изменить цвет заливки:
    //Надо поменять значения в массиве BackColor в функции Render() :
        // //Формат: { Красный, Зеленый, Синий, Прозрачность }
        //  //Каждое значение от 0.0f (минимум) до 1.0f (максимум)
        //static const FLOAT BackColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };  // Красный цвет
    // Для черного цвета: { 0.0f, 0.0f, 0.0f, 1.0f }
    // Для индиго: { 75.0f/255.0f, 0.0f, 130.0f/255.0f, 1.0f }
    // Темно-серый: { 0.25f, 0.25f, 0.25f, 1.0f } 


    // 4. Устанавливаем область вывода (viewport)
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (FLOAT)m_width;
    viewport.Height = (FLOAT)m_height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_pDeviceContext->RSSetViewports(1, &viewport);

    // 5. НАСТРОЙКА INPUT ASSEMBLER ДЛЯ ТРЕУГОЛЬНИКА (слайд 32, 52-54)
    ID3D11Buffer* vertexBuffers[] = { m_pVertexBuffer };
    UINT strides[] = { 16 };  // 12 байт (3×float) + 4 байта (COLORREF)
    UINT offsets[] = { 0 };
    m_pDeviceContext->IASetVertexBuffers(0, 1, vertexBuffers, strides, offsets);

    m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    m_pDeviceContext->IASetInputLayout(m_pInputLayout);
    m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 6. УСТАНОВКА ШЕЙДЕРОВ (слайд 54)
    m_pDeviceContext->VSSetShader(m_pVertexShader, nullptr, 0);
    m_pDeviceContext->PSSetShader(m_pPixelShader, nullptr, 0);

    // 7. ВЫЗОВ ОТРИСОВКИ (слайд 54)
    m_pDeviceContext->DrawIndexed(3, 0, 0);

    // 8. Выводим результат на экран
    HRESULT result = m_pSwapChain->Present(0, 0);
    assert(SUCCEEDED(result));
}  

// Функция очистки ресурсов
void Cleanup()
{
    // === НОВЫЕ СТРОКИ ДЛЯ ТРЕУГОЛЬНИКА ===
    // Освобождаем ресурсы треугольника
    SAFE_RELEASE(m_pInputLayout);
    SAFE_RELEASE(m_pVertexShader);
    SAFE_RELEASE(m_pPixelShader);
    SAFE_RELEASE(m_pIndexBuffer);
    SAFE_RELEASE(m_pVertexBuffer);

    // Освобождаем базовые ресурсы DirectX
    SAFE_RELEASE(m_pBackBufferRTV);
    SAFE_RELEASE(m_pSwapChain);
    SAFE_RELEASE(m_pDeviceContext);
    SAFE_RELEASE(m_pDevice);
}