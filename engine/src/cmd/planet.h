/* planet.h
 *  ok *
 * Unit type that is a gravity source, and follows a set orbit pattern
 */
#ifndef _PLANET_H_
#define _PLANET_H_

#include <stdio.h>

#include "unit.h"
#include "ai/order.h"
#include "collection.h"
#include <vector>
#include <string>
#include "configxml.h"
#include "gfxlib_struct.h"
#include "images.h"
#include "dummyunit.h"

struct GFXMaterial;
/* Orbits in the xy plane with the given radius. Depends on a reorientation of coordinate bases */

class Texture;
class Atmosphere;
class PlanetaryTransform;

class ContinuousTerrain;

class PlanetaryOrbit : public Order
{
private:
    double  velocity;
    double  theta;
    double  inittheta;

    QVector x_size;
    QVector y_size;
    QVector focus;
#define ORBIT_PRIORITY 8
#define NUM_ORBIT_AVERAGE (SIM_QUEUE_SIZE/ORBIT_PRIORITY)
    QVector orbiting_average[NUM_ORBIT_AVERAGE];
    float   orbiting_last_simatom;
    int     current_orbit_frame;
    bool    orbit_list_filled;
protected:
///A vector containing all lights currently activated on current planet
    std::vector< int >lights;

public: PlanetaryOrbit( Unit *p,
                        double velocity,
                        double initpos,
                        const QVector &x_axis,
                        const QVector &y_axis,
                        const QVector &Centre,
                        Unit *target = NULL );
    ~PlanetaryOrbit();
    void Execute();
};


class GamePlanet : public GameUnit< class DummyUnit >
{
// Fields
public:
    UnitCollection satellites;

protected:
    PlanetaryTransform *terraintrans;
    Atmosphere *atmosphere;
    ContinuousTerrain  *terrain;
    Vector TerrainUp;
    Vector TerrainH;
    bool   inside;
    bool   atmospheric; //then users can go inside!
    float  radius;
    float  gravity;
    UnitCollection    insiders;
    std::vector< int >lights;
private:
    Animation *shine;

// Constructors
protected:
/// default constructor - only to be called by UnitFactory
    GamePlanet();

/// constructor - only to be called by UnitFactory
    GamePlanet( QVector x,
                QVector y,
                float vely,
                const Vector &rotvel,
                float pos,
                float gravity,
                float radius,
                const std::string &filename,
                const std::string &technique,
                const std::string &unitname,
                BLENDFUNC blendsrc,
                BLENDFUNC blenddst,
                const std::vector< std::string > &dest,
                const QVector &orbitcent,
                Unit *parent,
                const GFXMaterial &ourmat,
                const std::vector< GFXLightLocal >&,
                int faction,
                string fullname,
                bool inside_out = false,
                unsigned int lights_num=0  );
    ~GamePlanet();

    friend class UnitFactory;

// Methods
public:
    const vector< int >& activeLights() { return lights; }

    void AddAtmosphere( const string &texture, float radius, BLENDFUNC blendSrc,
                        BLENDFUNC blendDst, bool inside_out );

    void AddCity( const string &texture, float radius, int numwrapx,
                  int numwrapy, BLENDFUNC blendSrc, BLENDFUNC blendDst,
                  bool inside_out = false, bool reverse_normals = true );

    void AddFog( const vector< AtmosphericFogMesh >&, bool optical_illusion );

    void AddSatellite( Unit *orbiter );

    Vector AddSpaceElevator( const string &name, const string &faction, char direction );

    void AddRing( const string &texture, float iradius, float oradius,
                  const QVector &r, const QVector &s, int slices,
                  int numwrapx, int numwrapy, BLENDFUNC blendSrc, BLENDFUNC blendDst );

    Unit * beginElement( QVector x, QVector y, float vely,
                         const Vector &rotvel, float pos, float gravity,
                         float radius, const string &filename, const string &technique,
                         const string &unitname, BLENDFUNC blendsrc, BLENDFUNC blenddst,
                         const vector< string > &dest, int level, const GFXMaterial &ourmat,
                         const vector< GFXLightLocal > &ligh, bool isunit, int faction,
                         string fullname, bool inside_out );

    void DisableLights();

    virtual void Draw( const Transformation &quat = identity_transformation,
                       const Matrix &m = identity_matrix ) override;

    void DrawTerrain();

    void EnableLights();

    void endElement();

    Atmosphere * getAtmosphere()
    {
        return atmosphere;
    }

    string getCargoUnitName() const { return getFullname(); }

    string getHumanReadablePlanetType() const;

    inline const float getRadius() const { return radius; }

    ContinuousTerrain * getTerrain( PlanetaryTransform* &t )
    {
        t = terraintrans;
        return terrain;
    }

    GamePlanet * GetTopPlanet( int level );

    bool hasLights() const { return !lights.empty(); }

    bool isAtmospheric() const { return hasLights() || atmospheric; }

    virtual enum clsptr isUnit() const override { return PLANETPTR; }

    // TODO: make virtual and override once we get rid of the template code
    void Kill( bool erasefromsave = false );

    static void ProcessTerrains();

    virtual void reactToCollision( Unit *smaller, const QVector &biglocation,
                                   const Vector &bignormal, const QVector &smalllocation,
                                   const Vector &smallnormal, float dist ) override;

    void setAtmosphere( Atmosphere* );

    PlanetaryTransform * setTerrain( ContinuousTerrain*, float ratiox, int numwraps,
                                     float scaleatmos );

public:
    class PlanetIterator
    {
public:
      PlanetIterator( GamePlanet *p )
      {
        localCollection.append( p );
        pos = localCollection.createIterator();
      }

      ~PlanetIterator() {}

      void preinsert( Unit *unit )
      {
        abort();
      }

      void postinsert( Unit *unit )
      {
        abort();
      }

      void remove()
      {
        abort();
      }

      inline Unit * current()
      {
        if ( !pos.isDone() )
          return *pos;
        return NULL;
      }

      void advance()
      {
        if (current() != NULL) {
            Unit *cur = *pos;
            if (cur->isUnit() == PLANETPTR)
              for (un_iter tmp( ( (GamePlanet*) cur )->satellites.createIterator() ); !tmp.isDone(); ++tmp)
                localCollection.append( (*tmp) );
            ++pos;
          }
      }

      inline PlanetIterator& operator++()
      {
        advance();
        return *this;
      }

      inline Unit* operator*()
      {
        return current();
      }

    private:
      inline un_iter operator++( int )
      {
        abort();
      }

      UnitCollection localCollection;
      un_iter pos;
    };

    PlanetIterator createIterator()
    {
      return PlanetIterator( this );
    }

    friend class GamePlanet::PlanetIterator;

    friend class PlanetaryOrbit;
};

#endif

