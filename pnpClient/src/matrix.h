#ifndef MATRIX_H
#define MATRIX_H

#include <string.h>
#include "scv/vec3.h"

// A basic 4x4 matrix

class Matrix
{
public:
    float m[16];

    Matrix() {
        identity();
    }

    inline void identity() {
        memset(m, 0, 16*sizeof(float));
        m[ 0] = 1;
        m[ 5] = 1;
        m[10] = 1;
        m[15] = 1;
    }

    inline Matrix operator * (const Matrix& n) {
        Matrix out;
        memset(out.m, 0, 16*sizeof(float));
        for (int i=0;i<4;i++)
                for (int j=0;j<4;j++)
                        for (int k=0;k<4;k++)
                                out.m[i*4+j] += m[k*4+j]*n.m[i*4+k];
        return out;
    }

    inline static Matrix xrotation(float rad) {
        Matrix ret;
        ret.m[ 5] = (float)cos(rad);
        ret.m[ 6] = (float)sin(rad);
        ret.m[ 9] = (float)-sin(rad);
        ret.m[10] = (float)cos(rad);
        return ret;
    }

    inline static Matrix yrotation(float rad) {
        Matrix ret;
        ret.m[ 0] = (float)cos(rad);
        ret.m[ 2] = (float)-sin(rad);
        ret.m[ 8] = (float)sin(rad);
        ret.m[10] = (float)cos(rad);
        return ret;
    }

    inline static Matrix zrotation(float rad) {
        Matrix ret;
        ret.m[ 0] = (float)cos(rad);
        ret.m[ 1] = (float)sin(rad);
        ret.m[ 4] = (float)-sin(rad);
        ret.m[ 5] = (float)cos(rad);
        return ret;
    }

    inline void transform(scv::vec3& v) const {
        float m_t_0 = v.x*m[0] + v.y*m[4] + v.z*m[8] + m[12];
        float m_t_1 = v.x*m[1] + v.y*m[5] + v.z*m[9] + m[13];
        float m_t_2 = v.x*m[2] + v.y*m[6] + v.z*m[10] + m[14];
        v.x = m_t_0;
        v.y = m_t_1;
        v.z = m_t_2;
    }
};

#endif
