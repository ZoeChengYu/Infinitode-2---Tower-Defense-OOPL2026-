#ifndef GATE_HPP
#define GATE_HPP

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include <memory>

// 定義閘門的方向
enum class GateType {
    HORIZONTAL, // 橫向閘門 (阻擋上下通行，例如你的閘門B)
    VERTICAL    // 直向閘門 (阻擋左右通行，例如你的閘門A)
};

class Gate : public Util::GameObject {
public:
    // 建構子：傳入圖片、方向，以及它依附的基準網格座標
    Gate(std::shared_ptr<Util::Image> image, GateType type, int gridX, int gridY);

    GateType GetType() const { return m_Type; }
    bool IsClosed() const { return m_IsClosed; }

    // 開關閘門的介面
    void SetClosed(bool closed);

    // 取得基準座標，方便之後讓 BFS 演算法查詢
    int GetGridX() const { return m_GridX; }
    int GetGridY() const { return m_GridY; }

private:
    GateType m_Type;
    bool m_IsClosed = true; // 預設閘門是關閉狀態（阻擋通行）
    std::shared_ptr<Util::Image> m_Image;

    // 記錄這道閘門是掛在哪一個格子的「右邊」或「下面」
    int m_GridX;
    int m_GridY;
};

#endif