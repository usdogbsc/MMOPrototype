// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MMODiscoveryZone.generated.h"

class UBoxComponent;
class USoundBase;

/**
 *  A named area of the world. Entering it shows its name; the first visit "discovers" it
 *  (banner + optional XP). Optionally plays an ambient loop while the player is inside.
 *  Detection is done by UMMOExplorationComponent (no overlap events needed).
 */
UCLASS()
class AMMODiscoveryZone : public AActor
{
	GENERATED_BODY()

	/** Zone bounds (box, may be rotated around Z) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> Bounds;

public:

	AMMODiscoveryZone();

	/** Stable id used to remember discovery */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zone")
	FName LocationId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zone")
	FText LocationName;

	/** Shown under the name, e.g. "Level 1-2" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zone")
	FText Subtitle;

	/** XP for discovering it (0 = none) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zone", meta=(ClampMin=0))
	int32 DiscoveryXP = 25;

	/** When zones overlap, the higher priority wins (use for small zones inside big ones) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zone")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zone|Audio")
	TSoftObjectPtr<USoundBase> AmbientLoop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zone|Audio", meta=(ClampMin=0))
	float AmbientVolume = 0.6f;

	/** Half size of the zone box */
	UFUNCTION(BlueprintCallable, Category="Zone")
	void SetExtent(const FVector& HalfSize);

	UFUNCTION(BlueprintPure, Category="Zone")
	bool ContainsPoint(const FVector& WorldLocation) const;

	/** Area on the ground (for picking the most specific zone) */
	float GetArea() const;
};
