#pragma once
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UObject/StrongObjectPtr.h"

class ARidePawn;
class SButton;
class UTexture2D;

class SRideMenu : public SCompoundWidget
{
  public:
    SLATE_BEGIN_ARGS(SRideMenu) : _Opening(false)
    {
    }
    SLATE_ARGUMENT(TWeakObjectPtr<ARidePawn>, Rider)
    SLATE_ARGUMENT(bool, Opening)
    SLATE_END_ARGS()
    void Construct(const FArguments &Args);
    TSharedPtr<SButton> ResumeButton;
    virtual FReply OnPreviewKeyDown(const FGeometry &Geometry, const FKeyEvent &Key) override;

  private:
    TWeakObjectPtr<ARidePawn> Rider;
    TStrongObjectPtr<UTexture2D> Cover;
    FSlateBrush CoverBrush;
    FButtonStyle Primary, Secondary;
    TSharedPtr<class SBox> Help;
    FReply ToggleHelp();
    FReply Resume();
    FReply ReturnToEntrance();
    FReply Quit();
};
