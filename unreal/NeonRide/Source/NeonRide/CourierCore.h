#pragma once
#include "CircuitCore.h"

namespace Courier
{
constexpr int RoadCount = 5, AddressCount = 6, JobCount = 8;
struct Address { const char *name, *district, *client; double s; };
struct Job { const char *parcel; int from, to, windowSeconds; };
struct Shift
{
    int status[JobCount]{}, visits[AddressCount]{}, number = 1, finished = 0;
    double pickedAt[JobCount]{};
    int onTime[JobCount]{};
};
struct Route { double meters, turnIn; int nextRoad, via[10], count; };
double RoadStart(int road);
double RoadLength(int road);
const char *RoadName(int road);
int RoadOf(double s);
bool ValidPosition(double s);
double Entry(int road);
double Exit(int road);
Circuit::Frame DistrictFrame(double s, double lateral);
double Move(double s, double ds, double lateral, bool useForks, bool rampOpen);
double DistanceBetween(double a, double b);
double TrafficPosition(int index, double travelled);
const Address &Stop(int id);
Job Contract(int id, int shiftNumber = 1);
Shift NewShift(int number = 1);
bool Accept(Shift &state, int job);
bool Handoff(Shift &state, const Circuit::Ride &ride, int job);
int Nearby(const Circuit::Ride &ride);
bool NextShift(Shift &state);
bool RampOpen(const Shift &state);
Route Directions(double s, int stopId, bool rampOpen);
void Sanitize(Shift &state);
}
