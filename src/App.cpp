#include "App.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "Util/AtlasLoader.hpp"
#include "Util/Image.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"

/// App.cpp 私有區域：放本檔案專用的型別與常數。
/// 用匿名 namespace 可避免符號外洩到其他 .cpp。
namespace {
/// GridPos: 地圖格子座標 (x, y)
/// WorldPos: 畫面/世界座標 (x, y)
using GridPos = std::pair<int, int>;
using WorldPos = std::pair<float, float>;

/// 地圖貼圖縮放比例。
constexpr float kTileScale = 0.25F;
/// BFS 四方向位移：右、下、左、上。
constexpr std::array<GridPos, 4> kNeighborOffsets = {{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}};

/// 地圖代碼 -> 圖集貼圖 key 對照表。
const std::unordered_map<std::string, std::string> kTileTextureByCode = {
    {"BUILD__", "tile-type-platform"},      // 建塔平台（不可走）
    {"SPAWN__", "tile-type-spawn-portal"},  // 出生點
    {"BASE___", "tile-type-target-base"},   // 終點

    {"HORIZ__", "tile-type-road-xoxo"},     // 水平直路
    {"VERT___", "tile-type-road-oxox"},     // 垂直直路

    {"TURN_LD", "tile-type-road-xxoo"},     // 轉角：左+下
    {"TURN_RD", "tile-type-road-xoox"},     // 轉角：右+下
    {"TURN_UL", "tile-type-road-oxxo"},     // 轉角：上+左
    {"TURN_UR", "tile-type-road-ooxx"},     // 轉角：上+右

    {"T_UP___", "tile-type-road-ooxo"},     // T 字：上+左+右
    {"T_DOWN_", "tile-type-road-xooo"},     // T 字：下+左+右
    {"T_LEFT_", "tile-type-road-oxox"},     // T 字：上+下+左
    {"T_RIGHT", "tile-type-road-ooox"},     // T 字：上+下+右

    {"CROSS__", "tile-type-road-oooo"},     // 十字路
    {"ALONE__", "tile-type-road-xxxx"},     // 孤立路
};

/// 可走地塊集合：BFS 只會在這些代碼上擴展。
const std::unordered_set<std::string> kWalkableTileCodes = {
    "HORIZ__", "CROSS__", "ALONE__", "T_UP___", "T_DOWN_",
    "VERT___", "T_RIGHT", "T_LEFT_", "TURN_LD", "TURN_RD",
    "TURN_UL", "TURN_UR", "BASE___",
};
} // namespace

void App::Start() {
    LOG_TRACE("Start");

    /// 步驟 1：載入圖集（地圖與敵人貼圖來源）。
    m_AtlasLoader = std::make_shared<Util::AtlasLoader>(
        RESOURCE_DIR "/combined.atlas",
        RESOURCE_DIR "/combined.png"
    );
    if (!m_AtlasLoader) {
        LOG_ERROR("m_AtlasLoader is null!");
        return;
    }

    /// 步驟 2：讀取地圖檔，建立 mapGrid[y][x]。
    std::ifstream mapFile(RESOURCE_DIR "/maps/map_01.txt");
    if (!mapFile.is_open()) {
        LOG_ERROR("無法開啟 maps/map_01.txt！請檢查檔案路徑。");
        return;
    }

    std::vector<std::vector<std::string>> mapGrid;
    int gridWidth = -1;
    std::string line;

    /// 地圖格式：
    /// - 每一行是一個 y 列。
    /// - 行內空白分隔多個 x 欄代碼。
    /// - 每列寬度必須一致（矩形地圖）。
    while (std::getline(mapFile, line)) {
        std::stringstream ss(line);
        std::vector<std::string> yTiles;
        for (std::string tileCode; ss >> tileCode;) {
            yTiles.push_back(tileCode);
        }

        if (yTiles.empty()) {
            continue;
        }

        /// 讀第一列時決定寬度，之後每列都要符合。
        if (gridWidth == -1) {
            gridWidth = static_cast<int>(yTiles.size());
        } else if (static_cast<int>(yTiles.size()) != gridWidth) {
            LOG_ERROR("地圖每一列寬度不一致。");
            return;
        }

        mapGrid.push_back(std::move(yTiles));
    }

    if (mapGrid.empty() || gridWidth <= 0) {
        LOG_ERROR("地圖是空的。");
        return;
    }

    /// 步驟 3：計算地圖在世界座標中的左上基準點。
    const int gridHeight = static_cast<int>(mapGrid.size());
    const float tileSize = m_AtlasLoader->Getsize("tile-type-platform") * kTileScale;
    const float mapOriginX = -(static_cast<float>(gridWidth) * tileSize) / 2.0F;
    const float mapOriginY = (static_cast<float>(gridHeight) * tileSize) / 2.0F;

    /// 格子座標 -> 世界座標（格子中心點）。
    auto toWorld = [&](int x, int y) -> WorldPos {
        const float worldX = mapOriginX + (static_cast<float>(x) * tileSize) + (tileSize / 2.0F);
        const float worldY = mapOriginY - (static_cast<float>(y) * tileSize) - (tileSize / 2.0F);
        return {worldX, worldY};
    };

    /// 步驟 4：建立地圖物件並同時收集出生點。
    std::vector<GridPos> spawnPoints;
    for (int y = 0; y < gridHeight; ++y) {
        for (int x = 0; x < gridWidth; ++x) {
            const std::string& tileCode = mapGrid[y][x];

            /// 出生點要記錄給後續尋路使用。
            if (tileCode == "SPAWN__") {
                spawnPoints.push_back({x, y});
            }

            /// 0000 視為空白格，不建立物件。
            if (tileCode == "0000") {
                continue;
            }

            /// 由代碼查貼圖。
            const auto textureIt = kTileTextureByCode.find(tileCode);
            if (textureIt == kTileTextureByCode.end()) {
                LOG_ERROR("字典中找不到此代碼: " + tileCode);
                continue;
            }

            /// 取貼圖失敗時跳過，避免建立無效物件。
            const auto tileImage = m_AtlasLoader->Get(textureIt->second);
            if (!tileImage) {
                LOG_ERROR("找不到對應的圖片: " + textureIt->second);
                continue;
            }

            /// 建立地塊物件，設定縮放與位置，加入 Renderer。
            auto tileObject = std::make_shared<Util::GameObject>();
            tileObject->SetDrawable(tileImage);
            tileObject->m_Transform.scale = {kTileScale, kTileScale};
            const auto [worldX, worldY] = toWorld(x, y);
            tileObject->m_Transform.translation = {worldX, worldY};
            m_Renderer.AddChild(tileObject);
            m_MapTiles.push_back(tileObject);
        }
    }

    /// 步驟 5：BFS 尋路（單一出生點 -> BASE___）。
    /// 原理：地圖是無權重網格圖，BFS 逐層擴展，第一次到 BASE___ 就是最短步數路徑。
    /// 流程：start 入隊 -> 出隊 current -> 檢查是否到終點 ->
    ///       展開四鄰居（邊界 + 可走 + 未訪問）-> 記錄 parent 並入隊。
    /// 收尾：若找到 target，沿 parent 回溯到 start，再 reverse 成前進方向。
    auto findPathToBase = [&](GridPos start) {
        std::vector<std::vector<bool>> visited(gridHeight, std::vector<bool>(gridWidth, false));
        std::vector<std::vector<GridPos>> parent(gridHeight, std::vector<GridPos>(gridWidth, {-1, -1}));

        std::queue<GridPos> q;
        q.push(start);
        visited[start.second][start.first] = true;

        GridPos target = {-1, -1};

        /// 主迴圈：直到找到終點或 queue 清空（不可達）。
        while (!q.empty()) {
            const GridPos current = q.front();
            q.pop();

            const int currX = current.first;
            const int currY = current.second;
            if (mapGrid[currY][currX] == "BASE___") {
                target = current;
                break;
            }

            for (const auto& [offsetX, offsetY] : kNeighborOffsets) {
                const int nextX = currX + offsetX;
                const int nextY = currY + offsetY;

                /// 先做邊界與重複訪問過濾。
                const bool inBounds = nextX >= 0 && nextX < gridWidth && nextY >= 0 && nextY < gridHeight;
                if (!inBounds || visited[nextY][nextX]) {
                    continue;
                }

                const std::string& nextTileCode = mapGrid[nextY][nextX];
                if (kWalkableTileCodes.find(nextTileCode) == kWalkableTileCodes.end()) {
                    continue;
                }

                visited[nextY][nextX] = true;
                parent[nextY][nextX] = current;
                q.push({nextX, nextY});
            }
        }

        std::vector<GridPos> path;
        if (target.first == -1) {
            return path;
        }

        for (GridPos current = target;; current = parent[current.second][current.first]) {
            path.push_back(current);
            if (current == start) {
                break;
            }
        }
        std::reverse(path.begin(), path.end());
        return path;
    };

    /// 步驟 6：為每個出生點建立世界座標路徑。
    for (size_t pathIndex = 0; pathIndex < spawnPoints.size(); ++pathIndex) {
        const auto gridPath = findPathToBase(spawnPoints[pathIndex]);
        if (gridPath.empty()) {
            LOG_ERROR("尋路失敗：第 " + std::to_string(pathIndex + 1) + " 個敵洞無法抵達主塔！");
            continue;
        }

        std::vector<WorldPos> worldPath;
        worldPath.reserve(gridPath.size());
        for (const auto& [x, y] : gridPath) {
            worldPath.push_back(toWorld(x, y));
        }

        /// m_AllPathsWorldPositions[pathIndex] 與 Enemy pathIndex 一一對應。
        m_AllPathsWorldPositions.push_back(std::move(worldPath));
        LOG_TRACE("尋路成功！已找到第 " + std::to_string(pathIndex + 1) + " 個敵洞的路徑。");
    }

    m_CurrentState = State::UPDATE;
}

void App::Update() {
    /// 鏡頭平移速度（每幀位移量）。
    /// 用除 kSpawnIntervalFrames 來確保提升幀率，不會導致鏡頭速度變化。
    const float cameraPanPerFrame = 800.0F / static_cast<float>(kSpawnIntervalFrames);
    float dx = 0.0F;
    float dy = 0.0F;

    /// WASD 輸入轉成位移向量。
    if (Util::Input::IsKeyPressed(Util::Keycode::A)) dx += cameraPanPerFrame;
    if (Util::Input::IsKeyPressed(Util::Keycode::D)) dx -= cameraPanPerFrame;
    if (Util::Input::IsKeyPressed(Util::Keycode::W)) dy -= cameraPanPerFrame;
    if (Util::Input::IsKeyPressed(Util::Keycode::S)) dy += cameraPanPerFrame;

    /// Q/E 縮放地圖（包含地圖、路徑、敵人）。
    /// 這裡同樣用 kSpawnIntervalFrames 控制每幀縮放步進。
    float zoomRequestFactor = 1.0F;
    /// 每幀縮放倍率，按住 Q/E 會連續乘上這個比例（放大/縮小）。
    const float zoomStepPerFrame = 1.0F + (2.0F / static_cast<float>(kSpawnIntervalFrames));
    if (Util::Input::IsKeyPressed(Util::Keycode::Q)) zoomRequestFactor *= zoomStepPerFrame;
    if (Util::Input::IsKeyPressed(Util::Keycode::E)) zoomRequestFactor /= zoomStepPerFrame;

    /// 目標縮放倍率限制在 [0.5, 3.0]，避免太小看不到或太大失真。
    const float targetZoom = std::clamp(m_MapZoom * zoomRequestFactor, 0.5F, 3.0F);
    /// 本幀真正要套用的「相對倍率」。
    const float appliedZoomFactor = targetZoom / m_MapZoom;
    if (appliedZoomFactor != 1.0F) {
        /// 地圖：位置與 scale 都要乘倍率，才能看起來以原點放大/縮小。
        for (auto& tile : m_MapTiles) {
            tile->m_Transform.translation.x *= appliedZoomFactor;
            tile->m_Transform.translation.y *= appliedZoomFactor;
            tile->m_Transform.scale.x *= appliedZoomFactor;
            tile->m_Transform.scale.y *= appliedZoomFactor;
        }

        /// 路徑節點也要同步縮放，敵人才會繼續貼著路線移動。
        for (auto& path : m_AllPathsWorldPositions) {
            for (auto& [x, y] : path) {
                x *= appliedZoomFactor;
                y *= appliedZoomFactor;
            }
        }

        /// 場上敵人的位置與 scale 同步縮放，避免和地圖尺寸不一致。
        for (auto& enemy : m_Enemies) {
            enemy->m_Transform.translation.x *= appliedZoomFactor;
            enemy->m_Transform.translation.y *= appliedZoomFactor;
            enemy->m_Transform.scale.x *= appliedZoomFactor;
            enemy->m_Transform.scale.y *= appliedZoomFactor;
        }

        /// 最後更新目前縮放狀態，下一幀會以這個倍率為基準。
        m_MapZoom = targetZoom;
    }

    /// 生怪邏輯：
    /// - 有路徑才生怪。
    /// - 冷卻為 0 時，每條路徑各生一隻。
    /// - 其餘幀遞減冷卻。
    if (!m_AllPathsWorldPositions.empty()) {
        if (m_SpawnCooldownFrames == 0) {
            auto enemyImage = m_AtlasLoader->Get("enemy-type-regular");
            for (size_t pathIndex = 0; pathIndex < m_AllPathsWorldPositions.size(); ++pathIndex) {
                /// pathIndex 決定這隻敵人走哪條路。
                auto enemy = std::make_shared<Enemy>(enemyImage, m_AllPathsWorldPositions[pathIndex], static_cast<int>(pathIndex));
                /// 新怪出生時要套用目前地圖縮放，避免和已縮放場景尺寸不一致。
                enemy->m_Transform.scale = {kTileScale * m_MapZoom, kTileScale * m_MapZoom};
                m_Enemies.push_back(enemy);
                m_Renderer.AddChild(enemy);
            }
            m_SpawnCooldownFrames = kSpawnIntervalFrames;
        } else {
            --m_SpawnCooldownFrames;
        }
    }

    /// 同步平移 helper：
    /// 鏡頭移動時，地圖、預算路徑、場上敵人要一起移動，避免錯位。
    auto translateBy = [&](float moveX, float moveY) {
        for (auto& tile : m_MapTiles) {
            tile->m_Transform.translation.x += moveX;
            tile->m_Transform.translation.y += moveY;
        }

        for (auto& path : m_AllPathsWorldPositions) {
            for (auto& [x, y] : path) {
                x += moveX;
                y += moveY;
            }
        }

        for (auto& enemy : m_Enemies) {
            enemy->m_Transform.translation.x += moveX;
            enemy->m_Transform.translation.y += moveY;
        }
    };

    /// 只有有位移輸入時才做整批平移。
    if (dx != 0.0F || dy != 0.0F) {
        translateBy(dx, dy);
    }

    /// 逐隻更新敵人移動。
    for (auto& enemy : m_Enemies) {
        enemy->Update(m_AllPathsWorldPositions[enemy->GetPathIndex()]);
    }

    /// 到達終點的敵人：先從 Renderer 移除，再從容器 erase。
    for (auto it = m_Enemies.begin(); it != m_Enemies.end();) {
        if ((*it)->HasReachedBase()) {
            m_Renderer.RemoveChild(*it);
            it = m_Enemies.erase(it);
        } else {
            ++it;
        }
    }

    /// 提交本幀渲染更新。
    m_Renderer.Update();

    /// ESC 或視窗關閉時結束遊戲迴圈。
    if (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) || Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}

/// 結束狀態入口。
void App::End() { // NOLINT(this method will mutate members in the future)
    LOG_TRACE("End");
}
