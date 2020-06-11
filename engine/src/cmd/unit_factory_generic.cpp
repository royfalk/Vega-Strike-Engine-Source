#include "unit_factory.h"
#include "unit_generic.h"
#include "universe_util.h"

Unit*UnitFactory::_masterPartList = NULL;
Unit* UnitFactory::getMasterPartList()
{
    if (_masterPartList == NULL) {
        static bool making = true;
        if (making) {
            making = false;
            _masterPartList = Unit::makeMasterPartList();
            making = true;
        }
    }
    return _masterPartList;
}













