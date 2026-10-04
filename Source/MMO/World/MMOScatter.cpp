// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMOScatter.h"
#include "World/MMOTerrain.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

AMMOScatter::AMMOScatter()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	for (int32 i = 0; i < MaxParts; ++i)
	{
		UHierarchicalInstancedStaticMeshComponent* Part = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(*FString::Printf(TEXT("Part%d"), i));
		Part->SetupAttachment(RootComponent);
		Part->SetMobility(EComponentMobility::Static);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PartComponents.Add(Part);
	}
}

void AMMOScatter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Rebuild();
}

void AMMOScatter::Rebuild()
{
	const AMMOTerrain* Terrain = AMMOTerrain::Find(GetWorld());
	const FVector Origin = GetActorLocation();

	// if the terrain isn't available yet (e.g. while the level is still loading) keep the saved instances
	if (!Terrain && PlacedCount > 0)
	{
		return;
	}

	// instances are written in world space; keep the root unrotated/unscaled
	RootComponent->SetWorldRotation(FRotator::ZeroRotator);
	RootComponent->SetWorldScale3D(FVector::OneVector);

	for (int32 i = 0; i < PartComponents.Num(); ++i)
	{
		UHierarchicalInstancedStaticMeshComponent* Component = PartComponents[i];
		Component->ClearInstances();

		const bool bUsed = Parts.IsValidIndex(i) && Parts[i].Mesh;
		Component->SetVisibility(bUsed);
		if (!bUsed)
		{
			continue;
		}

		const FMMOScatterPart& Part = Parts[i];
		Component->SetStaticMesh(Part.Mesh);
		Component->SetMaterial(0, Part.Material);
		Component->SetCullDistances(0, CullDistance > 0.0f ? FMath::RoundToInt(CullDistance) : 0);

		if (Part.bBlocksPawns)
		{
			Component->SetCollisionProfileName(TEXT("BlockAll"));
			Component->SetCanEverAffectNavigation(true);
		}
		else if (Part.bBlocksCamera)
		{
			Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Component->SetCollisionResponseToAllChannels(ECR_Ignore);
			Component->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
			Component->SetCanEverAffectNavigation(false);
		}
		else
		{
			Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Component->SetCanEverAffectNavigation(false);
		}
	}

	FRandomStream Random(Seed);
	TArray<FVector2D> Placed;
	const int32 MaxAttempts = Count * 12;

	for (int32 Attempt = 0; Attempt < MaxAttempts && Placed.Num() < Count; ++Attempt)
	{
		FVector2D Local(Random.FRandRange(-1.0f, 1.0f), Random.FRandRange(-1.0f, 1.0f));
		if (bElliptical && Local.SizeSquared() > 1.0f)
		{
			continue;
		}

		const FVector2D P(Origin.X + Local.X * Extent.X, Origin.Y + Local.Y * Extent.Y);

		bool bBlocked = false;
		for (const FVector& Exclusion : Exclusions)
		{
			if (FVector2D::Distance(P, FVector2D(Exclusion.X, Exclusion.Y)) < Exclusion.Z)
			{
				bBlocked = true;
				break;
			}
		}
		if (bBlocked || (Terrain && RoadClearance > 0.0f && Terrain->GetDistanceToRoad(P.X, P.Y) < RoadClearance))
		{
			continue;
		}

		const float SpacingSq = FMath::Square(MinSpacing);
		if (Placed.ContainsByPredicate([&P, SpacingSq](const FVector2D& Other) { return FVector2D::DistSquared(P, Other) < SpacingSq; }))
		{
			continue;
		}

		float Z = Origin.Z;
		if (Terrain)
		{
			Z = Terrain->GetHeightAt(P.X, P.Y);
			const float Dx = Terrain->GetHeightAt(P.X + 100.0f, P.Y) - Terrain->GetHeightAt(P.X - 100.0f, P.Y);
			const float Dy = Terrain->GetHeightAt(P.X, P.Y + 100.0f) - Terrain->GetHeightAt(P.X, P.Y - 100.0f);
			if (FVector(-Dx / 200.0f, -Dy / 200.0f, 1.0f).GetSafeNormal().Z < MinGroundUp)
			{
				continue;
			}
		}

		Placed.Add(P);

		const float Scale = Random.FRandRange(ScaleRange.X, ScaleRange.Y);
		const FRotator Base(Random.FRandRange(-MaxTilt, MaxTilt), Random.FRandRange(0.0f, 360.0f), Random.FRandRange(-MaxTilt, MaxTilt));
		const FTransform Prop(Base, FVector(P.X, P.Y, Z - Sink), FVector(Scale));

		for (int32 i = 0; i < PartComponents.Num() && i < Parts.Num(); ++i)
		{
			if (!Parts[i].Mesh)
			{
				continue;
			}
			const FTransform PartTransform(Parts[i].Rotation, Parts[i].Offset, Parts[i].Scale);
			PartComponents[i]->AddInstance(PartTransform * Prop, true);
		}
	}

	PlacedCount = Placed.Num();
}
