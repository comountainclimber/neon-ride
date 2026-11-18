#pragma once
#include "CircuitCore.h"

// Enter the production traffic collision from behind so both physical bodies initialize normally.
inline Circuit::Ride CircuitImpactAt(double Age)
{
    constexpr double Step = 1. / 120.;
    auto State = Circuit::Start();
    State.elapsed = 3.;
    const auto Car = Circuit::Traffic(4, State.elapsed);
    State.s = Car.s - Car.half - 3.25;
    State.lateral = Car.lane - .62;
    State.v = 60.;
    for (int I = 0; I < 30 && State.crashAge < 0.; ++I)
        Circuit::Advance(State, 1., 0., 0., false, Step);
    if (State.crashAge >= 0.)
        for (int I = 0, Count = int(Age / Step + .5); I < Count; ++I)
            Circuit::Advance(State, 0., 0., 0., false, Step);
    return State;
}
