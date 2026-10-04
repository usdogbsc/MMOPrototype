// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "MMOAnimNotify_MeleeHit.generated.h"

/**
 *  Place on an attack animation at the frame the strike connects.
 *  Forwards to the owning actor's IMMOMeleeAttacker so damage lands in sync with the visible hit.
 */
UCLASS(meta=(DisplayName="MMO Melee Hit"))
class UMMOAnimNotify_MeleeHit : public UAnimNotify
{
	GENERATED_BODY()

public:

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

	/** Returns the time of the first MMO Melee Hit notify in Animation, or a negative value if it has none */
	static float FindHitTime(const UAnimSequenceBase* Animation);
};
