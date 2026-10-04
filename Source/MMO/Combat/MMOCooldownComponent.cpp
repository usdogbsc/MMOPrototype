// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MMOCooldownComponent.h"
#include "Engine/World.h"

UMMOCooldownComponent::UMMOCooldownComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

double UMMOCooldownComponent::Now() const
{
	if (TimeSource)
	{
		return TimeSource();
	}
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

void UMMOCooldownComponent::StartCooldown(FName Key, float Duration)
{
	if (Key.IsNone() || Duration <= 0.0f)
	{
		return;
	}
	FCooldown& Cooldown = Cooldowns.FindOrAdd(Key);
	Cooldown.End = Now() + Duration;
	Cooldown.Duration = Duration;
}

void UMMOCooldownComponent::ClearCooldown(FName Key)
{
	Cooldowns.Remove(Key);
}

float UMMOCooldownComponent::GetRemaining(FName Key) const
{
	const FCooldown* Cooldown = Cooldowns.Find(Key);
	return Cooldown ? FMath::Max(0.0f, static_cast<float>(Cooldown->End - Now())) : 0.0f;
}

float UMMOCooldownComponent::GetDuration(FName Key) const
{
	const FCooldown* Cooldown = Cooldowns.Find(Key);
	return Cooldown && Cooldown->End > Now() ? Cooldown->Duration : 0.0f;
}
