#include "Util/Enemy.hpp"
#include <cmath>

Enemy::Enemy(std::shared_ptr<Util::Image> image, const std::vector<std::pair<float, float>>& path, int spawnIndex, int enemyId)
    : m_SpawnIndex(spawnIndex), m_EnemyId(enemyId) {

    SetDrawable(image);
    SetZIndex(10);

    // 根據 ID 給予不同的特性
    if (m_EnemyId == 2 || m_EnemyId == 9 || m_EnemyId == 10) {
        m_Speed = 1.8F; // 快速怪、噴射機 (jet)、輕型怪 (light) 速度快
    } else if (m_EnemyId == 3 || m_EnemyId == 5 || m_EnemyId == 11) {
        m_Speed = 0.8F; // 寒冰怪、裝甲怪 (armored)、強壯怪 (strong) 速度慢
    } else {
        m_Speed = 1.2F; // 其他 (包含直升機、普通怪等) 標準速度
    }

    if (!path.empty()) {
        m_Transform.translation = {path[0].first, path[0].second};
        m_Transform.scale = {0.25F, 0.25F};
    }
}

// ... Update 函式完全不用改 ...

void Enemy::Update(const std::vector<std::pair<float, float>>& currentPath) {
    // 如果已經抵達主塔，或路徑有問題，就不移動
    if (m_ReachedBase || currentPath.empty() || m_CurrentTargetIndex >= currentPath.size()) {
        return;
    }

    // 取得當前目標節點的座標
    float targetX = currentPath[m_CurrentTargetIndex].first;
    float targetY = currentPath[m_CurrentTargetIndex].second;

    // 計算 X 軸與 Y 軸的距離差異
    float dx = targetX - m_Transform.translation.x;
    float dy = targetY - m_Transform.translation.y;

    // 畢氏定理算直線距離
    float distance = std::sqrt(dx * dx + dy * dy);

    // ★ 傳送瞬移邏輯 ★
    // 如果距離異常遙遠 (大於兩倍的格子寬度，大約 60.0f)，代表牠剛踩進傳送門
    if (distance > 60.0f) { 
        m_Transform.translation = {targetX, targetY}; // 瞬間把牠移過去！
        m_CurrentTargetIndex++;
        if (m_CurrentTargetIndex >= currentPath.size()) m_ReachedBase = true;
        return;
    }

    // 如果距離已經小於一步的速度，代表抵達這個節點了
    if (distance <= m_Speed) {
        m_Transform.translation = {targetX, targetY}; // 對齊該節點
        m_CurrentTargetIndex++; // 切換到下一個節點

        if (m_CurrentTargetIndex >= currentPath.size()) {
            m_ReachedBase = true; // 走完全程
        }
    } else {
        // 向量正規化 (單位向量) 乘上速度，讓敵人以等速朝目標前進
        m_Transform.translation.x += (dx / distance) * m_Speed;
        m_Transform.translation.y += (dy / distance) * m_Speed;
    }
}