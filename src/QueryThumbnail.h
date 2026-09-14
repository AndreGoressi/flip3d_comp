#pragma once

#include <Windows.h>

enum class ThumbnailType : long
{
    Default       = 0,
    Snapshot      = 1,
    Iconic        = 2,
    BitmapPending = 3,
    Bitmap        = 4
};

using DwmpQueryThumbnailType_t = HRESULT(WINAPI*)(HTHUMBNAIL hThumbnailId, ThumbnailType* thumbType);

namespace ThumbQuery
{
    bool Initialize();
    bool GetType(HTHUMBNAIL hThumbnailId, ThumbnailType* outType);
}