#include "CircuitPracticals.h"
#include "CircuitCore.h"
#include "CourierCore.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Dom/JsonValue.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
    constexpr int32 MaxActiveLamps = 8;
    constexpr double LampRangeCm = 5400.;
    const FColor LensColors[] = {
        FColor(146,237,255), FColor(255,187,128), FColor(255,113,167), FColor(131,255,224)
    };
    const FColor SignColors[] = {
        FColor(162,255,225), FColor(255,172,113), FColor(252,129,178), FColor(135,207,255)
    };
    const TCHAR* const Sectors[] = {
        TEXT("RAINLINE"), TEXT("GLASS HEIGHTS"), TEXT("VIOLET REACH"), TEXT("LOWER RING")
    };

    UInstancedStaticMeshComponent* Batch(AActor& District, FName Name, UStaticMesh* Mesh,
        UMaterialInterface* Material)
    {
        auto* Layer = NewObject<UInstancedStaticMeshComponent>(&District,Name);
        District.AddInstanceComponent(Layer);
        Layer->SetupAttachment(District.GetRootComponent());
        Layer->SetMobility(EComponentMobility::Movable);
        Layer->SetStaticMesh(Mesh);
        Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Layer->SetCanEverAffectNavigation(false);
        Layer->SetCastShadow(false);
        if (Material) Layer->SetMaterial(0,Material);
        Layer->RegisterComponent();
        return Layer;
    }

    UMaterialInstanceDynamic* Tint(AActor& District, UMaterialInterface* Base, FLinearColor Color)
    {
        if (!Base) return nullptr;
        auto* Material = UMaterialInstanceDynamic::Create(Base,&District);
        Material->SetVectorParameterValue(TEXT("Color"),Color);
        return Material;
    }

    void SignLabel(AActor& District, const FVector& Position, const FRotator& Heading, double Size, int32 Index)
    {
        auto* Text = NewObject<UTextRenderComponent>(&District);
        District.AddInstanceComponent(Text);
        Text->SetupAttachment(District.GetRootComponent());
        Text->SetMobility(EComponentMobility::Movable);
        Text->SetRelativeLocationAndRotation(Position,Heading);
        Text->SetWorldSize(Size*.038);
        Text->SetHorizontalAlignment(EHTA_Center);
        Text->SetVerticalAlignment(EVRTA_TextCenter);
        Text->SetTextRenderColor(SignColors[Index%4]);
        Text->SetText(FText::FromString(FString::Printf(TEXT("NR / %02d    %s"),Index%6+1,Sectors[Index%4])));
        Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Text->SetCastShadow(false);
        Text->SetCullDistance(15500.f);
        Text->RegisterComponent();
    }
}

UCircuitPracticals::UCircuitPracticals()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = .12f;
}

void UCircuitPracticals::Populate(AActor& District, const TArray<TSharedPtr<FJsonValue>>& Placements)
{
    auto* Cube = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Plane = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane"));
    auto* Neon = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Environment/M_Neon.M_Neon"));
    if (!Cube || !Plane || !Neon) return;

    auto* Component = NewObject<UCircuitPracticals>(&District);
    District.AddInstanceComponent(Component);
    auto* Backing = Batch(District,TEXT("SignFrames"),Cube,Tint(District,Cube->GetMaterial(0),FLinearColor(.024f,.045f,.061f)));
    UInstancedStaticMeshComponent* Lenses[4];
    UInstancedStaticMeshComponent* Trim[4];
    UInstancedStaticMeshComponent* Glyphs[6];
    for (int32 I=0; I<4; ++I)
    {
        Lenses[I] = Batch(District,*FString::Printf(TEXT("LampLenses%d"),I),Cube,
            Tint(District,Neon,FLinearColor::FromSRGBColor(LensColors[I])));
        Trim[I] = Batch(District,*FString::Printf(TEXT("SignTrim%d"),I),Cube,
            Tint(District,Neon,FLinearColor::FromSRGBColor(SignColors[I])*.4f));
    }
    for (int32 I=0; I<6; ++I)
    {
        const FString Path=FString::Printf(TEXT("/Game/Environment/M_Glyph%d.M_Glyph%d"),I+1,I+1);
        Glyphs[I] = Batch(District,*FString::Printf(TEXT("SignGlyphs%d"),I),Plane,
            LoadObject<UMaterialInterface>(nullptr,*Path));
    }

    auto* Brackets = Batch(District,TEXT("OuterVergeBrackets"),Cube,
        Tint(District,Cube->GetMaterial(0),FLinearColor(.12f,.16f,.21f)));
    TArray<Circuit::Frame> RoadSamples;
    for(int Road=0;Road<Courier::RoadCount;++Road)
        for(double Along=0.;Along<Courier::RoadLength(Road);Along+=4.)
            RoadSamples.Add(Circuit::At(Courier::RoadStart(Road)+Along));
    auto Beam=[&](FVector From,FVector To,double WidthCm,double DepthCm)
    {
        const FVector Delta=To-From;
        Brackets->AddInstance(FTransform(Delta.Rotation(),(From+To)*.5,
            FVector(Delta.Size()/100.,WidthCm/100.,DepthCm/100.)));
    };
    auto InstallBracket=[&](const FString& Key,const FVector& Base,double Size,const FRotator& Yaw)
    {
        const Circuit::Frame* Near=&RoadSamples[0];
        double Closest=TNumericLimits<double>::Max();
        for(const auto& Sample:RoadSamples)
        {
            const double Dx=Base.X*.01-Sample.x, Dy=Base.Y*.01-Sample.y;
            const double Dz=Base.Z*.01-Sample.z;
            const double D2=Dx*Dx+Dy*Dy+Dz*Dz;
            if(D2<Closest){Near=&Sample;Closest=D2;}
        }
        const double Signed=(Base.X*.01-Near->x)*Near->rx+(Base.Y*.01-Near->y)*Near->ry;
        const double Offset=Signed<0.?-6.72:6.72;
        const FVector Lip=FVector(Near->x+Near->rx*Offset,Near->y+Near->ry*Offset,
            Near->z-Offset*FMath::Sin(Near->bank)-.56)*100.;
        const double Width=Key.StartsWith(TEXT("lamp"))?130.:FMath::Min(1000.,Size*.65);
        Brackets->AddInstance(FTransform(Yaw,Base-FVector(0,0,16),FVector(Width/100.,Width/100.,.32)));
        const FVector Tip=Base-FVector(0,0,34);
        Beam(Lip,Tip,FMath::Max(65.,Width*.36),34.);
        Beam(Lip-FVector(0,0,130),FMath::Lerp(Lip,Tip,.83),26.,25.);
    };
    int32 SignIndex = 0;
    for (const auto& Value : Placements)
    {
        const auto& P = Value->AsArray();
        if (P.Num()!=6) continue;
        const FString Key=P[0]->AsString();
        if (Key.StartsWith(TEXT("tower"))) continue;
        const FVector Base=FVector(P[1]->AsNumber(),P[2]->AsNumber(),P[3]->AsNumber())*100.;
        const double Size=P[4]->AsNumber()*100.;
        const FRotator Yaw(0,FMath::RadiansToDegrees(P[5]->AsNumber()),0);
        InstallBracket(Key,Base,Size,Yaw);
        if (!Key.StartsWith(TEXT("lamp")) && !Key.StartsWith(TEXT("sign"))) continue;
        const FVector Along=Yaw.Vector();
        const FVector Inward(-Along.Y,Along.X,0.);

        if (Key.StartsWith(TEXT("lamp")))
        {
            const int32 Variant=(Component->Lamps.Num()/30)%4;
            const FVector Head=Base+FVector::UpVector*(.90*Size);
            Lenses[Variant]->AddInstance(FTransform(Yaw,Head,FVector(.36,.64,.15)));
            const FRotator Aim=(Inward*610.-FVector::UpVector*(.90*Size)).Rotation();
            Component->Lamps.Add({Head,Aim,FLinearColor::FromSRGBColor(LensColors[Variant])});
            continue;
        }

        // Sidewalk-mounted frames face approaching traffic. All solid geometry stays at its manifest position.
        const FVector Center=Base+FVector::UpVector*(Size*1.30);
        const double Width=Size*.48;
        const FVector Front=Center+Inward*10.;
        auto Box=[&](UInstancedStaticMeshComponent* Layer, const FVector& CenterCm,const FVector& ExtentsCm)
        {
            Layer->AddInstance(FTransform(Yaw,CenterCm,ExtentsCm/100.));
        };
        Box(Backing,Base+FVector::UpVector*(Size*.70),FVector(20.,26.,Size*1.4));
        Box(Backing,Center,FVector(Width,16.,Size));
        auto* Border=Trim[SignIndex%4];
        for (const int Side : {-1,1})
        {
            Box(Border,Front+Along*(Side*.49*Width),FVector(Width*.012,3.,Size*.99));
            Box(Border,Front+FVector::UpVector*(Side*.49*Size),FVector(Width,3.,Size*.008));
        }
        // BasicShapes/Plane is an XY card with a +Z normal. Its U axis follows the sign's long horizontal edge.
        const FQuat Face=FRotationMatrix::MakeFromXY(Along,-FVector::UpVector).ToQuat();
        for (int32 Row=0; Row<3; ++Row)
        {
            const FVector Position=Front+FVector::UpVector*((.24-.20*Row)*Size);
            Glyphs[(SignIndex+Row)%6]->AddInstance(FTransform(Face,Position,FVector(Width*.87/100.,Size*.15/100.,1.)));
        }
        SignLabel(District,Front-FVector::UpVector*(Size*.33),Inward.Rotation(),Size,SignIndex);
        ++SignIndex;
    }

    const int32 Count = FMath::Min(MaxActiveLamps,Component->Lamps.Num());
    for (int32 I=0; I<Count; ++I)
    {
        auto* Light = NewObject<USpotLightComponent>(&District);
        District.AddInstanceComponent(Light);
        Light->SetupAttachment(District.GetRootComponent());
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetVisibility(false);
        Light->SetCastShadows(false);
        Light->SetIntensityUnits(ELightUnits::Lumens);
        Light->SetIntensity(4800.f);
        Light->SetSourceRadius(17.f);
        Light->SetAttenuationRadius(1800.f);
        Light->SetInnerConeAngle(24.f);
        Light->SetOuterConeAngle(54.f);
        Light->RegisterComponent();
        Component->Lights.Add(Light);
        Component->ActiveLampIndices.Add(INDEX_NONE);
    }
    Component->RegisterComponent();
    UE_LOG(LogTemp,Display,TEXT("Circuit practicals: %d lenses, %d signs, %d pooled lights"),
        Component->Lamps.Num(),SignIndex,Count);
}

void UCircuitPracticals::TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaTime,TickType,TickFunction);
    const auto* Player = GetWorld()->GetFirstPlayerController();
    if (!Player || !Player->PlayerCameraManager) return;
    const FVector Camera = GetOwner()->GetActorTransform().InverseTransformPosition(Player->PlayerCameraManager->GetCameraLocation());
    TArray<TPair<double,int32>,TInlineAllocator<16>> Nearby;
    for (int32 I=0; I<Lamps.Num(); ++I)
    {
        const double Distance=FVector::DistSquared(Camera,Lamps[I].Position);
        if (Distance<LampRangeCm*LampRangeCm) Nearby.Emplace(Distance,I);
    }
    Nearby.Sort([](const auto& A,const auto& B){ return A.Key<B.Key; });
    for (int32 I=0; I<Lights.Num(); ++I)
    {
        const int32 Next = Nearby.IsValidIndex(I) ? Nearby[I].Value : INDEX_NONE;
        if (ActiveLampIndices[I]==Next) continue;
        auto* Light = Lights[I].Get();
        if (Next!=INDEX_NONE)
        {
            const auto& Lamp = Lamps[Next];
            Light->SetRelativeLocationAndRotation(Lamp.Position,Lamp.Aim);
            Light->SetLightColor(Lamp.Color);
        }
        Light->SetVisibility(Next!=INDEX_NONE);
        ActiveLampIndices[I]=Next;
    }
}
