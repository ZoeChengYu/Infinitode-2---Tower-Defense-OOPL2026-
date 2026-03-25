#include "App.hpp" // App 類別宣告與成員定義

#include <algorithm>     // std::reverse std::clamp
#include <array>         // std::array 固定大小容器
#include <fstream>       // std::ifstream 讀地圖檔
#include <queue>         // std::queue BFS 佇列
#include <sstream>       // std::stringstream 拆地圖每行字串
#include <string>        // std::string
#include <unordered_map> // 地圖代碼對應貼圖 key
#include <unordered_set> // 可通行地塊集合查詢
#include <utility>       // std::pair
#include <vector>        // 動態陣列容器

#include "Util/AtlasLoader.hpp" // 圖集讀取與貼圖取得
#include "Util/Image.hpp"       // 圖片 Drawable 型別
#include "Util/Input.hpp"       // 鍵盤與視窗輸入
#include "Util/Keycode.hpp"     // 按鍵代碼列舉
#include "Util/Logger.hpp"      // LOG_TRACE LOG_ERROR

// 檔案內部常數與型別
namespace {
using GridPos = std::pair<int, int>;      // 地圖格子座標 (x, y)
using WorldPos = std::pair<float, float>; // 世界座標 (x, y)

constexpr float kTileScale = 0.3F; // 地圖貼圖縮放比例
// BFS 四方向位移: 右、下、左、上
constexpr std::array<GridPos, 4> kNeighborOffsets = {{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}};

// 地圖代碼 -> 圖集 key
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

// 可走地塊集合, BFS 只在這些代碼上擴展
const std::unordered_set<std::string> kWalkableTileCodes = {
    "HORIZ__", "CROSS__", "ALONE__", "T_UP___", "T_DOWN_",
    "VERT___", "T_RIGHT", "T_LEFT_", "TURN_LD", "TURN_RD",
    "TURN_UL", "TURN_UR", "BASE___",
};
} // namespace

int App::getkSpawnIntervalFrames(){
    return kSpawnIntervalFrames;
}

int App::setkSpawnIntervalFrames(int diffIntervalFrames)
{
    return kSpawnIntervalFrames+diffIntervalFrames;
}

bool App::HasClosedGate(int fromX, int fromY, int toX, int toY) const {
    for (const auto& gate : m_Gates) {
        if (!gate->IsClosed()) continue; // 打開的閘門不擋路

        if (gate->GetType() == GateType::VERTICAL) {
            // 直向閘門擋在 (gx, gy) 和 (gx+1, gy) 之間
            if (gate->GetGridY() == fromY && gate->GetGridY() == toY) {
                int minX = std::min(fromX, toX);
                if (gate->GetGridX() == minX) return true;
            }
        } else {
            // 橫向閘門擋在 (gx, gy) 和 (gx, gy+1) 之間
            if (gate->GetGridX() == fromX && gate->GetGridX() == toX) {
                int minY = std::min(fromY, toY);
                if (gate->GetGridY() == minY) return true;
            }
        }
    }
    return false;
}

void App::Start() {
    LOG_TRACE("Start");

    // 1. 載入圖集資源 失敗直接中止初始化
    m_AtlasLoader = std::make_shared<Util::AtlasLoader>(
        RESOURCE_DIR "/combined.atlas",
        RESOURCE_DIR "/combined.png"
    );
    if (!m_AtlasLoader) {
        LOG_ERROR("m_AtlasLoader is null!");
        return;
    }

    // 2. 讀取地圖檔到 mapGrid[y][x]
    std::ifstream mapFile(RESOURCE_DIR "/maps/map_01.txt");
    if (!mapFile.is_open()) {
        LOG_ERROR("無法開啟 maps/map_01.txt！請檢查檔案路徑。");
        return;
    }

    std::vector<std::vector<std::string>> mapGrid; // y=列, x=欄
    std::vector<std::string> gateLines;            // 暫存閘門資料
    int gridWidth = -1; // -1 代表寬度尚未決定
    std::string line;

    // 3. 逐行解析地圖 並驗證矩形格式
    bool parsingGrid = true; // 判斷現在在讀什麼

    while (std::getline(mapFile, line)) {
        if (line == "---") {
            parsingGrid = false; // 遇到分隔線，切換為暫存閘門資料
            continue;
        }

        if (parsingGrid) {
            std::stringstream ss(line);
            std::vector<std::string> yTiles; // 單一列代碼
            // 以空白切詞
            for (std::string tileCode; ss >> tileCode;) {
                yTiles.push_back(tileCode);
            }

            // 空行略過
            if (yTiles.empty()) continue;

            // 第一列決定寬度 後續列必須一致
            if (gridWidth == -1) {
                gridWidth = static_cast<int>(yTiles.size());
            } else if (static_cast<int>(yTiles.size()) != gridWidth) {
                LOG_ERROR("地圖每一列寬度不一致。");
                return;
            }

            // move 避免複製整列字串
            mapGrid.push_back(std::move(yTiles));
        } else {
            // 先把閘門的字串存起來，等 toWorld 定義好再處理
            gateLines.push_back(line);
        }
    }

    // 空地圖或非法寬度直接中止
    if (mapGrid.empty() || gridWidth <= 0) {
        LOG_ERROR("地圖是空的。");
        return;
    }

    const int gridHeight = static_cast<int>(mapGrid.size()); // 地圖高度 列數 = 讀進來的有效行數
    const float tileSize = m_AtlasLoader->Getsize("tile-type-platform") * kTileScale; // 每格世界邊長 = 原圖尺寸 * 縮放

    // 4. 計算置中基準與座標轉換函式
    const float mapOriginX = -(static_cast<float>(gridWidth) * tileSize) / 2.0F; // 地圖總寬的一半放到原點左邊 起始X落在最左側
    const float mapOriginY = (static_cast<float>(gridHeight) * tileSize) / 2.0F;  // 地圖總高的一半放到原點上方 起始Y落在最上側

    // 格子座標轉世界座標 取格子中心點
    auto toWorld = [&](int x, int y) -> WorldPos {
        const float worldX = mapOriginX + (static_cast<float>(x) * tileSize) + (tileSize / 2.0F);
        const float worldY = mapOriginY - (static_cast<float>(y) * tileSize) - (tileSize / 2.0F);
        return {worldX, worldY};
    };

    // 4.5. 現在 toWorld 定義好了，可以開始建立閘門物件
    for (const auto& gateLine : gateLines) {
        std::stringstream ss(gateLine);
        std::string typeStr;
        int gx, gy;

        if (ss >> typeStr >> gx >> gy) {
            GateType gType = (typeStr == "GATE_H") ? GateType::HORIZONTAL : GateType::VERTICAL;
            std::string texName = (gType == GateType::HORIZONTAL) ? "gate-barrier-type-horizontal" : "gate-barrier-type-vertical";

            // IDE 提示修復：把 gateImage 的宣告移到 if 條件內
            if (auto gateImage = m_AtlasLoader->Get(texName)) {
                auto gate = std::make_shared<Gate>(gateImage, gType, gx, gy);

                // 計算位置：放在兩格的中心點
                auto [world1X, world1Y] = toWorld(gx, gy);
                int nextX = gx + (gType == GateType::VERTICAL ? 1 : 0);
                int nextY = gy + (gType == GateType::HORIZONTAL ? 1 : 0);
                auto [world2X, world2Y] = toWorld(nextX, nextY);

                gate->m_Transform.translation = {(world1X + world2X) / 2.0f, (world1Y + world2Y) / 2.0f};
                gate->m_Transform.scale = {kTileScale, kTileScale};

                m_Gates.push_back(gate);
                m_Renderer.AddChild(gate); // 畫到畫面上
            }
        }
    }

    // 5. 單次掃描 同步建立地圖物件與收集出生點
    std::vector<GridPos> spawnPoints;
    for (int y = 0; y < gridHeight; ++y) {
        for (int x = 0; x < gridWidth; ++x) {
            const std::string& tileCode = mapGrid[y][x];

            // 出生點供後續尋路
            if (tileCode == "SPAWN__") {
                spawnPoints.push_back({x, y});
            }

            // 0000 視為空白格, 不建立物件
            if (tileCode == "0000") continue;

            // 查貼圖 key 查不到就略過該格
            const auto textureIt = kTileTextureByCode.find(tileCode);
            if (textureIt == kTileTextureByCode.end()) {
                LOG_ERROR("字典中找不到此代碼: " + tileCode);
                continue;
            }

            // 取圖失敗只跳過該格
            if (auto tileImage = m_AtlasLoader->Get(textureIt->second)) {
                auto tileObject = std::make_shared<Util::GameObject>();
                tileObject->SetDrawable(tileImage);
                tileObject->m_Transform.scale = {kTileScale, kTileScale};
                const auto [worldX, worldY] = toWorld(x, y);
                tileObject->m_Transform.translation = {worldX, worldY};

                m_Renderer.AddChild(tileObject);
                m_MapTiles.push_back(tileObject);
            } else {
                LOG_ERROR("找不到對應的圖片: " + textureIt->second);
            }
        }
    }

    // 6. BFS 尋路 start -> BASE___
    // IDE 提示修復：將 start 改為 const reference 傳遞避免不必要的複製
    auto findPathToBase = [&](const GridPos& start) {
        std::vector<std::vector<bool>> visited(gridHeight, std::vector<bool>(gridWidth, false));
        std::vector<std::vector<GridPos>> parent(gridHeight, std::vector<GridPos>(gridWidth, {-1, -1}));

        std::queue<GridPos> q;
        q.push(start);
        visited[start.second][start.first] = true;

        GridPos target = {-1, -1};

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

                const bool inBounds = nextX >= 0 && nextX < gridWidth && nextY >= 0 && nextY < gridHeight;
                if (!inBounds || visited[nextY][nextX]) continue;

                const std::string& nextTileCode = mapGrid[nextY][nextX];
                if (kWalkableTileCodes.find(nextTileCode) == kWalkableTileCodes.end()) continue;

                // 檢查閘門阻擋
                if (HasClosedGate(currX, currY, nextX, nextY)) continue;

                visited[nextY][nextX] = true;
                parent[nextY][nextX] = current;
                q.push({nextX, nextY});
            }
        }

        std::vector<GridPos> path;
        if (target.first == -1) return path;

        for (GridPos current = target;; current = parent[current.second][current.first]) {
            path.push_back(current);
            if (current == start) break;
        }
        std::reverse(path.begin(), path.end());
        return path;
    };

    // 7. 每個出生點建立一條世界座標路徑並儲存
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

        m_AllPathsWorldPositions.push_back(std::move(worldPath));
        LOG_TRACE("尋路成功！已找到第 " + std::to_string(pathIndex + 1) + " 個敵洞的路徑。");
    }

    // 8. 初始化完成 進入 UPDATE
    m_CurrentState = State::UPDATE;
}

void App::Update() {
    // 1. 讀取輸入 轉成平移與縮放參數
    // IDE 提示修復：改為 constexpr
    constexpr float cameraPanPerFrame = 800.0F / static_cast<float>(kSpawnIntervalFrames);
    float dx = 0.0F;
    float dy = 0.0F;

    if (Util::Input::IsKeyPressed(Util::Keycode::A)) dx += cameraPanPerFrame;
    if (Util::Input::IsKeyPressed(Util::Keycode::D)) dx -= cameraPanPerFrame;
    if (Util::Input::IsKeyPressed(Util::Keycode::W)) dy -= cameraPanPerFrame;
    if (Util::Input::IsKeyPressed(Util::Keycode::S)) dy += cameraPanPerFrame;

    float zoomRequestFactor = 1.0F;
    // IDE 提示修復：改為 constexpr
    constexpr float zoomStepPerFrame = 1.0F + (2.0F / static_cast<float>(kSpawnIntervalFrames));

    if (Util::Input::IsKeyPressed(Util::Keycode::Q)) zoomRequestFactor *= zoomStepPerFrame;
    if (Util::Input::IsKeyPressed(Util::Keycode::E)) zoomRequestFactor /= zoomStepPerFrame;

    const float targetZoom = std::clamp(m_MapZoom * zoomRequestFactor, 0.5F, 3.0F);
    const float appliedZoomFactor = targetZoom / m_MapZoom;

    if (appliedZoomFactor != 1.0F) {
        for (auto& tile : m_MapTiles) {
            tile->m_Transform.translation.x *= appliedZoomFactor;
            tile->m_Transform.translation.y *= appliedZoomFactor;
            tile->m_Transform.scale.x *= appliedZoomFactor;
            tile->m_Transform.scale.y *= appliedZoomFactor;
        }

        for (auto& gate : m_Gates) {
            gate->m_Transform.translation.x *= appliedZoomFactor;
            gate->m_Transform.translation.y *= appliedZoomFactor;
            gate->m_Transform.scale.x *= appliedZoomFactor;
            gate->m_Transform.scale.y *= appliedZoomFactor;
        }

        for (auto& path : m_AllPathsWorldPositions) {
            for (auto& [x, y] : path) {
                x *= appliedZoomFactor;
                y *= appliedZoomFactor;
            }
        }

        for (auto& enemy : m_Enemies) {
            enemy->m_Transform.translation.x *= appliedZoomFactor;
            enemy->m_Transform.translation.y *= appliedZoomFactor;
            enemy->m_Transform.scale.x *= appliedZoomFactor;
            enemy->m_Transform.scale.y *= appliedZoomFactor;
        }

        m_MapZoom = targetZoom;
    }

    // 2. 生怪冷卻與派怪...
    if (!m_AllPathsWorldPositions.empty()) {
        if (m_SpawnCooldownFrames == 0) {
            auto enemyImage = m_AtlasLoader->Get("enemy-type-regular");
            for (size_t pathIndex = 0; pathIndex < m_AllPathsWorldPositions.size(); ++pathIndex) {
                auto enemy = std::make_shared<Enemy>(enemyImage, m_AllPathsWorldPositions[pathIndex], static_cast<int>(pathIndex));
                enemy->m_Transform.scale = {kTileScale * m_MapZoom, kTileScale * m_MapZoom};
                m_Enemies.push_back(enemy);
                m_Renderer.AddChild(enemy);
            }
            m_SpawnCooldownFrames = getkSpawnIntervalFrames();
        } else {
            --m_SpawnCooldownFrames;
        }
    }

    // 3. 同步更新地圖 路徑 敵人座標 (把 m_Gates 加進去一起平移)
    auto translateBy = [&](float moveX, float moveY) {
        for (auto& tile : m_MapTiles) {
            tile->m_Transform.translation.x += moveX;
            tile->m_Transform.translation.y += moveY;
        }
        for (auto& gate : m_Gates) {
            gate->m_Transform.translation.x += moveX;
            gate->m_Transform.translation.y += moveY;
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

    if (dx != 0.0F || dy != 0.0F) {
        translateBy(dx, dy);
    }

    // 4. 逐隻更新敵人移動並回收到終點者...
    for (auto& enemy : m_Enemies) {
        enemy->Update(m_AllPathsWorldPositions[enemy->GetPathIndex()]);
    }

    for (auto it = m_Enemies.begin(); it != m_Enemies.end();) {
        if ((*it)->HasReachedBase()) {
            m_Renderer.RemoveChild(*it);
            it = m_Enemies.erase(it);
        } else {
            ++it;
        }
    }

    m_Renderer.Update();

    if (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) || Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}

void App::End() { // NOLINT(this method will mutate members in the future)
    LOG_TRACE("End");
}
