#ifndef ENEMY_HPP
#define ENEMY_HPP

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include <vector>
#include <utility>
#include <memory>

class Enemy : public Util::GameObject {
public:
    // 傳入敵人的圖片，以及剛剛算出來的路徑座標清單
    Enemy(std::shared_ptr<Util::Image> image, const std::vector<std::pair<float, float>>& path);
    
    // 每一幀都會呼叫，用來更新位置
    void Update(const std::vector<std::pair<float, float>>& currentPath);
    
    bool HasReachedBase() const { return m_ReachedBase; }

private:
    size_t m_CurrentTargetIndex = 0; // 目前要前往的節點索引
    float m_Speed = 3.0F;            // 敵人的移動速度
    bool m_ReachedBase = false;      // 是否已抵達終點
};

#endif