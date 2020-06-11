#ifndef _ASTEROID_H_
#define _ASTEROID_H_
#include "gfx/quaternion.h"
#include "gfx/vec.h"
#include "gfx/matrix.h"
#include "cmd/script/flightgroup.h"
#include "cmd/collection.h"
#include "cmd/unit_factory.h"
#include "cmd/unit.h"
#include "dummyunit.h"

class GameAsteroid : public GameUnit< DummyUnit >
{
private:
  unsigned int asteroid_physics_offset;

public:
    virtual void UpdatePhysics2( const Transformation &trans,
                                 const Transformation &old_physical_state,
                                 const Vector &accel,
                                 float difficulty,
                                 const Matrix &transmat,
                                 const Vector &CumulativeVelocity,
                                 bool ResolveLast,
                                 UnitCollection *uc = NULL );
  void Init( float difficulty);
  virtual enum clsptr isUnit() const { return(ASTEROIDPTR);}
  virtual void reactToCollision(Unit * smaller, const QVector& biglocation, const Vector& bignormal, const QVector& smalllocation, const Vector& smallnormal, float dist);

protected:
/** Constructor that can only be called by the UnitFactory.
 */
    GameAsteroid( const char *filename, int faction, Flightgroup *fg = NULL, int fg_snumber = 0, float difficulty = .01 );

    friend class UnitFactory;

private:
/// default constructor forbidden
    GameAsteroid();

/// copy constructor forbidden
    GameAsteroid( const DummyUnit& );

/// assignment operator forbidden
    GameAsteroid& operator=( const DummyUnit& );
};
#endif

