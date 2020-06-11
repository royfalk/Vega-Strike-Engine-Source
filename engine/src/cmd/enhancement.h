#ifndef _ENHANCER_H_
#define _ENHANCER_H_
#include "cmd/unit.h"
#include "dummyunit.h"
#include "savegame.h"

class GameEnhancement : public GameUnit< DummyUnit >
{
protected:
    std::string filename;

public:
    friend class UnitFactory;

    virtual void reactToCollision( Unit *smaller,
                                   const QVector &biglocation,
                                   const Vector &bignormal,
                                   const QVector &smalllocation,
                                   const Vector &smallnormal,
                                   float dist )
    {
        if (smaller->isUnit() != ASTEROIDPTR) {
            double percent;
            char tempdata[sizeof (this->shield)];
            memcpy( tempdata, &this->shield, sizeof (this->shield) );
            shield.number = 0;     //don't want them getting our boosted shields!
            shield.shield2fb.front = shield.shield2fb.back = shield.shield2fb.frontmax = shield.shield2fb.backmax = 0;
            smaller->Upgrade( this, 0, 0, true, true, percent );
            memcpy( &this->shield, tempdata, sizeof (this->shield) );
            string fn( filename );
            string fac( FactionUtil::GetFaction( faction ) );
            Kill();
            _Universe->AccessCockpit()->savegame->AddUnitToSave( fn.c_str(), ENHANCEMENTPTR, fac.c_str(), (long) this );
        }
    }
protected:
    GameEnhancement( std::vector< Mesh* >m, bool b, int i ) : GameUnit< DummyUnit >( m, b, i ) {}

/// constructor only to be called by UnitFactory
    GameEnhancement( const char *filename,
                     int faction,
                     const string &modifications,
                     Flightgroup *flightgrp = NULL,
                     int fg_subnumber = 0 ) :
        GameUnit< DummyUnit > ( filename, false, faction, modifications, flightgrp, fg_subnumber )
    {
        string file( filename );
        this->filename = filename;
    }

    virtual enum clsptr isUnit() const
    {
        return ENHANCEMENTPTR;
    }

};

#endif

