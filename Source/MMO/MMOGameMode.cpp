// Copyright Epic Games, Inc. All Rights Reserved.

#include "MMOGameMode.h"
#include "Creatures/MMOCreature.h"
#include "Creatures/MMOGreyWolf.h"
#include "UI/MMOHUD.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "MMO.h"

AMMOGameMode::AMMOGameMode()
{
	HUDClass = AMMOHUD::StaticClass();

	PrototypeCreatureClass = AMMOGreyWolf::StaticClass();
	PrototypeCreatureOffsets = { FVector(1300.0f, -500.0f, 0.0f), FVector(1500.0f, 700.0f, 0.0f) };
}

void AMMOGameMode::StartPlay()
{
	Super::StartPlay();

	if (bSpawnPrototypeCreatures)
	{
		SpawnPrototypeCreatures();
	}
}

void AMMOGameMode::SpawnPrototypeCreatures()
{
	UWorld* World = GetWorld();
	if (!World || !PrototypeCreatureClass)
	{
		return;
	}

	// creatures placed by hand take priority over the auto-spawned ones
	if (TActorIterator<AMMOCreature>(World))
	{
		return;
	}

	const AActor* Start = FindPlayerStart(nullptr);
	if (!Start)
	{
		UE_LOG(LogMMO, Warning, TEXT("No player start found; skipping prototype creature spawn."));
		return;
	}

	const FVector Origin = Start->GetActorLocation();
	const FRotator Facing(0.0f, Start->GetActorRotation().Yaw, 0.0f);
	const float HalfHeight = PrototypeCreatureClass->GetDefaultObject<AMMOCreature>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	for (const FVector& Offset : PrototypeCreatureOffsets)
	{
		FVector Location = Origin + Facing.RotateVector(Offset);

		// drop onto the floor
		FHitResult Hit;
		const FVector TraceStart = Location + FVector(0.0f, 0.0f, 1000.0f);
		const FVector TraceEnd = Location - FVector(0.0f, 0.0f, 5000.0f);
		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility))
		{
			Location = Hit.ImpactPoint + FVector(0.0f, 0.0f, HalfHeight + 5.0f);
		}

		// face back toward the player start
		FVector ToStart = Origin - Location;
		ToStart.Z = 0.0f;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		if (AMMOCreature* Creature = World->SpawnActor<AMMOCreature>(PrototypeCreatureClass, Location, ToStart.Rotation(), Params))
		{
			UE_LOG(LogMMO, Log, TEXT("Spawned prototype creature %s at %s"), *Creature->GetName(), *Creature->GetActorLocation().ToCompactString());
		}
	}
}
