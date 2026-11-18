#include "CircuitCore.h"
#ifdef __wasm__
extern "C"
{
    double sin(double);
    double cos(double);
    double atan2(double, double);
    double atan(double);
    double exp(double);
    double fmod(double, double);
}
#define ROOT __builtin_sqrt
#else
#include <cmath>
using std::atan;
using std::atan2;
using std::cos;
using std::exp;
using std::fmod;
using std::sin;
#define ROOT std::sqrt
#endif
namespace Circuit
{
namespace
{
struct Point
{
    double x, y, z;
};
constexpr Point Control[] = {
    {0, 0, 26},       {380, 0, 27},     {745, -65, 32},   {990, -330, 47},   {1040, -660, 63},  {930, -930, 78},
    {690, -1125, 85}, {385, -1175, 75}, {140, -1060, 55}, {-100, -1130, 39}, {-380, -1300, 30}, {-645, -1200, 34},
    {-802, -951, 51}, {-824, -630, 67}, {-712, -395, 61}, {-510, -337, 46},  {-358, -165, 33},  {-220, -22, 26}};
constexpr int Nodes = sizeof(Control) / sizeof(Point), Count = Nodes * 100;
Point Path[Count];
double Starts[Count], Distance = 0;
double limit(double v, double lo, double hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}
double wrap(double v, double n)
{
    double d = fmod(v, n);
    return d < 0 ? d + n : d;
}
double spline(double a, double b, double c, double d, double t)
{
    return .5 * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t * t + (-a + 3 * b - 3 * c + d) * t * t * t);
}
void Init()
{
    if (Distance > 0)
        return;
    for (int i = 0; i < Nodes; i++)
    {
        const auto a = Control[(i + Nodes - 1) % Nodes], b = Control[i], c = Control[(i + 1) % Nodes],
                   d = Control[(i + 2) % Nodes];
        for (int j = 0; j < 100; j++)
        {
            const double t = j / 100.;
            Path[i * 100 + j] = {spline(a.x, b.x, c.x, d.x, t), spline(a.y, b.y, c.y, d.y, t),
                                 spline(a.z, b.z, c.z, d.z, t)};
        }
    }
    double total = 0;
    for (int i = 0; i < Count; i++)
    {
        Starts[i] = total;
        const auto a = Path[i], b = Path[(i + 1) % Count];
        double x = b.x - a.x, y = b.y - a.y, z = b.z - a.z;
        total += ROOT(x * x + y * y + z * z);
    }
    Distance = total;
}
Point Sample(double at)
{
    Init();
    double s = wrap(at, Distance);
    int lo = 0, hi = Count - 1;
    while (lo < hi)
    {
        int m = (lo + hi + 1) / 2;
        if (Starts[m] > s)
            hi = m - 1;
        else
            lo = m;
    }
    const auto a = Path[lo], b = Path[(lo + 1) % Count];
    double t = (s - Starts[lo]) / ((lo + 1 < Count ? Starts[lo + 1] : Distance) - Starts[lo]);
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}
} // namespace
double Length()
{
    Init();
    return Distance;
}
Frame At(double s, double offset)
{
    const auto a = Sample(s - 4), b = Sample(s), c = Sample(s + 4);
    const double dx = c.x - a.x, dy = c.y - a.y, n = ROOT(dx * dx + dy * dy);
    const double fx = dx / n, fy = dy / n, ax = b.x - a.x, ay = b.y - a.y, bx = c.x - b.x, by = c.y - b.y;
    const double k = atan2(ax * by - ay * bx, ax * bx + ay * by) / 4, bank = limit(k * 22, -.14, .14);
    return {b.x - fy * offset,
            b.y + fx * offset,
            b.z - offset * sin(bank),
            fx,
            fy,
            -fy,
            fx,
            atan2(fy, fx),
            atan2(c.z - a.z, n),
            bank,
            k};
}
Car Traffic(int i, double time)
{
    const bool truck = i % 4 == 0;
    const double v = truck ? 18.5 : 22. + i % 5;
    const double start = i == 0 ? 108 : i * Length() / TrafficCount + 71 * (i % 3);
    return {wrap(start + v * time, Length()),
            i % 2 ? -2.85 : 2.85,
            v,
            truck ? 6.5 : 2.4,
            truck ? 1.25 : 1.06,
            truck ? 1. : 0.};
}
Ride Start()
{
    Ride r{};
    r.lateral = 2.65;
    r.charge = 1;
    r.gear = 1;
    r.rpm = .26;
    r.crashAge = -1;
    return r;
}
CrashPose Ejection(const Ride &r)
{
    const double t = limit(r.crashAge, 0, 3.2);
    const double air = limit(t, 0, 1.08), slide = limit(t - 1.08, 0, 2.12);
    const double travel = r.crashSpeed * (.76 * air + .48 * (1 - exp(-1.8 * slide)));
    const double height = limit(.82 + 4.3 * air - 4.905 * air * air, .20, 2.2);
    const double roll = r.crashSide * (.14 + 2.1 * air + 1.3 * (1 - exp(-2.2 * slide)));
    return {wrap(r.crashStart + travel, Length()),
            limit(r.lateral + r.crashSide * (.7 * air + 1.5 * (1 - exp(-1.8 * slide))), -RideLimit, RideLimit),
            height,
            -.32 - 1.18 * air + .32 * (1 - exp(-slide)),
            roll,
            r.crashSide * .3 * t,
            r.crashSide * limit(t * 2.55, 0, 1.32),
            5.5 + 3.2 * (1 - exp(-2 * t)),
            2.4 + 1.5 * (1 - exp(-1.5 * t))};
}
void Advance(Ride &p, double gas, double brake, double steer, bool request, double dt)
{
    if (p.crashAge >= 0)
    {
        p.elapsed += dt;
        p.crashAge += dt;
        p.v *= exp(-2.45 * dt);
        p.s = wrap(p.s + p.v * dt, Length());
        p.boost = 0;
        p.sideV = 0;
        p.rpm = .22;
        if (p.crashAge >= 3.2)
        {
            p.s = Ejection(p).riderS;
            p.lateral = p.lateral >= 0 ? -2.65 : 2.65;
            // Give the recovering bike space even when traffic is passing the impact point.
            for (int i = 0; i < TrafficCount; i++)
            {
                const auto c = Traffic(i, p.elapsed);
                const double gap = wrap(c.s - p.s + Length() * .5, Length()) - Length() * .5;
                if (gap < 15 && gap > -15 && c.lane * p.lateral > 0)
                    p.s = wrap(c.s - c.half - 14., Length());
            }
            p.v = 0;
            p.lean = 0;
            p.steer = 0;
            p.gear = 1;
            p.crashAge = -1;
            p.impact = 0;
        }
        return;
    }
    const auto f = At(p.s, p.lateral);
    gas = limit(gas, 0, 1);
    brake = limit(brake, 0, 1);
    if (!request && p.charge >= .18)
        p.locked = 0;
    p.boost = request && !p.locked && p.charge > 0 && brake < .05;
    if (p.boost)
    {
        p.charge = limit(p.charge - dt / 3.8, 0, 1);
        p.cool = 1.35;
        if (p.charge == 0)
            p.locked = 1;
    }
    else
    {
        p.cool = limit(p.cool - dt, 0, 1.35);
        if (p.cool == 0)
            p.charge = limit(p.charge + dt / 8.5, 0, 1);
    }
    const double old = p.v, drag = .6 + .00185 * old * old;
    const double drive = gas * (9.6 - .031 * old) + (p.boost ? 13.6 : 0) - brake * 19.5 - drag - 9.81 * sin(f.grade);
    p.v = limit(old + drive * dt, 0, p.boost ? 82 : (old - .9 * dt > 63 ? old - .9 * dt : 63));
    p.steer += (limit(steer, -1, 1) - p.steer) * (1 - exp(-7 * dt));
    const double sv = p.sideV;
    p.sideV += (p.steer * limit(p.v * .38, 0, 8.2) - p.sideV) * (1 - exp(-4.2 * dt));
    p.lateral += p.sideV * dt;
    if (p.lateral > RideLimit || p.lateral < -RideLimit)
    {
        p.lateral = limit(p.lateral, -RideLimit, RideLimit);
        p.v = limit(p.v - 6 * dt, 0, 82);
        p.sideV = 0;
    }
    p.s += (old + p.v) * .5 * dt;
    p.impact = limit(p.impact - dt, 0, 1);
    p.elapsed += dt;
    for (int i = 0; i < TrafficCount; i++)
    {
        auto c = Traffic(i, p.elapsed);
        double gap = wrap(c.s - p.s + Distance * .5, Distance) - Distance * .5;
        if (gap > -c.half - .9 && gap < c.half + 1.1 && p.lateral > c.lane - c.width - .43 &&
            p.lateral < c.lane + c.width + .43 && p.v > c.v)
        {
            p.s -= c.half + 1.12 - gap;
            p.impact = .32;
            p.boost = 0;
            if (p.v - c.v >= 13.0)
            {
                p.crashAge = 0;
                p.crashSpeed = p.v;
                p.crashStart = wrap(p.s, Distance);
                p.crashSide = p.lateral < c.lane ? -1. : 1.;
                p.v *= .56;
                p.cool = 1.35;
            }
            else
                p.v = c.v * .84;
            break;
        }
    }
    if (p.s >= Distance)
    {
        p.s -= Distance;
        p.laps++;
    }
    else if (p.s < 0)
        p.s += Distance;
    double target = limit(atan((p.v * p.v * f.curvature + (p.sideV - sv) / dt) / 9.81), -.91, .91);
    p.lean += (target - p.lean) * (1 - exp(-5 * dt));
    const double gears[] = {0, 12, 23, 35, 47, 58, 84};
    int g = 1;
    while (g < 6 && p.v > gears[g])
        g++;
    p.gear = g;
    p.rpm = limit(.26 + .71 * (p.v - gears[g - 1]) / (gears[g] - gears[g - 1]), .16, 1);
}
} // namespace Circuit
#ifdef __wasm__
namespace
{
Circuit::Ride Bike;
Circuit::Frame Pose;
Circuit::Car Vehicle;
Circuit::CrashPose Eject;
} // namespace
extern "C"
{
    double loop_length()
    {
        return Circuit::Length();
    }
    Circuit::Ride *reset_ride()
    {
        Bike = Circuit::Start();
        return &Bike;
    }
    Circuit::Frame *course_frame(double s, double l)
    {
        Pose = Circuit::At(s, l);
        return &Pose;
    }
    Circuit::Car *traffic_car(int i, double t)
    {
        Vehicle = Circuit::Traffic(i, t);
        return &Vehicle;
    }
    Circuit::CrashPose *crash_pose()
    {
        Eject = Circuit::Ejection(Bike);
        return &Eject;
    }
    void drive(double g, double b, double s, int boost, double dt)
    {
        Circuit::Advance(Bike, g, b, s, boost != 0, dt);
    }
}
#endif
