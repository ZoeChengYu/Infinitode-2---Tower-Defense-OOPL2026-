#include "Util/AtlasLoader.hpp"

#include <filesystem>
#include <fstream>

#include "Util/Image.hpp"
#include "Util/Logger.hpp"

namespace {

std::string MakeSafeFileName(const std::string &spriteName) {
    std::string safeName = spriteName;

    for (char &ch : safeName) {
        const bool isSafe = std::isalnum(static_cast<unsigned char>(ch)) != 0 ||
                            ch == '_' || ch == '-';
        if (!isSafe) {
            ch = '_';
        }
    }

    return safeName;
}

} // namespace

namespace Util {

AtlasLoader::AtlasLoader(const std::string &atlasPath,
                         const std::string &imagePath)
    : m_ImagePath(imagePath) {
    std::ifstream atlasFile(atlasPath);
    if (!atlasFile.is_open()) {
        LOG_ERROR("Failed to open atlas file: {}", atlasPath);
        return;
    }

    std::string currentLine;
    std::string currentSpriteName;

    while (std::getline(atlasFile, currentLine)) {
        const std::size_t first = currentLine.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            continue;
        }

        const std::size_t last = currentLine.find_last_not_of(" \t\r\n");
        currentLine = currentLine.substr(first, last - first + 1);

        // atlas 第一段像 combined.png 這種行先略過。
        if (currentLine.size() >= 4 &&
            currentLine.substr(currentLine.size() - 4) == ".png") {
            continue;
        }

        // 沒有冒號的行就是 sprite 名稱。
        if (currentLine.find(':') == std::string::npos) {
            currentSpriteName = currentLine;
            continue;
        }

        // 目前只處理 bounds。
        if (currentSpriteName.empty() ||
            currentLine.rfind("bounds:", 0) != 0) {
            continue;
        }

        AtlasRegion region;
        if (std::sscanf(currentLine.c_str(), "bounds:%d,%d,%d,%d", &region.x,
                        &region.y, &region.width, &region.height) != 4) {
            LOG_ERROR("Failed to parse bounds for sprite: {}",
                      currentSpriteName);
            continue;
        }

        m_AtlasRegions[currentSpriteName] = region;
    }
}

std::shared_ptr<Image> AtlasLoader::Get(const std::string &spriteName) const {

    if (m_Cache.count(spriteName)) {
        return m_Cache[spriteName];
    }

    const auto regionIt = m_AtlasRegions.find(spriteName);
    if (regionIt == m_AtlasRegions.end()) {
        LOG_ERROR("Sprite not found in atlas: {}", spriteName);
        return nullptr;
    }

    SDL_Surface *atlasSurface = IMG_Load(m_ImagePath.c_str());
    if (atlasSurface == nullptr) {
        LOG_ERROR("Failed to load atlas image: {}", m_ImagePath);
        LOG_ERROR("{}", IMG_GetError());
        return nullptr;
    }

    const AtlasRegion region = regionIt->second;

    if (region.x < 0 || region.y < 0 || region.width <= 0 ||
        region.height <= 0 || region.x + region.width > atlasSurface->w ||
        region.y + region.height > atlasSurface->h) {
        LOG_ERROR("Sprite region is outside atlas image: {}", spriteName);
        SDL_FreeSurface(atlasSurface);
        return nullptr;
    }

    // 建立一張新的小圖，大小就是這個 sprite 的大小。
    SDL_Surface *spriteSurface = SDL_CreateRGBSurfaceWithFormat(
        0, region.width, region.height, 32, atlasSurface->format->format);
    if (spriteSurface == nullptr) {
        LOG_ERROR("Failed to create sprite surface: {}", SDL_GetError());
        SDL_FreeSurface(atlasSurface);
        return nullptr;
    }

    // 從大圖中把這一塊複製到小圖。
    SDL_Rect sourceRect{region.x, region.y, region.width, region.height};
    if (SDL_BlitSurface(atlasSurface, &sourceRect, spriteSurface, nullptr) !=
        0) {
        LOG_ERROR("Failed to copy sprite surface: {}", SDL_GetError());
        SDL_FreeSurface(spriteSurface);
        SDL_FreeSurface(atlasSurface);
        return nullptr;
    }

    // Util::Image 需要圖片路徑，所以先存成暫存 bmp。
    const std::filesystem::path cacheDirectory =
        std::filesystem::temp_directory_path() / "ptsd_atlas_cache";
    std::filesystem::create_directories(cacheDirectory);

    const std::string cacheFilePath =
        (cacheDirectory / (MakeSafeFileName(spriteName) + ".bmp")).string();

    if (SDL_SaveBMP(spriteSurface, cacheFilePath.c_str()) != 0) {
        LOG_ERROR("Failed to save cached sprite: {}", SDL_GetError());
        SDL_FreeSurface(spriteSurface);
        SDL_FreeSurface(atlasSurface);
        return nullptr;
    }

    SDL_FreeSurface(spriteSurface);
    SDL_FreeSurface(atlasSurface);

    auto newImage = std::make_shared<Image>(cacheFilePath);
    m_Cache[spriteName] = newImage; 
    
    return newImage;
}

float AtlasLoader::Getsize(const std::string &spriteName) const {
    auto it = m_AtlasRegions.find(spriteName);

    if (it == m_AtlasRegions.end()) {
        LOG_ERROR("Sprite not found in atlas: {}", spriteName);
        return 0.0F;
    }

    // 回傳寬度（或者是你需要的尺寸單位）
    return static_cast<float>(it->second.width);
}

} // namespace Util