#ifndef ENEMY_HPP
#define ENEMY_HPP

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include <vector>
#include <utility>
#include <memory>

class Enemy : public Util::GameObject {
public:
    // 建構子多傳入 enemyId，並將 pathIndex 改名為 spawnIndex
    Enemy(std::shared_ptr<Util::Image> image, const std::vector<std::pair<float, float>>& path, int spawnIndex, int enemyId);

    void Update(const std::vector<std::pair<float, float>>& currentPath);
    bool HasReachedBase() const { return m_ReachedBase; }

    int GetSpawnIndex() const { return m_SpawnIndex; }
    int GetEnemyId() const { return m_EnemyId; } // 取得怪物種類
    void SetSpeed(float speed);

private:
    int m_SpawnIndex = 0;
    int m_EnemyId = 1;    // 記錄怪物種類 ID
    size_t m_CurrentTargetIndex = 0;
    float m_Speed = 1.0F;
    bool m_ReachedBase = false;
};

#endif