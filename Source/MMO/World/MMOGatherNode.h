// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/MMOInteractable.h"
#include "Professions/MMOProfessionTypes.h"
#include "MMOGatherNode.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;
class UMaterialInterface;

/**
 *  A mineral vein or herb the player can gather (right-click / F). Takes a short cast,
 *  gives items and skill, then disappears until it respawns.
 *  The look is built from basic shapes: a base (rock or bush) with accent pieces (ore or flowers).
 */
UCLASS()
class AMMOGatherNode : public AActor, public IMMOInteractable
{
	GENERATED_BODY()

public:

	AMMOGatherNode();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gathering")
	EMMOProfession Profession = EMMOProfession::Mining;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gathering")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gathering", meta=(ClampMin=0))
	int32 RequiredSkill = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gathering")
	FName YieldItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gathering", meta=(ClampMin=1))
	int32 MinYield = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gathering", meta=(ClampMin=1))
	int32 MaxYield = 2;

	/** Occasional extra item */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gathering")
	FName BonusItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gathering", meta=(ClampMin=0, ClampMax=1))
	float BonusChance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gathering", meta=(ClampMin=0, Units="s"))
	float GatherTime = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gathering", meta=(ClampMin=0, Units="s"))
	float RespawnTime = 60.0f;

	/** Look: base shape color and accent (ore / flower) color */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gathering|Look")
	FLinearColor BaseColor = FLinearColor(0.08f, 0.075f, 0.07f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gathering|Look")
	FLinearColor AccentColor = FLinearColor(0.5f, 0.2f, 0.06f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Gathering|Look")
	int32 LookSeed = 1;

	bool IsDepleted() const { return bDepleted; }

	/** Hides the node until it respawns */
	void Deplete();
	void Respawn();

	//~ Begin IMMOInteractable
	virtual FText GetInteractName() const override { return DisplayName; }
	virtual bool CanInteract(const AMMOCharacter* Player) const override { return !bDepleted; }
	virtual void Interact(AMMOCharacter* Player) override;
	virtual float GetInteractRange() const override { return 260.0f; }
	virtual FVector GetInteractLocation() const override { return GetActorLocation() + FVector(0.0f, 0.0f, 35.0f); }
	virtual float GetInteractPickRadius() const override { return 85.0f; }
	//~ End IMMOInteractable

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TArray<TObjectPtr<UStaticMeshComponent>> Accents;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UWidgetComponent> Plate;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ShapeMaterial;

	bool bDepleted = false;
	FTimerHandle RespawnTimer;

	void ApplyLook();
	void SetShown(bool bShown);
};
