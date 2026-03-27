#ifdef IENEMY_HPP
#define IENEMY_HPP

#include "Util/GameObject.hpp"
#include "Util/Image.hpp"
#include <vector>
#include <utility>
#include <memory>

class IEnemy : public Util::GameObject{
    private:

    public:
    //virtual void IEnemy(std::shared_ptr<Util::Image> image, const std::vector<std::pair<float, float>>& path, int spawnIndex, int enemyId) const =0;
    virtual ~IEnemy();

    virtual void Update(const std::vector<std::pair<float, float>>& currentPath)=0;
    virtual bool HasReachedBase()=0;
    
    virtual int GetSpawnIndex()=0
    virtual int GetEnemyId()=0;

}

#endif