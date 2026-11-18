#include "CourierCore.h"
#ifdef __wasm__
#define COURIER_SQRT __builtin_sqrt
extern "C" { double sin(double); double cos(double); double atan(double); double atan2(double,double); double fmod(double,double); }
#else
#include <cmath>
#define COURIER_SQRT std::sqrt
using std::sin; using std::cos; using std::atan; using std::atan2; using std::fmod;
#endif

namespace Courier
{
namespace
{
constexpr double Pi = 3.14159265358979323846, Stride = 10000.;
constexpr int Samples = 320;
struct Point { double x,y,z; };
struct Path { Point p[Samples+1]; double arc[Samples+1]; };
Path paths[RoadCount]{};
bool built = false;
const double entries[] = {0,155,1240,3020,0};
const double exits[] = {0,1180,2390,4380,0};
const char *const names[] = {"Express Ring","Rainline Streets","Floodworks Canal","Glass Heights Docks","Aurora Service Ramp"};
double clamp(double v,double lo,double hi) { return v<lo?lo:v>hi?hi:v; }
double absval(double v) {return v<0?-v:v;}
double wrap(double v,double n) {double r=fmod(v,n);return r<0?r+n:r;}
double norm(Point v) {return COURIER_SQRT(v.x*v.x+v.y*v.y+v.z*v.z);}
Point difference(Point a,Point b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
Point mix(Point a,Point b,double t) {return {a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
Point atLocal(int id,double s)
{
    const auto &p=paths[id];
    int a=0,b=Samples;
    while(b-a>1){const int m=(a+b)/2;if(p.arc[m]<=s)a=m;else b=m;}
    return mix(p.p[a],p.p[a+1],(s-p.arc[a])/(p.arc[a+1]-p.arc[a]));
}
void build()
{
    if(built)return;
    // Each district shares the tangent and elevation of the ring at both junctions.
    // Arc length is measured after the lateral and vertical profiles are applied.
    const double widths[]={0,180,265,205}, waves[]={0,34,54,42};
    Point handles[4]{};
    for(int id=1;id<RoadCount;++id){
        auto &p=paths[id];
        if(id==4){
            const double a=paths[2].arc[Samples]*.72,b=paths[3].arc[Samples]*.47;
            const auto entry=atLocal(2,a),ahead=atLocal(2,a+1),exit=atLocal(3,b),behind=atLocal(3,b-1);
            // A service viaduct cuts across the outer roads. Its first bend peels
            // to the right of the canal, then arcs back over the ring.
            handles[0]=entry; handles[3]=exit;
            handles[1]={entry.x+330*(ahead.x-entry.x),entry.y+330*(ahead.y-entry.y),entry.z+330*(ahead.z-entry.z)};
            handles[2]={exit.x-350*(exit.x-behind.x),exit.y-350*(exit.y-behind.y),exit.z-350*(exit.z-behind.z)};
        }
        for(int i=0;i<=Samples;++i){
            const double t=double(i)/Samples, envelope=sin(Pi*t)*sin(Pi*t);
            if(id<4){
                const auto f=Circuit::At(entries[id]+(exits[id]-entries[id])*t);
                const double offset=envelope*(widths[id]+waves[id]*sin(4*Pi*t));
                const double height=id==1?9.:id==2?6.:105.;
                p.p[i]={f.x+offset*f.rx,f.y+offset*f.ry,f.z+(height-f.z)*envelope};
            }else{
                const Point a=mix(handles[0],handles[1],t),b=mix(handles[1],handles[2],t),c=mix(handles[2],handles[3],t);
                auto q=mix(mix(a,b,t),mix(b,c,t),t);
                const Point d=difference(handles[1],handles[0]);
                const double n=COURIER_SQRT(d.x*d.x+d.y*d.y);
                const double offset=130.*sin(2.*Pi*t)*envelope;
                q.x-=d.y/n*offset; q.y+=d.x/n*offset; q.z+=52.*envelope;
                p.p[i]=q;
            }
            if(i)p.arc[i]=p.arc[i-1]+norm(difference(p.p[i],p.p[i-1]));
        }
    }
    built=true;
}
}
int RoadOf(double s){return int(clamp((s+256.)/Stride,0,RoadCount-1));}
double RoadStart(int road){return road*Stride;}
double RoadLength(int road){if(road==0)return Circuit::Length();build();return paths[road].arc[Samples];}
const char *RoadName(int road){return names[road];}
bool ValidPosition(double s){const int id=RoadOf(s);return s>=RoadStart(id)&&s<=RoadStart(id)+RoadLength(id);}
double Entry(int road){return road==4?RoadStart(2)+.72*RoadLength(2):entries[road];}
double Exit(int road){return road==4?RoadStart(3)+.47*RoadLength(3):exits[road];}
Circuit::Frame DistrictFrame(double s,double lateral)
{
    build();const int id=RoadOf(s);s-=RoadStart(id);
    const Point a=atLocal(id,s-3),b=atLocal(id,s),c=atLocal(id,s+3),d=difference(c,a);
    const double n=COURIER_SQRT(d.x*d.x+d.y*d.y),fx=d.x/n,fy=d.y/n;
    const Point u=difference(b,a),v=difference(c,b);
    const double k=atan2(u.x*v.y-u.y*v.x,u.x*v.x+u.y*v.y)/3.,bank=clamp(k*16.,-.11,.11);
    return {b.x-lateral*fy,b.y+lateral*fx,b.z-lateral*sin(bank),fx,fy,-fy,fx,atan2(fy,fx),atan2(d.z,n),bank,k};
}
double Move(double s,double ds,double lateral,bool forks,bool ramp)
{
    const int road=RoadOf(s);
    if(ds>0 && forks && lateral>1.1){
        for(int i=1;i<RoadCount;++i){
            if(i==4&&!ramp)continue;
            const double junction=Entry(i);
            if(s<=junction && s+ds>junction)return RoadStart(i)+(s+ds-junction);
        }
    }
    if(!road)return wrap(s+ds,Circuit::Length());
    const double end=RoadStart(road)+RoadLength(road);
    if(s+ds>=end)return Exit(road)+(s+ds-end);
    if(s+ds<RoadStart(road))return Entry(road)+s+ds-RoadStart(road);
    return s+ds;
}
double DistanceBetween(double a,double b)
{
    const int r=RoadOf(a);
    if(r!=RoadOf(b))return 1.e9;
    if(r)return a-b;
    const double n=Circuit::Length();
    return wrap(a-b+.5*n,n)-.5*n;
}
double TrafficPosition(int i,double travelled)
{
    const int road=1+i%3;
    const double len=RoadLength(road),ring=Circuit::Length();
    const double progress=wrap(travelled,len+ring-(Exit(road)-Entry(road)));
    return progress<len?RoadStart(road)+progress:wrap(Exit(road)+progress-len,ring);
}
const Address &Stop(int id)
{
    static Address stops[]={{"Rainline Repair","Rainline","Inez",0},{"Parcel Arcade","Rainline","Bao",0},
        {"Tide Pump Nine","Floodworks","Mara",0},{"Canal Exchange","Floodworks","Kito",0},
        {"Aurora Cinema","Glass Heights","Sol",0},{"Glasshouse Clinic","Glass Heights","Dr. Vale",0}};
    if(stops[1].s==0){
        stops[1].s=RoadStart(1)+.38*RoadLength(1);
        stops[2].s=RoadStart(2)+.38*RoadLength(2);
        stops[3].s=RoadStart(2)+.64*RoadLength(2);
        stops[4].s=RoadStart(3)+.63*RoadLength(3);
        stops[5].s=RoadStart(3)+.84*RoadLength(3);
    }
    return stops[id];
}
Job Contract(int id,int number)
{
    static const Job first[]={{"Pump controller",0,2,0},{"Midnight film reel",0,4,235},{"Sterile filters",1,5,0},
        {"Shutter relay",2,4,0},{"Night specimens",5,3,270},{"Canal sound tapes",3,1,0},
        {"Projector motor",4,0,0},{"Insulin cooler",1,5,220}};
    static const Job next[]={{"Turbine brushes",0,3,0},{"Dawn print",0,4,225},{"Suture kit",1,5,0},
        {"Canal survey negatives",2,4,0},{"Water cultures",5,2,265},{"Lockmaster's ledger",3,1,0},
        {"Rewinder gearbox",4,0,0},{"Dialysis cartridge",1,5,220}};
    static const Job dawn[]={{"Generator brushes",0,2,0},{"Projection lamp",0,4,245},
        {"First-aid satchel",1,5,0},{"Restored sound head",2,4,0},{"Night crew thermoses",5,3,0},
        {"Archive release keys",3,1,260},{"Cinema roof shutter motor",1,4,0},{"First Light master reel",3,4,265}};
    if(number==3)return dawn[id];
    return number%2?first[id]:next[id];
}
Shift NewShift(int number){Shift s{};s.number=number>0?number:1;return s;}
bool RampOpen(const Shift &s){return s.visits[4]>0;}
bool Accept(Shift &s,int id)
{
    if(id<0||id>=JobCount||s.status[id]||s.finished)return false;
    int slots=0;for(int v:s.status)if(v==1||v==2)++slots;
    if(slots>=2)return false;
    s.status[id]=1;return true;
}
int Nearby(const Circuit::Ride &r)
{
    if(r.crashAge>=0||r.v>3.||r.lateral<1.1)return -1;
    for(int i=0;i<AddressCount;++i)if(absval(DistanceBetween(r.s,Stop(i).s))<=30.)return i;
    return -1;
}
bool Handoff(Shift &s,const Circuit::Ride &r,int id)
{
    if(id<0||id>=JobCount||s.finished)return false;
    const auto job=Contract(id,s.number);const int at=Nearby(r);
    if(s.status[id]==1&&at==job.from){s.status[id]=2;s.pickedAt[id]=r.elapsed;return true;}
    if(s.status[id]!=2||at!=job.to)return false;
    s.status[id]=3;++s.visits[at];
    s.onTime[id]=job.windowSeconds>0&&r.elapsed>=s.pickedAt[id]&&r.elapsed-s.pickedAt[id]<=job.windowSeconds;
    s.finished=1;for(int v:s.status)if(v!=3)s.finished=0;
    return true;
}
bool NextShift(Shift &s)
{
    if(!s.finished)return false;
    auto fresh=NewShift(s.number+1);for(int i=0;i<AddressCount;++i)fresh.visits[i]=s.visits[i];
    s=fresh;return true;
}
void Sanitize(Shift &s)
{
    s.number=int(clamp(s.number,1,1000000));int used=0;s.finished=1;
    for(int i=0;i<JobCount;++i){
        auto &v=s.status[i];if(v<0||v>3)v=0;
        if((v==1||v==2)&&++used>2)v=0;
        if(!(s.pickedAt[i]>=0 && s.pickedAt[i]<1.e12))s.pickedAt[i]=0;
        s.onTime[i]=(v==3 && s.onTime[i]==1);
        if(v!=3)s.finished=0;
    }
    for(int &v:s.visits)v=int(clamp(v,0,10000000));
}

// Search only at road decisions. Passing an exit and circling once is legal, so
// a missed bay can be revisited from any edge without a teleport or a dead end.
Route Directions(double s,int stopId,bool ramp)
{
    Route best{};best.meters=1.e12;best.nextRoad=-1;best.turnIn=-1;
    if(stopId<0||stopId>=AddressCount||!ValidPosition(s))return best;
    const double dest=Stop(stopId).s;
    struct Search {
        double s,dist,turn;
        int first,via[10],n,visited;
    };
    auto walk=[&](auto &&self,Search q)->void{
        const int road=RoadOf(q.s);
        auto forward=[&](double at){if(RoadOf(at)!=road)return 1.e12;
            double delta=at-q.s;if(road==0&&delta<0)delta+=Circuit::Length();return delta<0?1.e12:delta;};
        const double goal=forward(dest);
        if(q.dist+goal<best.meters){best.meters=q.dist+goal;best.turnIn=q.turn;best.nextRoad=q.first;
            best.count=q.n;for(int j=0;j<q.n;++j)best.via[j]=q.via[j];}
        if(q.n>=9||q.dist>=best.meters)return;
        for(int child=1;child<RoadCount;++child){
            if((child==4&&!ramp)||(q.visited&(1<<child)))continue;
            const double to=forward(Entry(child));if(q.dist+to>=best.meters)continue;
            auto n=q;n.s=RoadStart(child);n.dist+=to;n.visited|=1<<child;
            if(n.first<0){n.first=child;n.turn=n.dist;}n.via[n.n++]=child;self(self,n);
        }
        if(road){
            auto n=q;n.dist+=RoadStart(road)+RoadLength(road)-q.s;n.s=Exit(road);
            n.via[n.n++]=RoadOf(n.s);self(self,n);
        }
    };
    Search initial{};initial.s=s;initial.first=-1;initial.turn=-1;
    initial.via[initial.n++]=RoadOf(s);walk(walk,initial);return best;
}
}
