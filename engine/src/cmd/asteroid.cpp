#include "cmd/asteroid.h"
#include "cmd/script/flightgroup.h"
#include "cmd/collection.h"
#include "gfx/vec.h"
#include "gfx/quaternion.h"
#include "gfx/matrix.h"
#include "cmd/unit.h"
#include "cmd/unit_generic.h"

static void RecursiveSetSchedule(Unit *un)
{
	if (un) {
		if (un->SubUnits.empty())
			un->schedule_priority = Unit::scheduleRoid;
		else {
			un->schedule_priority = Unit::scheduleAField;
			un->do_subunit_scheduling = true;
			for (un_iter it=un->getSubUnits(); !it.isDone(); ++it)
				RecursiveSetSchedule(*it);
		}
	}
}

GameAsteroid::GameAsteroid( const char *filename, int faction, Flightgroup *fg, int fg_snumber,
                            float difficulty ) : GameUnit< DummyUnit > ( filename, false, faction, string( "" ), fg, fg_snumber )
{
    GameAsteroid::Init( difficulty );
}

void GameAsteroid::Init(float difficulty)
{
	asteroid_physics_offset=0;
	un_iter iter = getSubUnits();
	while(*iter) {
		float x=2*difficulty*((float)rand())/RAND_MAX-difficulty;
		float y=2*difficulty*((float)rand())/RAND_MAX-difficulty;
		float z=2*difficulty*((float)rand())/RAND_MAX-difficulty;
		(*iter)->SetAngularVelocity(Vector(x,y,z));
		++iter;
	}
	RecursiveSetSchedule(this);
}

void GameAsteroid::UpdatePhysics2( const Transformation &trans,
                                   const Transformation &old_physical_state,
                                   const Vector &accel,
                                   float difficulty,
                                   const Matrix &transmat,
                                   const Vector &CumulativeVelocity,
                                   bool ResolveLast,
                                   UnitCollection *uc )
{
    GameUnit< DummyUnit >::UpdatePhysics2( trans,
                                          old_physical_state,
                                          accel,
                                          difficulty,
                                          transmat,
                                          CumulativeVelocity,
                                          ResolveLast,
                                          uc );
}

void GameAsteroid::reactToCollision(Unit * smaller, const QVector& biglocation, const Vector& bignormal, const QVector& smalllocation, const Vector& smallnormal, float dist)
{
	switch (smaller->isUnit()) {
		case ASTEROIDPTR:
		case ENHANCEMENTPTR:
			break;
		default:
			/***** DOES THAT STILL WORK WITH UNIT:: ?????????? *******/
			Unit::reactToCollision (smaller,biglocation,bignormal,smalllocation,smallnormal,dist);
			break;
	}
}
