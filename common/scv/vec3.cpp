
#include "vec3.h"

namespace scv {

    const vec3 vec3_zero(0,0,0);

    // dir must be normalized
    // lim is a positive upper bound (octant of a cube)
    vec3 getBoundedVector(vec3 dir, vec3 lim) {

        int yneg = dir.y < 0 ? -1 : 1;
        int xneg = dir.x < 0 ? -1 : 1;
        int zneg = dir.z < 0 ? -1 : 1;

        dir = abs(dir);
        vec3 v = lim.Length() * dir;

        scv_float f = 1;
        if ( v.x != 0 )
            f = min(f, lim.x / v.x);
        if ( v.y != 0 )
            f = min(f, lim.y / v.y);
        if ( v.z != 0 )
            f = min(f, lim.z / v.z);

        if ( f < 1 )
            v *= f;

        return vec3( xneg * v.x, yneg * v.y, zneg * v.z );
    }

}
