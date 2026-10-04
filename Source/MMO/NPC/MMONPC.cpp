// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/MMONPC.h"
#include "MMOCharacter.h"
#include "Quests/MMOQuestDefinition.h"
#include "Quests/MMOQuestLogComponent.h"
#include "UI/MMOHUD.h"
#include "UI/MMONPCPlateWidget.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AMMONPC::AMMONPC()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	AutoPossessAI = EAutoPossessAI::Disabled;
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	GetCharacterMovement()->GravityScale = 1.0f;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> DefaultMesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> DefaultIdle(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle.MM_Idle"));
	BodyMesh = DefaultMesh.Object;
	IdleAnimation = DefaultIdle.Object;

	GetMesh()->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -96.0f), FRotator(0.0f, -90.0f, 0.0f));
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

	Accessory = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Accessory"));
	Accessory->SetupAttachment(RootComponent);
	Accessory->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Plate = CreateDefaultSubobject<UWidgetComponent>(TEXT("Plate"));
	Plate->SetupAttachment(RootComponent);
	Plate->SetWidgetSpace(EWidgetSpace::Screen);
	Plate->SetWidgetClass(UMMONPCPlateWidget::StaticClass());
	Plate->SetDrawAtDesiredSize(true);
	Plate->SetPivot(FVector2D(0.5f, 1.0f));
	Plate->SetRelativeLocation(FVector(0.0f, 0.0f, 125.0f));
	Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DisplayName = NSLOCTEXT("MMONPC", "Villager", "Villager");
}

void AMMONPC::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLook();
}

void AMMONPC::BeginPlay()
{
	Super::BeginPlay();

	HomeRotation = GetActorRotation();
	MMOInteraction::Register(this);
	ApplyLook();
	if (IdleAnimation)
	{
		GetMesh()->PlayAnimation(IdleAnimation, true);
	}
}

void AMMONPC::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MMOInteraction::Unregister(this);
	Super::EndPlay(EndPlayReason);
}

void AMMONPC::ApplyLook()
{
	if (BodyMesh)
	{
		GetMesh()->SetSkeletalMeshAsset(BodyMesh);
	}
	for (int32 Index = 0; Index < BodyTints.Num() && Index < GetMesh()->GetNumMaterials(); ++Index)
	{
		if (BodyTints[Index].A > 0.0f)
		{
			if (UMaterialInstanceDynamic* Tinted = GetMesh()->CreateDynamicMaterialInstance(Index))
			{
				Tinted->SetVectorParameterValue(TEXT("Paint Tint"), BodyTints[Index]);
			}
		}
	}
	Accessory->SetStaticMesh(AccessoryMesh);
	Accessory->SetMaterial(0, AccessoryMaterial);
	Accessory->SetRelativeTransform(AccessoryTransform);
	Accessory->SetVisibility(AccessoryMesh != nullptr);
}

void AMMONPC::Interact(AMMOCharacter* Player)
{
	if (!Player)
	{
		return;
	}

	FaceActor(Player);
	const APlayerController* PC = Cast<APlayerController>(Player->GetController());
	if (AMMOHUD* HUD = PC ? Cast<AMMOHUD>(PC->GetHUD()) : nullptr)
	{
		HUD->OpenDialogue(this);
	}
}

void AMMONPC::FaceActor(const AActor* Other)
{
	FacingTarget = Other;
	FacingTime = 8.0f;
}

EMMONPCMarker AMMONPC::GetMarker(const UMMOQuestLogComponent* QuestLog) const
{
	if (!QuestLog)
	{
		return EMMONPCMarker::None;
	}

	EMMONPCMarker Marker = EMMONPCMarker::None;
	for (const UMMOQuestDefinition* Quest : Quests)
	{
		if (!Quest)
		{
			continue;
		}
		const EMMOQuestState State = QuestLog->GetQuestState(Quest);
		if (Quest->TurnInId == NPCId && State == EMMOQuestState::ReadyToTurnIn)
		{
			return EMMONPCMarker::QuestReady;
		}
		if (Quest->GiverId == NPCId && State == EMMOQuestState::Available)
		{
			Marker = EMMONPCMarker::QuestAvailable;
		}
		else if (Quest->TurnInId == NPCId && State == EMMOQuestState::Active && Marker == EMMONPCMarker::None)
		{
			Marker = EMMONPCMarker::QuestInProgress;
		}
	}
	return Marker;
}

TArray<UMMOQuestDefinition*> AMMONPC::GetDialogueQuests(const UMMOQuestLogComponent* QuestLog) const
{
	TArray<UMMOQuestDefinition*> Result;
	if (!QuestLog)
	{
		return Result;
	}
	for (UMMOQuestDefinition* Quest : Quests)
	{
		if (!Quest)
		{
			continue;
		}
		const EMMOQuestState State = QuestLog->GetQuestState(Quest);
		const bool bOffer = Quest->GiverId == NPCId && State == EMMOQuestState::Available;
		const bool bAccept = Quest->TurnInId == NPCId && (State == EMMOQuestState::Active || State == EMMOQuestState::ReadyToTurnIn);
		if (bOffer || bAccept)
		{
			Result.Add(Quest);
		}
	}
	return Result;
}

void AMMONPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// turn toward whoever is talking to us, then back to the usual pose
	FacingTime = FMath::Max(0.0f, FacingTime - DeltaSeconds);
	FRotator Desired = HomeRotation;
	if (FacingTime > 0.0f && FacingTarget.IsValid())
	{
		FVector To = FacingTarget->GetActorLocation() - GetActorLocation();
		To.Z = 0.0f;
		if (!To.IsNearlyZero())
		{
			Desired = To.Rotation();
		}
	}
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Desired, DeltaSeconds, 5.0f));

	UpdatePlate();
}

void AMMONPC::UpdatePlate()
{
	const AMMOCharacter* Player = Cast<AMMOCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	const bool bInRange = Player && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) <= PlateDistance;
	Plate->SetVisibility(bInRange);

	if (UMMONPCPlateWidget* Widget = Cast<UMMONPCPlateWidget>(Plate->GetUserWidgetObject()))
	{
		Widget->SetPlate(DisplayName, Title, Player ? GetMarker(Player->GetQuestLog()) : EMMONPCMarker::None);
	}
}
