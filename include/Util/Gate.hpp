#ifndef GATE_HPP
#define GATE_HPP

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include <memory>
#include <vector>

enum class GateType { HORIZONTAL, VERTICAL };

// 新增一個小結構：同時記住「物件」和它的「原圖」
struct ColorBar {
    std::shared_ptr<Util::GameObject> object;
    std::shared_ptr<Util::Image> image;
};

class Gate : public Util::GameObject {
public:
    Gate(std::shared_ptr<Util::Image> frameImage, GateType type, const std::vector<int>& targetIds, int gridX, int gridY);

    GateType GetType() const { return m_Type; }
    bool IsClosed() const { return m_IsClosed; }
    const std::vector<int>& GetTargetIds() const { return m_TargetIds; }

    void SetClosed(bool closed);

    int GetGridX() const { return m_GridX; }
    int GetGridY() const { return m_GridY; }

    // 把原本的 vector 改成裝我們定義的 ColorBar
    std::vector<ColorBar> m_ColorBars;

private:
    GateType m_Type;
    std::vector<int> m_TargetIds;
    bool m_IsClosed = true;
    int m_GridX;
    int m_GridY;

    std::shared_ptr<Util::Image> m_FrameImage;
};

#endif