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
    m_AtlasImage = std::make_shared<Util::AtlasLoader>(
        RESOURCE_DIR "/combined.atlas", RESOURCE_DIR "/combined.png");

    if (!m_AtlasImage) {
        LOG_ERROR("m_AtlasImage is null!");
        return;
    }
// --- 新增：從 map.txt 讀取地圖 ---
    std::vector<std::vector<std::string>> mapDesign;
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
            mapDesign.push_back(row); // 將這一橫列加入地圖中
        }
    }
    // --------------------------------

    // 動態取得地圖的寬與高
    if (mapDesign.empty()) return; 
    int Map_Height = mapDesign.size();
    int Map_Width = mapDesign[0].size();

    // 你的字典保持不變
    std::unordered_map<std::string, std::string> textureDict = {
        {"NOMO",   "tile-type-platform"},       // 普通建塔平台 (深灰色)
        {"EMPT",   "tile-type-spawn-portal"},   // 敵洞 (紫色漩渦)
        {"HOME",   "tile-type-target-base"},    // 主塔 (藍色六角形)
        {"R_H0",  "tile-type-road-xoxo"},      // 水平道路
        {"R_V0",  "tile-type-road-oxox"},      // 垂直道路
        {"R_LD", "tile-type-road-xxoo"},      // 轉角 (左接下)
        {"R_RD", "tile-type-road-xoox"},      // 轉角 (右接下)
        {"R_TL", "tile-type-road-oxxo"},       // 轉角 (上接左)
        {"R_TR", "tile-type-road-ooxx"}        // 轉角 (上接右)
    };

    const float Scale=0.25;
    const float block_Size=m_AtlasImage->Getsize("tile-type-platform")*Scale;


    float offsetX = -(static_cast<float>(Map_Width) * block_Size) / 2.0f;
    float offsetY = (static_cast<float>(Map_Height) * block_Size) / 2.0f;
    for(int y=0;y<Map_Height;y++){
        for(int x=0;x<Map_Width;x++){
            std::string tileCode = mapDesign[y][x];
            if (tileCode == "0000") {
                continue;
            }

            // 如果字典裡沒有這個代碼，就跳過並報錯，避免閃退
            if (textureDict.find(tileCode) == textureDict.end()) {
                LOG_ERROR("字典中找不到此代碼: " + tileCode);
                continue;
            }

            std::string atlasKey = textureDict[tileCode];
            auto currentImage = m_AtlasImage->Get(atlasKey);

            if (currentImage != nullptr) {
                    
                auto block = std::make_shared<Util::GameObject>();

                block->SetDrawable(currentImage);
                block->m_Transform.scale = {Scale , Scale};

                float posX = offsetX + (x * block_Size) + (block_Size / 2.0f);
                float posY = offsetY - (y * block_Size) - (block_Size / 2.0f);

                block->m_Transform.translation = {posX, posY};
                m_Renderer.AddChild(block);
                m_Maplist.push_back(block);

                if (!m_AtlasImage) {
                    LOG_ERROR("找不到對應的圖片: " + atlasKey);
                }

            }
        }
        
        
    }
    // ---------------------------------------------------------
    // === 自動尋路系統 (Pathfinding) ===
    // ---------------------------------------------------------
    
    // 1. 建立一個 2D 陣列來記錄「這格是不是走過了」，避免敵人原地打轉
    std::vector<std::vector<bool>> visited(Map_Height, std::vector<bool>(Map_Width, false));
    
    // 定義哪些代碼是合法可以走的道路
    std::vector<std::string> walkable = {"R_H0", "R_V0", "R_LD","R_RD", "R_TL","R_TR", "HOME"};

    int startX = -1, startY = -1;

    // 2. 尋找起點 (EP)
    for (int y = 0; y < Map_Height; y++) {
        for (int x = 0; x < Map_Width; x++) {
            if (mapDesign[y][x] == "EMPT") {
                startX = x;
                startY = y;
                break;
            }
        }
    }

    if (startX != -1 && startY != -1) {
        int currX = startX;
        int currY = startY;
        
        // 將陣列索引 (gridX, gridY) 轉換為畫面座標 (posX, posY) 的 Lambda 匿名函數
        // 這邊直接套用你剛剛寫的完美置中數學！
        auto GetWorldPos = [&](int gridX, int gridY) {
            float posX = offsetX + (gridX * block_Size) + (block_Size / 2.0f);
            float posY = offsetY - (gridY * block_Size) - (block_Size / 2.0f);
            return std::make_pair(posX, posY);
        };

        // 把起點加入路徑
        m_PathCoords.push_back(GetWorldPos(currX, currY));
        visited[currY][currX] = true;

        // 定義搜尋方向：右, 下, 左, 上
        int dx[] = {1, 0, -1, 0};
        int dy[] = {0, 1, 0, -1};

        bool reachedBase = false;

        // 3. 開始追蹤路徑
        while (!reachedBase) {
            bool moved = false;
            
            // 檢查上下左右四個相鄰的格子
            for (int i = 0; i < 4; i++) {
                int nextX = currX + dx[i];
                int nextY = currY + dy[i];
                
                // 檢查是否超出地圖邊界
                if (nextX >= 0 && nextX < Map_Width && nextY >= 0 && nextY < Map_Height) {
                    
                    // 取得相鄰格子的代碼
                    std::string nextTile = mapDesign[nextY][nextX];
                    
                    // 如果這格還沒走過，而且它是合法道路或是主塔
                    bool isWalkable = std::find(walkable.begin(), walkable.end(), nextTile) != walkable.end();
                    
                    if (!visited[nextY][nextX] && isWalkable) {
                        // 決定要走到這格
                        currX = nextX;
                        currY = nextY;
                        visited[currY][currX] = true; // 標記為已走過
                        
                        // 轉換為實際座標並存入路徑清單
                        m_PathCoords.push_back(GetWorldPos(currX, currY));
                        moved = true;
                        
                        // 如果這格是主塔 (HW)，尋路結束！
                        if (nextTile == "HOME") {
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

    // 1. 自動生怪邏輯
    if (!m_PathCoords.empty()) {
        if (m_SpawnCooldown > 0) {
            m_SpawnCooldown--;
        } else {
            auto enemyImage = m_AtlasImage->Get("enemy-type-regular");
            auto enemy = std::make_shared<Enemy>(enemyImage, m_PathCoords);
            m_Enemies.push_back(enemy);
            m_Renderer.AddChild(enemy);
            m_SpawnCooldown = SPAWN_INTERVAL;
        }
    }

    // 2. 處理玩家位移 (攝影機同步)
    if (dx != 0.0F || dy != 0.0F) {
        // 移動地圖
        for (auto& tile : m_Maplist) {
            tile->m_Transform.translation.x += dx;
            tile->m_Transform.translation.y += dy;
        }
        // 同步所有路徑節點
        for (auto& coord : m_PathCoords) {
            coord.first += dx;
            coord.second += dy;
        }
        // 同步所有已經在場上的敵人
        for (auto& enemy : m_Enemies) {
            enemy->m_Transform.translation.x += dx;
            enemy->m_Transform.translation.y += dy;
        }
    }

    // 3. 更新所有敵人的 AI 狀態 (整個 Update 函數只能呼叫這裡一次！)
    for (auto& enemy : m_Enemies) {
        enemy->Update(m_PathCoords);
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
