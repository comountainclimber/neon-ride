#include "RideMenu.h"
#include "RidePawn.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SRideMenu::Construct(const FArguments& Args)
{
    Rider = Args._Rider;
    auto Label = [](const TCHAR* Text, int Size, FLinearColor Colour)
    {
        return SNew(STextBlock).Text(FText::FromString(Text))
            .ColorAndOpacity(Colour).Font(FCoreStyle::GetDefaultFontStyle("Regular",Size));
    };
    const FLinearColor Ink(.025f,.052f,.071f,.98f), Ice(.68f,.92f,.95f);
    auto Button = [&](const TCHAR* Text,FOnClicked Click)
    {
        return SNew(SButton).OnClicked(Click).ButtonColorAndOpacity(FLinearColor(.095f,.21f,.24f))
            .ContentPadding(FMargin(16,9)).HAlign(HAlign_Left)[Label(Text,14,FLinearColor::White)];
    };
    ResumeButton = Button(TEXT("Resume"),FOnClicked::CreateSP(this,&SRideMenu::Resume));
    ChildSlot.HAlign(HAlign_Center).VAlign(VAlign_Center)
    [
        SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink)
        .Padding(FMargin(28,24))
        [
            SNew(SBox).WidthOverride(305)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[Label(TEXT("Neon Ride"),26,Ice)]
                + SVerticalBox::Slot().AutoHeight().Padding(0,3,0,19)[Label(TEXT("Paused"),13,FLinearColor::White)]
                + SVerticalBox::Slot().AutoHeight()[ResumeButton.ToSharedRef()]
                + SVerticalBox::Slot().AutoHeight().Padding(0,5)[Button(TEXT("Restart circuit"),FOnClicked::CreateSP(this,&SRideMenu::ReturnToEntrance))]
                + SVerticalBox::Slot().AutoHeight()[Button(TEXT("Save and quit"),FOnClicked::CreateSP(this,&SRideMenu::Quit))]
            ]
        ]
    ];
}

FReply SRideMenu::OnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
    if (Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Gamepad_FaceButton_Right) return Resume();
    return SCompoundWidget::OnPreviewKeyDown(Geometry,Event);
}

FReply SRideMenu::Resume() { if (Rider.IsValid()) Rider->ClosePauseMenu(); return FReply::Handled(); }
FReply SRideMenu::ReturnToEntrance() { if (Rider.IsValid()) Rider->ReturnFromMenu(); return FReply::Handled(); }
FReply SRideMenu::Quit() { if (Rider.IsValid()) Rider->QuitFromMenu(); return FReply::Handled(); }
