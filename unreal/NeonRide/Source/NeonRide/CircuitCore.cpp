#include "CircuitCore.h"
#include "CourierCore.h"
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
    if (Courier::RoadOf(s)) return Courier::DistrictFrame(s, offset);
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
    if (i >= 14) {
        const int local = i - 14;
        const double speed = 12. + local % 4;
        return {Courier::TrafficPosition(local, 170. + 171. * local + time * speed), local % 2 ? -2.85 : 2.85, speed, truck ? 6.5 : 2.4, truck ? 1.25 : 1.06, truck ? 1. : 0.};
    }
    const double start = i == 0 ? 108 : i * Length() / 14 + 71 * (i % 3);
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
namespace
{
constexpr double Pi = 3.141592653589793;
double absolute(double x)
{
    return x < 0 ? -x : x;
}
double magnitude(double x, double y)
{
    return ROOT(x * x + y * y);
}
double angle(double radians)
{
    return wrap(radians + Pi, 2. * Pi) - Pi;
}
double signedDistance(double x)
{
    return wrap(x + Length() * .5, Length()) - Length() * .5;
}

// Sweep the moving body against a vehicle in that vehicle's frame. The first
// entering slab identifies the impact face even when a frame crosses a corner.
struct Contact
{
    double time, nx, ny;
};
bool slab(double pos, double delta, double half, double &enter, double &leave)
{
    if (absolute(delta) < 1.e-10)
        return absolute(pos) <= half;
    const double a = (-half - pos) / delta, b = (half - pos) / delta;
    enter = a < b ? a : b;
    leave = a > b ? a : b;
    return true;
}
bool carContact(double oldS, double oldL, double ds, double dl, const Car &car, double dt, double front, double side,
                Contact &hit)
{
    if (Courier::RoadOf(oldS) != Courier::RoadOf(car.s)) return false;
    const double x = Courier::DistanceBetween(oldS, car.s) + car.v * dt, y = oldL - car.lane;
    const double dx = ds - car.v * dt, hx = car.half + front, hy = car.width + side;
    double ex = -1.e20, ey = -1.e20, lx = 1.e20, ly = 1.e20;
    if (!slab(x, dx, hx, ex, lx) || !slab(y, dl, hy, ey, ly))
        return false;
    const double enter = ex > ey ? ex : ey, leave = lx < ly ? lx : ly;
    if (enter > leave || leave < 0. || enter > 1.)
        return false;
    const bool longitudinal = enter < 0. ? hx - absolute(x) < hy - absolute(y) : ex >= ey;
    hit.time = limit(enter, 0., 1.);
    hit.nx = longitudinal ? (x + dx * hit.time < 0. ? -1. : 1.) : 0.;
    hit.ny = longitudinal ? 0. : (y + dl * hit.time < 0. ? -1. : 1.);
    return dx * hit.nx + dl * hit.ny < -1.e-7;
}
void launch(Ride &p, double forward, double lateral, double nx, double ny, double surfaceSpeed, double closing,
            double side)
{
    p.crashAge = 0.;
    p.crashSpeed = p.v;
    p.crashSide = side;
    p.crashStart = p.s;
    p.firstLanding = -1.;
    p.severity = limit((closing + 0.16 * p.v - 7.) / 38., .14, 1.);
    p.impact = .32;
    p.boost = 0.;
    p.cool = 1.35;
    const double severity = p.severity;
    const double relative = (forward - surfaceSpeed) * nx + lateral * ny;
    auto &rider = p.riderBody;
    auto &bike = p.bikeBody;
    rider = {};
    bike = {};
    rider.s = bike.s = p.s;
    rider.lateral = bike.lateral = p.lateral;
    rider.height = .96;
    bike.height = .54;
    rider.forwardV = forward - relative * .43 * nx;
    rider.sideV = lateral - relative * .65 * ny + side * (1.4 + severity * 2.8);
    bike.forwardV = forward - relative * 1.12 * nx;
    bike.sideV = lateral - relative * 1.12 * ny - side * (.6 + severity);
    rider.upV = 2.6 + severity * 4.4;
    bike.upV = .7 + 2.2 * severity;
    rider.yaw = bike.yaw = p.headingError;
    rider.roll = bike.roll = p.lean;
    rider.pitchRate = -(2.6 + severity * 3.1) * (.68 + .32 * absolute(nx));
    rider.rollRate = side * (1. + severity * 2.9 + absolute(ny));
    rider.yawRate = side * (.45 + severity * 1.1);
    bike.pitchRate = -(.8 + severity * 3.5) * nx;
    bike.rollRate = -side * (2.8 + severity * 4.1);
    bike.yawRate = -side * (.8 + 2.5 * severity);
}
void advanceBody(CrashBody &b, const Ride &p, bool rider, double dt)
{
    const Frame f = At(b.s, b.lateral);
    const double oldS = b.s, oldL = b.lateral;
    const double speed = magnitude(b.forwardV, b.sideV);
    // Wet sliding friction acts opposite the velocity vector. Airborne bodies
    // retain their inertial heading as the road's tangent rotates beneath them.
    const double friction = b.ground ? (rider ? 5.9 : 4.2) : 0.;
    const double damping = limit(1. - dt * (friction / (speed + .05) + .0018 * speed), 0., 1.);
    const double rotation = f.curvature * b.forwardV * dt;
    b.forwardV = b.forwardV * damping + rotation * b.sideV - 9.81 * sin(f.grade) * dt;
    b.sideV = b.sideV * damping - rotation * b.forwardV + 9.81 * sin(f.bank) * dt;
    b.upV -= 9.81 * dt;
    b.hit = limit(b.hit - 3.5 * dt, 0., 1.);
    b.pitch += b.pitchRate * dt;
    b.roll += b.rollRate * dt;
    b.yaw += b.yawRate * dt - rotation;
    b.height += b.upV * dt;
    const double ds = b.forwardV * dt / limit(1. - f.curvature * b.lateral, .75, 1.25), dl = b.sideV * dt;
    b.s = Courier::Move(oldS, ds, oldL, false, false);
    b.lateral = oldL + dl;
    const double radius = rider ? .32 : .52;
    const double edge = HalfWidth - radius - .12;
    if (absolute(b.lateral) > edge)
    {
        const double sign = b.lateral < 0. ? -1. : 1.;
        b.lateral = sign * edge;
        if (b.sideV * sign > 0.)
        {
            const double impact = b.sideV * sign;
            b.sideV *= -.27;
            b.forwardV *= .92;
            b.rollRate -= sign * impact * .24;
            b.yawRate += sign * impact * .17;
            b.hit = limit(b.hit + impact / 13., 0., 1.);
        }
    }
    for (int i = 0; i < TrafficCount; ++i)
    {
        const auto car = Traffic(i, p.elapsed);
        if (b.height - radius > (car.truck ? 3.25 : 1.48))
            continue;
        Contact hit{};
        if (!carContact(oldS, oldL, ds, dl, car, dt, radius, radius, hit))
            continue;
        const double normalV = (b.forwardV - car.v) * hit.nx + b.sideV * hit.ny;
        if (normalV >= 0.)
            continue;
        const double impulse = -normalV * (rider ? 1.16 : 1.08);
        if (hit.nx)
            b.s = Courier::Move(car.s, hit.nx * (car.half + radius + .015), oldL, false, false);
        if (hit.ny)
            b.lateral = car.lane + hit.ny * (car.width + radius + .015);
        b.forwardV += hit.nx * impulse;
        b.sideV += hit.ny * impulse;
        b.pitchRate += hit.nx * impulse * .09;
        b.rollRate -= hit.ny * impulse * .13;
        b.hit = limit(b.hit + impulse / 22., 0., 1.);
        break;
    }
    const double support = rider ? .30 + .10 * absolute(cos(b.roll))
                                 : .26 + .26 * absolute(cos(b.roll) * cos(b.pitch)) + .24 * absolute(sin(b.pitch));
    b.ground = 0.;
    if (b.height <= support)
    {
        b.height = support;
        const double impact = limit(-b.upV, 0., 25.);
        b.upV = impact > 1.35 ? impact * (rider ? .21 : .29) : 0.;
        b.ground = b.upV == 0.;
        if (impact > 1.35)
        {
            b.pitchRate *= .58;
            b.rollRate *= .68;
            b.yawRate += b.sideV * .06;
            b.forwardV *= .81;
            b.sideV *= .77;
            b.hit = limit(b.hit + impact / 10., 0., 1.);
        }
        if (b.ground)
        {
            const double decay = exp(-dt * (rider ? 3.6 : 2.4));
            if (rider)
            {
                // Ground support under a tucked rider is unstable upright.
                // Gravity rolls that pose onto the nearer shoulder during the slide.
                const double tilt = sin(b.roll);
                const double side = absolute(tilt) > .08 ? (tilt < 0. ? -1. : 1.) : p.crashSide;
                const double upright = absolute(cos(b.pitch) * cos(b.roll));
                b.rollRate += angle(side * 1.50 - b.roll) * upright * 9. * dt;
            }
            else
            {
                b.rollRate += angle((b.roll < 0. ? -1. : 1.) * 1.41 - b.roll) * 10. * dt;
                b.pitchRate += angle(-b.pitch) * 14. * dt;
            }
            b.pitchRate *= decay;
            b.rollRate *= decay;
            b.yawRate *= exp(-1.7 * dt);
        }
    }
}
void recover(Ride &p)
{
    const double lane = p.crashSide < 0. ? 2.65 : -2.65;
    const auto laps = p.laps, time = p.elapsed, charge = p.charge;
    double s = p.riderBody.s;
    // Search both lanes for a full gap ahead AND behind the stationary bike.
    double chosen = lane;
    bool clear = false;
    for (int step = 0; step < 22 && !clear; ++step)
    {
        s = Courier::Move(p.riderBody.s, -15. * (step / 2), lane, false, false);
        chosen = step % 2 ? -lane : lane;
        clear = true;
        for (int i = 0; i < TrafficCount; ++i)
        {
            const auto c = Traffic(i, time);
            const double d = Courier::DistanceBetween(c.s, s);
            if (chosen * c.lane > 0. && d > -(c.half + 34.) && d < c.half + 16.)
            {
                clear = false;
                break;
            }
        }
    }
    p = Start();
    p.s = s;
    p.lateral = chosen;
    p.elapsed = time;
    p.laps = laps;
    p.charge = charge;
}
} // namespace
CrashPose Ejection(const Ride &r)
{
    const auto &a = r.riderBody;
    const auto &b = r.bikeBody;
    const double t = limit(r.crashAge, 0., 6.8);
    const double clip = r.firstLanding < 0. ? limit(t * .85, 0., .97) : limit(1. + (t - r.firstLanding) * .86, 1., 3.2);
    return {a.s,
            a.lateral,
            a.height,
            a.pitch,
            a.roll,
            a.yaw,
            b.roll,
            5.5 + 2.2 * r.severity,
            2.6 + .65 * r.severity,
            b.s,
            b.lateral,
            b.height,
            b.pitch,
            b.yaw,
            a.ground,
            b.ground,
            a.hit,
            b.hit,
            magnitude(a.forwardV, a.sideV),
            magnitude(b.forwardV, b.sideV),
            r.severity,
            clip};
}
void Advance(Ride &p, double gas, double brake, double steer, bool request, double dt, bool rampOpen)
{
    if (dt <= 0.)
        return;
    if (p.crashAge >= 0)
    {
        p.elapsed += dt;
        p.crashAge += dt;
        advanceBody(p.riderBody, p, true, dt);
        advanceBody(p.bikeBody, p, false, dt);
        if (p.firstLanding < 0. && p.riderBody.height < .42 && p.riderBody.upV >= 0.)
            p.firstLanding = p.crashAge;
        p.s = p.bikeBody.s;
        p.lateral = p.bikeBody.lateral;
        p.v = magnitude(p.bikeBody.forwardV, p.bikeBody.sideV);
        p.rpm = .22;
        p.boost = 0.;
        if ((p.crashAge >= 3.5 && p.riderBody.ground && p.bikeBody.ground && p.v < 5.5 &&
             magnitude(p.riderBody.forwardV, p.riderBody.sideV) < 4.) ||
            p.crashAge >= 6.8)
            recover(p);
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
    // Integrate yaw in world space. The course frame is used only to express
    // position and contact, never as a steering request.
    const double input = limit(steer, -1., 1.);
    p.steer += (input - p.steer) * (1. - exp(-12. * dt));
    const double yawRate = (input == 0. ? 0. : p.steer) * limit(21. / (p.v + 2.), 0., limit(p.v * .15, 0., 1.12));
    const double yaw = f.yaw + p.headingError, halfHeading = p.headingError + yawRate * dt * .5;
    const double avgSpeed = .5 * (old + p.v);
    const double forward = avgSpeed * cos(halfHeading), lateral = avgSpeed * sin(halfHeading);
    const double ds = forward * dt / limit(1. - f.curvature * p.lateral, .75, 1.25);
    const double dl = lateral * dt;
    const double oldS = p.s, oldL = p.lateral;
    p.s = Courier::Move(oldS, ds, oldL + dl, true, rampOpen);
    p.lateral = oldL + dl;
    p.sideV = lateral;
    p.headingError = angle(yaw + yawRate * dt - At(p.s).yaw);
    p.elapsed += dt;
    p.impact = limit(p.impact - dt, 0., 1.);

    double when = 2.;
    int obstacle = -1;
    Contact first{};
    const double front = .93 * absolute(cos(halfHeading)) + .38 * absolute(sin(halfHeading));
    const double sideExtent = .38 * absolute(cos(halfHeading)) + .93 * absolute(sin(halfHeading));
    for (int i = 0; i < TrafficCount; ++i)
    {
        const auto c = Traffic(i, p.elapsed);
        Contact hit{};
        if (carContact(oldS, oldL, ds, dl, c, dt, front, sideExtent, hit) && hit.time < when)
        {
            obstacle = i;
            first = hit;
            when = hit.time;
        }
    }
    // A broadside motorcycle reaches the parapet sooner than one parallel to it.
    const double railLimit = limit(HalfWidth - .05 - sideExtent, 0., RideLimit);
    if (absolute(p.lateral) >= railLimit && absolute(dl) > 1.e-10)
    {
        const double side = p.lateral < 0. ? -1. : 1.;
        const double railTime = limit((side * railLimit - oldL) / dl, 0., 1.);
        if (side * dl > 0. && railTime < when)
        {
            p.lateral = side * railLimit;
            if (absolute(lateral) > 6.4 && p.v > 14.)
            {
                p.s = Courier::Move(oldS, ds * railTime, oldL, true, rampOpen);
                launch(p, forward, lateral, 0., -side, 0., absolute(lateral), -side);
                return;
            }
            // A shallow scrape dissipates energy without turning the handlebars.
            p.v = limit(p.v - (3.5 + 34. * absolute(sin(halfHeading))) * dt, 0., 82.);
            p.sideV = 0.;
            obstacle = -1;
        }
    }
    if (obstacle >= 0)
    {
        const auto c = Traffic(obstacle, p.elapsed);
        const double closing = -((forward - c.v) * first.nx + lateral * first.ny);
        p.s = Courier::Move(oldS, ds * when, oldL, true, rampOpen);
        p.lateral = oldL + dl * when;
        if (first.nx)
            p.s = Courier::Move(c.s, first.nx * (c.half + front + .018), oldL, false, false);
        if (first.ny)
            p.lateral = c.lane + first.ny * (c.width + sideExtent + .018);
        const double side = first.ny ? first.ny : (p.lateral < c.lane ? -1. : 1.);
        if (closing >= (first.ny ? 6.5 : 13.))
        {
            launch(p, forward, lateral, first.nx, first.ny, c.v, closing, side);
            return;
        }
        if (closing > 0.)
        {
            p.v = limit(p.v - closing * (first.nx < 0. ? 1.1 : .65), 0., 82.);
            p.impact = .32;
            p.boost = 0.;
        }
    }
    if (Courier::RoadOf(oldS)==0 && Courier::RoadOf(p.s)==0 && ds > 0. && oldS > Distance * .75 && p.s < Distance * .25)
        p.laps++;
    const double target = limit(atan(p.v * yawRate / 9.81), -.91, .91);
    p.lean += (target - p.lean) * (1. - exp(-5. * dt));
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
Courier::Shift Duty;
Courier::Route Guidance;
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
        Circuit::Advance(Bike, g, b, s, boost != 0, dt, Courier::RampOpen(Duty));
    }
    double road_info(int id, int field)
    {
        if(field==0)return Courier::RoadStart(id);
        if(field==1)return Courier::RoadLength(id);
        if(field==2)return Courier::Entry(id);
        return Courier::Exit(id);
    }
    const char *road_name(int id) {return Courier::RoadName(id);}
    const char *stop_label(int id,int field)
    {
        const auto &v=Courier::Stop(id);return field==0?v.name:field==1?v.client:v.district;
    }
    double stop_position(int id){return Courier::Stop(id).s;}
    const char *job_label(int id){return Courier::Contract(id,Duty.number).parcel;}
    int job_info(int id,int field){auto j=Courier::Contract(id,Duty.number);return field==0?j.from:field==1?j.to:j.windowSeconds;}
    Courier::Shift *shift_ptr(){return &Duty;}
    void shift_reset(){Duty=Courier::NewShift();}
    void shift_validate(){Courier::Sanitize(Duty);}
    int shift_accept(int id){return Courier::Accept(Duty,id);}
    int shift_handoff(int id){return Courier::Handoff(Duty,Bike,id);}
    int shift_next(){return Courier::NextShift(Duty);}
    int shift_nearby(){return Courier::Nearby(Bike);}
    Courier::Route *shift_route(int id){Guidance=Courier::Directions(Bike.s,id,Courier::RampOpen(Duty));return &Guidance;}
}
#endif
