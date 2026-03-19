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
        {"A0",   "tile-type-platform"},       // 普通建塔平台 (深灰色)
        {"EP",   "tile-type-spawn-portal"},   // 敵洞 (紫色漩渦)
        {"HW",   "tile-type-target-base"},    // 主塔 (藍色六角形)
        {"R_H",  "tile-type-road-xoxo"},      // 水平道路
        {"R_V",  "tile-type-road-oxox"},      // 垂直道路
        {"R_LD", "tile-type-road-xxoo"},      // 轉角 (左接下)
        {"R_TL", "tile-type-road-oxxo"}       // 轉角 (上接左)
    };

    const float Scale=0.25;
    const float block_Size=m_AtlasImage->Getsize("tile-type-platform")*Scale;


    float offsetX = -(static_cast<float>(Map_Width) * block_Size) / 2.0f;
    float offsetY = (static_cast<float>(Map_Height) * block_Size) / 2.0f;
    for(int y=0;y<Map_Height;y++){
        for(int x=0;x<Map_Width;x++){
            std::string tileCode = mapDesign[y][x];
            if (tileCode == "0") {
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

    m_CurrentState = State::UPDATE;
}

void App::Update() {
    constexpr float speed = 10.0F;
    float dx=0.0F;
    float dy=0.0F;

    // WASD 控制圖片移動。
    if (Util::Input::IsKeyDown(Util::Keycode::A)) {
       dx -= speed;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::D)) {
        dx+= speed;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::W)) {
        dy += speed;
    }
    if (Util::Input::IsKeyDown(Util::Keycode::S)) {
        dy -= speed;
    }

    if (dx != 0.0F || dy != 0.0F) {
        for (auto& tile : m_Maplist) {
            tile->m_Transform.translation.x += dx;
            tile->m_Transform.translation.y += dy;
        }
    }

    m_Renderer.Update();

    // 保留最基本的離開方式，避免視窗無法正常關閉。
    if (Util::Input::IsKeyUp(Util::Keycode::ESCAPE) ||
        Util::Input::IfExit()) {
        m_CurrentState = State::END;
    }
}

void App::End() { // NOLINT(this method will mutate members in the future)
    LOG_TRACE("End");
}
