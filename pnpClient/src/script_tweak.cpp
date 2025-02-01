#include "script_tweak.h"
#include "eventhooks.h"

using  namespace  std;

float script_getTweakValue(string name) {

    tweakInfo_t *info = getTweakByName(name);
    if ( ! info )
        return 0;

    return info->floatval;
}
