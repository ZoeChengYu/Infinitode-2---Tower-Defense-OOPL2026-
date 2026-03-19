#ifndef UTIL_ATLAS_LOADER_HPP
#define UTIL_ATLAS_LOADER_HPP

#include "pch.hpp" // IWYU pragma: export

namespace Util {

class Image;

class AtlasLoader {
public:
    AtlasLoader(const std::string &atlasPath, const std::string &imagePath);

    std::shared_ptr<Image> Get(const std::string &spriteName) const;
    float Getsize(const std::string &spriteName) const;

private:
    struct AtlasRegion {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };

private:
    std::string m_ImagePath;
    std::unordered_map<std::string, AtlasRegion> m_AtlasRegions;
    mutable std::unordered_map<std::string, std::shared_ptr<Image>> m_Cache;
};

} // namespace Util

#endif
