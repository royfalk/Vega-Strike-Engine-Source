#ifndef DUMMYUNIT_H
#define DUMMYUNIT_H

#include "cmd/unit_generic.h"
#include "gfx/vec.h"

class DummyUnit : public Unit
{
public:
    DummyUnit() {}

protected:
    DummyUnit( std::vector< Mesh* >m, bool b, int i ) : Unit( m, b, i ) {}
};

#endif // DUMMYUNIT_H
