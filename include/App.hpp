#ifndef APP_HPP
#define APP_HPP

#include "pch.hpp" // IWYU pragma: export

#include "Util/AtlasLoader.hpp"
#include "Util/GameObject.hpp"
#include "Util/Renderer.hpp"
#include "Util/Enemy.hpp"

class App {
public:
    enum class State {
        START,
        UPDATE,
        END,
    };

    State GetCurrentState() const { return m_CurrentState; }

    void Start();

    void Update();

    void End(); // NOLINT(readability-convert-member-functions-to-static)

private:
    State m_CurrentState = State::START;
    std::shared_ptr<Util::AtlasLoader> m_AtlasLoader;
    std::shared_ptr<Util::GameObject> m_DebugImageObject =
        std::make_shared<Util::GameObject>();
    std::vector<std::shared_ptr<Util::GameObject>> m_MapTiles;
    Util::Renderer m_Renderer;
    std::vector<std::pair<float, float>> m_PathWorldPositions; // 用來儲存從起點到終點的所有畫面座標 (posX, posY)
    std::vector<std::shared_ptr<Enemy>> m_Enemies; // 管理場上的敵人

    // --- 新增生怪控制變數 ---
    int m_SpawnCooldownFrames = 0;              // 目前的冷卻倒數
    static constexpr int kSpawnIntervalFrames = 60; // 假設遊戲是 60 FPS，60 大約就是 1 秒生一隻
};

#endif
