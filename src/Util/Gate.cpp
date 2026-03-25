#include "Util/Gate.hpp"

Gate::Gate(std::shared_ptr<Util::Image> frameImage, GateType type, const std::vector<int>& targetIds, int gridX, int gridY)
    : m_Type(type), m_TargetIds(targetIds), m_GridX(gridX), m_GridY(gridY), m_FrameImage(frameImage) {

    SetDrawable(m_FrameImage);
    SetZIndex(5);
}

void Gate::SetClosed(bool closed) {
    if (m_IsClosed == closed) return;
    m_IsClosed = closed;

    // 控制外框
    SetDrawable(m_IsClosed ? m_FrameImage : nullptr);

    // 控制中間的能量棒，使用我們自己存的 image
    for (auto& bar : m_ColorBars) {
        bar.object->SetDrawable(m_IsClosed ? bar.image : nullptr);
    }
}