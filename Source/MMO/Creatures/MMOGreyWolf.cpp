// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/MMOGreyWolf.h"
#include "Combat/MMOHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

AMMOGreyWolf::AMMOGreyWolf()
{
	DisplayName = NSLOCTEXT("MMOCreature", "GreyWolf", "Grey Wolf");
	CreatureLevel = 1;

	GetHealth()->MaxHealth = 60.0f;
	AttackDamage = 6.0f;
	AttackRange = 90.0f;
	AttackCooldown = 2.0f;
	AggroRange = 900.0f;
	LeashRange = 2200.0f;
	ChaseSpeed = 420.0f;
	ReturnSpeed = 700.0f;
	XPReward = 40;
	CorpseDuration = 4.0f;
	RespawnDelay = 8.0f;

	AggroSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/MMO/Audio/S_MMO_WolfGrowl.S_MMO_WolfGrowl")));
	AttackHitSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/MMO/Audio/S_MMO_WolfBite.S_MMO_WolfBite")));
	DeathSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/MMO/Audio/S_MMO_WolfDeath.S_MMO_WolfDeath")));

	// capsule bottom sits at Z = -50 relative to the actor
	GetCapsuleComponent()->InitCapsuleSize(50.0f, 50.0f);

	// Placeholder body built from engine basic shapes (100cm, so scale 1.0 == 1m), jointed so it can be animated in code.
	// Assign a skeletal mesh + anim class in a Blueprint child to replace it.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));

	// linear color values (0.08 linear is a mid-dark grey on screen)
	const FLinearColor Fur(0.075f, 0.075f, 0.085f);
	const FLinearColor LightFur(0.17f, 0.165f, 0.155f);
	const FLinearColor DarkFur(0.028f, 0.028f, 0.034f);
	const FLinearColor Eye(1.0f, 0.42f, 0.02f);
	const FLinearColor Nose(0.004f, 0.004f, 0.004f);

	// torso
	AddBodyPart(TEXT("Body"), CubeMesh.Object, nullptr, FVector(0, 0, 7), FRotator::ZeroRotator, FVector(0.95f, 0.40f, 0.38f), Fur);
	AddBodyPart(TEXT("Chest"), CubeMesh.Object, nullptr, FVector(36, 0, 12), FRotator::ZeroRotator, FVector(0.40f, 0.46f, 0.46f), LightFur);
	AddBodyPart(TEXT("Haunch"), CubeMesh.Object, nullptr, FVector(-36, 0, 9), FRotator::ZeroRotator, FVector(0.36f, 0.44f, 0.42f), Fur);
	AddBodyPart(TEXT("Mane"), CubeMesh.Object, nullptr, FVector(22, 0, 28), FRotator(-12, 0, 0), FVector(0.42f, 0.36f, 0.12f), DarkFur);

	// head on a neck joint, with an articulated jaw
	HeadPivot = AddPivot(TEXT("HeadPivot"), nullptr, FVector(52, 0, 24));
	AddBodyPart(TEXT("Neck"), CubeMesh.Object, HeadPivot, FVector(6, 0, 2), FRotator(25, 0, 0), FVector(0.26f, 0.30f, 0.30f), Fur);
	AddBodyPart(TEXT("Head"), CubeMesh.Object, HeadPivot, FVector(20, 0, 9), FRotator::ZeroRotator, FVector(0.32f, 0.32f, 0.28f), Fur);
	AddBodyPart(TEXT("Snout"), CubeMesh.Object, HeadPivot, FVector(43, 0, 5), FRotator::ZeroRotator, FVector(0.24f, 0.17f, 0.09f), LightFur);
	AddBodyPart(TEXT("Nose"), SphereMesh.Object, HeadPivot, FVector(55, 0, 8), FRotator::ZeroRotator, FVector(0.07f), Nose);
	AddBodyPart(TEXT("EyeL"), SphereMesh.Object, HeadPivot, FVector(33, -9, 15), FRotator::ZeroRotator, FVector(0.055f), Eye);
	AddBodyPart(TEXT("EyeR"), SphereMesh.Object, HeadPivot, FVector(33, 9, 15), FRotator::ZeroRotator, FVector(0.055f), Eye);
	AddBodyPart(TEXT("EarL"), ConeMesh.Object, HeadPivot, FVector(14, -10, 30), FRotator(0, 0, -8), FVector(0.10f, 0.10f, 0.16f), DarkFur);
	AddBodyPart(TEXT("EarR"), ConeMesh.Object, HeadPivot, FVector(14, 10, 30), FRotator(0, 0, 8), FVector(0.10f, 0.10f, 0.16f), DarkFur);

	JawPivot = AddPivot(TEXT("JawPivot"), HeadPivot, FVector(32, 0, -1));
	AddBodyPart(TEXT("Jaw"), CubeMesh.Object, JawPivot, FVector(10, 0, -3), FRotator::ZeroRotator, FVector(0.22f, 0.15f, 0.06f), LightFur);

	// tail
	TailPivot = AddPivot(TEXT("TailPivot"), nullptr, FVector(-54, 0, 18));
	AddBodyPart(TEXT("Tail"), CylinderMesh.Object, TailPivot, FVector(-20, 0, -10), FRotator(-60, 0, 0), FVector(0.09f, 0.09f, 0.45f), DarkFur);

	// legs hang from hip/shoulder joints; the capsule bottom (paws) is at Z = -50
	const TCHAR* LegNames[] = { TEXT("FL"), TEXT("FR"), TEXT("BL"), TEXT("BR") };
	const FVector LegRoots[] = { FVector(36, -15, -12), FVector(36, 15, -12), FVector(-38, -15, -12), FVector(-38, 15, -12) };
	for (int32 i = 0; i < 4; ++i)
	{
		USceneComponent* Hip = AddPivot(*FString::Printf(TEXT("Hip%s"), LegNames[i]), nullptr, LegRoots[i]);
		AddBodyPart(*FString::Printf(TEXT("Leg%s"), LegNames[i]), CylinderMesh.Object, Hip, FVector(0, 0, -17), FRotator::ZeroRotator, FVector(0.12f, 0.12f, 0.34f), DarkFur);
		AddBodyPart(*FString::Printf(TEXT("Paw%s"), LegNames[i]), CubeMesh.Object, Hip, FVector(3, 0, -35), FRotator::ZeroRotator, FVector(0.15f, 0.12f, 0.06f), DarkFur);
		LegPivots.Add(Hip);
	}
}
