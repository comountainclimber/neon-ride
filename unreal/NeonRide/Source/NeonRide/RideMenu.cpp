#include "RideMenu.h"
#include "RidePawn.h"
#include "Engine/Texture2D.h"
#include "Brushes/SlateColorBrush.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void SRideMenu::Construct(const FArguments &Args)
{
    Rider = Args._Rider;
    const bool Opening = Args._Opening;
    Cover.Reset(LoadObject<UTexture2D>(nullptr, TEXT("/Game/Environment/T_VesperCover.T_VesperCover")));
    if (Cover.IsValid())
    {
        Cover->SetForceMipLevelsToBeResident(8.f);
        CoverBrush.SetResourceObject(Cover.Get());
        CoverBrush.ImageSize = FVector2D(Cover->GetSizeX(), Cover->GetSizeY());
        CoverBrush.DrawAs = ESlateBrushDrawType::Image;
    }
    const FLinearColor Ivory(.91f, .92f, .85f), Acid(.77f, .94f, .16f), Ink(.013f, .036f, .044f);
    Primary.SetNormal(FSlateColorBrush(Acid))
        .SetHovered(FSlateColorBrush(FLinearColor(.91f, 1.f, .39f)))
        .SetPressed(FSlateColorBrush(FLinearColor(.61f, .78f, .10f)));
    Secondary.SetNormal(FSlateColorBrush(FLinearColor(.012f, .032f, .04f, .87f)))
        .SetHovered(FSlateColorBrush(FLinearColor(.076f, .14f, .15f, .95f)))
        .SetPressed(FSlateColorBrush(Ink));
    auto Label = [](const FString &Text, const FSlateFontInfo &Font, FLinearColor Colour) {
        return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font).ColorAndOpacity(Colour);
    };

    const auto Display = FCoreStyle::GetDefaultFontStyle("BoldCondensedItalic", 88);
    const auto Mono = FCoreStyle::GetDefaultFontStyle("Mono", 13);
    const auto Action = FCoreStyle::GetDefaultFontStyle("BoldCondensed", 19);
    auto Button = [&](const TCHAR *Title, bool Main, FOnClicked Click) {
        return SNew(SButton)
            .ButtonStyle(Main ? &Primary : &Secondary)
            .ContentPadding(FMargin(22, 11))
            .HAlign(HAlign_Left)
            .OnClicked(Click)[Label(Title, Action, Main ? Ink : Ivory)];
    };
    ResumeButton = Button(Opening ? TEXT("Start ride") : TEXT("Resume ride"), true,
                          FOnClicked::CreateSP(this, &SRideMenu::Resume));
    TSharedRef<SVerticalBox> Actions = SNew(SVerticalBox);
    Actions->AddSlot().AutoHeight()[ResumeButton.ToSharedRef()];
    Actions->AddSlot().AutoHeight().Padding(
        0, 5, 0, 0)[Button(TEXT("Controls"), false, FOnClicked::CreateSP(this, &SRideMenu::ToggleHelp))];
    if (!Opening)
        Actions->AddSlot().AutoHeight().Padding(
            0, 5, 0,
            0)[Button(TEXT("Restart circuit"), false, FOnClicked::CreateSP(this, &SRideMenu::ReturnToEntrance))];
    Actions->AddSlot().AutoHeight().Padding(
        0, 5, 0,
        0)[Button(Opening ? TEXT("Quit") : TEXT("Save and quit"), false, FOnClicked::CreateSP(this, &SRideMenu::Quit))];

    const FString Stage = Opening ? TEXT("MIDNIGHT CIRCUIT") : TEXT("RIDE PAUSED");
    const FString Keys = TEXT("W / S   or triggers     Throttle / brake\n")
                         TEXT("A / D   or left stick   Steer\n")
                         TEXT("Shift   or X / Square   Nitro\n")
                         TEXT("Mouse   or right stick  Look");

    TSharedRef<SHorizontalBox> Wordmark = SNew(SHorizontalBox);
    Wordmark->AddSlot().AutoWidth()[Label(TEXT("NEON "), Display, Ivory)];
    Wordmark->AddSlot().AutoWidth()[Label(TEXT("RIDE"), Display, Acid)];

    SAssignNew(Help,SBox).Visibility(EVisibility::Collapsed)
    [
        SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(.013f,.032f,.04f,.94f))
        .Padding(14)[Label(Keys,Mono,Ivory)]
    ];
    TSharedRef<SVerticalBox> Lockup = SNew(SVerticalBox);
    Lockup->AddSlot().AutoHeight()[Label(Stage,Mono,Acid)];
    Lockup->AddSlot().AutoHeight().Padding(0,-3,0,10)[Wordmark];
    Lockup->AddSlot().AutoHeight().Padding(0,0,0,19)[Label(TEXT("5.28 KM   /   FOUR DISTRICTS"),Mono,Ivory)];
    Lockup->AddSlot().AutoHeight().HAlign(HAlign_Left)[SNew(SBox).WidthOverride(222)[Actions]];
    Lockup->AddSlot().AutoHeight().Padding(0,9,0,0)[Help.ToSharedRef()];

    TSharedRef<SOverlay> Layout = SNew(SOverlay);
    Layout->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(70,0,0,75)[Lockup];
    Layout->AddSlot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(0,0,68,60)
        [Label(TEXT("VESPER  /  TEST RIDER"),Mono,Ivory)];
    ChildSlot
    [
        SNew(SOverlay)
        + SOverlay::Slot()
          [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Ink)]
        + SOverlay::Slot()
          [SNew(SScaleBox).Stretch(EStretch::ScaleToFill).Clipping(EWidgetClipping::ClipToBounds)
           [SNew(SImage).Image(&CoverBrush)]]
        + SOverlay::Slot()
          [SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1366).HeightOverride(768)[Layout]]]
    ];
}
FReply SRideMenu::OnPreviewKeyDown(const FGeometry &Geometry, const FKeyEvent &Event)
{
    if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right)
        return Resume();
    return SCompoundWidget::OnPreviewKeyDown(Geometry, Event);
}
FReply SRideMenu::ToggleHelp()
{
    if (Help.IsValid())
        Help->SetVisibility(Help->GetVisibility() == EVisibility::Collapsed ? EVisibility::Visible
                                                                            : EVisibility::Collapsed);
    return FReply::Handled();
}
FReply SRideMenu::Resume()
{
    if (Rider.IsValid())
        Rider->ClosePauseMenu();
    return FReply::Handled();
}
FReply SRideMenu::ReturnToEntrance()
{
    if (Rider.IsValid())
        Rider->ReturnFromMenu();
    return FReply::Handled();
}
FReply SRideMenu::Quit()
{
    if (Rider.IsValid())
        Rider->QuitFromMenu();
    return FReply::Handled();
}
