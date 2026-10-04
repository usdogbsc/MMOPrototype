// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMOCooldownComponent.generated.h"

/**
 *  Named cooldowns (item groups such as "Potion", and later abilities).
 *  Times come from the world clock; a component without a world (unit tests) can be given a clock.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class UMMOCooldownComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UMMOCooldownComponent();

	void StartCooldown(FName Key, float Duration);
	void ClearCooldown(FName Key);

	bool IsReady(FName Key) const { return GetRemaining(Key) <= 0.0f; }
	float GetRemaining(FName Key) const;

	/** Full length of the cooldown currently running for Key (0 if none) */
	float GetDuration(FName Key) const;

	/** Overrides the clock (tests) */
	void SetTimeSource(TFunction<double()> InTimeSource) { TimeSource = MoveTemp(InTimeSource); }

protected:

	struct FCooldown
	{
		double End = 0.0;
		float Duration = 0.0f;
	};

	TMap<FName, FCooldown> Cooldowns;
	TFunction<double()> TimeSource;

	double Now() const;
};
