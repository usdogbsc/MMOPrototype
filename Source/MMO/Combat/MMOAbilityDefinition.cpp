// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/MMOAbilityDefinition.h"
#include "UObject/UObjectIterator.h"

UMMOAbilityDefinition* UMMOAbilityDefinition::FindById(FName InAbilityId)
{
	if (InAbilityId.IsNone())
	{
		return nullptr;
	}

	static TMap<FName, TWeakObjectPtr<UMMOAbilityDefinition>> Cache;
	if (const TWeakObjectPtr<UMMOAbilityDefinition>* Cached = Cache.Find(InAbilityId))
	{
		if (Cached->IsValid() && (*Cached)->AbilityId == InAbilityId)
		{
			return Cached->Get();
		}
		Cache.Remove(InAbilityId);
	}

	for (TObjectIterator<UMMOAbilityDefinition> It; It; ++It)
	{
		if (It->AbilityId == InAbilityId && !It->HasAnyFlags(RF_ClassDefaultObject))
		{
			Cache.Add(InAbilityId, *It);
			return *It;
		}
	}

	const FString Name = FString::Printf(TEXT("DA_Ability_%s"), *InAbilityId.ToString());
	UMMOAbilityDefinition* Loaded = LoadObject<UMMOAbilityDefinition>(nullptr, *FString::Printf(TEXT("/Game/MMO/Abilities/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn);
	if (Loaded)
	{
		Cache.Add(InAbilityId, Loaded);
	}
	return Loaded;
}
