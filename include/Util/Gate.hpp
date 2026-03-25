#ifndef GATE_HPP
#define GATE_HPP

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include <memory>
#include <vector>

enum class GateType { HORIZONTAL, VERTICAL };

class Gate : public Util::GameObject {
public:
    // 傳入 vector<int> 而不是單一 int
    Gate(std::shared_ptr<Util::Image> image, GateType type, const std::vector<int>& targetIds, int gridX, int gridY);

    GateType GetType() const { return m_Type; }
    bool IsClosed() const { return m_IsClosed; }

    // 取得所有要阻擋的敵人 ID，這給未來 BFS 判斷特定敵人能不能過時使用
    const std::vector<int>& GetTargetIds() const { return m_TargetIds; }

    void SetClosed(bool closed);

    int GetGridX() const { return m_GridX; }
    int GetGridY() const { return m_GridY; }

    std::vector<std::shared_ptr<Util::GameObject>> m_ColorIcons;

private:
    GateType m_Type;
    std::vector<int> m_TargetIds; // 變成陣列
    bool m_IsClosed = true;
    int m_GridX;
    int m_GridY;
    std::shared_ptr<Util::Image> m_Image;
};

#endif