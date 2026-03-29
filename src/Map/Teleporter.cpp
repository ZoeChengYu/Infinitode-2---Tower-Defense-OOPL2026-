#include "Map/Teleporter.hpp"

Teleporter::Teleporter(std::shared_ptr<Util::Image> frameImage, GateType type, int teleportId, int gridX, int gridY)
    : m_Type(type), m_TeleportId(teleportId), m_GridX(gridX), m_GridY(gridY) {
    SetDrawable(frameImage);
    SetZIndex(4); // 畫在道路之上，但比閘門和怪物低
}