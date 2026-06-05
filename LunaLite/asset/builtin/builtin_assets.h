#pragma once
#include "../asset.h"

namespace lunalite::asset::builtin {
inline AssetHandle defaultMaterialHandle()
{
    return AssetHandle{0x4C'55'4E'41'4D'41'54'01ull};
}

inline AssetHandle errorMaterialHandle()
{
    return AssetHandle{0x4C'55'4E'41'4D'41'54'02ull};
}

inline AssetHandle cubeMeshHandle()
{
    return AssetHandle{0x4C'55'4E'41'4D'45'53'01ull};
}

inline AssetHandle planeMeshHandle()
{
    return AssetHandle{0x4C'55'4E'41'4D'45'53'02ull};
}
} // namespace lunalite::asset::builtin
