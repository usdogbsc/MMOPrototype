// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/MMOInteractable.h"
#include "MMOPortal.generated.h"

class USceneComponent;
class UWidgetComponent;

/**
 *  A doorway to somewhere else in the map (mine entrance <-> mine interior).
 *  Using it fades the camera and moves the player to the linked portal's arrival point.
 */
UCLASS()
class AMMOPortal : public AActor, public IMMOInteractable
{
	GENERATED_BODY()

public:

	AMMOPortal();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal")
	FName PortalId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal")
	FText DisplayName;

	/** e.g. "Enter" or "Leave" (shown under the name) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal")
	FText ActionText;

	/** Where using this portal takes the player */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal")
	FVector Destination = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal")
	float DestinationYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Portal", meta=(Units="cm"))
	float PlateHeight = 260.0f;

	//~ Begin IMMOInteractable
	virtual FText GetInteractName() const override { return DisplayName; }
	virtual bool CanInteract(const AMMOCharacter* Player) const override;
	virtual void Interact(AMMOCharacter* Player) override;
	virtual float GetInteractRange() const override { return 380.0f; }
	virtual FVector GetInteractLocation() const override { return GetActorLocation() + FVector(0.0f, 0.0f, 120.0f); }
	virtual float GetInteractPickRadius() const override { return 160.0f; }
	//~ End IMMOInteractable

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

protected:

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UWidgetComponent> Plate;
};
