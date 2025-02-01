#ifndef CAMERA_H
#define CAMERA_H

#include "scv/vec3.h"
#include "matrix.h"

// A basic camera for FPS game style movement

#define DEGTORAD 0.01745329252
#define RADTODEG 57.2957795131

class Camera {
public:
    float yaw, pitch;

    scv::vec3 right;
    scv::vec3 up;
    scv::vec3 forward;

    scv::vec3 location;
    scv::vec3 focuspoint;

    Camera()
    {
        setLocation(10, 10, 10);
        setDirection(45, -45);
    }

    void setLocation(float x, float y, float z)
    {
        location = scv::vec3(x,y,z);
    }

    void setDirection(float _yaw, float _pitch)
    {
        yaw = _yaw;
        pitch = _pitch;
        if ( pitch > 80 )
            pitch = 80;
        else if ( pitch < -80 )
            pitch = -80;

        right =   scv::vec3(1, 0, 0);
        forward = scv::vec3(0, 1, 0);
        up =      scv::vec3(0, 0, 1);
        Matrix t = Matrix::zrotation(-yaw*DEGTORAD) * Matrix::xrotation(pitch*DEGTORAD);
        t.transform(forward);
        t.transform(up);
        t.transform(right);
    }

    void translate(float _right, float _forward, float _up)
    {
        location += _right * right;
        location += _forward * forward;
        location += _up * up;
    }

    void gluLookAt()
    {
        focuspoint = location + forward;
        ::gluLookAt(
            location.x, location.y, location.z,
            focuspoint.x, focuspoint.y, focuspoint.z,
            0, 0, 1
        );
    }
};


#endif
