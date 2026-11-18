#pragma once
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"

class ARidePawn;
class SButton;
class SCourierDispatch : public SCompoundWidget
{
  public:
    SLATE_BEGIN_ARGS(SCourierDispatch)
    {
    }
    SLATE_ARGUMENT(TWeakObjectPtr<ARidePawn>, Rider)
    SLATE_END_ARGS()
    void Construct(const FArguments &Args);
    TSharedPtr<SButton> FirstAction;
    virtual FReply OnPreviewKeyDown(const FGeometry &Geometry, const FKeyEvent &Key) override;

  private:
    TWeakObjectPtr<ARidePawn> Rider;
    FButtonStyle ActionStyle;
    void Build();
    FReply ParcelAction(int Job);
    FReply Navigate(int Stop);
    FReply Close();
    FReply NewManifest();
};
