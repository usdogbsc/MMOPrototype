// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/MMOCreature.h"
#include "Creatures/MMOCreatureAIController.h"
#include "Animation/MMOAnimNotify_MeleeHit.h"
#include "Combat/MMOCombatComponent.h"
#include "Combat/MMOHealthComponent.h"
#include "Combat/MMOProgressionComponent.h"
#include "Items/MMOLootContainerComponent.h"
#include "Items/MMOLootTable.h"
#include "UI/MMONameplateWidget.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "MMO.h"

namespace MMOCreature
{
	static const FName ColorParam(TEXT("Color"));
	static const float HitReactDuration = 0.25f;
	static const float DeathFallDuration = 0.45f;
	static const float CorpseSinkDuration = 0.8f;
	static const int32 RingSegments = 16;

	/** Engine material with a "Color" parameter; the default Cube material has none */
	static UMaterialInterface* GetTintableMaterial()
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		return Material.Object;
	}

	/** 0..1 -> 0..1..0 bump peaking at Peak */
	static float Bump(float Alpha, float Peak)
	{
		if (Alpha <= 0.0f || Alpha >= 1.0f)
		{
			return 0.0f;
		}
		return Alpha < Peak ? FMath::InterpEaseOut(0.0f, 1.0f, Alpha / Peak, 2.0f) : FMath::InterpEaseInOut(1.0f, 0.0f, (Alpha - Peak) / (1.0f - Peak), 2.0f);
	}
}

AMMOCreature::AMMOCreature()
{
	PrimaryActorTick.bCanEverTick = true;

	AIControllerClass = AMMOCreatureAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;

	// spread out instead of stacking when several creatures chase the same target
	GetCharacterMovement()->bUseRVOAvoidance = true;
	GetCharacterMovement()->AvoidanceConsiderationRadius = 250.0f;

	// the skeletal mesh is only used once a real creature mesh is assigned
	GetMesh()->SetVisibility(false);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Health = CreateDefaultSubobject<UMMOHealthComponent>(TEXT("Health"));

	VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
	VisualRoot->SetupAttachment(RootComponent);

	// target ring: thin segments arranged in a circle (positioned in BeginPlay once the capsule size is known)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	TargetIndicator = CreateDefaultSubobject<USceneComponent>(TEXT("TargetIndicator"));
	TargetIndicator->SetupAttachment(RootComponent);
	TargetIndicator->SetHiddenInGame(true, true);

	for (int32 i = 0; i < MMOCreature::RingSegments; ++i)
	{
		UStaticMeshComponent* Segment = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("TargetRing%d"), i));
		Segment->SetupAttachment(TargetIndicator);
		Segment->SetStaticMesh(CubeMesh.Object);
		Segment->SetMaterial(0, MMOCreature::GetTintableMaterial());
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Segment->SetCastShadow(false);
		Segment->SetHiddenInGame(true);
		TargetRingSegments.Add(Segment);
	}

	Nameplate = CreateDefaultSubobject<UWidgetComponent>(TEXT("Nameplate"));
	Nameplate->SetupAttachment(RootComponent);
	Nameplate->SetWidgetSpace(EWidgetSpace::Screen);
	Nameplate->SetWidgetClass(UMMONameplateWidget::StaticClass());
	Nameplate->SetDrawAtDesiredSize(true);
	Nameplate->SetPivot(FVector2D(0.5f, 1.0f));
	Nameplate->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Loot = CreateDefaultSubobject<UMMOLootContainerComponent>(TEXT("Loot"));

	// gold diamond that bobs above lootable corpses
	LootMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LootMarker"));
	LootMarker->SetupAttachment(RootComponent);
	LootMarker->SetStaticMesh(CubeMesh.Object);
	LootMarker->SetMaterial(0, MMOCreature::GetTintableMaterial());
	LootMarker->SetRelativeScale3D(FVector(0.16f));
	LootMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LootMarker->SetCastShadow(false);
	LootMarker->SetHiddenInGame(true);

	DisplayName = NSLOCTEXT("MMOCreature", "DefaultName", "Creature");

	LootTable = TSoftObjectPtr<UMMOLootTable>(FSoftObjectPath(TEXT("/Game/MMO/Loot/DA_Loot_GreyWolf.DA_Loot_GreyWolf")));

	AttackImpactEffect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Variant_Combat/VFX/NS_Damage.NS_Damage")));
}

void AMMOCreature::BeginPlay()
{
	Super::BeginPlay();

	SpawnTransform = GetActorTransform();
	GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;

	// children of the capsule inherit its scale, so lay them out in unscaled units
	const float Radius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

	// lay out the target ring on the ground around the capsule
	TargetIndicator->SetRelativeLocation(FVector(0.0f, 0.0f, -HalfHeight + 2.0f));
	const float RingRadius = Radius * 1.35f;
	const float SegmentLength = 2.0f * PI * RingRadius / MMOCreature::RingSegments * 0.6f;
	for (int32 i = 0; i < TargetRingSegments.Num(); ++i)
	{
		const float Angle = 360.0f * i / TargetRingSegments.Num();
		const FRotator Rotation(0.0f, Angle + 90.0f, 0.0f);
		TargetRingSegments[i]->SetRelativeLocationAndRotation(FRotator(0.0f, Angle, 0.0f).RotateVector(FVector(RingRadius, 0.0f, 0.0f)), Rotation);
		TargetRingSegments[i]->SetRelativeScale3D(FVector(SegmentLength / 100.0f, 0.06f, 0.02f));
		if (UMaterialInstanceDynamic* Material = TargetRingSegments[i]->CreateDynamicMaterialInstance(0))
		{
			Material->SetVectorParameterValue(MMOCreature::ColorParam, FLinearColor(1.0f, 0.18f, 0.08f));
		}
	}

	Nameplate->SetRelativeLocation(FVector(0.0f, 0.0f, HalfHeight + 50.0f));

	// choose the presentation path
	if (UsesSkeletalMesh())
	{
		GetMesh()->SetVisibility(true);
		VisualRoot->SetVisibility(false, true);
	}

	BodyPartMaterials.Reset();
	for (int32 i = 0; i < BodyParts.Num(); ++i)
	{
		UMaterialInstanceDynamic* Material = BodyParts[i]->CreateDynamicMaterialInstance(0);
		if (Material)
		{
			Material->SetVectorParameterValue(MMOCreature::ColorParam, BodyPartColors[i]);
		}
		BodyPartMaterials.Add(Material);
	}

	LoadedAggroSound = AggroSound.LoadSynchronous();
	LoadedAttackHitSound = AttackHitSound.LoadSynchronous();
	LoadedDeathSound = DeathSound.LoadSynchronous();
	LoadedAttackImpactEffect = AttackImpactEffect.LoadSynchronous();
	LoadedLootTable = LootTable.LoadSynchronous();

	Loot->ContainerName = DisplayName;
	Loot->OnLootChanged.AddDynamic(this, &AMMOCreature::HandleLootChanged);
	if (UMaterialInstanceDynamic* MarkerMaterial = LootMarker->CreateDynamicMaterialInstance(0))
	{
		MarkerMaterial->SetVectorParameterValue(MMOCreature::ColorParam, FLinearColor(1.0f, 0.72f, 0.1f));
	}

	Health->OnDamaged.AddDynamic(this, &AMMOCreature::HandleDamaged);
	Health->OnDeath.AddDynamic(this, &AMMOCreature::HandleDeath);
}

USceneComponent* AMMOCreature::AddPivot(FName Name, USceneComponent* Parent, const FVector& Location)
{
	USceneComponent* Pivot = CreateDefaultSubobject<USceneComponent>(Name);
	Pivot->SetupAttachment(Parent ? Parent : VisualRoot.Get());
	Pivot->SetRelativeLocation(Location);
	return Pivot;
}

UStaticMeshComponent* AMMOCreature::AddBodyPart(FName Name, UStaticMesh* PartMesh, USceneComponent* Parent, const FVector& Location, const FRotator& Rotation, const FVector& Scale, const FLinearColor& Color)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	Part->SetupAttachment(Parent ? Parent : VisualRoot.Get());
	Part->SetStaticMesh(PartMesh);
	Part->SetMaterial(0, MMOCreature::GetTintableMaterial());
	Part->SetRelativeLocationAndRotation(Location, Rotation);
	Part->SetRelativeScale3D(Scale);
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);

	BodyParts.Add(Part);
	BodyPartColors.Add(Color);
	return Part;
}

bool AMMOCreature::UsesSkeletalMesh() const
{
	return GetMesh() && GetMesh()->GetSkeletalMeshAsset() != nullptr;
}

bool AMMOCreature::IsLootable() const
{
	return bIsDead && !IsHidden() && Loot->HasLoot();
}

bool AMMOCreature::IsTargetable() const
{
	return !bIsDead && !IsHidden();
}

void AMMOCreature::SetTargeted(bool bInTargeted)
{
	bTargeted = bInTargeted;
	TargetIndicator->SetHiddenInGame(!bTargeted, true);
}

FVector AMMOCreature::GetNameplateLocation() const
{
	return GetActorLocation() + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 45.0f);
}

bool AMMOCreature::IsInCombat() const
{
	const AMMOCreatureAIController* AI = Cast<AMMOCreatureAIController>(GetController());
	return AI && (AI->GetAIState() == EMMOCreatureAIState::Chasing || AI->GetAIState() == EMMOCreatureAIState::Attacking);
}

void AMMOCreature::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!UsesSkeletalMesh())
	{
		UpdateProceduralAnimation(DeltaSeconds);
	}

	UpdateTargetIndicator(DeltaSeconds);
	UpdateNameplate();
	UpdateLootMarker();
}

bool AMMOCreature::CanAttack(const AActor* Target) const
{
	if (bIsDead || !Target || IsAttacking())
	{
		return false;
	}

	if (GetWorld()->GetTimeSeconds() < LastAttackTime + AttackCooldown)
	{
		return false;
	}

	return UMMOCombatComponent::GetEdgeDistance(this, Target) <= AttackRange;
}

void AMMOCreature::PerformAttack(AActor* Target)
{
	if (bIsDead || !Target)
	{
		return;
	}

	LastAttackTime = GetWorld()->GetTimeSeconds();
	bAttackPending = true;
	PendingAttackTarget = Target;

	// time the bite from the animation: a montage notify, or the procedural attack's hit fraction
	float HitDelay = ProceduralAttackDuration * ProceduralAttackHitFraction;
	if (UsesSkeletalMesh() && AttackMontage)
	{
		PlayAnimMontage(AttackMontage);
		const float NotifyTime = UMMOAnimNotify_MeleeHit::FindHitTime(AttackMontage);
		HitDelay = NotifyTime >= 0.0f ? NotifyTime : AttackMontage->GetPlayLength() * 0.5f;
	}
	else
	{
		AttackAnimTime = 0.0f;
	}

	// safety net in case the animation event never arrives (interrupted montage, missing notify)
	GetWorldTimerManager().SetTimer(AttackResolveTimer, this, &AMMOCreature::ResolveAttack, HitDelay + 0.3f, false);
}

void AMMOCreature::NotifyMeleeHitFrame()
{
	ResolveAttack();
}

void AMMOCreature::ResolveAttack()
{
	if (!bAttackPending)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(AttackResolveTimer);
	bAttackPending = false;

	AActor* Target = PendingAttackTarget.Get();
	PendingAttackTarget.Reset();

	if (bIsDead || !Target)
	{
		return;
	}

	UMMOHealthComponent* TargetHealth = Target->FindComponentByClass<UMMOHealthComponent>();
	if (!TargetHealth || TargetHealth->IsDead())
	{
		return;
	}

	// the target can dodge by stepping well out of reach before the bite lands
	if (UMMOCombatComponent::GetEdgeDistance(this, Target) > AttackRange + AttackHitTolerance)
	{
		return;
	}

	if (TargetHealth->ApplyDamage(AttackDamage, this) > 0.0f)
	{
		PlaySoundHere(LoadedAttackHitSound);
		if (LoadedAttackImpactEffect)
		{
			const FVector ImpactLocation = Target->GetActorLocation() + (GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D() * 30.0f;
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, LoadedAttackImpactEffect, ImpactLocation);
		}
	}
}

void AMMOCreature::OnAggro(AActor* Target)
{
	PlaySoundHere(LoadedAggroSound);
}

void AMMOCreature::SetEvading(bool bEvading)
{
	Health->SetInvulnerable(bEvading);
	GetCharacterMovement()->MaxWalkSpeed = bEvading ? ReturnSpeed : ChaseSpeed;
}

void AMMOCreature::HandleDamaged(float Amount, AActor* DamageInstigator)
{
	HitReactTime = MMOCreature::HitReactDuration;

	if (UsesSkeletalMesh() && HitReactMontage && !IsAttacking())
	{
		PlayAnimMontage(HitReactMontage);
	}

	if (AMMOCreatureAIController* AI = Cast<AMMOCreatureAIController>(GetController()))
	{
		AI->NotifyDamagedBy(DamageInstigator);
	}
}

void AMMOCreature::HandleDeath(AActor* Killer)
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	bAttackPending = false;
	PendingAttackTarget.Reset();
	GetWorldTimerManager().ClearTimer(AttackResolveTimer);
	AttackAnimTime = -1.0f;
	HitReactTime = 0.0f;
	DeathElapsed = 0.0f;

	// stop the AI first so nothing else happens this frame
	if (AMMOCreatureAIController* AI = Cast<AMMOCreatureAIController>(GetController()))
	{
		AI->NotifyPawnDied();
	}

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// corpses don't block the player
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	if (UsesSkeletalMesh())
	{
		StopAnimMontage();
		if (DeathMontage)
		{
			PlayAnimMontage(DeathMontage);
		}
		else
		{
			GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
			GetMesh()->SetSimulatePhysics(true);
		}
	}

	PlaySoundHere(LoadedDeathSound);

	// award XP exactly once, guarded by bIsDead above
	if (Killer)
	{
		if (UMMOProgressionComponent* Progression = Killer->FindComponentByClass<UMMOProgressionComponent>())
		{
			Progression->AddXP(XPReward);
		}
	}

	// this corpse rolls its own loot; nothing carries over from earlier deaths
	DeathTime = GetWorld()->GetTimeSeconds();
	Loot->GenerateFrom(LoadedLootTable);

	UE_LOG(LogMMO, Log, TEXT("%s died (killer: %s, XP reward: %d, loot stacks: %d, currency: %d)"), *GetName(), *GetNameSafe(Killer), XPReward, Loot->GetItems().Num(), Loot->GetCurrency());

	ScheduleCorpseRemoval(Loot->HasLoot() ? LootableCorpseDuration : CorpseDuration);
}

void AMMOCreature::ScheduleCorpseRemoval(float Delay)
{
	Delay = FMath::Max(0.01f, Delay);
	CorpseRemoveTime = GetWorld()->GetTimeSeconds() + Delay;
	GetWorldTimerManager().SetTimer(CorpseTimer, this, &AMMOCreature::HideCorpse, Delay, false);
}

void AMMOCreature::HandleLootChanged()
{
	// fully looted: the corpse sinks away shortly instead of waiting out the lootable duration
	if (bIsDead && !IsHidden() && !Loot->HasLoot() && CorpseRemoveTime - GetWorld()->GetTimeSeconds() > MMOCreature::CorpseSinkDuration + 0.2f)
	{
		ScheduleCorpseRemoval(MMOCreature::CorpseSinkDuration + 0.2f);
	}
}

void AMMOCreature::UpdateLootMarker()
{
	const bool bShow = IsLootable();
	LootMarker->SetHiddenInGame(!bShow);
	if (bShow)
	{
		const float Bob = FMath::Sin(AnimTime * 3.0f) * 6.0f;
		LootMarker->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 20.0f + Bob), FRotator(45.0f, AnimTime * 90.0f, 45.0f));
	}
}

void AMMOCreature::HideCorpse()
{
	// unlooted items are lost with the corpse (prototype rule)
	Loot->ClearLoot();

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	TargetIndicator->SetHiddenInGame(true, true);
	LootMarker->SetHiddenInGame(true);

	const float SinceDeath = static_cast<float>(GetWorld()->GetTimeSeconds() - DeathTime);
	const float Delay = FMath::Max(MinRespawnAfterCorpse, RespawnDelay - SinceDeath);
	RespawnDueTime = GetWorld()->GetTimeSeconds() + Delay;
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AMMOCreature::Respawn, Delay, false);
}

bool AMMOCreature::IsPlayerNearSpawn() const
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	return Player && FVector::Dist(Player->GetActorLocation(), SpawnTransform.GetLocation()) < MinRespawnPlayerDistance;
}

void AMMOCreature::Respawn()
{
	// don't pop into existence right next to the player: wait for them to move on (up to a limit)
	if (IsPlayerNearSpawn() && GetWorld()->GetTimeSeconds() - RespawnDueTime < MaxRespawnDeferral)
	{
		GetWorldTimerManager().SetTimer(RespawnTimer, this, &AMMOCreature::Respawn, 3.0f, false);
		return;
	}

	TeleportTo(SpawnTransform.GetLocation(), SpawnTransform.Rotator(), false, true);

	bIsDead = false;
	DeathElapsed = 0.0f;
	LastAttackTime = -1000.0;
	Loot->ClearLoot();

	Health->ResetHealth();
	SetEvading(false);

	if (UsesSkeletalMesh())
	{
		GetMesh()->SetSimulatePhysics(false);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		GetMesh()->SetRelativeTransform(GetClass()->GetDefaultObject<AMMOCreature>()->GetMesh()->GetRelativeTransform());
		StopAnimMontage();
	}

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	SetActorEnableCollision(true);
	SetActorHiddenInGame(false);
	TargetIndicator->SetHiddenInGame(true, true);
	bTargeted = false;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	if (AMMOCreatureAIController* AI = Cast<AMMOCreatureAIController>(GetController()))
	{
		AI->NotifyPawnRespawned();
	}

	UE_LOG(LogMMO, Log, TEXT("%s respawned"), *GetName());
}

void AMMOCreature::UpdateProceduralAnimation(float DeltaSeconds)
{
	using namespace MMOCreature;

	AnimTime += DeltaSeconds;
	HitReactTime = FMath::Max(0.0f, HitReactTime - DeltaSeconds);

	const float Speed = GetVelocity().Size2D();
	const float MoveAlpha = bIsDead ? 0.0f : FMath::Clamp(Speed / 300.0f, 0.0f, 1.0f);
	GaitPhase += DeltaSeconds * Speed * 0.045f;

	FVector BodyOffset = FVector::ZeroVector;
	FRotator BodyRotation = FRotator::ZeroRotator;
	float HeadPitch = 0.0f;
	float HeadYaw = 0.0f;
	float JawOpen = 0.0f;
	float TailYaw = 0.0f;
	float LegSwing = 0.0f;

	if (!bIsDead)
	{
		// idle breathing / look-around, or a lowered, snarling head in combat
		const bool bCombat = IsInCombat();
		BodyOffset.Z += FMath::Sin(AnimTime * 2.2f) * 1.2f * (1.0f - MoveAlpha);
		HeadYaw = FMath::Sin(AnimTime * 0.7f) * 10.0f * (1.0f - MoveAlpha) * (bCombat ? 0.2f : 1.0f);
		HeadPitch = bCombat ? -8.0f : FMath::Sin(AnimTime * 0.5f) * 4.0f;
		JawOpen = bCombat ? 6.0f + FMath::Sin(AnimTime * 9.0f) * 3.0f : 0.0f;
		TailYaw = FMath::Sin(AnimTime * FMath::Lerp(3.0f, 11.0f, MoveAlpha)) * FMath::Lerp(16.0f, 10.0f, MoveAlpha);

		// trot: diagonal leg pairs swing, the body bobs
		LegSwing = FMath::Sin(GaitPhase) * 28.0f * MoveAlpha;
		BodyOffset.Z += FMath::Abs(FMath::Cos(GaitPhase)) * 4.0f * MoveAlpha;
		HeadPitch += FMath::Sin(GaitPhase * 2.0f) * 3.0f * MoveAlpha;

		// attack: crouch and rear back, lunge and snap at the hit frame, then recover
		if (AttackAnimTime >= 0.0f)
		{
			const float PrevAlpha = AttackAnimTime / ProceduralAttackDuration;
			AttackAnimTime += DeltaSeconds;
			const float Alpha = AttackAnimTime / ProceduralAttackDuration;

			const float Windup = Bump(Alpha / ProceduralAttackHitFraction, 0.75f) * (Alpha < ProceduralAttackHitFraction ? 1.0f : 0.0f);
			const float Strike = Alpha < ProceduralAttackHitFraction * 0.7f ? 0.0f : Bump((Alpha - ProceduralAttackHitFraction * 0.7f) / (1.0f - ProceduralAttackHitFraction * 0.7f), 0.3f);

			BodyOffset.X += -14.0f * Windup + 48.0f * Strike;
			BodyOffset.Z += -7.0f * Windup + 4.0f * Strike;
			BodyRotation.Pitch += 6.0f * Windup - 8.0f * Strike;
			HeadPitch += 16.0f * Windup - 22.0f * Strike;
			JawOpen = FMath::Max(JawOpen, Alpha < ProceduralAttackHitFraction ? 32.0f * FMath::Clamp(Alpha / ProceduralAttackHitFraction, 0.0f, 1.0f) : 32.0f * (1.0f - Strike) * 0.2f);

			// the animation's hit event: fired as the lunge reaches the target
			if (PrevAlpha < ProceduralAttackHitFraction && Alpha >= ProceduralAttackHitFraction)
			{
				NotifyMeleeHitFrame();
			}

			if (Alpha >= 1.0f)
			{
				AttackAnimTime = -1.0f;
			}
		}

		// hit reaction: flinch back, head jerks up
		if (HitReactTime > 0.0f)
		{
			const float React = FMath::Sin((1.0f - HitReactTime / HitReactDuration) * PI);
			BodyOffset.X -= 16.0f * React;
			BodyRotation.Roll += 6.0f * React;
			HeadPitch += 22.0f * React;
		}
	}
	else
	{
		// death: collapse onto its side, head drops, then sink before the corpse is removed
		DeathElapsed += DeltaSeconds;
		const float Fall = FMath::InterpEaseIn(0.0f, 1.0f, FMath::Clamp(DeathElapsed / DeathFallDuration, 0.0f, 1.0f), 2.0f);
		BodyRotation.Roll = Fall * 88.0f;
		BodyOffset.Z -= Fall * 22.0f;
		HeadPitch = -12.0f * Fall;
		JawOpen = 12.0f * Fall;
		LegSwing = 12.0f * Fall;

		const float UntilRemoved = static_cast<float>(CorpseRemoveTime - GetWorld()->GetTimeSeconds());
		if (UntilRemoved < CorpseSinkDuration)
		{
			BodyOffset.Z -= 70.0f * FMath::Clamp(1.0f - UntilRemoved / CorpseSinkDuration, 0.0f, 1.0f);
		}
	}

	VisualRoot->SetRelativeLocationAndRotation(BodyOffset, BodyRotation);

	if (HeadPivot)
	{
		HeadPivot->SetRelativeRotation(FRotator(HeadPitch, HeadYaw, 0.0f));
	}
	if (JawPivot)
	{
		JawPivot->SetRelativeRotation(FRotator(-JawOpen, 0.0f, 0.0f));
	}
	if (TailPivot)
	{
		TailPivot->SetRelativeRotation(FRotator(0.0f, TailYaw, 0.0f));
	}
	for (int32 i = 0; i < LegPivots.Num(); ++i)
	{
		if (LegPivots[i])
		{
			// front-left and back-right move together, as do front-right and back-left
			const float Sign = (i == 0 || i == 3) ? 1.0f : -1.0f;
			LegPivots[i]->SetRelativeRotation(FRotator(LegSwing * Sign, 0.0f, 0.0f));
		}
	}

	// color: white flash on hit, darkened when dead
	const float Flash = HitReactTime / HitReactDuration;
	const float Darken = bIsDead ? FMath::Clamp(DeathElapsed / DeathFallDuration, 0.0f, 1.0f) : 0.0f;
	for (int32 i = 0; i < BodyPartMaterials.Num(); ++i)
	{
		if (!BodyPartMaterials[i])
		{
			continue;
		}

		FLinearColor Color = BodyPartColors[i];
		if (Flash > 0.0f)
		{
			Color = FLinearColor::LerpUsingHSV(Color, FLinearColor(1.0f, 0.9f, 0.85f), Flash * 0.85f);
		}
		if (Darken > 0.0f)
		{
			Color = Color * FMath::Lerp(1.0f, 0.4f, Darken);
			Color.A = 1.0f;
		}
		BodyPartMaterials[i]->SetVectorParameterValue(MMOCreature::ColorParam, Color);
	}
}

void AMMOCreature::UpdateTargetIndicator(float DeltaSeconds)
{
	if (!bTargeted || bIsDead)
	{
		return;
	}

	// slow rotation and a subtle pulse keep the selection readable without being flashy
	TargetIndicator->AddRelativeRotation(FRotator(0.0f, 25.0f * DeltaSeconds, 0.0f));
	const float Pulse = 1.0f + FMath::Sin(AnimTime * 4.0f) * 0.04f;
	TargetIndicator->SetRelativeScale3D(FVector(Pulse, Pulse, 1.0f));
}

void AMMOCreature::UpdateNameplate()
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const bool bInRange = Player && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) <= NameplateDistance;
	Nameplate->SetVisibility(bInRange && !IsHidden());

	if (UMMONameplateWidget* Widget = Cast<UMMONameplateWidget>(Nameplate->GetUserWidgetObject()))
	{
		Widget->SetNameplateState(DisplayName, CreatureLevel, Health->GetHealthPercent(), bTargeted, bIsDead, IsInCombat(), IsLootable());
	}
}

void AMMOCreature::PlaySoundHere(USoundBase* Sound) const
{
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
}
