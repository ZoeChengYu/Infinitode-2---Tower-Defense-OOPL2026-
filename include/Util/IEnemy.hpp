#ifdef IENEMY_HPP
#define IENEMY_HPP

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include <vector>
#include <utility>
#include <memory>

class Enepy_New : public Util::GameObject{
    private:

    public:
    virtual void IEnepy(std::shared_ptr<Util::Image> image, const std::vector<std::pair<float, float>>& path, int spawnIndex, int enemyId) const =0;
    virtual ~IEnepy();
    



    

}

#endif