#include "App.hpp"
#include <fstream>   // 加入這個用來讀取檔案
#include <sstream>   // 加入這個用來切割字串
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
    std::unordered_map<std::string, std::string> tileTextureKeys = {
        {"A0",   "tile-type-platform"},       // 普通建塔平台 (深灰色)
        {"EP",   "tile-type-spawn-portal"},   // 敵洞 (紫色漩渦)
        {"HW",   "tile-type-target-base"},    // 主塔 (藍色六角形)
        {"R_H",  "tile-type-road-xoxo"},      // 水平道路
        {"R_V",  "tile-type-road-oxox"},      // 垂直道路
        {"R_LD", "tile-type-road-xxoo"},      // 轉角 (左接下)
        {"R_TL", "tile-type-road-oxxo"}       // 轉角 (上接左)
    };

    const float tileScale = 0.25F;
    const float tileSize =
        m_AtlasLoader->Getsize("tile-type-platform") * tileScale;


    float mapOriginX = -(static_cast<float>(columnCount) * tileSize) / 2.0F;
    float mapOriginY = (static_cast<float>(rowCount) * tileSize) / 2.0F;
    for (int row = 0; row < rowCount; row++) {
        for (int column = 0; column < columnCount; column++) {
            std::string tileCode = mapGrid[row][column];
            if (tileCode == "0") {
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
    // === 自動尋路系統 (Pathfinding) ===
    // ---------------------------------------------------------
    
    // 1. 建立一個 2D 陣列來記錄「這格是不是走過了」，避免敵人原地打轉
    std::vector<std::vector<bool>> visited(
        rowCount, std::vector<bool>(columnCount, false));
    
    // 定義哪些代碼是合法可以走的道路
    std::vector<std::string> walkableTileCodes = {"R_H", "R_V", "R_LD", "R_TL", "HW"};

    int spawnColumn = -1;
    int spawnRow = -1;

    // 2. 尋找起點 (EP)
    for (int row = 0; row < rowCount; row++) {
        for (int column = 0; column < columnCount; column++) {
            if (mapGrid[row][column] == "EP") {
                spawnColumn = column;
                spawnRow = row;
                break;
            }
        }
    }

    if (spawnColumn != -1 && spawnRow != -1) {
        int currentColumn = spawnColumn;
        int currentRow = spawnRow;
        
        // 將陣列索引 (gridX, gridY) 轉換為畫面座標 (posX, posY) 的 Lambda 匿名函數
        // 這邊直接套用你剛剛寫的完美置中數學！
        auto getWorldPosition = [&](int gridColumn, int gridRow) {
            float worldX =
                mapOriginX + (gridColumn * tileSize) + (tileSize / 2.0F);
            float worldY =
                mapOriginY - (gridRow * tileSize) - (tileSize / 2.0F);
            return std::make_pair(worldX, worldY);
        };

        // 把起點加入路徑
        m_PathWorldPositions.push_back(
            getWorldPosition(currentColumn, currentRow));
        visited[currentRow][currentColumn] = true;

        // 定義搜尋方向：右, 下, 左, 上
        int columnOffsets[] = {1, 0, -1, 0};
        int rowOffsets[] = {0, 1, 0, -1};

        bool reachedBase = false;

        // 3. 開始追蹤路徑
        while (!reachedBase) {
            bool moved = false;
            
            // 檢查上下左右四個相鄰的格子
            for (int i = 0; i < 4; i++) {
                int nextColumn = currentColumn + columnOffsets[i];
                int nextRow = currentRow + rowOffsets[i];
                
                // 檢查是否超出地圖邊界
                if (nextColumn >= 0 && nextColumn < columnCount &&
                    nextRow >= 0 && nextRow < rowCount) {
                    
                    // 取得相鄰格子的代碼
                    std::string nextTileCode = mapGrid[nextRow][nextColumn];
                    
                    // 如果這格還沒走過，而且它是合法道路或是主塔
                    bool isWalkable =
                        std::find(walkableTileCodes.begin(),
                                  walkableTileCodes.end(),
                                  nextTileCode) != walkableTileCodes.end();
                    
                    if (!visited[nextRow][nextColumn] && isWalkable) {
                        // 決定要走到這格
                        currentColumn = nextColumn;
                        currentRow = nextRow;
                        visited[currentRow][currentColumn] = true; // 標記為已走過
                        
                        // 轉換為實際座標並存入路徑清單
                        m_PathWorldPositions.push_back(
                            getWorldPosition(currentColumn, currentRow));
                        moved = true;
                        
                        // 如果這格是主塔 (HW)，尋路結束！
                        if (nextTileCode == "HW") {
                            reachedBase = true;
                            LOG_TRACE("尋路成功！已找到抵達主塔的路徑。");
                        }
                        
                        break; // 已經找到下一步了，跳出 for 迴圈，繼續 while 往前走
                    }
                }
            }
            
            // 如果四個方向都沒路走，代表遇到死胡同
            if (!moved) {
                LOG_ERROR("尋路失敗：遇到死胡同，請檢查 map.txt 的道路是否斷線！");
                break;
            }
        }
    } else {
        LOG_ERROR("尋路失敗：地圖上找不到起點 (EP)！");
    }

    m_CurrentState = State::UPDATE;
}

void App::Update() {
    constexpr float speed = 10.0F;
    float dx = 0.0F;
    float dy = 0.0F;

    if (Util::Input::IsKeyDown(Util::Keycode::A)) dx -= speed;
    if (Util::Input::IsKeyDown(Util::Keycode::D)) dx += speed;
    if (Util::Input::IsKeyDown(Util::Keycode::W)) dy += speed;
    if (Util::Input::IsKeyDown(Util::Keycode::S)) dy -= speed;

    // ---------------------------------------------------------
    // 1. 自動生怪邏輯 (Spawner)
    // ---------------------------------------------------------
    if (!m_PathWorldPositions.empty()) {
        if (m_SpawnCooldownFrames > 0) {
            m_SpawnCooldownFrames--; // 冷卻中，倒數減一
        } else {
            // 冷卻結束，生出一隻新敵人！
            auto enemyImage = m_AtlasLoader->Get("enemy-type-regular");
            auto enemy = std::make_shared<Enemy>(enemyImage, m_PathWorldPositions);
            
            m_Enemies.push_back(enemy);
            m_Renderer.AddChild(enemy);
            
            m_SpawnCooldownFrames = kSpawnIntervalFrames; // 重置冷卻時間
        }
    }

    // ---------------------------------------------------------
    // 2. 更新所有敵人的狀態
    // ---------------------------------------------------------
    for (auto& enemy : m_Enemies) {
        enemy->Update(m_PathWorldPositions);
    }

    // ---------------------------------------------------------
    // 3. 記憶體回收 (移除已經抵達終點的敵人)
    // ---------------------------------------------------------
    for (auto it = m_Enemies.begin(); it != m_Enemies.end(); ) {
        if ((*it)->HasReachedBase()) {
            // 先從渲染樹上拔除，這樣畫面才不會留下殘影
            m_Renderer.RemoveChild(*it); 
            // 再從管理清單中刪除，釋放記憶體
            it = m_Enemies.erase(it);    
        } else {
            ++it;
        }
    }

    if (dx != 0.0F || dy != 0.0F) {
        // 1. 移動地圖
        for (auto& tile : m_MapTiles) {
            tile->m_Transform.translation.x += dx;
            tile->m_Transform.translation.y += dy;
        }
        
        // 2. 移動敵人，確保他們跟著地圖一起平移
        for (auto& enemy : m_Enemies) {
            enemy->m_Transform.translation.x += dx;
            enemy->m_Transform.translation.y += dy;
        }
        
        // 3. 【關鍵】同步更新所有的「路徑節點座標」
        for (auto& coord : m_PathWorldPositions) {
            coord.first += dx;
            coord.second += dy;
        }
    }

    // 更新每個敵人的 AI 邏輯 (朝著當前最新的路徑節點前進)
    for (auto& enemy : m_Enemies) {
        enemy->Update(m_PathWorldPositions);
    }

    m_Renderer.Update();

    if (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) || Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}

void App::End() { // NOLINT(this method will mutate members in the future)
    LOG_TRACE("End");
}
