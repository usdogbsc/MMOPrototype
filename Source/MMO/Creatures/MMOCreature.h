// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Combat/MMOTargetable.h"
#include "MMOCreature.generated.h"

class UMMOHealthComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;

/**
 *  Base class for hostile world creatures.
 *  Holds the creature's combat stats, death/XP/respawn rules and simple procedural presentation
 *  (hit flash, attack lunge, death pose). Behaviour is driven by AMMOCreatureAIController.
 */
UCLASS(abstract)
class AMMOCreature : public ACharacter, public IMMOTargetable
{
	GENERATED_BODY()

	/** Creature health */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMMOHealthComponent> Health;

	/** Parent for all visual parts, animated procedurally */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> VisualRoot;

	/** Ring shown under the creature while it is the player's target */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> TargetRing;

public:

	AMMOCreature();

	/** Name shown on nameplates and the target frame */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature", meta=(ClampMin=1))
	int32 CreatureLevel = 1;

	/** Damage per attack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Combat", meta=(ClampMin=0))
	float AttackDamage = 5.0f;

	/** Max gap between collision edges to land an attack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Combat", meta=(ClampMin=0, Units="cm"))
	float AttackRange = 90.0f;

	/** Seconds between attacks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Combat", meta=(ClampMin=0.1, Units="s"))
	float AttackCooldown = 2.0f;

	/** Distance at which the creature notices the player */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|AI", meta=(ClampMin=0, Units="cm"))
	float AggroRange = 900.0f;

	/** Max distance from its spawn point before the creature gives up and returns */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|AI", meta=(ClampMin=0, Units="cm"))
	float LeashRange = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|AI", meta=(ClampMin=0, Units="cm/s"))
	float ChaseSpeed = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|AI", meta=(ClampMin=0, Units="cm/s"))
	float ReturnSpeed = 700.0f;

	/** XP granted to the killer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Rewards", meta=(ClampMin=0))
	int32 XPReward = 40;

	/** Seconds the corpse stays visible */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Respawn", meta=(ClampMin=0, Units="s"))
	float CorpseDuration = 4.0f;

	/** Seconds from death until respawn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Respawn", meta=(ClampMin=0, Units="s"))
	float RespawnDelay = 8.0f;

protected:

	/** Visual parts and their base colors (parallel arrays). Built by subclasses with AddBodyPart */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BodyParts;

	UPROPERTY()
	TArray<FLinearColor> BodyPartColors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyPartMaterials;

	/** Where the creature spawned and returns to */
	FTransform SpawnTransform;

	double LastAttackTime = -1000.0;

	bool bIsDead = false;

	/** Presentation timers */
	float HitFlashTime = 0.0f;
	float LungeTime = 0.0f;
	float DeathBlend = 0.0f;
	float AnimTime = 0.0f;

	FTimerHandle CorpseTimer;
	FTimerHandle RespawnTimer;

public:

	//~ Begin IMMOTargetable
	virtual FText GetTargetDisplayName() const override { return DisplayName; }
	virtual int32 GetTargetLevel() const override { return CreatureLevel; }
	virtual bool IsTargetable() const override;
	virtual UMMOHealthComponent* GetTargetHealth() const override { return Health; }
	virtual void SetTargeted(bool bTargeted) override;
	virtual FVector GetNameplateLocation() const override;
	//~ End IMMOTargetable

	virtual void Tick(float DeltaSeconds) override;

	/** True if alive, off cooldown and Target is within attack range */
	bool CanAttack(const AActor* Target) const;

	/** Attacks Target immediately. Callers should check CanAttack first */
	void PerformAttack(AActor* Target);

	/** Evading creatures are immune to damage and move at return speed */
	void SetEvading(bool bEvading);

	bool IsDead() const { return bIsDead; }

	const FTransform& GetSpawnTransform() const { return SpawnTransform; }

	UMMOHealthComponent* GetHealth() const { return Health; }

protected:

	virtual void BeginPlay() override;

	/** Creates a non-colliding static mesh part under VisualRoot. Call from subclass constructors */
	UStaticMeshComponent* AddBodyPart(FName Name, UStaticMesh* PartMesh, const FVector& Location, const FRotator& Rotation, const FVector& Scale, const FLinearColor& Color);

	UFUNCTION()
	void HandleDamaged(float Amount, AActor* DamageInstigator);

	UFUNCTION()
	void HandleDeath(AActor* Killer);

	void HideCorpse();

	void Respawn();

	void UpdatePresentation(float DeltaSeconds);
};
