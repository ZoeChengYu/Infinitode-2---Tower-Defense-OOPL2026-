#include "Util/Gate.hpp"

Gate::Gate(std::shared_ptr<Util::Image> image, GateType type, const std::vector<int>& targetIds, int gridX, int gridY)
    : m_Type(type), m_TargetIds(targetIds), m_GridX(gridX), m_GridY(gridY), m_Image(image) {
    SetDrawable(m_Image);
    SetZIndex(5);
}

void Gate::SetClosed(bool closed) {
    if (m_IsClosed == closed) return;
    m_IsClosed = closed;
    SetDrawable(m_IsClosed ? m_Image : nullptr);
    for (auto& icon : m_ColorIcons) {
        icon->SetVisible(m_IsClosed);
    }
}