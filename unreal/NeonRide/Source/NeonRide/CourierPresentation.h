#pragma once
#include "CoreMinimal.h"

namespace CourierPresentation
{
inline const TCHAR *RoadName(int Road)
{
    static const TCHAR *Names[] = {TEXT("Midnight Ring"), TEXT("Rainline"), TEXT("Floodworks"), TEXT("Glass Heights"),
                                   TEXT("Aurora Ramp")};
    return Names[FMath::Clamp(Road, 0, 4)];
}
inline FLinearColor Colour(int Road)
{
    static const FLinearColor Colours[] = {
        {.11f, .72f, .90f}, {.12f, .81f, .67f}, {.98f, .47f, .08f}, {.78f, .38f, .95f}, {.94f, .79f, .29f}};
    return Colours[FMath::Clamp(Road, 0, 4)];
}
}
