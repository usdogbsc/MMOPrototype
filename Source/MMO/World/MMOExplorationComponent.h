// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMOExplorationComponent.generated.h"

class AMMODiscoveryZone;
class UAudioComponent;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMMOZoneChangedSignature, AMMODiscoveryZone*, NewZone);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMMOLocationDiscoveredSignature, AMMODiscoveryZone*, Zone, int32, XPAwarded);

/**
 *  Tracks which named zone the player is in, discovers zones on first visit (once per character state),
 *  and crossfades zone ambience. Discovered ids are kept in memory (no save system yet).
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class UMMOExplorationComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UMMOExplorationComponent();

	/** Quiet loop that plays everywhere outdoors (wind) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Exploration|Audio")
	TSoftObjectPtr<USoundBase> GlobalAmbientLoop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Exploration|Audio", meta=(ClampMin=0))
	float GlobalAmbientVolume = 0.25f;

	/** Crossfade time between zone ambiences */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Exploration|Audio", meta=(ClampMin=0, Units="s"))
	float AmbientFadeTime = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Exploration|Audio")
	TSoftObjectPtr<USoundBase> DiscoverySound;

	UPROPERTY(BlueprintAssignable, Category="Exploration")
	FMMOZoneChangedSignature OnZoneChanged;

	UPROPERTY(BlueprintAssignable, Category="Exploration")
	FMMOLocationDiscoveredSignature OnLocationDiscovered;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category="Exploration")
	bool HasDiscovered(FName LocationId) const { return Discovered.Contains(LocationId); }

	UFUNCTION(BlueprintPure, Category="Exploration")
	int32 GetDiscoveredCount() const { return Discovered.Num(); }

	UFUNCTION(BlueprintPure, Category="Exploration")
	AMMODiscoveryZone* GetCurrentZone() const { return CurrentZone.Get(); }

	/** Re-evaluates the current zone immediately (also runs every tick) */
	void UpdateZone();

protected:

	UPROPERTY(VisibleInstanceOnly, Category="Exploration")
	TSet<FName> Discovered;

	TWeakObjectPtr<AMMODiscoveryZone> CurrentZone;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ZoneAmbience;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> GlobalAmbience;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedDiscoverySound;

	void PlayZoneAmbience(AMMODiscoveryZone* Zone);
};
