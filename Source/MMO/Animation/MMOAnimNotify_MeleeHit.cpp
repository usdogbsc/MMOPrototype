// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/MMOAnimNotify_MeleeHit.h"
#include "Animation/AnimSequenceBase.h"
#include "Combat/MMOMeleeAttacker.h"
#include "Components/SkeletalMeshComponent.h"

void UMMOAnimNotify_MeleeHit::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (IMMOMeleeAttacker* Attacker = Cast<IMMOMeleeAttacker>(MeshComp ? MeshComp->GetOwner() : nullptr))
	{
		Attacker->NotifyMeleeHitFrame();
	}
}

FString UMMOAnimNotify_MeleeHit::GetNotifyName_Implementation() const
{
	return TEXT("MMO Melee Hit");
}

float UMMOAnimNotify_MeleeHit::FindHitTime(const UAnimSequenceBase* Animation)
{
	if (!Animation)
	{
		return -1.0f;
	}

	for (const FAnimNotifyEvent& Event : Animation->Notifies)
	{
		if (Event.Notify && Event.Notify->IsA<UMMOAnimNotify_MeleeHit>())
		{
			return Event.GetTriggerTime();
		}
	}

	return -1.0f;
}
