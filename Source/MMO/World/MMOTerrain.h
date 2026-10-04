// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MMOTerrain.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

/** A raised or lowered rounded area (hill, hollow, mound) */
USTRUCT(BlueprintType)
struct FMMOTerrainHill
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain")
	FVector2D Center = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain", meta=(Units="cm"))
	float Radius = 2000.0f;

	/** Peak height in cm (negative = hollow) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain", meta=(Units="cm"))
	float Height = 500.0f;
};

/** A circular area flattened to a fixed height (village, building pads) */
USTRUCT(BlueprintType)
struct FMMOTerrainFlat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain")
	FVector2D Center = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain", meta=(Units="cm"))
	float Radius = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain", meta=(Units="cm"))
	float Height = 0.0f;

	/** Ground color inside the area (alpha = strength) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain")
	FLinearColor Tint = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
};

/** A ground-color region (meadow, woods...) blended by distance to its center */
USTRUCT(BlueprintType)
struct FMMOTerrainBiome
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain")
	FVector2D Center = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain", meta=(Units="cm"))
	float Radius = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain")
	FLinearColor GrassColor = FLinearColor(0.2f, 0.4f, 0.1f);

	/** Extra rolling-hill noise amplitude in this region */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain", meta=(Units="cm"))
	float Roughness = 150.0f;
};

/**
 *  Sculpted heightfield terrain for prototype zones, generated from a few readable parameters:
 *  rolling noise, hills, flattened areas, roads (polylines carved and painted as dirt), biome colors,
 *  a north cliff and raised map edges that keep players inside. Colors are written to vertex colors.
 *  GetHeightAt is analytic, so props and creatures can be placed on the ground without traces.
 */
UCLASS()
class AMMOTerrain : public AActor
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UProceduralMeshComponent> Mesh;

public:

	AMMOTerrain();

	/** Terrain covers [Min, Max] in world X/Y (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Shape")
	FVector2D BoundsMin = FVector2D(-5000.0f, -9000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Shape")
	FVector2D BoundsMax = FVector2D(24000.0f, 9000.0f);

	/** Grid spacing in cm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Shape", meta=(ClampMin=50))
	float CellSize = 200.0f;

	/** Height of the raised rim along the map edges (keeps players in) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Shape", meta=(Units="cm"))
	float EdgeHeight = 3000.0f;

	/** Width of the band over which the rim rises */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Shape", meta=(Units="cm"))
	float EdgeWidth = 2500.0f;

	/** X beyond which a steep cliff rises (north wall, mine) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Shape", meta=(Units="cm"))
	float CliffStartX = 21500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Shape", meta=(Units="cm"))
	float CliffHeight = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Shape", meta=(Units="cm"))
	float CliffWidth = 1200.0f;

	/** Base rolling-noise amplitude everywhere */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Shape", meta=(Units="cm"))
	float BaseRoughness = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Shape")
	int32 Seed = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Shape")
	TArray<FMMOTerrainHill> Hills;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Shape")
	TArray<FMMOTerrainFlat> FlatAreas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Biomes")
	TArray<FMMOTerrainBiome> Biomes;

	/** Grass color where no biome applies */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Biomes")
	FLinearColor DefaultGrassColor = FLinearColor(0.16f, 0.32f, 0.07f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Biomes")
	FLinearColor RockColor = FLinearColor(0.2f, 0.19f, 0.18f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Roads")
	FLinearColor RoadColor = FLinearColor(0.33f, 0.24f, 0.13f);

	/** Each road is a polyline of points; roads are smoothed into the ground and painted as dirt */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Roads")
	TArray<FVector2D> MainRoad;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Roads")
	TArray<FVector2D> SideRoad;

	/** Narrow trail (less visible than roads) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Roads")
	TArray<FVector2D> Trail;

	/** A second, separate trail */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Roads")
	TArray<FVector2D> Trail2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Roads", meta=(Units="cm"))
	float RoadWidth = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain|Roads", meta=(Units="cm"))
	float TrailWidth = 220.0f;

	/** Vertex-color material */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain")
	TObjectPtr<UMaterialInterface> Material;

	/** Ground height (world Z, cm) at a world X/Y position */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Terrain")
	float GetHeightAt(float X, float Y) const;

	/** 0..1, how much a position is on a road or trail (1 = center of a road) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Terrain")
	float GetRoadAmount(float X, float Y) const;

	/** Distance to the nearest road/trail centerline (cm) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Terrain")
	float GetDistanceToRoad(float X, float Y) const;

	/** Rebuilds the mesh from the current parameters */
	UFUNCTION(BlueprintCallable, CallInEditor, Category="Terrain")
	void Rebuild();

	/** Finds the terrain in a world (first one) */
	static AMMOTerrain* Find(const UWorld* World);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

protected:

	float GetRawHeight(float X, float Y) const;
	float Noise(float X, float Y) const;
	FLinearColor GetGroundColor(float X, float Y, float Height, const FVector& Normal) const;

	static float DistanceToPolyline(const TArray<FVector2D>& Line, const FVector2D& P);
};
