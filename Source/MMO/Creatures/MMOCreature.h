// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Combat/MMOTargetable.h"
#include "Combat/MMOMeleeAttacker.h"
#include "MMOCreature.generated.h"

class UMMOHealthComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;
class UWidgetComponent;
class UAnimMontage;
class USoundBase;
class UNiagaraSystem;
class UMMOLootContainerComponent;
class UMMOLootTable;

/**
 *  Base class for hostile world creatures.
 *  Holds the creature's combat stats, death/XP/respawn rules and presentation.
 *
 *  Presentation has two paths:
 *   - Skeletal: assign a Skeletal Mesh + Anim Class (a UMMOCreatureAnimInstance child) and the Attack/HitReact/Death montages.
 *     Put an "MMO Melee Hit" notify on the attack montage's bite frame.
 *   - Procedural (default): a placeholder body built from static parts by the subclass, animated in code.
 *  Behaviour is driven by AMMOCreatureAIController.
 */
UCLASS(abstract)
class AMMOCreature : public ACharacter, public IMMOTargetable, public IMMOMeleeAttacker
{
	GENERATED_BODY()

	/** Creature health */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMMOHealthComponent> Health;

	/** Parent for the procedural body, animated in code */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> VisualRoot;

	/** Segmented ring shown under the creature while it is the player's target */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> TargetIndicator;

	/** Overhead UMG nameplate */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWidgetComponent> Nameplate;

	/** This corpse's loot (rolled fresh on every death) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMMOLootContainerComponent> Loot;

	/** Floating marker shown while the corpse has loot */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> LootMarker;

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

	/** Max gap between collision edges to start an attack */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Combat", meta=(ClampMin=0, Units="cm"))
	float AttackRange = 90.0f;

	/** Extra reach allowed at the bite frame, so a target stepping back mid-attack is still hit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Combat", meta=(ClampMin=0, Units="cm"))
	float AttackHitTolerance = 70.0f;

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

	/** Idle wandering: how far from its spawn point the creature roams (0 = stands still) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|AI|Wander", meta=(ClampMin=0, Units="cm"))
	float WanderRadius = 550.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|AI|Wander", meta=(ClampMin=0, Units="cm/s"))
	float WanderSpeed = 160.0f;

	/** Seconds to pause (sniffing around) between wander moves */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|AI|Wander")
	FVector2D WanderPause = FVector2D(3.0f, 8.0f);

	/** XP granted to the killer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Rewards", meta=(ClampMin=0))
	int32 XPReward = 40;

	/** Id that kill quests count (e.g. GreyWolf, DenWolf). Defaults to the creature type */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Rewards")
	FName QuestTag;

	/** What this creature can drop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Rewards")
	TSoftObjectPtr<UMMOLootTable> LootTable;

	/** Seconds the corpse stays when it has no loot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Respawn", meta=(ClampMin=0, Units="s"))
	float CorpseDuration = 4.0f;

	/** Seconds a corpse with unlooted items stays before it (and its loot) disappears */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Respawn", meta=(ClampMin=0, Units="s"))
	float LootableCorpseDuration = 60.0f;

	/** Seconds from death until respawn (never sooner than MinRespawnAfterCorpse after the corpse is gone) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Respawn", meta=(ClampMin=0, Units="s"))
	float RespawnDelay = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Respawn", meta=(ClampMin=0, Units="s"))
	float MinRespawnAfterCorpse = 2.0f;

	/** Respawn is postponed while the player stands this close to the spawn point */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Respawn", meta=(ClampMin=0, Units="cm"))
	float MinRespawnPlayerDistance = 1200.0f;

	/** Give up waiting for the player to leave after this long */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Respawn", meta=(ClampMin=0, Units="s"))
	float MaxRespawnDeferral = 45.0f;

	/** Skeletal path: attack montage. Needs an MMO Melee Hit notify on the bite frame */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Animation")
	TObjectPtr<UAnimMontage> AttackMontage;

	/** Skeletal path: optional hit reaction montage */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Animation")
	TObjectPtr<UAnimMontage> HitReactMontage;

	/** Skeletal path: optional death montage (ragdoll is used if unset) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

	/** Procedural path: length of the attack animation */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Animation", meta=(ClampMin=0.1, Units="s"))
	float ProceduralAttackDuration = 0.6f;

	/** Procedural path: fraction of the attack animation at which the bite connects */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Animation", meta=(ClampMin=0, ClampMax=1))
	float ProceduralAttackHitFraction = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Audio")
	TSoftObjectPtr<USoundBase> AggroSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Audio")
	TSoftObjectPtr<USoundBase> AttackHitSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Audio")
	TSoftObjectPtr<USoundBase> DeathSound;

	/** Effect spawned on the target when a bite lands */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|Effects")
	TSoftObjectPtr<UNiagaraSystem> AttackImpactEffect;

	/** Nameplates are hidden beyond this distance from the player */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature|UI", meta=(Units="cm"))
	float NameplateDistance = 3500.0f;

protected:

	/** Procedural body parts and their base colors (parallel arrays). Built by subclasses with AddBodyPart */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BodyParts;

	UPROPERTY()
	TArray<FLinearColor> BodyPartColors;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyPartMaterials;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TargetRingSegments;

	/** Optional joints the procedural animation drives. Set by subclasses */
	UPROPERTY()
	TObjectPtr<USceneComponent> HeadPivot;

	UPROPERTY()
	TObjectPtr<USceneComponent> JawPivot;

	UPROPERTY()
	TObjectPtr<USceneComponent> TailPivot;

	/** Leg joints in order: front-left, front-right, back-left, back-right */
	UPROPERTY()
	TArray<TObjectPtr<USceneComponent>> LegPivots;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedAggroSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedAttackHitSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedDeathSound;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> LoadedAttackImpactEffect;

	UPROPERTY(Transient)
	TObjectPtr<UMMOLootTable> LoadedLootTable;

	/** World times for the corpse lifecycle */
	double DeathTime = 0.0;
	double RespawnDueTime = 0.0;
	double CorpseRemoveTime = 0.0;

	/** Where the creature spawned and returns to */
	FTransform SpawnTransform;

	double LastAttackTime = -1000.0;

	bool bIsDead = false;

	/** True between an attack starting and its bite frame */
	bool bAttackPending = false;

	TWeakObjectPtr<AActor> PendingAttackTarget;

	/** Procedural animation state */
	float AttackAnimTime = -1.0f;
	float HitReactTime = 0.0f;
	float DeathElapsed = 0.0f;
	float AnimTime = 0.0f;
	float GaitPhase = 0.0f;
	bool bTargeted = false;

	FTimerHandle AttackResolveTimer;
	FTimerHandle CorpseTimer;
	FTimerHandle RespawnTimer;

public:

	//~ Begin IMMOTargetable
	virtual FText GetTargetDisplayName() const override { return DisplayName; }
	virtual int32 GetTargetLevel() const override { return CreatureLevel; }
	virtual bool IsTargetable() const override;
	virtual UMMOHealthComponent* GetTargetHealth() const override { return Health; }
	virtual void SetTargeted(bool bInTargeted) override;
	virtual FVector GetNameplateLocation() const override;
	//~ End IMMOTargetable

	//~ Begin IMMOMeleeAttacker
	virtual void NotifyMeleeHitFrame() override;
	//~ End IMMOMeleeAttacker

	virtual void Tick(float DeltaSeconds) override;

	/** True if alive, not mid-attack, off cooldown and Target is within attack range */
	bool CanAttack(const AActor* Target) const;

	/** Starts an attack on Target. Damage is applied on the bite frame */
	void PerformAttack(AActor* Target);

	/** Called by the AI when the creature first notices a target */
	void OnAggro(AActor* Target);

	/** Evading creatures are immune to damage and move at return speed */
	void SetEvading(bool bEvading);

	bool IsDead() const { return bIsDead; }

	/** True while dead, visible and holding loot */
	bool IsLootable() const;

	UMMOLootContainerComponent* GetLoot() const { return Loot; }

	bool IsAttacking() const { return bAttackPending || AttackAnimTime >= 0.0f; }

	/** True while the AI is chasing or attacking */
	bool IsInCombat() const;

	/** World time the last attack started */
	double GetLastAttackTime() const { return LastAttackTime; }

	/** True when a skeletal mesh is assigned (the procedural body is hidden) */
	bool UsesSkeletalMesh() const;

	const FTransform& GetSpawnTransform() const { return SpawnTransform; }

	/** World time the pending respawn is (or was) due */
	double GetRespawnDueTime() const { return RespawnDueTime; }

	UMMOHealthComponent* GetHealth() const { return Health; }

protected:

	virtual void BeginPlay() override;

	/** Creates a joint under Parent (VisualRoot if null). Call from subclass constructors */
	USceneComponent* AddPivot(FName Name, USceneComponent* Parent, const FVector& Location);

	/** Creates a non-colliding static mesh part under Parent (VisualRoot if null). Call from subclass constructors */
	UStaticMeshComponent* AddBodyPart(FName Name, UStaticMesh* PartMesh, USceneComponent* Parent, const FVector& Location, const FRotator& Rotation, const FVector& Scale, const FLinearColor& Color);

	/** Applies the pending attack's damage if the target is still alive and in reach */
	void ResolveAttack();

	UFUNCTION()
	void HandleDamaged(float Amount, AActor* DamageInstigator);

	UFUNCTION()
	void HandleDeath(AActor* Killer);

	void HideCorpse();

	/** (Re)schedules corpse removal Delay seconds from now */
	void ScheduleCorpseRemoval(float Delay);

	UFUNCTION()
	void HandleLootChanged();

	void UpdateLootMarker();

	void Respawn();

	/** True if the player is close enough that popping into existence would look wrong */
	bool IsPlayerNearSpawn() const;

	void UpdateProceduralAnimation(float DeltaSeconds);

	void UpdateTargetIndicator(float DeltaSeconds);

	void UpdateNameplate();

	void PlaySoundHere(USoundBase* Sound) const;
};
