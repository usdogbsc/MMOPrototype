// Copyright Epic Games, Inc. All Rights Reserved.

#include "MMOCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Combat/MMOHealthComponent.h"
#include "Combat/MMOProgressionComponent.h"
#include "Combat/MMOCombatComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "MMO.h"

AMMOCharacter::AMMOCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Combat, progression and health
	Health = CreateDefaultSubobject<UMMOHealthComponent>(TEXT("Health"));
	Health->MaxHealth = 100.0f;

	Progression = CreateDefaultSubobject<UMMOProgressionComponent>(TEXT("Progression"));

	Combat = CreateDefaultSubobject<UMMOCombatComponent>(TEXT("Combat"));

	// default attack swing from the shared mannequin animations. Can be overridden in the Blueprint
	static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> AttackAnim(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/MM_Attack_01.MM_Attack_01"));
	if (AttackAnim.Succeeded())
	{
		BasicAttackAnimation = AttackAnim.Object;
	}

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AMMOCharacter::BeginPlay()
{
	Super::BeginPlay();

	BaseMaxHealth = Health->GetMaxHealth();
	BaseAttackDamage = Combat->BasicAttackDamage;
	RespawnTransform = GetActorTransform();
	MeshRelativeTransform = GetMesh()->GetRelativeTransform();
	MeshCollisionProfile = GetMesh()->GetCollisionProfileName();

	ApplyLevelStats(Progression->GetLevel(), true);

	Health->OnDamaged.AddDynamic(this, &AMMOCharacter::HandleDamaged);
	Health->OnDeath.AddDynamic(this, &AMMOCharacter::HandleDeath);
	Progression->OnLevelUp.AddDynamic(this, &AMMOCharacter::HandleLevelUp);
	Combat->OnBasicAttack.AddDynamic(this, &AMMOCharacter::HandleBasicAttack);

	GetWorldTimerManager().SetTimer(RegenTimer, this, &AMMOCharacter::TickRegeneration, 0.5f, true);
}

void AMMOCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {

		// Combat
		CreateDefaultCombatInput();
		EnhancedInputComponent->BindAction(TargetAction, ETriggerEvent::Started, this, &AMMOCharacter::DoTarget);
		EnhancedInputComponent->BindAction(CycleTargetAction, ETriggerEvent::Started, this, &AMMOCharacter::DoCycleTarget);
		EnhancedInputComponent->BindAction(BasicAttackAction, ETriggerEvent::Started, this, &AMMOCharacter::DoBasicAttack);
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMMOCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AMMOCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMMOCharacter::Look);
	}
	else
	{
		UE_LOG(LogMMO, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AMMOCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AMMOCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AMMOCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AMMOCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AMMOCharacter::DoJumpStart()
{
	// signal the character to jump
	Jump();
}

void AMMOCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

void AMMOCharacter::CreateDefaultCombatInput()
{
	// Runtime defaults keep the milestone playable without authoring new input assets.
	// Assign real Input Action / Mapping Context assets in the Blueprint to override these.
	const bool bCreateContext = CombatMappingContext == nullptr;
	if (bCreateContext)
	{
		CombatMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_MMOCombat_Runtime"));
	}

	auto EnsureAction = [this, bCreateContext](TObjectPtr<UInputAction>& Action, const TCHAR* Name, const FKey& Key)
	{
		if (!Action)
		{
			Action = NewObject<UInputAction>(this, Name);
			Action->ValueType = EInputActionValueType::Boolean;
		}
		if (bCreateContext)
		{
			CombatMappingContext->MapKey(Action, Key);
		}
	};

	EnsureAction(TargetAction, TEXT("IA_MMOTarget_Runtime"), EKeys::LeftMouseButton);
	EnsureAction(CycleTargetAction, TEXT("IA_MMOCycleTarget_Runtime"), EKeys::Tab);
	EnsureAction(BasicAttackAction, TEXT("IA_MMOBasicAttack_Runtime"), EKeys::One);

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (!Subsystem->HasMappingContext(CombatMappingContext))
			{
				Subsystem->AddMappingContext(CombatMappingContext, 1);
			}
		}
	}
}

void AMMOCharacter::DoTarget()
{
	if (!IsDead())
	{
		Combat->TargetFromView();
	}
}

void AMMOCharacter::DoCycleTarget()
{
	if (!IsDead())
	{
		Combat->CycleTarget();
	}
}

void AMMOCharacter::DoBasicAttack()
{
	if (!IsDead())
	{
		Combat->TryBasicAttack();
	}
}

bool AMMOCharacter::IsDead() const
{
	return Health && Health->IsDead();
}

float AMMOCharacter::GetRespawnTimeRemaining() const
{
	if (!IsDead() || !GetWorld())
	{
		return 0.0f;
	}
	return FMath::Max(0.0f, static_cast<float>(RespawnTime - GetWorld()->GetTimeSeconds()));
}

void AMMOCharacter::ApplyLevelStats(int32 Level, bool bFillHealth)
{
	const int32 LevelsAboveFirst = FMath::Max(0, Level - 1);
	Health->SetMaxHealth(BaseMaxHealth + MaxHealthPerLevel * LevelsAboveFirst, bFillHealth);
	Combat->BasicAttackDamage = BaseAttackDamage + AttackDamagePerLevel * LevelsAboveFirst;
}

void AMMOCharacter::HandleLevelUp(int32 NewLevel)
{
	// levelling up fully heals: a classic, satisfying reward that also keeps the test loop moving
	ApplyLevelStats(NewLevel, true);

	UE_LOG(LogMMO, Log, TEXT("Player reached level %d (Max Health %.0f, Basic Attack %.0f)"), NewLevel, Health->GetMaxHealth(), Combat->BasicAttackDamage);
}

void AMMOCharacter::HandleDamaged(float Amount, AActor* DamageInstigator)
{
	LastDamageTakenTime = GetWorld()->GetTimeSeconds();
}

void AMMOCharacter::HandleDeath(AActor* Killer)
{
	UE_LOG(LogMMO, Log, TEXT("Player was killed by %s"), *GetNameSafe(Killer));

	Combat->ClearTarget();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		DisableInput(PC);
	}

	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// collapse into a ragdoll
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));
	GetMesh()->SetSimulatePhysics(true);

	RespawnTime = GetWorld()->GetTimeSeconds() + RespawnDelay;
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AMMOCharacter::RespawnPlayer, FMath::Max(0.1f, RespawnDelay), false);
}

void AMMOCharacter::RespawnPlayer()
{
	// restore the mesh from ragdoll
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->SetCollisionProfileName(MeshCollisionProfile);
	GetMesh()->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	GetMesh()->SetRelativeTransform(MeshRelativeTransform);

	TeleportTo(RespawnTransform.GetLocation(), RespawnTransform.Rotator(), false, true);
	if (AController* PlayerController = GetController())
	{
		PlayerController->SetControlRotation(RespawnTransform.Rotator());
	}

	Health->ResetHealth();
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		EnableInput(PC);
	}

	UE_LOG(LogMMO, Log, TEXT("Player respawned"));
}

void AMMOCharacter::HandleBasicAttack(AActor* Target, float Damage)
{
	if (!BasicAttackAnimation)
	{
		return;
	}

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->PlaySlotAnimationAsDynamicMontage(BasicAttackAnimation, TEXT("DefaultSlot"), 0.1f, 0.2f, BasicAttackAnimationPlayRate);
	}
}

void AMMOCharacter::TickRegeneration()
{
	if (IsDead() || Health->GetCurrentHealth() >= Health->GetMaxHealth())
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	const double LastCombat = FMath::Max(LastDamageTakenTime, Combat->GetLastAttackTime());
	if (Now - LastCombat >= OutOfCombatDelay)
	{
		Health->Heal(OutOfCombatRegenPerSecond * 0.5f);
	}
}
