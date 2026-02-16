#include "DDS.h"
#include <windows.h>
#include <d3d11.h>
#include <assert.h>
#include <dxgi.h>
#include <cstring>
#include <vector>
#include <string>
#include <malloc.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

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