// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMOTerrain.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace MMOTerrainMath
{
	static float SmoothStep(float A, float B, float X)
	{
		const float T = FMath::Clamp((X - A) / (B - A), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	static float Hash(int32 X, int32 Y, int32 Seed)
	{
		uint32 H = static_cast<uint32>(X) * 374761393u + static_cast<uint32>(Y) * 668265263u + static_cast<uint32>(Seed) * 2246822519u;
		H = (H ^ (H >> 13)) * 1274126177u;
		H ^= H >> 16;
		return (H & 0xFFFFFF) / static_cast<float>(0xFFFFFF);
	}

	/** Smooth value noise in [-1, 1] */
	static float ValueNoise(float X, float Y, int32 Seed)
	{
		const int32 X0 = FMath::FloorToInt(X);
		const int32 Y0 = FMath::FloorToInt(Y);
		const float FX = X - X0;
		const float FY = Y - Y0;
		const float SX = FX * FX * (3.0f - 2.0f * FX);
		const float SY = FY * FY * (3.0f - 2.0f * FY);
		const float A = FMath::Lerp(Hash(X0, Y0, Seed), Hash(X0 + 1, Y0, Seed), SX);
		const float B = FMath::Lerp(Hash(X0, Y0 + 1, Seed), Hash(X0 + 1, Y0 + 1, Seed), SX);
		return FMath::Lerp(A, B, SY) * 2.0f - 1.0f;
	}
}

AMMOTerrain::AMMOTerrain()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->bUseComplexAsSimpleCollision = true;
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetCanEverAffectNavigation(true);
	Mesh->CastShadow = true;
}

AMMOTerrain* AMMOTerrain::Find(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<AMMOTerrain> It(const_cast<UWorld*>(World)); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AMMOTerrain::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// the terrain is authored in world space
	SetActorTransform(FTransform::Identity);
	Rebuild();
}

void AMMOTerrain::BeginPlay()
{
	Super::BeginPlay();

	// regenerate at runtime so collision (and the navmesh built from it) always matches the parameters
	Rebuild();
}

float AMMOTerrain::Noise(float X, float Y) const
{
	using namespace MMOTerrainMath;
	// three octaves of rolling hills (~60m, ~25m, ~10m)
	return ValueNoise(X / 6000.0f, Y / 6000.0f, Seed) * 0.6f
		+ ValueNoise(X / 2500.0f, Y / 2500.0f, Seed + 11) * 0.3f
		+ ValueNoise(X / 1000.0f, Y / 1000.0f, Seed + 23) * 0.1f;
}

float AMMOTerrain::DistanceToPolyline(const TArray<FVector2D>& Line, const FVector2D& P)
{
	float Best = TNumericLimits<float>::Max();
	for (int32 i = 0; i + 1 < Line.Num(); ++i)
	{
		const FVector2D A = Line[i];
		const FVector2D AB = Line[i + 1] - A;
		const float T = FMath::Clamp(FVector2D::DotProduct(P - A, AB) / FMath::Max(1.0f, AB.SizeSquared()), 0.0f, 1.0f);
		Best = FMath::Min(Best, FVector2D::Distance(P, A + AB * T));
	}
	return Best;
}

float AMMOTerrain::GetDistanceToRoad(float X, float Y) const
{
	const FVector2D P(X, Y);
	return FMath::Min(FMath::Min3(DistanceToPolyline(MainRoad, P), DistanceToPolyline(SideRoad, P), DistanceToPolyline(Trail, P)), DistanceToPolyline(Trail2, P));
}

float AMMOTerrain::GetRoadAmount(float X, float Y) const
{
	using namespace MMOTerrainMath;
	const FVector2D P(X, Y);
	const float Road = 1.0f - SmoothStep(RoadWidth * 0.35f, RoadWidth * 0.5f, FMath::Min(DistanceToPolyline(MainRoad, P), DistanceToPolyline(SideRoad, P)));
	const float TrailAmount = 0.75f * (1.0f - SmoothStep(TrailWidth * 0.3f, TrailWidth * 0.5f, FMath::Min(DistanceToPolyline(Trail, P), DistanceToPolyline(Trail2, P))));
	return FMath::Max(Road, TrailAmount);
}

float AMMOTerrain::GetRawHeight(float X, float Y) const
{
	using namespace MMOTerrainMath;
	const FVector2D P(X, Y);

	// region roughness: the base amount plus each biome's extra, blended by distance
	float Roughness = BaseRoughness;
	for (const FMMOTerrainBiome& Biome : Biomes)
	{
		const float W = 1.0f - SmoothStep(Biome.Radius * 0.6f, Biome.Radius, FVector2D::Distance(P, Biome.Center));
		Roughness += Biome.Roughness * W;
	}

	// roads smooth out the bumps under them
	const float RoadAmount = GetRoadAmount(X, Y);
	float H = Noise(X, Y) * Roughness * (1.0f - 0.75f * RoadAmount);

	for (const FMMOTerrainHill& Hill : Hills)
	{
		const float D = FVector2D::Distance(P, Hill.Center) / FMath::Max(1.0f, Hill.Radius);
		if (D < 1.0f)
		{
			// smooth dome that meets the ground with zero slope
			const float T = 1.0f - D * D;
			H += Hill.Height * T * T;
		}
	}

	// flattened areas (village, pads): blend toward their height
	for (const FMMOTerrainFlat& Flat : FlatAreas)
	{
		const float D = FVector2D::Distance(P, Flat.Center);
		const float W = 1.0f - SmoothStep(Flat.Radius * 0.75f, Flat.Radius, D);
		H = FMath::Lerp(H, Flat.Height, W);
	}

	// north cliff
	H += SmoothStep(CliffStartX, CliffStartX + CliffWidth, X) * CliffHeight;

	// raised rim along the edges
	const float EdgeDistance = FMath::Min(FMath::Min(X - BoundsMin.X, BoundsMax.X - X), FMath::Min(Y - BoundsMin.Y, BoundsMax.Y - Y));
	if (EdgeDistance < EdgeWidth)
	{
		const float T = 1.0f - FMath::Max(0.0f, EdgeDistance) / EdgeWidth;
		H += T * T * EdgeHeight * (0.85f + 0.3f * (Noise(X * 0.5f, Y * 0.5f) * 0.5f + 0.5f));
	}

	return H;
}

float AMMOTerrain::GetHeightAt(float X, float Y) const
{
	return GetRawHeight(X, Y);
}

FLinearColor AMMOTerrain::GetGroundColor(float X, float Y, float Height, const FVector& Normal) const
{
	using namespace MMOTerrainMath;
	const FVector2D P(X, Y);

	// grass: blend biome colors by distance
	FLinearColor Grass = DefaultGrassColor;
	for (const FMMOTerrainBiome& Biome : Biomes)
	{
		const float W = 1.0f - SmoothStep(Biome.Radius * 0.5f, Biome.Radius, FVector2D::Distance(P, Biome.Center));
		Grass = FLinearColor::LerpUsingHSV(Grass, Biome.GrassColor, W);
	}

	// patchy variation so large fields don't look flat
	const float Patch = ValueNoise(X / 900.0f, Y / 900.0f, Seed + 101);
	Grass = Grass * (1.0f + Patch * 0.12f);
	const float Dry = FMath::Max(0.0f, ValueNoise(X / 2200.0f, Y / 2200.0f, Seed + 202)) * 0.35f;
	Grass = FMath::Lerp(Grass, FLinearColor(0.32f, 0.3f, 0.1f), Dry);

	FLinearColor Color = Grass;

	// flattened areas can carry their own ground tint (village square)
	for (const FMMOTerrainFlat& Flat : FlatAreas)
	{
		if (Flat.Tint.A > 0.0f)
		{
			const float W = (1.0f - SmoothStep(Flat.Radius * 0.55f, Flat.Radius * 0.85f, FVector2D::Distance(P, Flat.Center))) * Flat.Tint.A;
			Color = FMath::Lerp(Color, FLinearColor(Flat.Tint.R, Flat.Tint.G, Flat.Tint.B), W);
		}
	}

	// roads
	const float Road = GetRoadAmount(X, Y);
	FLinearColor Dirt = RoadColor * (1.0f + ValueNoise(X / 300.0f, Y / 300.0f, Seed + 303) * 0.1f);
	Color = FMath::Lerp(Color, Dirt, Road);

	// rock on steep slopes
	const float Slope = 1.0f - Normal.Z;
	const float Rock = SmoothStep(0.18f, 0.32f, Slope);
	Color = FMath::Lerp(Color, RockColor * (1.0f + Patch * 0.15f), Rock);

	Color.A = 1.0f;
	return Color;
}

void AMMOTerrain::Rebuild()
{
	if (!Mesh)
	{
		return;
	}

	const int32 NumX = FMath::Clamp(FMath::CeilToInt((BoundsMax.X - BoundsMin.X) / CellSize) + 1, 2, 600);
	const int32 NumY = FMath::Clamp(FMath::CeilToInt((BoundsMax.Y - BoundsMin.Y) / CellSize) + 1, 2, 600);
	const float StepX = (BoundsMax.X - BoundsMin.X) / (NumX - 1);
	const float StepY = (BoundsMax.Y - BoundsMin.Y) / (NumY - 1);

	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	Vertices.SetNumUninitialized(NumX * NumY);
	for (int32 iy = 0; iy < NumY; ++iy)
	{
		for (int32 ix = 0; ix < NumX; ++ix)
		{
			const float X = BoundsMin.X + ix * StepX;
			const float Y = BoundsMin.Y + iy * StepY;
			Vertices[iy * NumX + ix] = FVector(X, Y, GetRawHeight(X, Y));
		}
	}

	Normals.SetNumUninitialized(Vertices.Num());
	Colors.SetNumUninitialized(Vertices.Num());
	UVs.SetNumUninitialized(Vertices.Num());
	for (int32 iy = 0; iy < NumY; ++iy)
	{
		for (int32 ix = 0; ix < NumX; ++ix)
		{
			const int32 i = iy * NumX + ix;
			const FVector& V = Vertices[i];
			const float HL = Vertices[iy * NumX + FMath::Max(0, ix - 1)].Z;
			const float HR = Vertices[iy * NumX + FMath::Min(NumX - 1, ix + 1)].Z;
			const float HD = Vertices[FMath::Max(0, iy - 1) * NumX + ix].Z;
			const float HU = Vertices[FMath::Min(NumY - 1, iy + 1) * NumX + ix].Z;
			const FVector Normal = FVector(-(HR - HL) / (2.0f * StepX), -(HU - HD) / (2.0f * StepY), 1.0f).GetSafeNormal();
			Normals[i] = Normal;
			Colors[i] = GetGroundColor(V.X, V.Y, V.Z, Normal);
			UVs[i] = FVector2D(V.X / 400.0f, V.Y / 400.0f);
		}
	}

	Triangles.Reserve((NumX - 1) * (NumY - 1) * 6);
	for (int32 iy = 0; iy + 1 < NumY; ++iy)
	{
		for (int32 ix = 0; ix + 1 < NumX; ++ix)
		{
			const int32 A = iy * NumX + ix;
			const int32 B = A + 1;
			const int32 C = A + NumX;
			const int32 D = C + 1;
			// counter-clockwise when seen from above (+Z)
			Triangles.Append({ A, C, B, B, C, D });
		}
	}

	Mesh->ClearAllMeshSections();
	Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, true);
	if (Material)
	{
		Mesh->SetMaterial(0, Material);
	}
}
