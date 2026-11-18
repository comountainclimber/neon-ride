// Portable test executable for compare_circuit.mjs.
#include "CircuitCore.h"
#include <cstdio>

int main()
{
    auto ride = Circuit::Start();
    const double dt = 1. / 120.;
    // Centre line clears traffic for two laps. Later scenarios strike the first truck.
    for (int scenario = 0; scenario < 3; ++scenario)
    {
        ride = Circuit::Start();
        auto truck = Circuit::Traffic(0, 0);
        ride.lateral = scenario == 0 ? 0. : truck.lane;
        if (scenario != 0)
        {
            ride.s = truck.s - truck.half - 2.;
            ride.v = truck.v + (scenario == 1 ? 28. : 4.);
        }
        int ticks = scenario == 0 ? 33000 : 500;
        for (int tick = 0; tick < ticks; ++tick)
        {
            Circuit::Advance(ride, 1., 0., 0., tick < 600, dt);
            if (tick % 30 == 0)
            {
                const auto pose = Circuit::Ejection(ride);
                std::printf("%d %d %.12g %.12g %.12g %.12g %.12g %.12g %.12g %.12g %.12g\n",
                            scenario, tick, ride.s, ride.v, ride.charge, ride.laps, ride.crashAge,
                            ride.impact, pose.riderS, pose.riderHeight, ride.lateral);
            }
        }
    }
}
