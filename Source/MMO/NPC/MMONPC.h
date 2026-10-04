// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "World/MMOInteractable.h"
#include "MMONPC.generated.h"

class UWidgetComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class USkeletalMesh;
class UAnimSequence;
class UMMOQuestDefinition;
class UMMOQuestLogComponent;

/** What floats over an NPC's head */
UENUM(BlueprintType)
enum class EMMONPCMarker : uint8
{
	None,
	/** Has a quest you can accept */
	QuestAvailable,
	/** Has a quest of yours that isn't finished */
	QuestInProgress,
	/** Has a quest of yours ready to turn in */
	QuestReady
};

/**
 *  A friendly townsperson. Talk to them (right-click / F) to open dialogue: a greeting plus the quests
 *  they give or accept. Uses the mannequin body with an idle loop and an optional simple accessory (hat).
 */
UCLASS()
class AMMONPC : public ACharacter, public IMMOInteractable
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWidgetComponent> Plate;

	/** Simple hat/hood made from a basic shape */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Accessory;

public:

	AMMONPC();

	/** Stable id quests refer to (GiverId / TurnInId) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC")
	FName NPCId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC")
	FText DisplayName;

	/** e.g. "Village Warden" (shown as <Village Warden>) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC", meta=(MultiLine=true))
	FText Greeting;

	/** Quests this NPC gives or accepts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC")
	TArray<TObjectPtr<UMMOQuestDefinition>> Quests;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC|Look")
	TObjectPtr<USkeletalMesh> BodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC|Look")
	TObjectPtr<UAnimSequence> IdleAnimation;

	/** Clothing colors applied to the body's "Paint Tint" (per material slot; alpha 0 keeps the mesh's own color) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC|Look")
	TArray<FLinearColor> BodyTints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC|Look")
	TObjectPtr<UStaticMesh> AccessoryMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC|Look")
	TObjectPtr<UMaterialInterface> AccessoryMaterial;

	/** Relative to the capsule center */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC|Look")
	FTransform AccessoryTransform = FTransform(FRotator::ZeroRotator, FVector(0.0f, 0.0f, 92.0f), FVector(0.3f, 0.3f, 0.12f));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="NPC", meta=(Units="cm"))
	float PlateDistance = 3000.0f;

	//~ Begin IMMOInteractable
	virtual FText GetInteractName() const override { return DisplayName; }
	virtual bool CanInteract(const AMMOCharacter* Player) const override { return true; }
	virtual void Interact(AMMOCharacter* Player) override;
	virtual FVector GetInteractLocation() const override { return GetActorLocation(); }
	virtual float GetInteractPickRadius() const override { return 75.0f; }
	//~ End IMMOInteractable

	/** Marker to show for this player's quest log */
	EMMONPCMarker GetMarker(const UMMOQuestLogComponent* QuestLog) const;

	/** Quests to list in dialogue: ones this NPC offers that are available, and ones it accepts that are active/ready */
	TArray<UMMOQuestDefinition*> GetDialogueQuests(const UMMOQuestLogComponent* QuestLog) const;

	/** Turns to face the player while talking */
	void FaceActor(const AActor* Other);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

protected:

	TWeakObjectPtr<const AActor> FacingTarget;
	float FacingTime = 0.0f;
	FRotator HomeRotation;

	void ApplyLook();
	void UpdatePlate();
};
