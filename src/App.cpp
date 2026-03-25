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
    int gridWidth = -1; // -1 代表寬度尚未決定
    std::string line;

    // 3. 逐行解析地圖 並驗證矩形格式
    while (std::getline(mapFile, line)) {
        std::stringstream ss(line);
        std::vector<std::string> yTiles; // 單一列代碼
        // 以空白切詞
        for (std::string tileCode; ss >> tileCode;) {
            yTiles.push_back(tileCode);
        }

        // 空行略過
        if (yTiles.empty()) {
            continue;
        }

        // 第一列決定寬度 後續列必須一致
        if (gridWidth == -1) {
            gridWidth = static_cast<int>(yTiles.size());
        } else if (static_cast<int>(yTiles.size()) != gridWidth) {
            LOG_ERROR("地圖每一列寬度不一致。");
            return;
        }

        // move 避免複製整列字串
        mapGrid.push_back(std::move(yTiles));
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
        const float worldX = mapOriginX + (static_cast<float>(x) * tileSize) + (tileSize / 2.0F); // 左邊界 + x格偏移 + 半格 取得該格中心X
        const float worldY = mapOriginY - (static_cast<float>(y) * tileSize) - (tileSize / 2.0F); // 上邊界 - y格偏移 - 半格 因螢幕Y向下為正
        return {worldX, worldY};
    };

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
            if (tileCode == "0000") {
                continue;
            }

            // 查貼圖 key 查不到就略過該格
            const auto textureIt = kTileTextureByCode.find(tileCode);
            if (textureIt == kTileTextureByCode.end()) {
                LOG_ERROR("字典中找不到此代碼: " + tileCode);
                continue;
            }

            // 取圖失敗只跳過該格
            const auto tileImage = m_AtlasLoader->Get(textureIt->second);
            if (!tileImage) {
                LOG_ERROR("找不到對應的圖片: " + textureIt->second);
                continue;
            }

            // 建立地塊物件並設定位置/縮放
            auto tileObject = std::make_shared<Util::GameObject>();
            tileObject->SetDrawable(tileImage);
            tileObject->m_Transform.scale = {kTileScale, kTileScale};
            const auto [worldX, worldY] = toWorld(x, y);
            tileObject->m_Transform.translation = {worldX, worldY};

            // Renderer 負責顯示, m_MapTiles 供後續平移/縮放
            m_Renderer.AddChild(tileObject);
            m_MapTiles.push_back(tileObject);
        }
    }

    // 6. BFS 尋路 start -> BASE___
    // 原理 佇列先進先出會一層一層擴張 無權重圖中第一次抵達終點就是最短步數
    auto findPathToBase = [&](GridPos start) {
        std::vector<std::vector<bool>> visited(gridHeight, std::vector<bool>(gridWidth, false)); // 避免重複走回頭路與無限循環
        std::vector<std::vector<GridPos>> parent(gridHeight, std::vector<GridPos>(gridWidth, {-1, -1})); // 記錄每格是從哪一格走來 供最後回溯路徑

        // 1 起點入隊 代表第0層 並先標記已訪問
        std::queue<GridPos> q;
        q.push(start);
        visited[start.second][start.first] = true;

        GridPos target = {-1, -1}; // -1 代表目前還沒找到可到達的 BASE___

        // 2 逐層擴展 每次 pop 都是目前已知最短步數層級的節點
        while (!q.empty()) {
            const GridPos current = q.front();
            q.pop();

            const int currX = current.first;
            const int currY = current.second;
            if (mapGrid[currY][currX] == "BASE___") {
                target = current;
                break;
            }

            // 3 往四個方向嘗試延伸 只接受 合法座標 + 未拜訪 + 可行走地形
            for (const auto& [offsetX, offsetY] : kNeighborOffsets) {
                const int nextX = currX + offsetX;
                const int nextY = currY + offsetY;

                const bool inBounds = nextX >= 0 && nextX < gridWidth && nextY >= 0 && nextY < gridHeight; // 邊界檢查
                if (!inBounds || visited[nextY][nextX]) {
                    continue;
                }

                const std::string& nextTileCode = mapGrid[nextY][nextX];
                if (kWalkableTileCodes.find(nextTileCode) == kWalkableTileCodes.end()) {
                    continue;
                }

                visited[nextY][nextX] = true;
                parent[nextY][nextX] = current; // next 的前一格是 current 回溯時靠這個還原完整路徑
                q.push({nextX, nextY}); // 入隊等待下一層處理 仍維持 BFS 層序特性
            }
        }

        std::vector<GridPos> path;
        if (target.first == -1) {
            return path; // 空路徑 代表此出生點無法連到主塔
        }

        // 4 目前方向是 target -> start 先回溯收集 再 reverse 成 start -> target
        for (GridPos current = target;; current = parent[current.second][current.first]) {
            path.push_back(current);
            if (current == start) {
                break;
            }
        }
        std::reverse(path.begin(), path.end()); // 轉成 start -> target
        return path;
    };

    // 7. 每個出生點建立一條世界座標路徑並儲存
    for (size_t pathIndex = 0; pathIndex < spawnPoints.size(); ++pathIndex) {
        const auto gridPath = findPathToBase(spawnPoints[pathIndex]);
        if (gridPath.empty()) {
            LOG_ERROR("尋路失敗：第 " + std::to_string(pathIndex + 1) + " 個敵洞無法抵達主塔！");
            continue;
        }

        // 預先轉世界座標, 避免每幀重算
        std::vector<WorldPos> worldPath;
        worldPath.reserve(gridPath.size());
        for (const auto& [x, y] : gridPath) {
            worldPath.push_back(toWorld(x, y));
        }

        // pathIndex 與 Enemy 路線索引一一對應
        m_AllPathsWorldPositions.push_back(std::move(worldPath));
        LOG_TRACE("尋路成功！已找到第 " + std::to_string(pathIndex + 1) + " 個敵洞的路徑。");
    }

    // 8. 初始化完成 進入 UPDATE
    m_CurrentState = State::UPDATE;
}

void App::Update() {
    // 1. 讀取輸入 轉成平移與縮放參數
    const float cameraPanPerFrame = 800.0F / static_cast<float>(kSpawnIntervalFrames); // 鏡頭每幀位移
    float dx = 0.0F;
    float dy = 0.0F;

    // WASD 輸入轉成位移向量
    if (Util::Input::IsKeyPressed(Util::Keycode::A)) dx += cameraPanPerFrame;
    if (Util::Input::IsKeyPressed(Util::Keycode::D)) dx -= cameraPanPerFrame;
    if (Util::Input::IsKeyPressed(Util::Keycode::W)) dy -= cameraPanPerFrame;
    if (Util::Input::IsKeyPressed(Util::Keycode::S)) dy += cameraPanPerFrame;

    // Q/E 縮放地圖 路徑 敵人
    float zoomRequestFactor = 1.0F;
    const float zoomStepPerFrame = 1.0F + (2.0F / static_cast<float>(kSpawnIntervalFrames)); // 每幀縮放倍率
    if (Util::Input::IsKeyPressed(Util::Keycode::Q)) zoomRequestFactor *= zoomStepPerFrame;
    if (Util::Input::IsKeyPressed(Util::Keycode::E)) zoomRequestFactor /= zoomStepPerFrame;

    const float targetZoom = std::clamp(m_MapZoom * zoomRequestFactor, 0.5F, 3.0F); // 縮放上下限
    const float appliedZoomFactor = targetZoom / m_MapZoom; // 本幀相對倍率
    if (appliedZoomFactor != 1.0F) {
        // 地圖位置與 scale 同步縮放
        for (auto& tile : m_MapTiles) {
            tile->m_Transform.translation.x *= appliedZoomFactor;
            tile->m_Transform.translation.y *= appliedZoomFactor;
            tile->m_Transform.scale.x *= appliedZoomFactor;
            tile->m_Transform.scale.y *= appliedZoomFactor;
        }

        // 路徑節點也要同步縮放
        for (auto& path : m_AllPathsWorldPositions) {
            for (auto& [x, y] : path) {
                x *= appliedZoomFactor;
                y *= appliedZoomFactor;
            }
        }

        // 場上敵人位置與 scale 同步縮放
        for (auto& enemy : m_Enemies) {
            enemy->m_Transform.translation.x *= appliedZoomFactor;
            enemy->m_Transform.translation.y *= appliedZoomFactor;
            enemy->m_Transform.scale.x *= appliedZoomFactor;
            enemy->m_Transform.scale.y *= appliedZoomFactor;
        }

        m_MapZoom = targetZoom;
    }

    // 2. 生怪冷卻與派怪 冷卻為 0 時 每條路徑生成一隻敵人
    if (!m_AllPathsWorldPositions.empty()) {
        if (m_SpawnCooldownFrames == 0) {
            auto enemyImage = m_AtlasLoader->Get("enemy-type-regular");
            for (size_t pathIndex = 0; pathIndex < m_AllPathsWorldPositions.size(); ++pathIndex) {
                auto enemy = std::make_shared<Enemy>(enemyImage, m_AllPathsWorldPositions[pathIndex], static_cast<int>(pathIndex));
                enemy->m_Transform.scale = {kTileScale * m_MapZoom, kTileScale * m_MapZoom};
                m_Enemies.push_back(enemy);
                m_Renderer.AddChild(enemy);
            }

            m_SpawnCooldownFrames = getkSpawnIntervalFrames(); // 這裡假設你本來的 kSpawnIntervalFrames 是 60
        } else {
            --m_SpawnCooldownFrames;
        }
    }

    // 3. 同步更新地圖 路徑 敵人座標 平移 helper 地圖 路徑 敵人一起移動 避免錯位
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

    // 有位移輸入才做整批平移
    if (dx != 0.0F || dy != 0.0F) {
        translateBy(dx, dy);
    }

    // 4. 逐隻更新敵人移動並回收到終點者
    for (auto& enemy : m_Enemies) {
        enemy->Update(m_AllPathsWorldPositions[enemy->GetPathIndex()]);
    }

    // 到達終點: 先從 Renderer 移除, 再從容器 erase
    for (auto it = m_Enemies.begin(); it != m_Enemies.end();) {
        if ((*it)->HasReachedBase()) {
            m_Renderer.RemoveChild(*it);
            it = m_Enemies.erase(it);
        } else {
            ++it;
        }
    }

    // 提交本幀渲染更新
    m_Renderer.Update();

    // ESC 或視窗關閉時結束
    if (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) || Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}

void App::End() { // NOLINT(this method will mutate members in the future)
    LOG_TRACE("End");
}
