
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include "planner.h"

#include "../log.h"

// The straight-line part of this is described here:
// http://www.et.byu.edu/~ered/ME537/Notes/Ch5.pdf

namespace scv {

// Stolen from GSL library
// https://github.com/Starlink/gsl/blob/master/poly/solve_quadratic.c
static int gsl_poly_solve_quadratic(scv_float a, scv_float b, scv_float c, scv_float *x0, scv_float *x1)
{
  if (a == 0) /* Handle linear case */
    {
      if (b == 0)
        {
          return 0;
        }
      else
        {
          *x0 = -c / b;
          return 1;
        };
    }

  {
    scv_float disc = b * b - 4 * a * c;

    if (disc > 0)
      {
        if (b == 0)
          {
            scv_float r = sqrt (-c / a);
            *x0 = -r;
            *x1 =  r;
          }
        else
          {
            scv_float sgnb = (b > 0 ? 1 : -1);
            scv_float temp = -0.5 * (b + sgnb * sqrt (disc));
            scv_float r1 = temp / a ;
            scv_float r2 = c / temp ;

            if (r1 < r2)
              {
                *x0 = r1 ;
                *x1 = r2 ;
              }
            else
              {
                *x0 = r2 ;
                  *x1 = r1 ;
              }
          }
        return 2;
      }
    else if (disc == 0)
      {
        *x0 = -0.5 * b / a ;
        *x1 = -0.5 * b / a ;
        return 2 ;
      }
    else
      {
        return 0;
      }
  }
}



planner::planner()
{
    cornerBlendMethod = CBM_INTERPOLATED_MOVES;
    maxOverlapFraction = 0.8;
    posLimitLower = vec3_zero;
    posLimitUpper = vec3_zero;
    velLimit = vec3_zero;
    accLimit = vec3_zero;
    jerkLimit = vec3_zero;
    resetTraverse();
}

bool planner::calculateMoves()
{
    bool invalidSettings = false;
    if ( velLimit.anyZero() ) {
        g_log.log(LL_ERROR, "Global velocity limit has zero component!\n");
        invalidSettings = true;
    }
    if ( accLimit.anyZero() ) {
        g_log.log(LL_ERROR, "Global acceleration limit has zero component!\n");
        invalidSettings = true;
    }
    if ( jerkLimit.anyZero() ) {
        g_log.log(LL_ERROR, "Global jerk limit has zero component!\n");
        invalidSettings = true;
    }
    if ( rotationVelLimit <= 0 ) {
        g_log.log(LL_ERROR, "Global rotation velocity limit is zero!\n");
        invalidSettings = true;
    }
    if ( rotationAccLimit <= 0 ) {
        g_log.log(LL_ERROR, "Global rotation acceleration limit is zero!\n");
        invalidSettings = true;
    }
    if ( rotationJerkLimit <= 0 ) {
        g_log.log(LL_ERROR, "Global rotation jerk limit is zero!\n");
        invalidSettings = true;
    }

    for (size_t i = 0; i < moves.size(); i++) {
        scv::move& m = moves[i];
        if ( m.moveType == MT_SYNC || m.moveType == MT_WAIT )
            continue;
        if ( m.vel <= 0 ) {
            printf("Move velocity limit is zero!\n");
            invalidSettings = true;
        }
        if ( m.acc <= 0 ) {
            printf("Move acceleration limit is zero!\n");
            invalidSettings = true;
        }
        if ( m.jerk <= 0 ) {
            printf("Move jerk limit is zero!\n");
            invalidSettings = true;
        }
    }

    if ( invalidSettings ) {
        g_log.log(LL_ERROR, "Aborting move calculation due to invalid global move limits!\n");
        return false;
    }

    for (size_t i = 0; i < delayableEvents.size(); i++) {
        delayableEvent& de = delayableEvents[i];
        if ( de.type != DET_ROTATION )
            continue;
        calculateRotation(de.rot);
    }

    for (size_t i = 0; i < moves.size(); i++) {
        scv::move& m = moves[i];

        calculateMove(m);

        if ( cornerBlendMethod == CBM_CONSTANT_JERK_SEGMENTS ) {
            if ( i > 0 && m.blendType != CBT_NONE ) {
                move& prevMove = moves[i-1];
                bool isFirst = i == 1;
                bool isLast = i == (moves.size()-1);
                blendCorner( prevMove, m, isFirst, isLast );
            }
        }
    }

    if ( cornerBlendMethod == CBM_CONSTANT_JERK_SEGMENTS ) {
        // remove segments that were replaced by a blended corner
        for (size_t i = 0; i < moves.size(); i++) {
            move& m = moves[i];
            std::vector<segment>& segs = m.segments;
            segs.erase( std::remove_if(std::begin(segs), std::end(segs), [](segment& s) { return s.toDelete || s.duration <= 0; }), segs.end());
        }

        // move the second segment of each blended corner to the following move
        for (size_t i = 1; i < moves.size(); i++) {
            move& m0 = moves[i-1];
            if ( m0.containsBlend ) {
                move& m1 = moves[i];
                segment blendSeg = m0.segments.back();
                m0.segments.pop_back();
                m1.segments.insert( m1.segments.begin(), blendSeg );
            }
        }
    }

    // set the 'duration' of each move from its segments
    for (size_t i = 0; i < moves.size(); i++) {
        move& m = moves[i];
        m.duration = 0;
        if ( m.moveType == MT_SYNC )
            continue;
        for (size_t i = 0; i < m.segments.size(); i++) {
            segment& s = m.segments[i];
            m.duration += s.duration;
        }
    }

    if ( cornerBlendMethod == CBM_INTERPOLATED_MOVES )
        calculateSchedules();

    collateSegments();

    return true;
}

void planner::clear()
{
    moves.clear();
    segments.clear();
    delayableEvents.clear();
    rotationsInProgress.clear();
    unsyncedDelayableEvents.clear();
}

void planner::setCornerBlendMethod(cornerBlendMethod_e m)
{
    cornerBlendMethod = m;
}

void planner::setMaxCornerBlendOverlapFraction(float f)
{
    // Overlap fraction of zero causes problems for advanceTraverse because there
    // will be tiny slices of time where no move is active. Overlap fraction of 1
    maxOverlapFraction = max(0.01f, min(f, 1.0f));
}

void planner::calculateMove(move& m)
{
    m.segments.clear();

    if ( m.moveType == MT_SYNC ) {
        return;
    }

    if ( m.moveType == MT_WAIT ) {
        segment w1;
        w1.pos = m.src;
        w1.vel = scv::vec3_zero;
        w1.acc = scv::vec3_zero;
        w1.jerk = scv::vec3_zero;
        w1.duration = m.waitDuration;
        m.segments.push_back(w1);

        return;
    }

    scv::vec3 srcPos = m.src;
    scv::vec3 dstPos = m.dst;
    scv::vec3 ldir = dstPos - srcPos;
    scv_float llen = ldir.Normalize();

    scv::vec3 bv = getBoundedVector(ldir, velLimit);
    scv::vec3 ba = getBoundedVector(ldir, accLimit);
    scv::vec3 bj = getBoundedVector(ldir, jerkLimit);

    float v = min( bv.Length(), m.vel); // target speed
    float a = min( ba.Length(), m.acc);
    float j = min( bj.Length(), m.jerk);
    float halfDistance = 0.5 * llen;     // half of the total distance we want to move

    float T = 2 * a / j;   // duration of both curve sections
    float T1 = 0.5 * T;    // duration of concave section
    float TL = 0;          // duration of linear asection
    float T2 = 0.5 * T;    // duration of convex section


    // Values at the start of each segment. These will be updated as we go to keep track of the overall progress.
    //    ps = starting position
    //    vs = starting velocity
    //    as = starting acceleration
    // They're all zero for the first segment, so let's set them for the second segment (the end of first the segment).
    float t = T1;
    float ps = (j * t * t * t) / 6.0;
    float vs = (j * t * t) / 2.0;
    float as = (j * t);


    float dvInCurve = (a*a) / (2*j);   // change in velocity caused by a curve segment
    float v1 = 0 + dvInCurve;          // velocity at end of concave curve
    float v2 = v - dvInCurve;          // velocity at start of convex curve

    if ( v1 > v2 ) {
        // Fully performing both curve segments would exceed the velocity set-point.
        // Need to reduce the time spent in each curve segment.
        // Alter T1,T2 such that the concave and convex segments meet with a tangential transition.
        T = sqrt(4*v*j) / j;
        T1 = (j * T) / (2 * j);
        T2 = T - T1;

        // recalculate values for end of first segment
        t = T1;
        ps = (j * t * t * t) / 6.0;
        vs = (j * t * t) / 2.0;
        as = (j * t);
    }
    else if ( v2 > v1 ) {
        // The velocity change due to the curve segments is not enough to reach the set-point velocity.
        // Add a linear segment in between them where velocity will change with a constant acceleration.
        float vr = v2 - v1; // remaining velocity to make up
        TL = vr / as;        // duration of linear segment

        // The inserted linear segment must not cause the required distance to be exceeded.
        // First, find out the total distance of all three segments:
        //     led = ss + vs * TL + as * TL * TL / 2.0;                                         distance at end of linear phase
        //     lev = vs + (as * TL);                                                            velocity at end of linear phase
        //     tot = led + (lev * T2) + ((as * T2 * T2) / 2.0) - ((j * T2 * T2 * T2) / 6.0);    distance at end of convex segment
        // These condense down to:
        //     tot =   0.5 * j * t * TL * TL   + 1.5 * j * t * t * TL   + j * t * t * t;
        // which is a quadratic equation for TL.

        float totalDistance =  0.5 * j * t * TL * TL   + 1.5 * j * t * t * TL   + j * t * t * t;

        if ( totalDistance > halfDistance ) {
            float qa = 0.5 * j * t;
            float qb = 1.5 * j * t * t;
            float qc = j * t * t * t       - halfDistance;
            float x0 = -99999;
            float x1 = -99999;
            gsl_poly_solve_quadratic( qa, qb, qc, &x0, &x1 );
            float bestSolution = max(x0, x1);
            if ( bestSolution >= 0 )
                TL = bestSolution;
        }
    }


    float bothCurvesDistance = (j * t * t * t);    // distance traveled during both concave and convex segments, if no linear phase involved
    if ( bothCurvesDistance > halfDistance ) {
        // Distance required is too short to allow fully performing both curves.
        // Reduce the time allowed in each curve. No linear phases will be used anywhere.

        T = std::cbrt( halfDistance / j );     // this is the bothCurvesDistance equation above, solved for t
        T1 = T2 = T;    // same duration for both concave and convex curves
        TL = 0;

        // recalculate values for end of first segment
        t = T1;
        ps = (j * t * t * t) / 6.0;
        vs = (j * t * t) / 2.0;
        as = (j * t);
    }

    scv::vec3 origin = srcPos;

    // segment 1, concave rising
    segment c1;
    c1.pos = origin;
    c1.vel = scv::vec3_zero;
    c1.acc = scv::vec3_zero;
    c1.jerk = j * ldir;
    c1.duration = T1;
    m.segments.push_back(c1);

    // ss,vs,as are already calculated above, no need to change them for this one

    // segment 2, rising linear phase (maybe)
    if ( TL > 0 ) {
        segment c2;
        c2.pos = origin + ps * ldir;
        c2.vel = vs * ldir;
        c2.acc = as * ldir;
        c2.jerk = scv::vec3_zero;
        c2.duration = TL;
        m.segments.push_back(c2);

        t = TL;
        ps += (vs * t) + (as * t * t) / 2.0;
        vs += (as * t);
    }

    // segment 3, convex rising
    segment c3;
    c3.pos = origin + ps * ldir;
    c3.vel = vs * ldir;
    c3.acc = as * ldir;
    c3.jerk = -j * ldir;
    c3.duration = T2;
    m.segments.push_back(c3);

//    if ( m.moveType == MT_JOGSTART )
//        return;

    t = T2;
    ps += (vs * t) + ((as * t * t) / 2.0) - ((j * t * t * t) / 6.0);
    vs += ((j * t * t) / 2.0);
    as = 0;

    // segment 4, constant velocity linear phase (maybe)
    float totalRiseDistance = 2 * ps;
    float remainingDistance = llen - totalRiseDistance;
    if ( remainingDistance > 0.000001 ) {

        segment c4;
        c4.pos = origin + ps * ldir;
        c4.vel = vs * ldir;
        c4.acc = scv::vec3_zero;
        c4.jerk = scv::vec3_zero;
        c4.duration = remainingDistance / v;
        m.segments.push_back(c4);

        ps += vs * c4.duration; // a nice simple calculation for a change
    }

    if ( m.moveType == MT_ESTOP ) {
        m.segments.clear();
        ps = 0;
    }

    // segment 5, convex falling
    segment c5;
    c5.pos = origin + ps * ldir;
    c5.vel = vs * ldir;
    c5.acc = as * ldir;
    c5.jerk = -j * ldir;
    c5.duration = T2;
    m.segments.push_back(c5);

    t = T2;
    ps += (vs * t) + ((as * t * t) / 2.0) + (-j * t * t * t) / 6.0;
    vs += (-j * t * t) / 2.0;
    as += (-j * t);

    // segment 6, falling linear phase (maybe)
    if ( TL > 0 ) {
        segment c6;
        c6.pos = origin + ps * ldir;
        c6.vel = vs * ldir;
        c6.acc = as * ldir;
        c6.jerk = scv::vec3_zero;
        c6.duration = TL;
        m.segments.push_back(c6);

        t = TL;
        ps += (vs * t) + (as * t * t) / 2.0;
        vs += (as * t);
    }

    // segment 7, falling convex
    segment c7;
    c7.pos = origin + ps * ldir;
    c7.vel = vs * ldir;
    c7.acc = as * ldir;
    c7.jerk = j * ldir;
    c7.duration = T1;
    m.segments.push_back(c7);
}

void planner::calculateSchedules() {

    if ( moves.empty() )
        return;

    move &firstMove = moves[0];
    firstMove.scheduledTime = 0;

    bool lastMoveHadNoBlend = false;

    for (size_t i = 1; i < moves.size(); i++) {
        move& m0 = moves[i-1];
        move& m1 = moves[i];

//        if ( m0.moveType == MT_SYNC || m1.moveType == MT_SYNC )
//            continue;

        if ( m0.moveType == MT_WAIT || m0.moveType == MT_SYNC ) {
            m1.scheduledTime = m0.scheduledTime + m0.duration;
            continue;
        }

        if ( m1.blendType == CBT_NONE ) {
            m1.scheduledTime = m0.scheduledTime + m0.duration;
            lastMoveHadNoBlend = true;
            continue;
        }

        bool isFirst = i == 1 || lastMoveHadNoBlend;
        bool isLast = (i == moves.size()-1) || ((i < moves.size()-1) && (moves[i+1].blendType == CBT_NONE));

        lastMoveHadNoBlend = false;

        //scv_float rampDuration0 = m0.segments[0].duration + m0.segments[1].duration;
        //scv_float rampDuration1 = m1.segments[0].duration + m1.segments[1].duration;
        //if ( m0.segments.size() == 7 )
        //    rampDuration0 += m0.segments[2].duration;
        //if ( m1.segments.size() == 7 )
        //    rampDuration1 += m1.segments[2].duration;

        // Use 0.49 here instead of 0.5, because we absolutely must not blend more than two moves at a time.
        // With an overlap of 0.5, it is theoretically possible that three moves could be blended together.
        float allowableFraction0 = isFirst ? 1 : 0.49;
        float allowableFraction1 = isLast  ? 1 : 0.49;

        allowableFraction0 = min(allowableFraction0, maxOverlapFraction);
        allowableFraction1 = min(allowableFraction1, maxOverlapFraction);

        float allowableDuration0 = allowableFraction0 * m0.duration;
        float allowableDuration1 = allowableFraction1 * m1.duration;

        //scv_float blendTime = max(rampDuration0, rampDuration1);
        //blendTime = min(blendTime, min(allowableDuration0,allowableDuration1));
        scv_float blendTime = min(allowableDuration0,allowableDuration1);

        if ( blendTime > 0 )
            m1.scheduledTime = m0.scheduledTime + m0.duration - blendTime;
    }

}

void planner::calculateRotation(rotate& r)
{
    r.rotation_segments.clear();

    scv_float srcPos = r.src;
    scv_float dstPos = r.dst;
    scv_float dir = dstPos - srcPos;
    float len = fabs(dir);
    dir = (dir > 0) ? 1 : -1;

    float v = min( r.vel, rotationVelLimit ); // target speed
    float a = min( r.acc, rotationAccLimit);
    float j = min( r.jerk, rotationJerkLimit);
    float halfDistance = 0.5 * len;     // half of the total distance we want to move

    float T = 2 * a / j;   // duration of both curve sections
    float T1 = 0.5 * T;    // duration of concave section
    float TL = 0;          // duration of linear asection
    float T2 = 0.5 * T;    // duration of convex section


    // Values at the start of each segment. These will be updated as we go to keep track of the overall progress.
    //    ps = starting position
    //    vs = starting velocity
    //    as = starting acceleration
    // They're all zero for the first segment, so let's set them for the second segment (the end of first the segment).
    float t = T1;
    float ps = (j * t * t * t) / 6.0;
    float vs = (j * t * t) / 2.0;
    float as = (j * t);


    float dvInCurve = (a*a) / (2*j);   // change in velocity caused by a curve segment
    float v1 = 0 + dvInCurve;          // velocity at end of concave curve
    float v2 = v - dvInCurve;          // velocity at start of convex curve

    if ( v1 > v2 ) {
        // Fully performing both curve segments would exceed the velocity set-point.
        // Need to reduce the time spent in each curve segment.
        // Alter T1,T2 such that the concave and convex segments meet with a tangential transition.
        T = sqrt(4*v*j) / j;
        T1 = (j * T) / (2 * j);
        T2 = T - T1;

        // recalculate values for end of first segment
        t = T1;
        ps = (j * t * t * t) / 6.0;
        vs = (j * t * t) / 2.0;
        as = (j * t);
    }
    else if ( v2 > v1 ) {
        // The velocity change due to the curve segments is not enough to reach the set-point velocity.
        // Add a linear segment in between them where velocity will change with a constant acceleration.
        float vr = v2 - v1; // remaining velocity to make up
        TL = vr / as;        // duration of linear segment

        // The inserted linear segment must not cause the required distance to be exceeded.
        // First, find out the total distance of all three segments:
        //     led = ss + vs * TL + as * TL * TL / 2.0;                                         distance at end of linear phase
        //     lev = vs + (as * TL);                                                            velocity at end of linear phase
        //     tot = led + (lev * T2) + ((as * T2 * T2) / 2.0) - ((j * T2 * T2 * T2) / 6.0);    distance at end of convex segment
        // These condense down to:
        //     tot =   0.5 * j * t * TL * TL   + 1.5 * j * t * t * TL   + j * t * t * t;
        // which is a quadratic equation for TL.

        float totalDistance =  0.5 * j * t * TL * TL   + 1.5 * j * t * t * TL   + j * t * t * t;

        if ( totalDistance > halfDistance ) {
            float qa = 0.5 * j * t;
            float qb = 1.5 * j * t * t;
            float qc = j * t * t * t       - halfDistance;
            float x0 = -99999;
            float x1 = -99999;
            gsl_poly_solve_quadratic( qa, qb, qc, &x0, &x1 );
            float bestSolution = max(x0, x1);
            if ( bestSolution >= 0 )
                TL = bestSolution;
        }
    }


    float bothCurvesDistance = (j * t * t * t);    // distance traveled during both concave and convex segments, if no linear phase involved
    if ( bothCurvesDistance > halfDistance ) {
        // Distance required is too short to allow fully performing both curves.
        // Reduce the time allowed in each curve. No linear phases will be used anywhere.

        T = std::cbrt( halfDistance / j );     // this is the bothCurvesDistance equation above, solved for t
        T1 = T2 = T;    // same duration for both concave and convex curves
        TL = 0;

        // recalculate values for end of first segment
        t = T1;
        ps = (j * t * t * t) / 6.0;
        vs = (j * t * t) / 2.0;
        as = (j * t);
    }

    scv_float origin = srcPos;

    // segment 1, concave rising
    rotateSegment c1;
    c1.pos = origin;
    c1.vel = 0;
    c1.acc = 0;
    c1.jerk = j * dir;
    c1.duration = T1;
    r.rotation_segments.push_back(c1);

    // ss,vs,as are already calculated above, no need to change them for this one

    // segment 2, rising linear phase (maybe)
    if ( TL > 0 ) {
        rotateSegment c2;
        c2.pos = origin + ps * dir;
        c2.vel = vs * dir;
        c2.acc = as * dir;
        c2.jerk = 0;
        c2.duration = TL;
        r.rotation_segments.push_back(c2);

        t = TL;
        ps += (vs * t) + (as * t * t) / 2.0;
        vs += (as * t);
    }

    // segment 3, convex rising
    rotateSegment c3;
    c3.pos = origin + ps * dir;
    c3.vel = vs * dir;
    c3.acc = as * dir;
    c3.jerk = -j * dir;
    c3.duration = T2;
    r.rotation_segments.push_back(c3);

//    if ( m.moveType == MT_JOGSTART )
//        return;

    t = T2;
    ps += (vs * t) + ((as * t * t) / 2.0) - ((j * t * t * t) / 6.0);
    vs += ((j * t * t) / 2.0);
    as = 0;

    // segment 4, constant velocity linear phase (maybe)
    float totalRiseDistance = 2 * ps;
    float remainingDistance = len - totalRiseDistance;
    if ( remainingDistance > 0.000001 ) {

        rotateSegment c4;
        c4.pos = origin + ps * dir;
        c4.vel = vs * dir;
        c4.acc = 0;
        c4.jerk = 0;
        c4.duration = remainingDistance / v;
        r.rotation_segments.push_back(c4);

        ps += vs * c4.duration; // a nice simple calculation for a change
    }

//    if ( m.moveType == MT_ESTOP ) {
//        r.rotation_segments.clear();
//        ps = 0;
//    }

    // segment 5, convex falling
    rotateSegment c5;
    c5.pos = origin + ps * dir;
    c5.vel = vs * dir;
    c5.acc = as * dir;
    c5.jerk = -j * dir;
    c5.duration = T2;
    r.rotation_segments.push_back(c5);

    t = T2;
    ps += (vs * t) + ((as * t * t) / 2.0) + (-j * t * t * t) / 6.0;
    vs += (-j * t * t) / 2.0;
    as += (-j * t);

    // segment 6, falling linear phase (maybe)
    if ( TL > 0 ) {
        rotateSegment c6;
        c6.pos = origin + ps * dir;
        c6.vel = vs * dir;
        c6.acc = as * dir;
        c6.jerk = 0;
        c6.duration = TL;
        r.rotation_segments.push_back(c6);

        t = TL;
        ps += (vs * t) + (as * t * t) / 2.0;
        vs += (as * t);
    }

    // segment 7, falling convex
    rotateSegment c7;
    c7.pos = origin + ps * dir;
    c7.vel = vs * dir;
    c7.acc = as * dir;
    c7.jerk = j * dir;
    c7.duration = T1;
    r.rotation_segments.push_back(c7);
}

void planner::addOffsetToMoves(vec3 offset)
{
    if ( cornerBlendMethod == CBM_INTERPOLATED_MOVES ) {
        for (size_t i = 0; i < moves.size(); i++) {
            move& m = moves[i];
            m.src -= offset;
            m.dst -= offset; // has no effect but makes a lot more sense in printMoves
            for (size_t k = 0; k < m.segments.size(); k++) {
                segment& s = m.segments[k];
                s.pos -= offset;
            }
        }
    }
    else {
        for (size_t i = 0; i < segments.size(); i++) {
            segment& s = segments[i];
            s.pos -= offset;
        }
    }
}

void planner::getSegmentState(segment& s, float t, vec3* pos, vec3* vel, vec3* acc, vec3* jerk )
{
    *pos = s.pos + (t * s.vel) + ((t * t) / 2.0) * s.acc + ((t * t * t) / 6.0) * s.jerk;
    *vel = s.vel + (t * s.acc) + ((t * t) / 2.0) * s.jerk;
    *acc = s.acc + t * s.jerk;
    *jerk = s.jerk;
}

void getSegmentPosition(segment& s, float t, scv::vec3* pos )
{
    *pos = s.pos + (t * s.vel) + ((t * t) / 2.0) * s.acc + ((t * t * t) / 6.0) * s.jerk;
}

void getSegmentPosVelAcc(segment &s, float t, vec3 *pos, vec3 *vel, vec3 *acc)
{
    *pos = s.pos + (t * s.vel) + ((t * t) / 2.0) * s.acc + ((t * t * t) / 6.0) * s.jerk;
    *vel = s.vel + (t * s.acc) + ((t * t) / 2.0) * s.jerk;
    *acc = s.acc + t * s.jerk;
}

/*bool planner::getTrajectoryState_constantJerkSegments(float t, int *segmentIndex, vec3 *pos, vec3 *vel, vec3 *acc, vec3 *jerk)
{
    // no segments, return zero vectors
    if ( segments.empty() ) {
        *segmentIndex = -1;
        *pos = vec3_zero;
        *vel = vec3_zero;
        *acc = vec3_zero;
        *jerk = vec3_zero;
        return false;
    }

    // time is negative, return starting point
    if ( t <= 0 ) {
        *pos = segments[0].pos;
        *vel = segments[0].vel;
        *acc = segments[0].acc;
        *jerk = segments[0].jerk;
        return t == 0;
    }

    float totalT = 0;
    size_t segmentInd = 0;
    while (segmentInd < segments.size()) {
        *segmentIndex = segmentInd;
        segment& s = segments[segmentInd];
        float endT = totalT + s.duration;
        if ( t >= totalT && t < endT ) {
            getSegmentState(s, t - totalT, pos, vel, acc, jerk);
            return true;
        }
        segmentInd++;
        totalT = endT;
    }

    // time exceeds total time of trajectory, return end point
    scv::segment& lastSegment = segments[segments.size()-1];
    getSegmentState(lastSegment, lastSegment.duration, pos, vel, acc, jerk);
    return false;
}*/

scv_float planner::getRotationDuration(rotate& r) {
    scv_float t = 0;
    for (size_t k = 0; k < r.rotation_segments.size(); k++) {
        rotateSegment& s = r.rotation_segments[k];
        t += s.duration;
    }
    return t;
}

scv_float planner::getTraverseTime()
{
    scv_float t = 0;

    if ( cornerBlendMethod == CBM_INTERPOLATED_MOVES ) {
        for (size_t i = 0; i < moves.size(); i++) {
            move& m = moves[i];
            scv_float endTime = m.scheduledTime + m.duration;
            if ( endTime > t )
                t = endTime;
        }
    }
    else {
        for (size_t i = 0; i < segments.size(); i++) {
            segment& s = segments[i];
            t += s.duration;
        }
    }

    for (size_t i = 0; i < delayableEvents.size(); i++) {
        delayableEvent& de = delayableEvents[i];
        if ( de.type == DET_ROTATION ) {
            float endTime = de.triggerTime + de.delay + getRotationDuration(de.rot);
            if ( endTime > t )
                t = endTime;
        }
    }

    return t;
}

void planner::resetTraverse()
{
    traversal_totalTime = 0;
    traversal_delayableEventIndex = 0;
    traversal_segmentIndex = 0;
    traversal_segmentTime = 0;

    for (size_t i = 0; i < delayableEvents.size(); i++) {
        delayableEvent& de = delayableEvents[i];
        if ( de.type == DET_ROTATION ) {
            for (size_t k = 0; k < de.rot.rotation_segments.size(); k++) {
                de.rot.rotation_segmentIndex = 0;
                de.rot.rotation_segmentTime = 0;
            }
        }
    }

    memcpy(traversal_rots, startingRotations, sizeof(traversal_rots));

    rotationsInProgress.clear();

    traversal_time = 0;
    //traversal_pos = vec3_zero;
    //traversal_vel = vec3_zero;
    for (size_t i = 0; i < moves.size(); i++) {
        move& m = moves[i];
        m.traversal_segmentIndex = 0;
        m.traversal_segmentTime = 0;
    }
}

scv_float planner::getRotateSegmentPos(rotateSegment &s, float t)
{
    return s.pos + (t * s.vel) + ((t * t) / 2.0) * s.acc + ((t * t * t) / 6.0) * s.jerk;
}

bool planner::advanceRotations(float dt)
{
    bool anyStillGoing = false;

    for (int i = 0; i < (int)rotationsInProgress.size(); i++) {

        rotate* r = rotationsInProgress[i];

        if ( r->rotation_segments.size() < 1 ) {
            continue;
        }

        rotateSegment seg = r->rotation_segments[r->rotation_segmentIndex];

        r->rotation_segmentTime += dt;

        bool foundAnswer = false;

        // Use 'while' here to consume zero-duration (or otherwise very short) segments immediately!
        // It's pretty important to make sure that dt is actually within the next segment instead of
        // just assuming it is, otherwise we might return a location beyond the end of the next
        // segment, and then in the following iteration a location near the start of the following
        // segment, which could potentially reverse the direction of travel!
        while ( ! foundAnswer && r->rotation_segmentTime > seg.duration ) {
            // exceeded current segment
            if ( r->rotation_segmentIndex < ((int)r->rotation_segments.size()-1) ) {
                // more segments remain
                r->rotation_segmentIndex++;
                r->rotation_segmentTime -= seg.duration;
                seg = r->rotation_segments[r->rotation_segmentIndex];
            }
            else {
                // already on final segment
                traversal_rots[r->axis] = getRotateSegmentPos(seg, seg.duration);
                foundAnswer = true;
            }
        }

        if ( ! foundAnswer ) {
            traversal_rots[r->axis] = getRotateSegmentPos(seg, r->rotation_segmentTime);
            anyStillGoing |= true;
        }
    }

    if ( ! anyStillGoing )
        rotationsInProgress.clear();

    return anyStillGoing;
}

bool advanceMoveTraverse(move &m, scv_float dt, vec3 *p, vec3 *v)
{
    if ( m.segments.empty() ) {
        *p = m.src;
        *v = vec3_zero;
        return false;
    }

    m.traversal_segmentTime += dt;
    segment seg = m.segments[m.traversal_segmentIndex];

    // Use 'while' here to consume zero-duration (or otherwise very short) segments immediately!
    // It's pretty important to make sure that dt is actually within the next segment instead of
    // just assuming it is, otherwise we might return a location beyond the end of the next
    // segment, and then in the following iteration a location near the start of the following
    // segment, which could potentially reverse the direction of travel!
    while ( m.traversal_segmentTime > seg.duration ) {
        // exceeded current segment
        if ( m.traversal_segmentIndex < ((int)m.segments.size()-1) ) {
            // more segments remain
            m.traversal_segmentIndex++;
            m.traversal_segmentTime -= seg.duration;
            seg = m.segments[m.traversal_segmentIndex];
        }
        else {
            // already on final segment
            vec3 tmpA;
            getSegmentPosVelAcc(seg, seg.duration, p, v, &tmpA);
            return false;
        }
    }

    vec3 tmpA;
    getSegmentPosVelAcc(seg, m.traversal_segmentTime, p, v, &tmpA);
    return true;
}

bool planner::advanceTraverse(float dt, float speedScale, vec3 *p, vec3 *v, float* rots, traverseFeedback_t *feedback)
{
    feedback->stillRunning = false;
    feedback->digitalOutputBits = 0;
    feedback->digitalOutputChanged = 0;
    initInvalidFloats(feedback->pwmOutput, NUM_PWM_VALS);
    initInvalidFloats(feedback->rotationStarts, NUM_ROTATION_AXES);

    dt *= speedScale;

    traversal_totalTime += dt;

    while ( traversal_delayableEventIndex < (int8_t)delayableEvents.size() ) {
        delayableEvent& de = delayableEvents[traversal_delayableEventIndex];
        if ( traversal_totalTime >= (de.triggerTime + de.delay) ) {
            if ( de.type == DET_DIGITAL_OUTPUT ) {
                feedback->digitalOutputBits = (feedback->digitalOutputBits & ~de.changed) | (de.bits & de.changed);
                feedback->digitalOutputChanged |= de.changed; // or-equals here, to apply all changes if more than one digital output in a row
            }
            else if ( de.type == DET_PWM_OUTPUT ) {
                memcpy(feedback->pwmOutput, de.pwm, sizeof(feedback->pwmOutput));
            }
            else if ( de.type == DET_ROTATION ) {
                feedback->rotationStarts[de.rot.axis] = de.rot.dst;
                rotationsInProgress.push_back( &de.rot );
            }
            traversal_delayableEventIndex++;
        }
        else {
            feedback->stillRunning = true;
            break;
        }
    }

    bool rotationsStillRunning = advanceRotations(dt);
    memcpy(rots, traversal_rots, sizeof(traversal_rots));

    if ( cornerBlendMethod == CBM_INTERPOLATED_MOVES ) {
        bool stillRunning = false;

        vec3 lastSrc = vec3_zero;
        int movesUsed = 0;

        traversal_time += dt;

        for (size_t i = 0; i < moves.size(); i++) {
            move& m = moves[i];

            if ( m.moveType == MT_SYNC )
                continue;

            if ( traversal_time < m.scheduledTime ) {
                stillRunning |= true;
                break;
            }
            if ( traversal_time > m.scheduledTime + m.duration )
                continue;

            vec3 tmpP, tmpV;
            stillRunning |= advanceMoveTraverse(m, dt, &tmpP, &tmpV);

            if ( movesUsed == 0 ) {
                *p = vec3_zero;
                *v = vec3_zero;
            }

            *p += tmpP;
            *v += tmpV;

            lastSrc = m.src;
            movesUsed++;
        }

        if ( movesUsed > 0 ) { // some move was used, point is valid
            //traversal_pos = *p;
            //traversal_vel = *v;
            if ( movesUsed > 1 ) //
                *p -= lastSrc;
        }
        /*else {
            *p = traversal_pos;
            *v = traversal_vel;
        }*/

        return stillRunning | rotationsStillRunning;
    }
    else {
        if ( segments.empty() ) {
            return rotationsStillRunning;
        }

        segment seg = segments[traversal_segmentIndex];

        traversal_segmentTime += dt;

        // Use 'while' here to consume zero-duration (or otherwise very short) segments immediately!
        // It's pretty important to make sure that dt is actually within the next segment instead of
        // just assuming it is, otherwise we might return a location beyond the end of the next
        // segment, and then in the following iteration a location near the start of the following
        // segment, which could potentially reverse the direction of travel!
        while ( traversal_segmentTime > seg.duration ) {
            // exceeded current segment
            if ( traversal_segmentIndex < ((int)segments.size()-1) ) {
                // more segments remain
                traversal_segmentIndex++;
                traversal_segmentTime -= seg.duration;
                seg = segments[traversal_segmentIndex];
            }
            else {
                // already on final segment
                vec3 tmpA;
                getSegmentPosVelAcc(seg, seg.duration, p, v, &tmpA);
                return rotationsStillRunning;
            }
        }

        vec3 tmpA;
        getSegmentPosVelAcc(seg, traversal_segmentTime, p, v, &tmpA);
        return true;
    }
}

scv::vec3 getClosestPointOnInfiniteLine(scv::vec3 line_start, scv::vec3 line_dir, scv::vec3 point, float* d)
{
    *d = scv::dot( point - line_start, line_dir);
    return line_start + *d * line_dir;
}

// This assumes the jerk and acceleration are in the same direction
scv_float calculateDurationFromJerkAndAcceleration(scv::vec3 j, scv::vec3 a)
{
    if ( j.x != 0 )
        return sqrt( a.x / j.x );
    else if ( j.y != 0 )
        return sqrt( a.y / j.y );
    else if ( j.z != 0 )
        return sqrt( a.z / j.z );
    // jerk is zero, so duration would be infinite!
    return 0;
}

void markSkippedSegments(move& l, int whichEnd)
{
    int numSegments = l.segments.size();
    if ( whichEnd == 0 ) { // remove the latter end
        if ( numSegments == 5 ) {
            l.segments[3].toDelete = true;
            l.segments[4].toDelete = true;
        }
        else {
            l.segments[4].toDelete = true;
            l.segments[5].toDelete = true;
            l.segments[6].toDelete = true;
        }
    }
    else { // remove the beginning
        if ( numSegments == 5 ) {
            l.segments[0].toDelete = true;
            l.segments[1].toDelete = true;
        }
        else {
            l.segments[0].toDelete = true;
            l.segments[1].toDelete = true;
            l.segments[2].toDelete = true;
        }
    }
}

void planner::blendCorner(move& m0, move& m1, bool isFirst, bool isLast)
{
    if ( m0.moveType == MT_WAIT || m1.moveType == MT_WAIT )
        return;
    if ( m0.moveType == MT_SYNC || m1.moveType == MT_SYNC )
        return;

    int numPrevSegments = m0.segments.size();
    int numNextSegments = m1.segments.size();

    if ( ! (numPrevSegments == 5 || numPrevSegments == 7) ||
         ! (numNextSegments == 5 || numNextSegments == 7))
        return;

    scv::vec3 m0srcPos = m0.src;
    scv::vec3 m0dstPos = m0.dst;

    scv::vec3 m1srcPos = m1.src;
    scv::vec3 m1dstPos = m1.dst;

    scv::vec3 m0dir = m0dstPos - m0srcPos;
    scv::vec3 m1dir = m1dstPos - m1srcPos;
    m0dir.Normalize();
    m1dir.Normalize();

    // find the constant speed sections in the middle of each move
    segment& seg0 = numPrevSegments == 5 ? m0.segments[2] : m0.segments[3];
    segment& seg1 = numNextSegments == 5 ? m1.segments[2] : m1.segments[3];
    segment& seg2 = numNextSegments == 5 ? m1.segments[3] : m1.segments[4]; // segment after the outgoing linear phase


    scv::vec3 v0 = seg0.vel;
    scv::vec3 v1 = seg1.vel;

    scv::vec3 dv = v1 - v0;
    scv::vec3 jerkDir = dv;
    jerkDir.Normalize();

    scv::vec3 a = jerkDir;
    scv::vec3 j = jerkDir;

    // in each axis, make these vectors too long, then trim them down
    a *= 1.5 * scv::max(accLimit.x, accLimit.y);
    j *= 1.5 * scv::max(jerkLimit.x, jerkLimit.y);

    if ( fabs(a.x) > accLimit.x )
        a *= accLimit.x / fabs(a.x);
    if ( fabs(a.y) > accLimit.y )
        a *= accLimit.y / fabs(a.y);
    if ( fabs(a.z) > accLimit.z )
        a *= accLimit.z / fabs(a.z);

    if ( fabs(j.x) > jerkLimit.x )
        j *= jerkLimit.x / fabs(j.x);
    if ( fabs(j.y) > jerkLimit.y )
        j *= jerkLimit.y / fabs(j.y);
    if ( fabs(j.z) > jerkLimit.z )
        j *= jerkLimit.z / fabs(j.z);

    float amag = a.Length();
    float jmag = j.Length();
    if ( m0.acc < amag )
        a *= m0.acc / amag;
    if ( m0.jerk < jmag )
        j *= m0.jerk / jmag;

    float maxJerkLim = 1; // max allowable jerk for smooth velocity transition

    // at least one component of dv should be non-zero, so use the highest one for this part
    if ( fabs(dv.x) > 0 ) {
        float mjx = (a.x*a.x) / dv.x;
        maxJerkLim = scv::min(maxJerkLim, (float)fabsf(mjx / j.x));
    }
    if ( fabs(dv.y) > 0 ) {
        float mjy = (a.y*a.y) / dv.y;
        maxJerkLim = scv::min(maxJerkLim, (float)fabsf(mjy / j.y));
    }
    if ( fabs(dv.z) > 0 ) {
        float mjz = (a.z*a.z) / dv.z;
        maxJerkLim = scv::min(maxJerkLim, (float)fabsf(mjz / j.z));
    }

    if ( maxJerkLim < 1 ) { // only use this to reduce jerk
        j *= maxJerkLim;
    }


    scv::vec3 earliestStart;
    scv::vec3 latestStart;
    scv::vec3 earliestEnd;
    scv::vec3 latestEnd;

    float maxJerkLength = 0;


    float T = 0;

    scv::vec3 startPoint = 0.5 * (m0srcPos + m0dstPos);
    scv::vec3 endPoint =   0.5 * (m1srcPos + m1dstPos);

    if ( dv.Length() < 0.00001 ) {
        float distance = (endPoint - startPoint).Length();
        T = 0.5 * distance / v0.Length();
    }
    else {
        T = calculateDurationFromJerkAndAcceleration(j, v1-v0);
    }

    bool doubleBack = false;

    float dot = scv::dot(m1dir, m0dir);
    dot = scv::min( 1.0f, scv::max(-1.0f, dot) );
    float angleToTurn = acos( dot );
    if ( angleToTurn < 0.00001 ) {

        // easy case where movement doesn't turn

        float t = T;

        scv::vec3 maxJerkEndPoint = 2 * t * v0    +    (t * t * t) * j;
        maxJerkLength = maxJerkEndPoint.Length();

        segment& seg0After =  numPrevSegments == 5 ? m0.segments[3] : m0.segments[4];

        earliestStart = startPoint;
        latestEnd = endPoint;
        latestStart = seg0After.pos;
        earliestEnd = seg1.pos;

    }
    else if ( angleToTurn > 3.14159 ) {

        // A special annoying case of movement going back in the exact direction it came from.

        float curveSpan = 0; // the furthest point the deceleration curve will reach, measured from the start (or finish) point, whichever is furthest

        float qa = j.Length() / 2.0;
        float qb = 0;
        float qc = -v0.Length();
        float x0 = -99999;
        float x1 = -99999;
        gsl_poly_solve_quadratic( qa, qb, qc, &x0, &x1 );
        float t = scv::max(x0, x1);

        scv::vec3 p0 =      (t * v0) + (( t * t * t) / 6.0) * j; // furthest point reached in first half of reversal
        curveSpan = scv::max(curveSpan, p0.Length());

        qa = j.Length() / 2.0;
        qb = 0;
        qc = -v1.Length();
        x0 = 0;
        x1 = 0;
        gsl_poly_solve_quadratic( qa, qb, qc, &x0, &x1 );
        t = scv::max(x0, x1);

        scv::vec3 p1 = /*sh +*/ (t * v1) /*+ ((t * t) / 2.0) * ah*/ + ((t * t * t) / 6.0) * -j; // furthest point reached in second half of reversal
        curveSpan = scv::max(curveSpan, p1.Length());

        t = T;
        scv::vec3 maxJerkDelta = 2 * t * v0    +    (t * t * t) * j;

        float longestAllowableLength = (startPoint - m0dstPos).Length();
        longestAllowableLength = scv::min(longestAllowableLength, (endPoint - m0dstPos).Length());
        if ( longestAllowableLength == 0 )
            return; // impossible

        float ratio = (curveSpan + maxJerkDelta.Length()) / longestAllowableLength;
        if ( ratio > 1 )
            return; // not enough room

        if ( m1.blendType == CBT_MIN_JERK ) {
            j *= ratio*ratio;

            T = calculateDurationFromJerkAndAcceleration(j, v1-v0);

            curveSpan /= ratio;
            maxJerkDelta *= 1 / ratio;
        }

        scv::vec3 v0dir = v0;
        v0dir.Normalize();
        startPoint = m0dstPos + -curveSpan * v0dir;
        endPoint = startPoint; // will change below

        if ( scv::dot(maxJerkDelta, v0dir) < 0 )
            maxJerkDelta *= -1;
        if ( v0.LengthSquared() > v1.LengthSquared() )
            startPoint += -maxJerkDelta;
        else
            endPoint += -maxJerkDelta;

        doubleBack = true;
    }
    else {

        float t = T;

        scv::vec3 finalVelocity = v0 + t * t * j;
        finalVelocity.Normalize();


        scv::vec3 curveEndPoint = 2 * t * v0    +    (t * t * t) * j;


        // find the usable start and end of each constant speed section
        scv::vec3 seg0Start, seg0End, seg1Start, seg1End;

        seg0Start = 0.5 * (m0srcPos + m0dstPos); // earliest start is at middle of preceding move
        seg0End = m0dstPos; // latest start is at end of preceding move

        seg1Start = m0dstPos;// seg0End; // earliest end is a beginning of following move
        seg1End = 0.5 * (m1srcPos + m1dstPos); // latest end is at middle of following segment

        if ( isFirst && m1.blendType == CBT_MIN_JERK ) {
            if ( m1.blendClearance >= 0 ) {
                float distanceToMid = (seg0Start - m0srcPos).Length();
                float distanceToEarliest = (seg0.pos - m0srcPos).Length();
                float useClearance = max(distanceToEarliest, min(m1.blendClearance, distanceToMid));
                seg0Start = m0srcPos + useClearance * m0dir;
            }
            else
                seg0Start = seg0.pos;
        }
        else if ( isLast && m1.blendType == CBT_MIN_JERK ) {
            if ( m1.blendClearance >= 0 ) {
                float distanceToMid = (seg1End - m1dstPos).Length();
                float distanceToLatest = (seg2.pos - m1dstPos).Length();
                float useClearance = max(distanceToLatest, min(m1.blendClearance, distanceToMid));
                seg1End = m1dstPos - useClearance * m1dir;
            }
            else
                seg1End = seg2.pos;
        }

        scv::vec3 projBase = m0dstPos;

        scv::vec3 curveEndPointNormalized = curveEndPoint;
        curveEndPointNormalized.Normalize();
        scv_float dummy;
        scv::vec3 cpoSpan = getClosestPointOnInfiniteLine( m0srcPos, curveEndPointNormalized, projBase, &dummy);

        scv::vec3 dirForProjection = projBase - cpoSpan;

        dirForProjection.Normalize();

        float A0, A1, B0, B1;

        getClosestPointOnInfiniteLine( projBase, dirForProjection, seg0Start, &A0);
        getClosestPointOnInfiniteLine( projBase, dirForProjection, seg0End, &A1 );
        getClosestPointOnInfiniteLine( projBase, dirForProjection, seg1Start, &B0 );
        getClosestPointOnInfiniteLine( projBase, dirForProjection, seg1End, &B1 );

        float D0 = A1;
        float D1 = B0;

        D0 = A0;
        D1 = B1;

        // ensure the 0 is less than the 1
        if ( A0 > A1 )
            std::swap(A0, A1);
        if ( B0 > B1 )
            std::swap(B0, B1);

        if (( A0 > B0 && A0 > B1) ||
            ( A1 < B0 && A1 < B1)) {
            // no overlap between constant velocity sections
            return;
        }

        float ds[4];
        ds[0] = A0;
        ds[1] = A1;
        ds[2] = B0;
        ds[3] = B1;
        std::sort(std::begin(ds), std::end(ds));

        float inner = ds[1];
        float outer = ds[2];
        if ( fabs(inner) > fabs(outer) )
            std::swap(inner, outer);

        earliestStart = projBase + (outer / D0) * (seg0Start - projBase);
        latestStart =   projBase + (inner / D0) * (seg0Start - projBase);
        earliestEnd =   projBase + (inner / D1) * (seg1End - projBase);
        latestEnd =     projBase + (outer / D1) * (seg1End - projBase);

        maxJerkLength = curveEndPoint.Length();
    }

    float shortestAllowableLength = (latestStart - earliestEnd).Length();  // higher jerk
    float longestAllowableLength = (earliestStart - latestEnd).Length(); // lower jerk

    if ( longestAllowableLength != 0 ) {
        if ( maxJerkLength > (longestAllowableLength + 0.0000001) ) {
            // jerk limit does not allow turning as tight as required
            return;
        }
    }


    if ( doubleBack ) {
        // startPoint and endPoint already determined
    }
    else if ( m1.blendType == CBT_MAX_JERK ) {
        if ( maxJerkLength <= shortestAllowableLength ) {
            // lower jerk to fit shortest allowable curve
            float ratio = maxJerkLength / shortestAllowableLength;
            j *= ratio*ratio;
            T = calculateDurationFromJerkAndAcceleration(j, v1-v0);
            startPoint = latestStart;
            endPoint = earliestEnd;
        }
        else {
            // jerk is already between the limits, just need to position the start correctly
            float f = fabs((maxJerkLength - shortestAllowableLength) / (longestAllowableLength - shortestAllowableLength));
            scv::vec3 midStart = latestStart + f * (earliestStart - latestStart);
            scv::vec3 midEnd = earliestEnd + f * (latestEnd - earliestEnd);
            startPoint = midStart;
            endPoint = midEnd;
        }
    }
    else {
        // lower jerk to match longest corner curve        
        if ( j.LengthSquared() == 0 ) { // a straight-line case where T was already decided, don't change it

        }
        else if ( longestAllowableLength != 0 ) { // a move that doubles back can have a zero length
            float ratio = maxJerkLength / longestAllowableLength;
            j *= ratio*ratio;
            T = calculateDurationFromJerkAndAcceleration(j, v1-v0);
        }

        startPoint = earliestStart;
        endPoint = latestEnd;
    }

    // remove latter part of linear segment of original first line
    float linear0Len = (startPoint - seg0.pos).Length();
    seg0.duration = linear0Len / seg0.vel.Length();

    // remove first part of linear segment of original second line
    float linear1Len = (seg2.pos - endPoint).Length();
    seg1.duration = linear1Len / seg1.vel.Length();
    seg1.pos = endPoint;

    markSkippedSegments(m0, 0);
    markSkippedSegments(m1, 1);

    // update midpoint values
    float t = T;
    scv::vec3 sh =      t * v0 + (( t * t * t) / 6.0) * j;
    scv::vec3 vh = v0 + ((t * t) / 2.0) * j;
    scv::vec3 ah =      t * j;

    segment c0;
    c0.pos = startPoint;
    c0.vel = v0;
    c0.acc = scv::vec3_zero;
    c0.jerk = j;
    c0.duration = T;
    m0.segments.push_back(c0);

    segment c1;
    c1.pos = sh + startPoint;
    c1.vel = vh;
    c1.acc = ah;
    c1.jerk = -j;
    c1.duration = T;
    m0.segments.push_back(c1);

    m0.containsBlend = true;
}

void planner::appendMove(move &m)
{
    if ( m.vel == 0 ) {
        g_log.log(LL_WARN, "Ignoring move with zero velocity");
        return;
    }
    if ( m.acc == 0 ) {
        g_log.log(LL_WARN, "Ignoring move with zero acceleration");
        return;
    }
    if ( m.jerk == 0 ) {
        g_log.log(LL_WARN, "Ignoring move with zero jerk");
        return;
    }

    if ( ! moves.empty() ) {
        move& prevMove = moves.back();
        m.src = prevMove.dst;
    }

    if ( m.src == m.dst ) {
        //g_log.log(LL_WARN, "Ignoring move with no actual position change");
        return;
    }

    moves.push_back(m);
}

void planner::appendWait( vec3& where, float sec)
{
    move m;
    m.moveType = MT_WAIT;
    m.blendType = CBT_NONE;
    m.waitDuration = sec;

    if ( ! moves.empty() ) {
        move& lastMove = moves[moves.size()-1];
        m.src = lastMove.dst;
    }
    else {
        m.src = where;
    }

    m.dst = m.src;
    moves.push_back(m);
}

void planner::appendDigitalOutput(uint16_t bits, uint16_t changed, float delay)
{
    delayableEvent de;
    de.type = DET_DIGITAL_OUTPUT;
    de.moveIndex = moves.size();
    de.bits = bits;
    de.changed = changed;
    de.delay = delay;

    delayableEvents.push_back( de );
}

void planner::appendPWMOutput(float *pwm, float delay)
{
    if ( ! pwm ) {
        g_log.log(LL_ERROR, "appendPWMOutput given null");
        return;
    }

    delayableEvent de;
    de.type = DET_PWM_OUTPUT;
    de.moveIndex = moves.size();
    memcpy(de.pwm, pwm, sizeof(de.pwm));
    de.delay = delay;

    delayableEvents.push_back( de );
}

void planner::appendRotate( rotate& r, float delay )
{
    if ( r.vel == 0 ) {
        g_log.log(LL_WARN, "Ignoring rotate with zero velocity");
        return;
    }
    if ( r.acc == 0 ) {
        g_log.log(LL_WARN, "Ignoring rotate with zero acceleration");
        return;
    }
    if ( r.jerk == 0 ) {
        g_log.log(LL_WARN, "Ignoring rotate with zero jerk");
        return;
    }
    if ( r.src == r.dst ) {
        //g_log.log(LL_WARN, "Ignoring rotate with no actual position change");
        return;
    }

    delayableEvent de;
    de.type = DET_ROTATION;
    de.moveIndex = moves.size();

    de.rot = r;
    de.delay = delay;

    int id = delayableEvents.size();
    de.id = id;

    delayableEvents.push_back( de );
    unsyncedDelayableEvents.push_back( id );
}

void planner::appendSync(vec3& where)
{
    move m;
    m.moveType = MT_SYNC;
    m.blendType = CBT_NONE;
    m.waitDuration = 0;

    if ( ! moves.empty() ) {
        move& lastMove = moves[moves.size()-1];
        m.src = lastMove.dst;
    }
    else {
        m.src = where;
    }

    m.dst = m.src;

    m.eventIndicesToSync = unsyncedDelayableEvents;
    unsyncedDelayableEvents.clear();

    moves.push_back(m);
}

struct actualTriggerAscending
{
    bool operator() (const delayableEvent& d1, const delayableEvent& d2) {
        float actualTrigger1 = d1.triggerTime + d1.delay;
        float actualTrigger2 = d2.triggerTime + d2.delay;
        return actualTrigger1 < actualTrigger2;
    }
};

void planner::collateSegments()
{
    segments.clear();

    size_t eventIndex = 0;    // index in the delayableEvents vector, of the next event to consider
    int eventMoveIndex = -1;  // index in the moves vector, of the move that event matches with

    if ( eventIndex < delayableEvents.size() ) {
        delayableEvent& de = delayableEvents[eventIndex];
        eventMoveIndex = de.moveIndex;
    }

    // Find trigger times for delayable events.
    // These are based on the start time of the move they begin *before*
    float totalDuration = 0;
    for (size_t i = 0; i < moves.size(); i++) {
        move& m = moves[i];

        // if the current move is the one this event matches with, set the trigger time
        // and move to the next event. Multiple events can match with one move, so use
        // 'while' to make sure we get them all
        while ( (int)i == eventMoveIndex ) {
            delayableEvent& de = delayableEvents[eventIndex];

            if ( cornerBlendMethod == CBM_INTERPOLATED_MOVES )
                de.triggerTime = m.scheduledTime;
            else
                de.triggerTime = totalDuration;

            eventIndex++;
            if ( eventIndex < delayableEvents.size() ) {
                delayableEvent& deNext = delayableEvents[eventIndex];
                eventMoveIndex = deNext.moveIndex;
            }
            else
                break;
        }

        if ( cornerBlendMethod == CBM_INTERPOLATED_MOVES )
            totalDuration = max(totalDuration, m.scheduledTime + m.duration);
        else
            totalDuration += m.duration;
    }

    // if there are still events remaining after all moves are done, they will all
    // be triggered together at the end.
    while ( eventIndex < delayableEvents.size() ) {
        delayableEvent& deNext = delayableEvents[eventIndex];
        deNext.triggerTime = totalDuration;
        eventIndex++;
    }

    // Now build the list of segments, checking where we need to insert wait durations for syncs.
    // Each sync has a list of delayable events that must finish before proceeding. The end time
    // of those events can be compared with the current time, to see how much extra time is needed.
    // As we insert extra delays, the end time of subsequent checks must be shifted later in time.

    totalDuration = 0;
    for (size_t i = 0; i < moves.size(); i++) {
        move& m = moves[i];

        if ( m.moveType == MT_SYNC ) {
            //printf("sync at %f\n", totalDuration); fflush(stdout);

            int highestUsedId = 0;

            float extraDelay = 0;
            for (size_t k = 0; k < m.eventIndicesToSync.size(); k++) {
                int id = m.eventIndicesToSync[k];
                highestUsedId = max(highestUsedId, id);
                delayableEvent& de = delayableEvents[id];
                float endTime = de.triggerTime + de.delay + getRotationDuration(de.rot);
                //printf("  et: %f\n", endTime);
                if ( endTime > totalDuration ) {
                    float delay = endTime - totalDuration;
                    if ( delay > extraDelay ) {
                        extraDelay = delay;
                    }
                }
            }
            //printf("  extraDelay: %f\n", extraDelay);

            if ( extraDelay > 0 ) {
                if ( cornerBlendMethod == CBM_INTERPOLATED_MOVES ) {

                    // convert sync to wait and add a segment
                    m.moveType = MT_WAIT;
                    m.duration = extraDelay;

                    segment w1;
                    w1.pos = m.src;
                    w1.vel = scv::vec3_zero;
                    w1.acc = scv::vec3_zero;
                    w1.jerk = scv::vec3_zero;
                    w1.duration = extraDelay;
                    w1.moveType = MT_WAIT;
                    m.segments.push_back(w1);

                    // all subsequent moves need their scheduled time bumped
                    for ( int k = i+1; k < (int)moves.size(); k++ ) {
                        move& sm = moves[k];
                        sm.scheduledTime += extraDelay;
                    }
                }
                else {
                    segment s;
                    s.pos = m.src;
                    s.vel = scv::vec3_zero;
                    s.acc = scv::vec3_zero;
                    s.jerk = scv::vec3_zero;
                    s.duration = extraDelay;
                    segments.push_back(s);
                }

                // shift all subsequent events back by the delay we just added
                for ( int k = highestUsedId+1; k < (int)delayableEvents.size(); k++ ) {
                    delayableEvent &de = delayableEvents[k];
                    de.triggerTime += extraDelay;
                }

                totalDuration += extraDelay;
            }

        }
        else {
            if ( cornerBlendMethod == CBM_INTERPOLATED_MOVES ) {
                float endTime = m.scheduledTime + m.duration;
                if ( endTime > totalDuration )
                    totalDuration = endTime;
            }
            else {
                for (size_t k = 0; k < m.segments.size(); k++) {
                    segment& s = m.segments[k];
                    if ( s.duration > 0 ) {
                        s.moveType = m.moveType;
                        segments.push_back(s);
                        totalDuration += s.duration;
                    }
                }
            }
        }
    }

    // The actual time an event starts is the trigger time plus the delay time. Depending on what
    // the user gave as the delay time, the start could be out of order, so sort by actual start.
    std::sort( delayableEvents.begin(), delayableEvents.end(), actualTriggerAscending() );

}

void planner::setPositionLimits(scv_float lx, scv_float ly, scv_float lz, scv_float ux, scv_float uy, scv_float uz)
{
    posLimitLower = vec3(lx,ly,lz);
    posLimitUpper = vec3(ux,uy,uz);
}

void planner::setVelocityLimits(scv_float x, scv_float y, scv_float z)
{
    velLimit = vec3(x,y,z);
}

void planner::setAccelerationLimits(scv_float x, scv_float y, scv_float z)
{
    accLimit = vec3(x,y,z);
}

void planner::setJerkLimits(scv_float x, scv_float y, scv_float z)
{
    jerkLimit = vec3(x,y,z);
}

void planner::setRotationPositionLimits(int axis, scv_float lower, scv_float upper) {
    rotationPositionLimits[axis].x = lower;
    rotationPositionLimits[axis].y = upper;
}

void planner::setRotationVAJLimits(scv_float vel, scv_float acc, scv_float jerk) {
    rotationVelLimit = vel;
    rotationAccLimit = acc;
    rotationJerkLimit = jerk;
}

void planner::printConstraints()
{
    printf("Planner global constraints:\n");
    printf("  Min pos:  %f, %f, %f\n", posLimitLower.x, posLimitLower.y, posLimitLower.z);
    printf("  Max pos:  %f, %f, %f\n", posLimitUpper.x, posLimitUpper.y, posLimitUpper.z);
    printf("  Max vel:  %f, %f, %f\n", velLimit.x, velLimit.y, velLimit.z);
    printf("  Max acc:  %f, %f, %f\n", accLimit.x, accLimit.y, accLimit.z);
    printf("  Max jerk: %f, %f, %f\n", jerkLimit.x, jerkLimit.y, jerkLimit.z);
    printf("  Rotation max vaj:  %f, %f, %f\n", rotationVelLimit, rotationAccLimit, rotationJerkLimit);
}

void planner::printMoves()
{
    for (size_t i = 0; i < moves.size(); i++) {
        move& l = moves[i];
        printf("  Move %d: (type=%d)\n", (int)i, l.moveType);
        printf("    src: %f, %f, %f\n", l.src.x, l.src.y, l.src.z);
        printf("    dst: %f, %f, %f\n", l.dst.x, l.dst.y, l.dst.z);
        printf("    Vel: %f\n", l.vel);
        printf("    Acc: %f\n", l.acc);
        printf("    Jerk: %f\n", l.jerk);
        printf("    Blend: %s\n", l.blendType==CBT_MAX_JERK ? "max jerk":l.blendType==CBT_MIN_JERK?"min jerk":"none");
        printf("    Scheduled: %f\n", l.scheduledTime);
        printf("    Duration: %f\n", l.duration);
        //printf("    %d, %f, %f, %f\n", (int)i, l.scheduledTime, l.duration, l.scheduledTime + l.duration);
    }
    for (size_t i = 0; i < delayableEvents.size(); i++) {
        delayableEvent& de = delayableEvents[i];
        printf("  Delayable event %d: (type = %d)\n", (int)i, de.type);
        printf("    trigger time: %f\n", de.triggerTime);
        printf("    delay: %f\n", de.delay);
        printf("    actual: %f\n", de.triggerTime + de.delay);
        if ( de.type == DET_DIGITAL_OUTPUT )
            printf("    bits: %d, changed: %d\n", de.bits, de.changed);
        else if ( de.type == DET_PWM_OUTPUT )
            printf("    pwm: %f %f %f %f\n", de.pwm[0], de.pwm[1], de.pwm[2], de.pwm[3]);
        else if ( de.type == DET_ROTATION ) {
            float dur = getRotationDuration(de.rot);
            printf("    rotation: axis: %d, src: %f, dst: %f vel: %f, acc: %f jerk: %f, duration: %f\n", de.rot.axis, de.rot.src, de.rot.dst, de.rot.vel, de.rot.acc, de.rot.jerk, dur);
        }
    }
}

void planner::printSegments()
{
    float totalDuration = 0;
    for (size_t i = 0; i < segments.size(); i++) {
        segment& s = segments[i];
        printf("  Segment %d, start time = %f\n", (int)i, totalDuration);
        printf("    pos : %f, %f, %f\n", s.pos.x, s.pos.y, s.pos.z);
        printf("    vel : %f, %f, %f\n", s.vel.x, s.vel.y, s.vel.z);
        printf("    acc : %f, %f, %f\n", s.acc.x, s.acc.y, s.acc.z);
        printf("    jerk: %f, %f, %f\n", s.jerk.x, s.jerk.y, s.jerk.z);
        printf("    duration: %f\n", s.duration);
        totalDuration += s.duration;
    }
    for (size_t i = 0; i < delayableEvents.size(); i++) {
        delayableEvent& de = delayableEvents[i];
        if ( de.type == DET_ROTATION ) {
            for (size_t k = 0; k < de.rot.rotation_segments.size(); k++) {
                rotateSegment& rs = de.rot.rotation_segments[k];
                printf("  Rotation segment %d-%d\n", (int)i, (int)k);
                printf("    pos : %f\n", rs.pos);
                printf("    vel : %f\n", rs.vel);
                printf("    acc : %f\n", rs.acc);
                printf("    jerk: %f\n", rs.jerk);
                printf("    duration: %f\n", rs.duration);
            }
        }
    }
}

std::vector<segment> &planner::getSegments()
{
    return segments;
}

void planner::getFinalRotations(float *rots)
{
    memcpy(rots, startingRotations, sizeof(startingRotations));
    for (delayableEvent &de : delayableEvents) {
        if ( de.type == DET_ROTATION ) {
            rots[de.rot.axis] = de.rot.dst;
        }
    }
}


} // namespace
