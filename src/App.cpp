#include "App.hpp"
#include <fstream>   // 加入這個用來讀取檔案
#include <sstream>   // 加入這個用來切割字串
#include <queue>      // BFS 需要用到佇列
#include <algorithm>  // 需要用到 std::reverse
#include <vector>
#include <string>
#include "Util/AtlasLoader.hpp"
#include "Util/Image.hpp"
#include "Util/Input.hpp"
#include "Util/Keycode.hpp"
#include "Util/Logger.hpp"

void App::Start() {
    LOG_TRACE("Start");

    // 建立圖片讀取物件。
    m_AtlasLoader = std::make_shared<Util::AtlasLoader>(
        RESOURCE_DIR "/combined.atlas", RESOURCE_DIR "/combined.png");

    if (!m_AtlasLoader) {
        LOG_ERROR("m_AtlasLoader is null!");
        return;
    }
// --- 新增：從 map.txt 讀取地圖 ---
    std::vector<std::vector<std::string>> mapGrid;
    std::ifstream mapFile(RESOURCE_DIR "/map.txt"); // 讀取你的地圖檔
    
    if (!mapFile.is_open()) {
        LOG_ERROR("無法開啟 map.txt！請檢查檔案路徑。");
        return;
    }

    std::string line;
    while (std::getline(mapFile, line)) {
        std::vector<std::string> row;
        std::stringstream ss(line);
        std::string tileCode;
        
        // 依照空格切割每一行的代碼
        while (ss >> tileCode) {
            row.push_back(tileCode);
        }
        
        if (!row.empty()) {
            mapGrid.push_back(row); // 將這一橫列加入地圖中
        }
    }
    // --------------------------------

    // 動態取得地圖的寬與高
    if (mapGrid.empty()) return;
    const int rowCount = static_cast<int>(mapGrid.size());
    const int columnCount = static_cast<int>(mapGrid[0].size());

    // 你的字典保持不變
    //tile-type-road-上右下左
    std::unordered_map<std::string, std::string> tileTextureKeys = {
        {"NOMO",   "tile-type-platform"},       // 普通建塔平台 (深灰色)
        {"EMPT",   "tile-type-spawn-portal"},   // 敵洞 (紫色漩渦)
        {"HOME",   "tile-type-target-base"},    // 主塔 (藍色六角形)
        {"R_HV",  "tile-type-road-oooo"},       // 四方來財
        {"R_H0",  "tile-type-road-xoxo"},       // 水平道路
        {"R_HT",  "tile-type-road-ooxo"},       // 水平上岔路
        {"R_HD",  "tile-type-road-xooo"},       // 水平下岔路
        {"R_V0",  "tile-type-road-oxox"},      // 垂直道
        {"R_VR",  "tile-type-road-ooox"},      // 垂直右岔路
        {"R_VL",  "tile-type-road-oxox"},      // 垂直左岔路
        {"R_LD", "tile-type-road-xxoo"},      // 轉角 (左接下)
        {"R_RD", "tile-type-road-xoox"},      // 轉角 (右接下)
        {"R_TL", "tile-type-road-oxxo"},       // 轉角 (上接左)
        {"R_TR", "tile-type-road-ooxx"},        // 轉角 (上接右)
        {"R_00", "tile-type-road-xxxx"}        // 孤兒
    };

    const float tileScale = 0.25F;
    const float tileSize =
        m_AtlasLoader->Getsize("tile-type-platform") * tileScale;


    float mapOriginX = -(static_cast<float>(columnCount) * tileSize) / 2.0F;
    float mapOriginY = (static_cast<float>(rowCount) * tileSize) / 2.0F;
    for (int row = 0; row < rowCount; row++) {
        for (int column = 0; column < columnCount; column++) {
            std::string tileCode = mapGrid[row][column];
            if (tileCode == "0000") {
                continue;
            }

            // 如果字典裡沒有這個代碼，就跳過並報錯，避免閃退
            if (tileTextureKeys.find(tileCode) == tileTextureKeys.end()) {
                LOG_ERROR("字典中找不到此代碼: " + tileCode);
                continue;
            }

            std::string atlasKey = tileTextureKeys[tileCode];
            auto tileImage = m_AtlasLoader->Get(atlasKey);

            if (tileImage != nullptr) {
                    
                auto tileObject = std::make_shared<Util::GameObject>();

                tileObject->SetDrawable(tileImage);
                tileObject->m_Transform.scale = {tileScale, tileScale};

                float tileCenterX =
                    mapOriginX + (column * tileSize) + (tileSize / 2.0F);
                float tileCenterY =
                    mapOriginY - (row * tileSize) - (tileSize / 2.0F);

                tileObject->m_Transform.translation = {tileCenterX, tileCenterY};
                m_Renderer.AddChild(tileObject);
                m_MapTiles.push_back(tileObject);

                if (!m_AtlasLoader) {
                    LOG_ERROR("找不到對應的圖片: " + atlasKey);
                }

            }
        }
        
        
    }
    // ---------------------------------------------------------
    // === 自動尋路系統 (多起點 BFS) ===
    // ---------------------------------------------------------

    std::vector<std::string> walkableTileCodes =
        {"R_H0","R_HV","R_00", "R_HT", "R_HD", "R_V0", "R_VR", "R_VL", "R_LD","R_RD", "R_TL","R_TR", "HOME"};

    // 1. 找出所有的起點 (EMPT)
    std::vector<std::pair<int, int>> spawnPoints;
    for (int row = 0; row < rowCount; row++) {
        for (int column = 0; column < columnCount; column++) {
            if (mapGrid[row][column] == "EMPT") {
                spawnPoints.push_back({column, row});
            }
        }
    }

    auto getWorldPosition = [&](int gridColumn, int gridRow) {
        float worldX = mapOriginX + (gridColumn * tileSize) + (tileSize / 2.0F);
        float worldY = mapOriginY - (gridRow * tileSize) - (tileSize / 2.0F);
        return std::make_pair(worldX, worldY);
    };

    // 2. 針對「每一個」起點都跑一次 BFS
    for (size_t pIndex = 0; pIndex < spawnPoints.size(); pIndex++) {
        int startCol = spawnPoints[pIndex].first;
        int startRow = spawnPoints[pIndex].second;

        // 每次尋路都要重新宣告乾淨的 visited 和 parent
        std::vector<std::vector<bool>> visited(rowCount, std::vector<bool>(columnCount, false));
        std::vector<std::vector<std::pair<int, int>>> parent(rowCount, std::vector<std::pair<int, int>>(columnCount, {-1, -1}));

        std::queue<std::pair<int, int>> q;
        q.push({startCol, startRow});
        visited[startRow][startCol] = true;

        int columnOffsets[] = {1, 0, -1, 0};
        int rowOffsets[] = {0, 1, 0, -1};
        int targetCol = -1, targetRow = -1;

        while (!q.empty()) {
            auto current = q.front();
            q.pop();

            int currCol = current.first;
            int currRow = current.second;

            if (mapGrid[currRow][currCol] == "HOME") {
                targetCol = currCol;
                targetRow = currRow;
                break;
            }

            for (int i = 0; i < 4; i++) {
                int nextCol = currCol + columnOffsets[i];
                int nextRow = currRow + rowOffsets[i];

                if (nextCol >= 0 && nextCol < columnCount && nextRow >= 0 && nextRow < rowCount) {
                    std::string nextTileCode = mapGrid[nextRow][nextCol];
                    bool isWalkable = std::find(walkableTileCodes.begin(), walkableTileCodes.end(), nextTileCode) != walkableTileCodes.end();

                    if (!visited[nextRow][nextCol] && isWalkable) {
                        visited[nextRow][nextCol] = true;
                        parent[nextRow][nextCol] = {currCol, currRow};
                        q.push({nextCol, nextRow});
                    }
                }
            }
        }

        // 3. 回溯並儲存這條獨立的路徑
        if (targetCol != -1 && targetRow != -1) {
            std::vector<std::pair<int, int>> pathReversed;
            int currC = targetCol;
            int currR = targetRow;

            while (currC != startCol || currR != startRow) {
                pathReversed.push_back({currC, currR});
                auto p = parent[currR][currC];
                currC = p.first;
                currR = p.second;
            }
            pathReversed.push_back({startCol, startRow});
            std::reverse(pathReversed.begin(), pathReversed.end());

            std::vector<std::pair<float, float>> singlePathWorldPositions;
            for (auto& p : pathReversed) {
                singlePathWorldPositions.push_back(getWorldPosition(p.first, p.second));
            }

            // 把這條路線加入「所有路線」的清單中
            m_AllPathsWorldPositions.push_back(singlePathWorldPositions);
            LOG_TRACE("尋路成功！已找到第 " + std::to_string(pIndex + 1) + " 個敵洞的路徑。");
        } else {
            LOG_ERROR("尋路失敗：第 " + std::to_string(pIndex + 1) + " 個敵洞無法抵達主塔！");
        }
    }

    m_CurrentState = State::UPDATE;
}

void App::Update() {
    constexpr float speed = 10.0F;
    float dx = 0.0F;
    float dy = 0.0F;

    if (Util::Input::IsKeyPressed(Util::Keycode::A)) dx -= speed;
    if (Util::Input::IsKeyPressed(Util::Keycode::D)) dx += speed;
    if (Util::Input::IsKeyPressed(Util::Keycode::W)) dy += speed;
    if (Util::Input::IsKeyPressed(Util::Keycode::S)) dy -= speed;

    // 1. 自動生怪邏輯 (每個洞口都生一隻)
    if (!m_AllPathsWorldPositions.empty()) {
        if (m_SpawnCooldownFrames > 0) {
            m_SpawnCooldownFrames--;
        } else {
            auto enemyImage = m_AtlasLoader->Get("enemy-type-regular");

            // 走訪所有算出來的路線，每條路都派出一隻怪
            for (size_t i = 0; i < m_AllPathsWorldPositions.size(); i++) {
                auto enemy = std::make_shared<Enemy>(enemyImage, m_AllPathsWorldPositions[i], i);
                m_Enemies.push_back(enemy);
                m_Renderer.AddChild(enemy);
            }

            m_SpawnCooldownFrames = 200; // 這裡假設你本來的 kSpawnIntervalFrames 是 60
        }
    }

    if (dx != 0.0F || dy != 0.0F) {
        for (auto& tile : m_MapTiles) {
            tile->m_Transform.translation.x += dx;
            tile->m_Transform.translation.y += dy;
        }

        // 同步【所有的】路徑節點
        for (auto& path : m_AllPathsWorldPositions) {
            for (auto& coord : path) {
                coord.first += dx;
                coord.second += dy;
            }
        }

        for (auto& enemy : m_Enemies) {
            enemy->m_Transform.translation.x += dx;
            enemy->m_Transform.translation.y += dy;
        }
    }

    // 3. 更新所有敵人的 AI 狀態
    for (auto& enemy : m_Enemies) {
        // 依照這隻敵人身上的 index，餵給他正確的那條路徑
        enemy->Update(m_AllPathsWorldPositions[enemy->GetPathIndex()]);
    }

    // 4. 記憶體回收
    for (auto it = m_Enemies.begin(); it != m_Enemies.end(); ) {
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
