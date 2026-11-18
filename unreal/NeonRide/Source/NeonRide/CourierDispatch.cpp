#include "CourierDispatch.h"
#include "CourierPresentation.h"
#include "RidePawn.h"
#include "Brushes/SlateColorBrush.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
const FLinearColor Ink(.012f, .027f, .035f), Card(.029f, .056f, .063f), Paper(.92f, .93f, .84f),
    Quiet(.51f, .65f, .65f), Acid(.79f, .91f, .29f), Gold(.99f, .62f, .18f);
TSharedRef<STextBlock> Text(const FString &Value, int Size, FLinearColor Colour = Paper,
                            const TCHAR *Face = TEXT("Regular"))
{
    return SNew(STextBlock)
        .Text(FText::FromString(Value))
        .ColorAndOpacity(Colour)
        .Font(FCoreStyle::GetDefaultFontStyle(Face, Size));
}
TSharedRef<SBorder> Panel(FLinearColor Colour, FMargin Padding)
{
    return SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(Colour)
        .Padding(Padding);
}
class SCourierMap : public SLeafWidget
{
  public:
    SLATE_BEGIN_ARGS(SCourierMap)
    {
    }
    SLATE_ARGUMENT(TWeakObjectPtr<ARidePawn>, Rider)
    SLATE_END_ARGS()
    void Construct(const FArguments &Args)
    {
        Rider = Args._Rider;
        for (int I = 0; I < Courier::RoadCount; ++I)
        {
            auto &Road = Roads.AddDefaulted_GetRef();
            const int Steps = FMath::CeilToInt(Courier::RoadLength(I) / 18.);
            for (int J = 0; J <= Steps; ++J)
            {
                const auto F = Circuit::At(Courier::RoadStart(I) + Courier::RoadLength(I) * J / Steps);
                Road.Emplace(F.x, F.y);
                Bounds += Road.Last();
            }
        }
    }
    FVector2D ComputeDesiredSize(float) const override
    {
        return FVector2D(470, 240);
    }
    int32 OnPaint(const FPaintArgs &, const FGeometry &Geo, const FSlateRect &, FSlateWindowElementList &Out, int32 Z,
                  const FWidgetStyle &, bool) const override
    {
        if (!Rider.IsValid())
            return Z;
        const FVector2D Dim = Geo.GetLocalSize(), Centre = Bounds.GetCenter(), Span = Bounds.GetSize();
        const double Scale = FMath::Min((Dim.X - 48.) / FMath::Max(1., Span.X), (Dim.Y - 36.) / FMath::Max(1., Span.Y));
        auto Pixel = [&](FVector2D P) {
            return FVector2D((P.X - Centre.X) * Scale + Dim.X * .5, (Centre.Y - P.Y) * Scale + Dim.Y * .5);
        };
        const auto Brush = FCoreStyle::Get().GetBrush("WhiteBrush");
        const int Dest = Rider->TrackedStop();
        auto Plot = [&](FVector2D P, FVector2f Size, FLinearColor Colour, int32 Layer) {
            FSlateDrawElement::MakeBox(Out, Layer, Geo.ToPaintGeometry(Size, FSlateLayoutTransform(FVector2f(P))),
                                       Brush, ESlateDrawEffect::None, Colour);
        };
        for (int Y = 0; Y < Dim.Y; Y += 42)
            Plot({0., double(Y)}, {float(Dim.X), .6f}, FLinearColor(.063f, .10f, .112f), Z);
        for (int X = 0; X < Dim.X; X += 42)
            Plot({double(X), 0.}, {.6f, float(Dim.Y)}, FLinearColor(.063f, .10f, .112f), Z);
        for (int Road = 0; Road < Roads.Num(); ++Road)
        {
            TArray<FVector2f> Points;
            for (FVector2D P : Roads[Road])
                Points.Add(FVector2f(Pixel(P)));
            auto Colour = CourierPresentation::Colour(Road);
            if (Road == 4 && !Courier::RampOpen(Rider->GetShift()))
                Colour = FLinearColor(.24f, .29f, .30f);
            FSlateDrawElement::MakeLines(Out, Z + 1, Geo.ToPaintGeometry(), Points, ESlateDrawEffect::None, Colour,
                                         true, Road == 0 ? 2.f : 2.8f);
        }
        for (int Id = 0; Id < Courier::AddressCount; ++Id)
        {
            const auto F = Circuit::At(Courier::Stop(Id).s);
            const FVector2D P = Pixel({F.x, F.y});
            const bool Aim = Id == Dest;
            Plot(P - FVector2D(7), {14, 14}, Aim ? Gold : Ink, Z + 2);
            const auto Font = FCoreStyle::GetDefaultFontStyle("Bold", 9);
            FSlateDrawElement::MakeText(
                Out, Z + 3,
                Geo.ToPaintGeometry(FVector2f(16, 16), FSlateLayoutTransform(FVector2f(P - FVector2D(3.5, 7.5)))),
                FText::AsNumber(Id + 1), Font, ESlateDrawEffect::None, Aim ? Ink : Paper);
        }
        const auto F = Circuit::At(Rider->GetRide().s, Rider->GetRide().lateral);
        const FVector2D P = Pixel({F.x, F.y});
        TArray<FVector2f> Arrow;
        const double Heading = F.yaw + Rider->GetRide().headingError;
        for (FVector2D Local :
             {FVector2D(9, 0), FVector2D(-5, 4), FVector2D(-3, 0), FVector2D(-5, -4), FVector2D(9, 0)})
        {
            const FVector2D Rot = Local.GetRotated(FMath::RadiansToDegrees(Heading));
            Arrow.Emplace(P + FVector2D(Rot.X, -Rot.Y));
        }
        FSlateDrawElement::MakeLines(Out, Z + 4, Geo.ToPaintGeometry(), Arrow, ESlateDrawEffect::None,
                                     FLinearColor::White, true, 2.f);
        return Z + 4;
    }

  private:
    TWeakObjectPtr<ARidePawn> Rider;
    TArray<TArray<FVector2D>> Roads;
    FBox2D Bounds{ForceInit};
};
}

void SCourierDispatch::Construct(const FArguments &Args)
{
    Rider = Args._Rider;
    ActionStyle.SetNormal(FSlateColorBrush(Card))
        .SetHovered(FSlateColorBrush(FLinearColor(.095f, .15f, .15f)))
        .SetPressed(FSlateColorBrush(FLinearColor(.16f, .22f, .19f)));
    Build();
}
void SCourierDispatch::Build()
{
    if (!Rider.IsValid())
        return;
    const auto &Shift = Rider->GetShift();
    TSharedRef<SVerticalBox> Main = SNew(SVerticalBox), Left = SNew(SVerticalBox), Right = SNew(SVerticalBox);
    TSharedRef<SHorizontalBox> Body = SNew(SHorizontalBox);
    TSharedRef<SHorizontalBox> Header = SNew(SHorizontalBox);
    Header->AddSlot().FillWidth(1)[Text(TEXT("NIGHT DISPATCH"), 44, Paper, TEXT("BoldCondensedItalic"))];
    const TCHAR *Names[] = {TEXT("First Night"), TEXT("Regular Service"), TEXT("First Light")};
    const TCHAR *ShiftName = Shift.number <= 3 ? Names[FMath::Clamp(Shift.number - 1, 0, 2)] : TEXT("Open dispatch");
    Header->AddSlot().AutoWidth().VAlign(VAlign_Center)[Text(
        FString::Printf(TEXT("SHIFT  %02d   /   %s"), Shift.number, ShiftName), 17, Acid, TEXT("Mono"))];
    Main->AddSlot().AutoHeight().Padding(0, 0, 0, 14)[Header];
    if (Shift.number > 3 || (Shift.number == 3 && Shift.finished))
        Main->AddSlot().AutoHeight().Padding(
            0, 0, 0, 8)[Text(TEXT("First Light complete   /   Aurora Cinema's dawn screening is restored."), 16, Gold)];
    Main->AddSlot().FillHeight(1.)[Body];
    Body->AddSlot().FillWidth(.47f).Padding(0, 0, 18, 0)[Left];
    Body->AddSlot().FillWidth(.53f)[Right];
    Left->AddSlot().AutoHeight()[Text(TEXT("CITY NETWORK"), 15, Acid, TEXT("Mono"))];
    auto MapPanel = Panel(Ink, FMargin(0));
    MapPanel->SetContent(SNew(SCourierMap).Rider(Rider));
    Left->AddSlot().AutoHeight().Padding(0, 7, 0, 7)[MapPanel];
    for (int Id = 0; Id < Courier::AddressCount; ++Id)
    {
        const auto &Addr = Courier::Stop(Id);
        auto Line = SNew(SHorizontalBox);
        Line->AddSlot().FillWidth(1)[Text(FString::Printf(TEXT("%02d   %s"), Id + 1, UTF8_TO_TCHAR(Addr.name)), 14,
                                          Id == Rider->TrackedStop() ? Gold : Paper)];
        const FString Recipient =
            FString::Printf(TEXT("%s   /   %d delivered"), UTF8_TO_TCHAR(Addr.client), Shift.visits[Id]);
        Line->AddSlot().AutoWidth()[Text(Recipient, 12, Quiet)];
        Left->AddSlot().AutoHeight().Padding(0,
                                             1)[SNew(SButton)
                                                    .ButtonStyle(&ActionStyle)
                                                    .ContentPadding(FMargin(5, 2))
                                                    .ToolTipText(FText::FromString(TEXT("Set or clear this waypoint")))
                                                    .OnClicked(this, &SCourierDispatch::Navigate, Id)[Line]];
    }
    Left->AddSlot().AutoHeight().Padding(
        0, 12, 0, 0)[Text(Courier::RampOpen(Shift) ? TEXT("Aurora service ramp   /   OPEN")
                                                   : TEXT("Aurora service ramp   /   Cinema delivery unlocks access"),
                          12, Quiet)];
    int Filled = 0, Done = 0;
    for (int I = 0; I < Courier::JobCount; ++I)
    {
        if (Shift.status[I] == 3)
            ++Done;
        if (Shift.status[I] == 1 || Shift.status[I] == 2)
        {
            ++Filled;
        }
    }
    for (int I = 0; I < 2; ++I)
    {
        const int J = Rider->PannierJob(I);
        FString Contents = TEXT("Empty pannier");
        if (J >= 0)
            Contents = FString(Shift.status[J] == 1 ? TEXT("Reserved   /   ") : TEXT("Loaded   /   ")) +
                       UTF8_TO_TCHAR(Courier::Contract(J, Shift.number).parcel);
        const FString Name = (I == 0 ? TEXT("LEFT    ") : TEXT("RIGHT   ")) + Contents;
        Left->AddSlot().AutoHeight().Padding(0, I == 0 ? 12 : 3, 0,
                                             0)[Text(Name, 13, J >= 0 ? Paper : Quiet, TEXT("Mono"))];
    }
    Right->AddSlot().AutoHeight().Padding(
        0, 0, 0, 8)[Text(FString::Printf(TEXT("PARCEL MANIFEST     %d / %d DELIVERED"), Done, Courier::JobCount), 15,
                         Acid, TEXT("Mono"))];
    auto List = SNew(SScrollBox);
    Right->AddSlot().FillHeight(1)[List];
    const int Here = Courier::Nearby(Rider->GetRide());
    TSharedPtr<SButton> Focus;
    for (int I = 0; I < Courier::JobCount; ++I)
    {
        const auto J = Courier::Contract(I, Shift.number);
        const int Status = Shift.status[I];
        const auto &From = Courier::Stop(J.from);
        const auto &To = Courier::Stop(J.to);
        FString Action = TEXT("Reserve");
        if (Status == 1)
            Action = Here == J.from ? TEXT("Collect") : TEXT("Track pickup");
        if (Status == 2)
            Action = Here == J.to ? TEXT("Deliver") : TEXT("Track delivery");
        if (Status == 3)
            Action = J.windowSeconds > 0 ? (Shift.onTime[I] ? TEXT("Delivered on time") : TEXT("Delivered late"))
                                         : TEXT("Delivered");
        const bool Enabled = Status < 3 && (Status != 0 || Filled < 2);
        auto Row = SNew(SVerticalBox);
        auto Line = SNew(SHorizontalBox);
        Line->AddSlot().FillWidth(
            1)[Text(UTF8_TO_TCHAR(J.parcel), 19, Status == 3 ? Quiet : Paper, TEXT("BoldCondensed"))];
        Line->AddSlot().AutoWidth().VAlign(VAlign_Center)[Text(Action, 12, Enabled ? Acid : Quiet, TEXT("Mono"))];
        Row->AddSlot().AutoHeight()[Line];
        const FString FromTo = FString::Printf(TEXT("%s   >   %s"), UTF8_TO_TCHAR(From.name), UTF8_TO_TCHAR(To.name));
        FString Window = J.windowSeconds > 0
                             ? FString::Printf(TEXT("   /   %d:%02d"), J.windowSeconds / 60, J.windowSeconds % 60)
                             : FString();
        Row->AddSlot().AutoHeight()[Text(FromTo + Window, 13, Quiet)];
        TSharedRef<SButton> Button = SNew(SButton)
                                         .ButtonStyle(&ActionStyle)
                                         .IsEnabled(Enabled)
                                         .ContentPadding(FMargin(12, 5))
                                         .OnClicked(this, &SCourierDispatch::ParcelAction, I)[Row];
        if (Enabled && !Focus)
            Focus = Button;
        List->AddSlot().Padding(0, 0, 0, 4)[Button];
    }
    auto Footer = SNew(SHorizontalBox);
    TSharedRef<SButton> Back =
        SNew(SButton)
            .ButtonStyle(&ActionStyle)
            .ContentPadding(FMargin(20, 9))
            .OnClicked(this, &SCourierDispatch::Close)[Text(TEXT("Return to bike"), 16, Acid, TEXT("Bold"))];
    Footer->AddSlot().AutoWidth()[Back];
    if (Done == Courier::JobCount)
        Footer->AddSlot().AutoWidth().Padding(
            6,
            0)[SNew(SButton)
                   .ButtonStyle(&ActionStyle)
                   .OnClicked(this,
                              &SCourierDispatch::NewManifest)[Text(TEXT("Start next shift"), 16, Acid, TEXT("Bold"))]];
    Footer->AddSlot()
        .FillWidth(1)
        .HAlign(HAlign_Right)
        .VAlign(VAlign_Center)[Text(TEXT("Tab   map     /     E   curbside handoff"), 13, Quiet, TEXT("Mono"))];
    Main->AddSlot().AutoHeight().Padding(0, 15, 0, 0)[Footer];
    FirstAction = Focus.IsValid() ? Focus : TSharedPtr<SButton>(Back);
    auto Surface = Panel(FLinearColor(.014f, .032f, .039f, .986f), FMargin(38, 26));
    Surface->SetContent(Main);
    auto Screen = Panel(FLinearColor(.002f, .008f, .012f, .85f), FMargin(24));
    Screen->SetContent(
        SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1180).HeightOverride(735)[Surface]]);
    ChildSlot[Screen];
}
FReply SCourierDispatch::ParcelAction(int Id)
{
    if (!Rider.IsValid())
        return FReply::Handled();
    const int Status = Rider->GetShift().status[Id];
    const auto Job = Courier::Contract(Id, Rider->GetShift().number);
    if (Status == 0)
        Rider->ReserveParcel(Id);
    else if (Courier::Nearby(Rider->GetRide()) == (Status == 1 ? Job.from : Job.to))
        Rider->ServiceParcel(Id);
    else
        Rider->TrackParcel(Id);
    Build();
    return FReply::Handled().SetUserFocus(FirstAction.ToSharedRef(), EFocusCause::SetDirectly);
}
FReply SCourierDispatch::Close()
{
    if (Rider.IsValid())
        Rider->CloseDispatch();
    return FReply::Handled();
}
FReply SCourierDispatch::Navigate(int Stop)
{
    if (Rider.IsValid())
        Rider->MarkAddress(Stop);
    return Close();
}
FReply SCourierDispatch::NewManifest()
{
    if (Rider.IsValid())
        Rider->FinishShift();
    Build();
    return FReply::Handled().SetUserFocus(FirstAction.ToSharedRef());
}
FReply SCourierDispatch::OnPreviewKeyDown(const FGeometry &Geometry, const FKeyEvent &Key)
{
    const auto K = Key.GetKey();
    if (K == EKeys::Tab || K == EKeys::M || K == EKeys::Escape || K == EKeys::Gamepad_DPad_Up ||
        K == EKeys::Gamepad_FaceButton_Right)
        return Close();
    return SCompoundWidget::OnPreviewKeyDown(Geometry, Key);
}
