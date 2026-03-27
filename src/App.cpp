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

    // 怪物 ID 轉名稱的小幫手
    std::string GetEnemySubName(int id) {
        switch(id) {
            case 1: return "regular";
            case 2: return "fast";
            case 3: return "icy";
            case 4: return "toxic";
            case 5: return "armored";
            case 6: return "fighter";
            case 7: return "healer";
            case 8: return "heli";
            case 9: return "jet";
            case 10: return "light";
            case 11: return "strong";
            default: return "regular";
        }
    }
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
    std::ifstream mapFile(RESOURCE_DIR "/maps/map_demo02.txt");
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

    // 4.5. 現在 toWorld 定義好了，開始建立閘門與傳送門物件
    for (const auto& gateLine : gateLines) {
        std::stringstream ss(gateLine);
        std::string typeStr;

        // ★ 修正：先只讀取第一個字串，確認物件種類 ★
        if (ss >> typeStr) {
            
            // 情況一：如果是閘門
            if (typeStr == "GATE_H" || typeStr == "GATE_V") {
                int gx, gy;
                if (ss >> gx >> gy) {
                    std::vector<int> targetIds;
                    int id;
                    while (ss >> id) {
                        targetIds.push_back(id);
                    }

                    GateType gType = (typeStr == "GATE_H") ? GateType::HORIZONTAL : GateType::VERTICAL;
                    std::string texName = (gType == GateType::HORIZONTAL) ? "gate-barrier-type-horizontal" : "gate-barrier-type-vertical";

                    if (auto gateImage = m_AtlasLoader->Get(texName)) {
                        auto gate = std::make_shared<Gate>(gateImage, gType, targetIds, gx, gy);

                        auto [world1X, world1Y] = toWorld(gx, gy);
                        int nextX = gx + (gType == GateType::VERTICAL ? 1 : 0);
                        int nextY = gy + (gType == GateType::HORIZONTAL ? 1 : 0);
                        auto [world2X, world2Y] = toWorld(nextX, nextY);

                        float gateWorldX = (world1X + world2X) / 2.0f;
                        float gateWorldY = (world1Y + world2Y) / 2.0f;

                        gate->m_Transform.translation = {gateWorldX, gateWorldY};
                        gate->m_Transform.scale = {kTileScale, kTileScale};

                        int iconCount = targetIds.size();
                        float totalBarLength = tileSize * 0.75f; 
                        float segmentLength = totalBarLength / iconCount; 
                        float barThickness = tileSize * 0.15f; 

                        float startOffset = -(totalBarLength / 2.0f) + (segmentLength / 2.0f);

                        for (int i = 0; i < iconCount; ++i) {
                            // ★ 瞬間簡化：透過 ID 取得名稱，自動拼湊出對應的純色圖檔名！
                            std::string texNameForBar = "color-" + GetEnemySubName(targetIds[i]);

                            if (auto colorBlockImage = m_AtlasLoader->Get(texNameForBar)) {
                                auto colorObj = std::make_shared<Util::GameObject>();
                                colorObj->SetDrawable(colorBlockImage);
                                colorObj->SetZIndex(6); 

                                float currentOffset = startOffset + (i * segmentLength);
                                float iconX = gateWorldX + (gType == GateType::HORIZONTAL ? currentOffset : 0.0f);
                                float iconY = gateWorldY + (gType == GateType::VERTICAL ? currentOffset : 0.0f);

                                colorObj->m_Transform.translation = {iconX, iconY};

                                float imgSize = m_AtlasLoader->Getsize(texNameForBar);

                                if (gType == GateType::HORIZONTAL) {
                                    colorObj->m_Transform.scale = { segmentLength / imgSize, barThickness / imgSize };
                                } else {
                                    colorObj->m_Transform.scale = { barThickness / imgSize, segmentLength / imgSize };
                                }

                                gate->m_ColorBars.push_back({colorObj, colorBlockImage});
                                m_Renderer.AddChild(colorObj);
                            }
                        }

                        m_Gates.push_back(gate);
                        m_Renderer.AddChild(gate);
                    }
                }
            }
            // 情況二：如果是傳送門
            else if (typeStr == "TELEPORT_H" || typeStr == "TELEPORT_V") {
                int tpId, gx, gy;
                if (ss >> tpId >> gx >> gy) {
                    GateType gType = (typeStr == "TELEPORT_H") ? GateType::HORIZONTAL : GateType::VERTICAL;
                    std::string texName = (gType == GateType::HORIZONTAL) ? "gate-teleport-horizontal" : "gate-teleport-vertical";

                    if (auto tpImage = m_AtlasLoader->Get(texName)) {
                        auto tp = std::make_shared<Teleporter>(tpImage, gType, tpId, gx, gy);

                        auto [world1X, world1Y] = toWorld(gx, gy);
                        int nextX = gx + (gType == GateType::VERTICAL ? 1 : 0);
                        int nextY = gy + (gType == GateType::HORIZONTAL ? 1 : 0);
                        auto [world2X, world2Y] = toWorld(nextX, nextY);

                        float tpWorldX = (world1X + world2X) / 2.0f;
                        float tpWorldY = (world1Y + world2Y) / 2.0f;

                        tp->m_Transform.translation = {tpWorldX, tpWorldY};
                        tp->m_Transform.scale = {kTileScale, kTileScale};

                        // 將原本這行：
                        // std::string colorName = (tpId == 1) ? "color-toxic" : "color-regular"; 
                        
                        // ★ 換成這行：讓 1~11 號傳送門自動套用我們定義好的 11 種顏色！
                        std::string colorName = "color-" + GetEnemySubName(tpId);
                        if (auto colorBlock = m_AtlasLoader->Get(colorName)) {
                            auto barObj = std::make_shared<Util::GameObject>();
                            barObj->SetDrawable(colorBlock);
                            barObj->SetZIndex(5);
                            barObj->m_Transform.translation = {tpWorldX, tpWorldY};

                            float imgSize = m_AtlasLoader->Getsize(colorName);
                            float barLength = tileSize * 0.8f;
                            float barThickness = tileSize * 0.1f;

                            if (gType == GateType::HORIZONTAL) {
                                barObj->m_Transform.scale = { barLength / imgSize, barThickness / imgSize };
                            } else {
                                barObj->m_Transform.scale = { barThickness / imgSize, barLength / imgSize };
                            }
                            tp->m_ColorBar = barObj;
                            m_Renderer.AddChild(barObj);
                        }
                        m_Teleporters.push_back(tp);
                        m_Renderer.AddChild(tp);
                    }
                }
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
            if (tileCode == "0000000") continue;

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

    // --- 建立傳送門的蟲洞連結 ---
    std::unordered_map<int, std::vector<std::shared_ptr<Teleporter>>> tpPairs;
    for (auto& tp : m_Teleporters) {
        tpPairs[tp->GetTeleportId()].push_back(tp);
    }

    // 輔助函式：判斷座標是否在界內且為「可走的道路」
    auto isWalkable = [&](GridPos p) {
        if (p.first < 0 || p.first >= gridWidth || p.second < 0 || p.second >= gridHeight) return false;
        return kWalkableTileCodes.count(mapGrid[p.second][p.first]) > 0;
    };

    for (const auto& [id, pairs] : tpPairs) {
        if (pairs.size() == 2) {
            auto tA = pairs[0];
            auto tB = pairs[1];

            GridPos a1 = {tA->GetGridX(), tA->GetGridY()};
            GridPos a2 = (tA->GetType() == GateType::VERTICAL) ? GridPos{a1.first + 1, a1.second} : GridPos{a1.first, a1.second + 1};

            GridPos b1 = {tB->GetGridX(), tB->GetGridY()};
            GridPos b2 = (tB->GetType() == GateType::VERTICAL) ? GridPos{b1.first + 1, b1.second} : GridPos{b1.first, b1.second + 1};

            // 找出 A 和 B 哪一側是真正可以安全降落的「路」
            // 終極智慧判斷：比較兩側的「路網發達程度」，誰連接的路多，誰就是真正的出口！
            auto getValidDest = [&](GridPos p1, GridPos p2) {
                int p1_n = 0, p2_n = 0;
                
                // 計算 p1 旁邊有幾條路 (扣除掉 p2)
                for (const auto& offset : kNeighborOffsets) {
                    GridPos n = {p1.first + offset.first, p1.second + offset.second};
                    if (n != p2 && isWalkable(n)) p1_n++;
                }
                // 計算 p2 旁邊有幾條路 (扣除掉 p1)
                for (const auto& offset : kNeighborOffsets) {
                    GridPos n = {p2.first + offset.first, p2.second + offset.second};
                    if (n != p1 && isWalkable(n)) p2_n++;
                }
                
                // 誰的路多，誰就是真正的安全降落點
                if (p1_n > p2_n) return p1;
                if (p2_n > p1_n) return p2;
                
                // 如果一樣多，就隨便挑一個可以走的
                return isWalkable(p1) ? p1 : p2;
            };

            GridPos destA = getValidDest(a1, a2);
            GridPos destB = getValidDest(b1, b2);

            // ★ 無敵雙向邏輯：不管怪物從哪一側跨過 A 門，都強制送到 B 門的安全降落點！
            m_TeleportLinks[{a1, a2}] = destB;
            m_TeleportLinks[{a2, a1}] = destB;
            
            // ★ 同理，跨過 B 門，就強制送到 A 門
            m_TeleportLinks[{b1, b2}] = destA;
            m_TeleportLinks[{b2, b1}] = destA;
        }
    }

    // 6. BFS 尋路 start -> BASE___
    // ★ 修正：多傳入一個 enemyId，代表現在是為哪種敵人找路
    auto findPathToBase = [&](const GridPos& start, int enemyId) {
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
                int nextX = currX + offsetX;
                int nextY = currY + offsetY;

                // 1. 確保這兩行宣告有確實寫上去
                GridPos from = {currX, currY};
                GridPos intendedTo = {nextX, nextY};

                // 2. 修正：使用 std::make_pair 明確包裝成 Key，解決編譯器的型別混淆
                auto teleKey = std::make_pair(from, intendedTo);

                // ★ 傳送門攔截！改用 teleKey 查詢 ★
                if (m_TeleportLinks.count(teleKey)) {
                    GridPos dest = m_TeleportLinks[teleKey];
                    nextX = dest.first;
                    nextY = dest.second;
                }

                const bool inBounds = nextX >= 0 && nextX < gridWidth && nextY >= 0 && nextY < gridHeight;
                if (!inBounds || visited[nextY][nextX]) continue;

                // ... (下面維持原本的 inBounds 檢查、地形檢查與閘門阻擋邏輯) ...

                const std::string& nextTileCode = mapGrid[nextY][nextX];
                if (kWalkableTileCodes.find(nextTileCode) == kWalkableTileCodes.end()) continue;

                // ★ 關鍵邏輯：檢查這道閘門是否「真的」會擋住這隻敵人
                bool blocked = false;
                for (const auto& gate : m_Gates) {
                    if (!gate->IsClosed()) continue; // 打開的閘門誰都能過

                    // 檢查座標是否重疊 (這部分保留原本的判斷邏輯)
                    bool isHit = false;
                    if (gate->GetType() == GateType::VERTICAL) {
                        if (gate->GetGridY() == currY && gate->GetGridY() == nextY) {
                            int minX = std::min(currX, nextX);
                            if (gate->GetGridX() == minX) isHit = true;
                        }
                    } else {
                        if (gate->GetGridX() == currX && gate->GetGridX() == nextX) {
                            int minY = std::min(currY, nextY);
                            if (gate->GetGridY() == minY) isHit = true;
                        }
                    }

                    // 如果撞到閘門，檢查這隻敵人的 ID 是否在阻擋名單內
                    if (isHit) {
                        const auto& targetIds = gate->GetTargetIds();
                        // std::find 會在 targetIds 裡面找 enemyId
                        if (std::find(targetIds.begin(), targetIds.end(), enemyId) != targetIds.end()) {
                            blocked = true; // 完蛋，被擋住了
                            break;
                        }
                    }
                }

                if (blocked) continue; // 此路不通，不加入 Queue

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

    // 7. 每個出生點為「每種敵人」建立專屬路線
    for (size_t i = 0; i < spawnPoints.size(); ++i) {
        std::unordered_map<int, std::vector<WorldPos>> pathsForThisSpawn;

        // 讓 11 種怪物都去探路！
        for (int enemyId = 1; enemyId <= 11; ++enemyId) { 
            auto gridPath = findPathToBase(spawnPoints[i], enemyId);
            // ...

            // 如果這隻怪物找得到路，才把它存起來
            if (!gridPath.empty()) {
                std::vector<WorldPos> worldPath;
                worldPath.reserve(gridPath.size());
                for (const auto& [x, y] : gridPath) {
                    worldPath.push_back(toWorld(x, y));
                }
                pathsForThisSpawn[enemyId] = std::move(worldPath);
            } else {
                // 如果找不到路 (被完全擋死)，印出提示
                LOG_TRACE("出生點 " + std::to_string(i) + " 的敵人 ID " + std::to_string(enemyId) + " 被閘門徹底擋死了！");
            }
        }
        m_AllPathsBySpawnAndType.push_back(std::move(pathsForThisSpawn));
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

            for (auto& bar : gate->m_ColorBars) {
                // 注意這裡多了一個 .object
                bar.object->m_Transform.translation.x *= appliedZoomFactor;
                bar.object->m_Transform.translation.y *= appliedZoomFactor;
                bar.object->m_Transform.scale.x *= appliedZoomFactor;
                bar.object->m_Transform.scale.y *= appliedZoomFactor;
            }
        }

        // ... (上面是 m_Gates 的縮放迴圈) ...

        // ★ 補上傳送門的縮放邏輯
        for (auto& tp : m_Teleporters) {
            tp->m_Transform.translation.x *= appliedZoomFactor;
            tp->m_Transform.translation.y *= appliedZoomFactor;
            tp->m_Transform.scale.x *= appliedZoomFactor;
            tp->m_Transform.scale.y *= appliedZoomFactor;

            if (tp->m_ColorBar) {
                tp->m_ColorBar->m_Transform.translation.x *= appliedZoomFactor;
                tp->m_ColorBar->m_Transform.translation.y *= appliedZoomFactor;
                tp->m_ColorBar->m_Transform.scale.x *= appliedZoomFactor;
                tp->m_ColorBar->m_Transform.scale.y *= appliedZoomFactor;
            }
        }

        // 縮放路徑
        for (auto& spawnMap : m_AllPathsBySpawnAndType) {
            for (auto& [id, path] : spawnMap) {
                for (auto& [x, y] : path) {
                    x *= appliedZoomFactor;
                    y *= appliedZoomFactor;
                }
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

    // 2. 生怪冷卻與派怪
    if (!m_AllPathsBySpawnAndType.empty()) {
        if (m_SpawnCooldownFrames == 0) {

            for (size_t spawnIndex = 0; spawnIndex < m_AllPathsBySpawnAndType.size(); ++spawnIndex) {
                const auto& validPaths = m_AllPathsBySpawnAndType[spawnIndex];

                // 如果這個出生點被四面八方的閘門完全封死，就跳過不出怪
                if (validPaths.empty()) continue;

                // 收集這個出生點「真正可以走」的怪物 ID 名單
                std::vector<int> availableIds;
                for (const auto& [id, path] : validPaths) {
                    availableIds.push_back(id);
                }

                // 從可以走的 ID 裡面隨機抽一個
                int randomIndex = std::rand() % availableIds.size();
                int chosenEnemyId = availableIds[randomIndex];

                // ★ 瞬間簡化：自動拼湊出怪物的貼圖名稱
                std::string enemyTexName = "enemy-type-" + GetEnemySubName(chosenEnemyId);

                if (auto enemyImage = m_AtlasLoader->Get(enemyTexName)) {
                    // 取出這隻怪物的專屬路徑傳入
                    const auto& specialPath = validPaths.at(chosenEnemyId);
                    auto enemy = std::make_shared<Enemy>(enemyImage, specialPath, static_cast<int>(spawnIndex), chosenEnemyId);
                    enemy->m_Transform.scale = {kTileScale * m_MapZoom, kTileScale * m_MapZoom};

                    m_Enemies.push_back(enemy);
                    m_Renderer.AddChild(enemy);
                }
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

            // 新增：讓彩色圖示也一起平移
            for (auto& bar : gate->m_ColorBars) {
                bar.object->m_Transform.translation.x += moveX;
                bar.object->m_Transform.translation.y += moveY;
            }
        }

        // ... (上面是 m_Gates 的平移迴圈) ...

        // ★ 補上傳送門的平移邏輯
        for (auto& tp : m_Teleporters) {
            tp->m_Transform.translation.x += moveX;
            tp->m_Transform.translation.y += moveY;

            if (tp->m_ColorBar) {
                tp->m_ColorBar->m_Transform.translation.x += moveX;
                tp->m_ColorBar->m_Transform.translation.y += moveY;
            }
        }

        // 同步平移路徑
        for (auto& spawnMap : m_AllPathsBySpawnAndType) {
            // 第二層：解開 Map
            for (auto& [id, path] : spawnMap) {
                // 第三層：修改真正的座標
                for (auto& [x, y] : path) {
                    x += moveX;
                    y += moveY;
                }
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

    // 4. 逐隻更新敵人移動並回收到終點者
    for (auto& enemy : m_Enemies) {
        // 從 Map 裡面精準拿出這隻敵人的專屬路線！
        const auto& itsOwnPath = m_AllPathsBySpawnAndType[enemy->GetSpawnIndex()][enemy->GetEnemyId()];
        enemy->Update(itsOwnPath);
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
