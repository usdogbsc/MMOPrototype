// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/MMOInteractable.h"
#include "Professions/MMOProfessionTypes.h"
#include "MMOCraftingStation.generated.h"

class UMMORecipeDefinition;
class USceneComponent;
class UWidgetComponent;

/**
 *  Where crafting happens (forge, alchemy table, cooking fire). Invisible on its own: the level dresses it
 *  with props. Right-click / F opens the crafting window with this station's recipes.
 */
UCLASS()
class AMMOCraftingStation : public AActor, public IMMOInteractable
{
	GENERATED_BODY()

public:

	AMMOCraftingStation();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crafting")
	EMMOProfession Profession = EMMOProfession::Smithing;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crafting")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crafting")
	TArray<TObjectPtr<UMMORecipeDefinition>> Recipes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crafting", meta=(ClampMin=0, Units="cm"))
	float UseRange = 330.0f;

	/** Height of the name plate above the actor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crafting", meta=(Units="cm"))
	float PlateHeight = 160.0f;

	/** True if Player is close enough to use (or keep using) the station */
	bool IsInReach(const AActor* Player) const;

	//~ Begin IMMOInteractable
	virtual FText GetInteractName() const override { return DisplayName; }
	virtual bool CanInteract(const AMMOCharacter* Player) const override { return true; }
	virtual void Interact(AMMOCharacter* Player) override;
	virtual float GetInteractRange() const override { return UseRange; }
	virtual FVector GetInteractLocation() const override { return GetActorLocation() + FVector(0.0f, 0.0f, 60.0f); }
	virtual float GetInteractPickRadius() const override { return 110.0f; }
	//~ End IMMOInteractable

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UWidgetComponent> Plate;
};
