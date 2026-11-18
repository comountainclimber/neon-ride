// Feed the same controlled, head-on, glancing and neutral-steering cases to both builds.
#include "CircuitCore.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
int main()
{
    const double dt = 1. / 120.;
    for (int scenario = 0; scenario < 6; ++scenario)
    {
        auto p = Circuit::Start();
        const auto truck = Circuit::Traffic(0, 0.);
        p.lateral = 0.;
        if (scenario == 1 || scenario == 2)
        {
            p.s = truck.s - truck.half - 2.;
            p.lateral = truck.lane;
            p.v = truck.v + (scenario == 1 ? 28. : 4.);
        }
        if (scenario == 3)
        {
            p.s = 410.;
            p.lateral = -4.9;
            p.v = 38.;
            p.headingError = -.42;
        }
        if (scenario == 4)
        {
            p.s = truck.s;
            p.lateral = truck.lane - 2.3;
            p.v = 25.;
            p.headingError = .56;
        }
        if (scenario == 5)
        {
            p.s = 850.;
            p.v = 22.;
        }
        const int ticks = scenario == 0 ? 44000 : (scenario == 5 ? 100 : 940);
        for (int tick = 0; tick < ticks; ++tick)
        {
            double turn = 0.;
            // A test driver explicitly follows the route. Production has no such controller.
            if (scenario == 0)
            {
                const double rate = std::min({21. / (p.v + 2.), p.v * .15, 1.12});
                turn = (Circuit::At(p.s + 4.).curvature * p.v - 2.8 * p.headingError - .20 * p.lateral) /
                       std::max(.02, rate);
            }
            Circuit::Advance(p, scenario == 0 && p.v < 37. ? 1. : 0., 0., turn, scenario == 0 && tick < 600, dt);
            if (tick % 30 != 0)
                continue;
            const auto e = Circuit::Ejection(p);
            std::printf("%d %d", scenario, tick);
            for (double value :
                 {p.s,        p.v,       p.charge,       p.laps,        p.crashAge,   p.impact,  p.headingError,
                  p.lateral,  e.riderS,  e.riderLateral, e.riderHeight, e.riderPitch, e.bikeS,   e.bikeHeight,
                  e.bikeRoll, e.bikeYaw, e.riderGround,  e.bikeGround,  e.severity,   e.clipTime})
                std::printf(" %.12g", value);
            std::puts("");
        }
    }
}
