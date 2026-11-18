#include "RidePawn.h"
#include "CourierDispatch.h"
#include "RideWeather.h"
#include "Skyway.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/Input/SButton.h"

void ARidePawn::SyncPanniers()
{
    for (int Side = 0; Side < 2; ++Side)
    {
        const int Job = Panniers[Side];
        if (Job < 0 || Job >= Courier::JobCount || (Shift.status[Job] != 1 && Shift.status[Job] != 2) ||
            (Side == 1 && Job == Panniers[0]))
            Panniers[Side] = -1;
    }
    for (int Job = 0; Job < Courier::JobCount; ++Job)
        if ((Shift.status[Job] == 1 || Shift.status[Job] == 2) && Panniers[0] != Job && Panniers[1] != Job)
            Panniers[Panniers[0] < 0 ? 0 : 1] = Job;
}
int ARidePawn::TrackedJob() const
{
    auto Active = [&](int I) {
        return I >= 0 && I < Courier::JobCount && (Shift.status[I] == 1 || Shift.status[I] == 2);
    };
    if (Active(Tracked))
        return Tracked;
    for (int I = 0; I < Courier::JobCount; ++I)
        if (Active(I))
            return I;
    return -1;
}
int ARidePawn::TrackedStop() const
{
    if (Waypoint >= 0 && Waypoint < Courier::AddressCount)
        return Waypoint;
    const int Job = TrackedJob();
    if (Job < 0)
        return -1;
    const auto Parcel = Courier::Contract(Job, Shift.number);
    return Shift.status[Job] == 1 ? Parcel.from : Parcel.to;
}
void ARidePawn::Notify(const FString &Message)
{
    Notice = Message;
    NoticeUntil = Ride.elapsed + 5.;
}
FString ARidePawn::CourierNotice() const
{
    return Ride.elapsed < NoticeUntil ? Notice : FString();
}
void ARidePawn::TrackParcel(int Job)
{
    if (Job >= 0 && Job < Courier::JobCount && (Shift.status[Job] == 1 || Shift.status[Job] == 2))
    {
        Waypoint = -1;
        Tracked = Job;
        SaveLocation();
    }
}
void ARidePawn::MarkAddress(int Address)
{
    if (Address >= 0 && Address < Courier::AddressCount)
    {
        Waypoint = Waypoint == Address ? -1 : Address;
        SaveLocation();
    }
}
bool ARidePawn::ReserveParcel(int Job)
{
    if (!Courier::Accept(Shift, Job))
        return false;
    SyncPanniers();
    Waypoint = -1;
    Tracked = Job;
    Notify(TEXT("Pannier reserved"));
    SaveLocation();
    return true;
}
bool ARidePawn::ServiceParcel(int Job)
{
    if (Job < 0 || Job >= Courier::JobCount || Ride.foot.active || Ride.crashAge >= 0.)
        return false;
    const bool WasOpen = Courier::RampOpen(Shift);
    if (!Courier::Handoff(Shift, Ride, Job))
        return false;
    if (Skyway)
        Skyway->Acknowledge(Courier::Nearby(Ride));
    const bool Delivered = Shift.status[Job] == 3;
    SyncPanniers();
    const FString Parcel = UTF8_TO_TCHAR(Courier::Contract(Job, Shift.number).parcel);
    if (Shift.finished)
        Notify(Shift.number == 3 ? TEXT("First Light complete  /  Aurora's dawn screening is restored")
                                 : TEXT("Shift complete  /  Your next manifest is ready"));
    else if (!WasOpen && Courier::RampOpen(Shift))
        Notify(TEXT("Delivered  /  Aurora service ramp unlocked"));
    else
        Notify((Delivered ? FString(TEXT("Delivered  /  ")) : FString(TEXT("Collected  /  "))) + Parcel);
    Waypoint = -1;
    Tracked = Delivered ? -1 : Job;
    SaveLocation();
    if (Shift.finished && !Dispatch.IsValid())
        ToggleDispatch();
    return true;
}
void ARidePawn::ServiceStop()
{
    const int Stop = Courier::Nearby(Ride);
    if (Stop < 0)
    {
        Notify(TEXT("Pull into the right-hand bay and slow to a stop"));
        return;
    }
    int Count = 0, Wanted = -1;
    for (const int Job : Panniers)
    {
        if (Job < 0)
            continue;
        const auto Parcel = Courier::Contract(Job, Shift.number);
        if ((Shift.status[Job] == 1 ? Parcel.from : Parcel.to) == Stop)
        {
            ++Count;
            Wanted = Job;
        }
    }
    if (Count == 1)
        ServiceParcel(Wanted);
    else
        ToggleDispatch();
}
void ARidePawn::FinishShift()
{
    if (Courier::NextShift(Shift))
    {
        Tracked = -1;
        Waypoint = -1;
        SyncPanniers();
        Notify(TEXT("New manifest available"));
        SaveLocation();
    }
}
void ARidePawn::ToggleDispatch()
{
    if (Dispatch.IsValid())
    {
        CloseDispatch();
        return;
    }
    auto *PC = Cast<APlayerController>(Controller);
    auto *View = GetWorld()->GetGameViewport();
    if (!PC || !View || Menu.IsValid() || !PC->SetPause(true))
        return;
    bBoost = false;
    Weather->SetAudioPaused(true);
    Dispatch = SNew(SCourierDispatch).Rider(this);
    View->AddViewportWidgetContent(Dispatch.ToSharedRef(), 110);
    FInputModeUIOnly Input;
    Input.SetWidgetToFocus(Dispatch->FirstAction);
    PC->SetInputMode(Input);
    PC->bShowMouseCursor = true;
}
void ARidePawn::CloseDispatch()
{
    if (!Dispatch.IsValid())
        return;
    if (auto *View = GetWorld()->GetGameViewport())
        View->RemoveViewportWidgetContent(Dispatch.ToSharedRef());
    Dispatch.Reset();
    if (auto *PC = Cast<APlayerController>(Controller))
    {
        PC->SetPause(false);
        Weather->SetAudioPaused(false);
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }
}
