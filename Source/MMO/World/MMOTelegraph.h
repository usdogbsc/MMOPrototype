// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MMOTelegraph.generated.h"

class AMMOTelegraph;
class UStaticMeshComponent;
class USoundBase;
class UMaterialInterface;

DECLARE_MULTICAST_DELEGATE_TwoParams(FMMOTelegraphDetonated, const AMMOTelegraph* /*Telegraph*/, const TArray<AActor*>& /*Hit*/);

/**
 *  A glowing circle on the ground that warns of an incoming area attack. After Delay it erupts,
 *  damaging every player standing inside, then fades. Step out of the circle to avoid it.
 */
UCLASS()
class AMMOTelegraph : public AActor
{
	GENERATED_BODY()

public:

	AMMOTelegraph();

	/** Sets everything up; call right after spawning */
	void Arm(float InRadius, float InDelay, float InDamage, AActor* InSource);

	float GetRadius() const { return Radius; }
	bool HasDetonated() const { return bDetonated; }

	/** Fired by every telegraph when it erupts (tests and effects) */
	static FMMOTelegraphDetonated OnAnyDetonation;

	virtual void Tick(float DeltaSeconds) override;

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Disc;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Rim;

	/** Soft references: hard-loading content in a constructor roots it and breaks the content scripts that rebuild it */
	UPROPERTY(EditAnywhere, Category="Telegraph")
	TSoftObjectPtr<UMaterialInterface> FillMaterial;

	UPROPERTY(EditAnywhere, Category="Telegraph")
	TSoftObjectPtr<UMaterialInterface> RimMaterial;

	UPROPERTY(EditAnywhere, Category="Telegraph")
	TSoftObjectPtr<USoundBase> EruptSoundAsset;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> EruptSound;

	TWeakObjectPtr<AActor> Source;
	float Radius = 300.0f;
	float Delay = 2.0f;
	float Damage = 30.0f;
	float Age = 0.0f;
	bool bDetonated = false;

	void Detonate();
};
