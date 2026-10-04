// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMOExplorationComponent.h"
#include "World/MMODiscoveryZone.h"
#include "Combat/MMOHealthComponent.h"
#include "Combat/MMOProgressionComponent.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "MMO.h"

UMMOExplorationComponent::UMMOExplorationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;

	GlobalAmbientLoop = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/MMO/Audio/Ambient/S_MMO_Amb_Wind.S_MMO_Amb_Wind")));
	DiscoverySound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/MMO/Audio/S_MMO_Discovery.S_MMO_Discovery")));
}

void UMMOExplorationComponent::BeginPlay()
{
	Super::BeginPlay();

	LoadedDiscoverySound = DiscoverySound.LoadSynchronous();

	// only the locally controlled player hears ambience
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (Pawn && Pawn->IsLocallyControlled())
	{
		if (USoundBase* Wind = GlobalAmbientLoop.LoadSynchronous())
		{
			GlobalAmbience = UGameplayStatics::CreateSound2D(this, Wind, GlobalAmbientVolume, 1.0f, 0.0f, nullptr, true, false);
			if (GlobalAmbience)
			{
				GlobalAmbience->FadeIn(AmbientFadeTime, GlobalAmbientVolume);
			}
		}
	}
}

void UMMOExplorationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (UAudioComponent* Audio : { ZoneAmbience.Get(), GlobalAmbience.Get() })
	{
		if (Audio)
		{
			Audio->Stop();
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UMMOExplorationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateZone();
}

void UMMOExplorationComponent::UpdateZone()
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// the most specific zone containing the player: highest priority, then smallest
	AMMODiscoveryZone* Best = nullptr;
	const FVector Location = Owner->GetActorLocation();
	for (TActorIterator<AMMODiscoveryZone> It(GetWorld()); It; ++It)
	{
		AMMODiscoveryZone* Zone = *It;
		if (!Zone->ContainsPoint(Location))
		{
			continue;
		}
		if (!Best || Zone->Priority > Best->Priority || (Zone->Priority == Best->Priority && Zone->GetArea() < Best->GetArea()))
		{
			Best = Zone;
		}
	}

	if (Best != CurrentZone.Get())
	{
		CurrentZone = Best;
		PlayZoneAmbience(Best);
		OnZoneChanged.Broadcast(Best);
	}

	if (!Best || Best->LocationId.IsNone() || Discovered.Contains(Best->LocationId))
	{
		return;
	}

	// a dead player discovers nothing yet; it happens once they are alive in the zone
	const UMMOHealthComponent* Health = Owner->FindComponentByClass<UMMOHealthComponent>();
	if (Health && Health->IsDead())
	{
		return;
	}

	Discovered.Add(Best->LocationId);
	if (Best->DiscoveryXP > 0)
	{
		if (UMMOProgressionComponent* Progression = Owner->FindComponentByClass<UMMOProgressionComponent>())
		{
			Progression->AddXP(Best->DiscoveryXP);
		}
	}
	if (LoadedDiscoverySound)
	{
		UGameplayStatics::PlaySound2D(this, LoadedDiscoverySound);
	}

	UE_LOG(LogMMO, Log, TEXT("Discovered %s (+%d XP)"), *Best->LocationName.ToString(), Best->DiscoveryXP);
	OnLocationDiscovered.Broadcast(Best, Best->DiscoveryXP);
}

void UMMOExplorationComponent::PlayZoneAmbience(AMMODiscoveryZone* Zone)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	if (ZoneAmbience)
	{
		ZoneAmbience->FadeOut(AmbientFadeTime, 0.0f);
		ZoneAmbience = nullptr;
	}

	USoundBase* Loop = Zone ? Zone->AmbientLoop.LoadSynchronous() : nullptr;
	if (Loop)
	{
		ZoneAmbience = UGameplayStatics::CreateSound2D(this, Loop, Zone->AmbientVolume, 1.0f, 0.0f, nullptr, false, true);
		if (ZoneAmbience)
		{
			ZoneAmbience->FadeIn(AmbientFadeTime, Zone->AmbientVolume);
		}
	}
}
