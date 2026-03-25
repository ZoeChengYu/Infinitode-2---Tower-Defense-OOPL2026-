// 在 Gate.hpp 的 private 區塊加入：
// std::shared_ptr<Util::Image> m_Image;

// Gate.cpp 的實作：
#include "Util/Gate.hpp"

Gate::Gate(std::shared_ptr<Util::Image> image, GateType type, int gridX, int gridY)
    : m_Type(type), m_GridX(gridX), m_GridY(gridY), m_Image(image) {
    SetDrawable(image);
    SetZIndex(5);
}

void Gate::SetClosed(bool closed) {
    if (m_IsClosed == closed) return;
    m_IsClosed = closed;

    // 如果關閉就顯示圖片，如果開啟就設為 nullptr 讓它消失
    SetDrawable(m_IsClosed ? m_Image : nullptr);
}