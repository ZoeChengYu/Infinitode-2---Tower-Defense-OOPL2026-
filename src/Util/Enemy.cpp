#include "Util/Enemy.hpp"
#include <cmath> // 為了使用 std::sqrt

Enemy::Enemy(std::shared_ptr<Util::Image> image, const std::vector<std::pair<float, float>>& path) {
    SetDrawable(image);
    // 👇 新增這行！將敵人的渲染層級拉高，確保它永遠蓋在地圖上方
    SetZIndex(10);
    if (!path.empty()) {
        // 將敵人初始位置設定在路徑的第一個點 (EP)
        m_Transform.translation = {path[0].first, path[0].second};
        m_Transform.scale = {0.25F, 0.25F}; // 配合地圖的縮放比例
    }
}

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