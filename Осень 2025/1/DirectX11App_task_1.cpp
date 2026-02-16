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

UINT m_width = 1280;  // Начальная ширина
UINT m_height = 720;  // Начальная высота

// Макрос для безопасного освобождения памяти (как в лекции)
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = nullptr; } }

// Прототипы функций
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
bool InitWindow(HINSTANCE hInstance, int nCmdShow);
bool InitDirectX();
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
        L"DirectX 11 Application - Заливка цветом",
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

// Функция рендеринга (как на слайде 30)
void Render()
{
    // 1. Очищаем состояние конвейера
    m_pDeviceContext->ClearState();

    // 2. Устанавливаем render target (куда будем рисовать)
    ID3D11RenderTargetView* views[] = { m_pBackBufferRTV };
    m_pDeviceContext->OMSetRenderTargets(1, views, nullptr);

    // 3. Устанавливаем область вывода (весь экран)
    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (FLOAT)m_width;
    viewport.Height = (FLOAT)m_height;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    m_pDeviceContext->RSSetViewports(1, &viewport);

    // 4. Заливаем экран фиксированным цветом (сине-зеленый)
    static const FLOAT BackColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f };  // RGBA
    m_pDeviceContext->ClearRenderTargetView(m_pBackBufferRTV, BackColor);

    //Чтобы изменить цвет заливки:
    //Надо поменять значения в массиве BackColor в функции Render() :
        // //Формат: { Красный, Зеленый, Синий, Прозрачность }
        //  //Каждое значение от 0.0f (минимум) до 1.0f (максимум)
        //static const FLOAT BackColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };  // Красный цвет

    // 5. Выводим результат на экран
    HRESULT result = m_pSwapChain->Present(0, 0);
    assert(SUCCEEDED(result));
}

// Функция очистки ресурсов
void Cleanup()
{
    SAFE_RELEASE(m_pBackBufferRTV);
    SAFE_RELEASE(m_pSwapChain);
    SAFE_RELEASE(m_pDeviceContext);
    SAFE_RELEASE(m_pDevice);
}