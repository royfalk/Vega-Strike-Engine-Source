#ifndef UNIT_TEMPLATE_H_
#define UNIT_TEMPLATE_H_
#error
//currnetly causes multiple definitions for a STUPID reason... die, gcc die
#include "unit_generic.h"
#include "unit.h"
#include "enhancement_generic.h"
#include "dummyunit.h"

template class GameUnit< Enhancement >;
template class GameUnit< Unit >;
template class GameUnit< DummyUnit >;
#endif

