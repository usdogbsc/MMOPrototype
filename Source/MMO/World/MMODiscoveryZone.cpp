// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMODiscoveryZone.h"
#include "Components/BoxComponent.h"
#include "Sound/SoundBase.h"

AMMODiscoveryZone::AMMODiscoveryZone()
{
	PrimaryActorTick.bCanEverTick = false;

	Bounds = CreateDefaultSubobject<UBoxComponent>(TEXT("Bounds"));
	RootComponent = Bounds;
	Bounds->SetBoxExtent(FVector(2000.0f, 2000.0f, 2000.0f));
	Bounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Bounds->SetCanEverAffectNavigation(false);
	Bounds->ShapeColor = FColor(255, 200, 60);
	Bounds->SetHiddenInGame(true);

	LocationName = NSLOCTEXT("MMOWorld", "Unnamed", "Unnamed Place");
}

void AMMODiscoveryZone::SetExtent(const FVector& HalfSize)
{
	Bounds->SetBoxExtent(HalfSize);
}

bool AMMODiscoveryZone::ContainsPoint(const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPositionNoScale(WorldLocation);
	const FVector Extent = Bounds->GetScaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}

float AMMODiscoveryZone::GetArea() const
{
	const FVector Extent = Bounds->GetScaledBoxExtent();
	return 4.0f * Extent.X * Extent.Y;
}
