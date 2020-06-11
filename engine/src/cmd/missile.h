#ifndef MISSILE_H_
#define MISSILE_H_

#include "dummyunit.h"
#include "cmd/unit_util.h"

#include "gfx/quaternion.h"
#include "gfx/vec.h"
#include "gfx/matrix.h"
#include "cmd/script/flightgroup.h"
#include "cmd/collection.h"
#include "cmd/unit_factory.h"
#include "cmd/unit.h"

class MissileEffect {
    QVector pos;
    float  damage;
    float  phasedamage;
    float  radius;
    float  radialmultiplier;
    void  *ownerDoNotDereference;
    void DoApplyDamage( Unit *parent, Unit *un, float distance, float damage_fraction );
public:
    void ApplyDamage( Unit* );
    MissileEffect( const QVector &pos, float dam, float pdam, float radius, float radmult, void *owner ) : pos( pos )
    {
        damage = dam;
        phasedamage = pdam;
        this->radius     = radius;
        radialmultiplier = radmult;
        this->ownerDoNotDereference = owner;
    }
    float GetRadius() const
    {
        return radius;
    }
    const QVector& GetCenter() const
    {
        return pos;
    }
};

class GameMissile : public GameUnit< DummyUnit >
{
// Fields
protected:
  float time;
  float damage;
  float phasedamage;
  float radial_effect;
  float radial_multiplier;
  float detonation_radius;
  bool  discharged;
  bool  had_target;
  signed char retarget;


protected:
/// constructor only to be called by UnitFactory
    GameMissile( const char *filename,
                 int faction,
                 const string &modifications,
                 const float damage,
                 float phasedamage,
                 float time,
                 float radialeffect,
                 float radmult,
                 float detonation_radius ) :
        GameUnit< DummyUnit > ( filename, false, faction, modifications )
      , time( time )
      , damage( damage )
      , phasedamage( phasedamage )
      , radial_effect( radialeffect )
      , radial_multiplier( radmult )
      , detonation_radius( detonation_radius )
      , discharged( false )
      , retarget( -1 )
    {
      maxhull   *= 10;
      had_target = false;
        this->InitMissile( time, damage, phasedamage, radialeffect, radmult, detonation_radius, false, -1 );
        static bool missilesparkle = XMLSupport::parse_bool( vs_config->getVariable( "graphics", "missilesparkle", "false" ) );
        if (missilesparkle)
            maxhull *= 4;
    }

    virtual float ExplosionRadius();
    friend class UnitFactory;

    void InitMissile( float ptime,
                      const float pdamage,
                      float pphasedamage,
                      float pradial_effect,
                      float pradial_multiplier,
                      float pdetonation_radius,
                      bool pdischarged,
                      signed char pretarget )
    {
        time   = ptime;
        damage = pdamage;
        phasedamage = pphasedamage;
        radial_effect     = pradial_effect;
        radial_multiplier = pradial_multiplier;
        detonation_radius = pdetonation_radius;
        discharged = pdischarged;
        retarget   = pretarget;
        had_target = false;
    }
public:
    virtual enum clsptr isUnit() const
    {
        return MISSILEPTR;
    }

    virtual void Kill( bool erase = true );
    /*{
        Discharge();
        GameUnit< DummyUnit >::Kill( erase );
    }*/

    void Discharge();

    virtual void reactToCollision( Unit *smaller,
                                   const QVector &biglocation,
                                   const Vector &bignormal,
                                   const QVector &smalllocation,
                                   const Vector &smallnormal,
                                   float dist );

    virtual void UpdatePhysics2( const Transformation &trans,
                                 const Transformation &old_physical_state,
                                 const Vector &accel,
                                 float difficulty,
                                 const Matrix &transmat,
                                 const Vector &CumulativeVelocity,
                                 bool ResolveLast,
                                 UnitCollection *uc = NULL );


private:
/// default constructor forbidden
    GameMissile();
/// copy constructor forbidden
    GameMissile( const GameMissile& );
/// assignment operator forbidden
    GameMissile& operator=( const GameMissile& );
};

#endif

