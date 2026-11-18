#pragma once
#include "CoreMinimal.h"

class ASkyway;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
namespace CourierStreets
{
void Build(ASkyway &World, UProceduralMeshComponent &Asphalt, UInstancedStaticMeshComponent &Steel,
           UInstancedStaticMeshComponent &Edge, UInstancedStaticMeshComponent &Marker);
}
