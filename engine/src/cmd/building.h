#ifndef __BUILDING_H
#define __BUILDING_H

#include "unit.h"
#include "dummyunit.h"

class Terrain;
class ContinuousTerrain;
class Flightgroup;

class GameBuilding : public GameUnit< DummyUnit >
{
protected:
    union Buildingparent
    {
        Terrain *terrain;
        ContinuousTerrain *plane;
    }
    parent;
    bool continuous;
    bool vehicle;

protected:

    GameBuilding( ContinuousTerrain *parent,
                         bool vehicle,
                         const char *filename,
                         bool SubUnit,
                         int faction,
                         const std::string &unitModifications = std::string( "" ),
                         Flightgroup *fg = NULL );
    GameBuilding( Terrain *parent,
                  bool vehicle,
                  const char *filename,
                  bool SubUnit,
                  int faction,
                  const std::string &unitModifications = std::string( "" ),
                  Flightgroup *fg = NULL );

    friend class UnitFactory;

public:

    bool ownz( void *parent )
    {
        return this->parent.terrain == (Terrain*) parent;
    }

    virtual enum clsptr isUnit() const
    {
        return BUILDINGPTR;
    }

    virtual void UpdatePhysics2( const Transformation &trans,
                                 const Transformation &oldtranssmat,
                                 const Vector&,
                                 float difficulty,
                                 const Matrix&,
                                 const Vector &CumulativeVelocity,
                                 bool ResolveLast,
                                 UnitCollection *uc = NULL );
};

#endif

