#ifndef APP_HPP
#define APP_HPP

#include "pch.hpp" // IWYU pragma: export

#include "Util/AtlasLoader.hpp"
#include "Util/GameObject.hpp"
#include "Util/Renderer.hpp"
#include "Util/Enemy.hpp"
#include "Util/Gate.hpp"

class App {
public:
    enum class State {
        START,
        UPDATE,
        END,
    };

    State GetCurrentState() const { return m_CurrentState; }

    int getkSpawnIntervalFrames();

    int setkSpawnIntervalFrames(int diffIntervalFrames);

    bool HasClosedGate(int fromX, int fromY, int toX, int toY) const;

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
    // 把原本的 m_PathWorldPositions 改成二維陣列
    std::vector<std::vector<std::pair<float, float>>> m_AllPathsWorldPositions; // 把原本的 m_PathWorldPositions 改成二維陣列
    std::vector<std::shared_ptr<Enemy>> m_Enemies; // 管理場上的敵人

    // --- 新增生怪控制變數 ---
    int m_SpawnCooldownFrames = 0;              // 目前的冷卻倒數
    static constexpr int kSpawnIntervalFrames = 120; // 假設遊戲是 60 FPS，60 大約就是 1 秒生一隻
    float m_MapZoom = 1.0F; // 目前地圖縮放倍率（1.0 = 原始大小）

    std::vector<std::shared_ptr<Gate>> m_Gates;
};

#endif
