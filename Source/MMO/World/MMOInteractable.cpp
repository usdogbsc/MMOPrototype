// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMOInteractable.h"
#include "GameFramework/Actor.h"

namespace MMOInteraction
{
	static TArray<TWeakObjectPtr<AActor>> Registered;

	void Register(AActor* Actor)
	{
		if (Actor && Cast<IMMOInteractable>(Actor))
		{
			Registered.AddUnique(Actor);
		}
	}

	void Unregister(AActor* Actor)
	{
		Registered.RemoveAll([Actor](const TWeakObjectPtr<AActor>& Entry) { return !Entry.IsValid() || Entry.Get() == Actor; });
	}

	TArray<AActor*> GetAll(const UWorld* World)
	{
		TArray<AActor*> Result;
		for (const TWeakObjectPtr<AActor>& Entry : Registered)
		{
			AActor* Actor = Entry.Get();
			if (Actor && Actor->GetWorld() == World && !Actor->IsHidden())
			{
				Result.Add(Actor);
			}
		}
		return Result;
	}
}
