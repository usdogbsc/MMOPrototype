// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quests/MMOQuestDefinition.h"
#include "UObject/UObjectIterator.h"

UMMOQuestDefinition* UMMOQuestDefinition::FindById(FName InQuestId)
{
	if (InQuestId.IsNone())
	{
		return nullptr;
	}

	// ids are looked up often (quest progress, markers), so remember what we found
	static TMap<FName, TWeakObjectPtr<UMMOQuestDefinition>> Cache;
	if (const TWeakObjectPtr<UMMOQuestDefinition>* Cached = Cache.Find(InQuestId))
	{
		if (Cached->IsValid() && (*Cached)->QuestId == InQuestId)
		{
			return Cached->Get();
		}
		Cache.Remove(InQuestId);
	}

	for (TObjectIterator<UMMOQuestDefinition> It; It; ++It)
	{
		if (It->QuestId == InQuestId && !It->HasAnyFlags(RF_ClassDefaultObject))
		{
			Cache.Add(InQuestId, *It);
			return *It;
		}
	}

	const FString Name = FString::Printf(TEXT("DA_Quest_%s"), *InQuestId.ToString());
	UMMOQuestDefinition* Loaded = LoadObject<UMMOQuestDefinition>(nullptr, *FString::Printf(TEXT("/Game/MMO/Quests/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn);
	if (Loaded)
	{
		Cache.Add(InQuestId, Loaded);
	}
	return Loaded;
}
