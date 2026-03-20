#ifndef ENEMY_HPP
#define ENEMY_HPP

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include <vector>
#include <utility>
#include <memory>

class Enemy : public Util::GameObject {
public:
    // 建構子多傳入一個 pathIndex (路徑索引)
    Enemy(std::shared_ptr<Util::Image> image, const std::vector<std::pair<float, float>>& path, int pathIndex);

    void Update(const std::vector<std::pair<float, float>>& currentPath);
    bool HasReachedBase() const { return m_ReachedBase; }

    // 讓 App 知道這隻怪目前在走哪一條路
    int GetPathIndex() const { return m_PathIndex; }

private:
    int m_PathIndex = 0; // 新增這行：記錄這是第幾條路線的怪
    size_t m_CurrentTargetIndex = 0;
    float m_Speed = 3.0F;
    bool m_ReachedBase = false;
};

#endif