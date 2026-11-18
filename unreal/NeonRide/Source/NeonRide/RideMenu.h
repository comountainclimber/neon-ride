#pragma once

#include "Widgets/SCompoundWidget.h"

class ARidePawn;
class SButton;

class SRideMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRideMenu) {}
        SLATE_ARGUMENT(TWeakObjectPtr<ARidePawn>, Rider)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    TSharedPtr<SButton> ResumeButton;
    virtual FReply OnPreviewKeyDown(const FGeometry& Geometry,const FKeyEvent& Key) override;
private:
    TWeakObjectPtr<ARidePawn> Rider;
    FReply Resume();
    FReply ReturnToEntrance();
    FReply Quit();
};
