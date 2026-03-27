#ifndef TELEPORTER_HPP
#define TELEPORTER_HPP

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include "Gate.hpp" // 借用 GateType (HORIZONTAL, VERTICAL)
#include <memory>

class Teleporter : public Util::GameObject {
public:
    Teleporter(std::shared_ptr<Util::Image> frameImage, GateType type, int teleportId, int gridX, int gridY);

    GateType GetType() const { return m_Type; }
    int GetTeleportId() const { return m_TeleportId; }
    int GetGridX() const { return m_GridX; }
    int GetGridY() const { return m_GridY; }

    std::shared_ptr<Util::GameObject> m_ColorBar; // 中間的彩色能量棒

private:
    GateType m_Type;
    int m_TeleportId;
    int m_GridX;
    int m_GridY;
};

#endif