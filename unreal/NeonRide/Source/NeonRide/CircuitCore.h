#pragma once
// Coordinates are metres. X is forward at the start, Y is right, Z is elevation.
namespace Circuit
{
struct Frame
{
    double x, y, z, fx, fy, rx, ry, yaw, grade, bank, curvature;
};
struct CrashBody
{
    double s, lateral, height, forwardV, sideV, upV;
    double pitch, roll, yaw, pitchRate, rollRate, yawRate;
    double ground, hit;
};
struct Ride
{
    double s, lateral, v, sideV, lean, steer, charge, boost, locked, cool, laps, gear, rpm, impact, elapsed, crashAge,
        crashSpeed, crashSide, crashStart, headingError, severity, firstLanding;
    CrashBody riderBody, bikeBody;
};
struct CrashPose
{
    double riderS, riderLateral, riderHeight, riderPitch, riderRoll, riderYaw, bikeRoll, cameraDistance, cameraHeight,
        bikeS, bikeLateral, bikeHeight, bikePitch, bikeYaw, riderGround, bikeGround, riderHit, bikeHit, riderSpeed,
        bikeSpeed, severity, clipTime;
};
struct Car
{
    double s, lane, v, half, width, truck;
};
constexpr double HalfWidth = 6.55, RideLimit = 6.12;
constexpr int TrafficCount = 14;
double Length();
Frame At(double distance, double lateral = 0);
Car Traffic(int index, double time);
Ride Start();
CrashPose Ejection(const Ride &r);
void Advance(Ride &r, double gas, double brake, double steer, bool boost, double dt);
} // namespace Circuit
