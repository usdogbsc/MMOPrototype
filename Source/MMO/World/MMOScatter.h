// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MMOScatter.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

/** One part of a scattered prop (e.g. a tree's trunk, or one layer of its canopy) */
USTRUCT(BlueprintType)
struct FMMOScatterPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter")
	TObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter")
	TObjectPtr<UMaterialInterface> Material;

	/** Offset from the prop's ground point (scaled with the prop) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter")
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter")
	FVector Scale = FVector::OneVector;

	/** Blocks pawns (trunks, rocks) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter")
	bool bBlocksPawns = false;

	/** Blocks the camera only, so it pulls in under canopies instead of clipping through them */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter")
	bool bBlocksCamera = false;
};

/**
 *  Scatters a multi-part prop (tree, rock, grass tuft...) across an area on the terrain using instanced meshes.
 *  Deterministic (seeded), keeps clear of roads and exclusion circles, and respects a minimum spacing.
 *  Up to 4 parts; each part becomes one hierarchical instanced mesh (one draw call per part).
 */
UCLASS()
class AMMOScatter : public AActor
{
	GENERATED_BODY()

public:

	AMMOScatter();

	/** Half size of the scatter rectangle around the actor (X/Y, cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter")
	FVector2D Extent = FVector2D(3000.0f, 3000.0f);

	/** Use an ellipse inside the rectangle instead of the full rectangle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter")
	bool bElliptical = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter", meta=(ClampMin=0, ClampMax=20000))
	int32 Count = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter")
	int32 Seed = 1;

	/** Minimum distance between props */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter", meta=(Units="cm"))
	float MinSpacing = 300.0f;

	/** Stay this far from road/trail centerlines (0 = allowed on roads) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter", meta=(Units="cm"))
	float RoadClearance = 400.0f;

	/** Areas to keep empty: X, Y = center, Z = radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter")
	TArray<FVector> Exclusions;

	/** Uniform scale range per prop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter")
	FVector2D ScaleRange = FVector2D(0.85f, 1.25f);

	/** Random lean in degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter")
	float MaxTilt = 3.0f;

	/** Skip spots steeper than this (0..1 of the up vector) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter", meta=(ClampMin=0, ClampMax=1))
	float MinGroundUp = 0.8f;

	/** Sink props slightly into the ground */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter", meta=(Units="cm"))
	float Sink = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter")
	TArray<FMMOScatterPart> Parts;

	/** Distance at which instances stop rendering (0 = never) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scatter", meta=(Units="cm"))
	float CullDistance = 0.0f;

	/** Re-scatters using the current settings */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Scatter")
	void Rebuild();

	/** Number of placed props after the last rebuild */
	UFUNCTION(BlueprintPure, Category="Scatter")
	int32 GetPlacedCount() const { return PlacedCount; }

	virtual void OnConstruction(const FTransform& Transform) override;

protected:

	static constexpr int32 MaxParts = 4;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> PartComponents;

	UPROPERTY(VisibleInstanceOnly, Category="Scatter")
	int32 PlacedCount = 0;
};
