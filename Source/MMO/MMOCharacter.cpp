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
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Combat/MMOHealthComponent.h"
#include "Combat/MMOProgressionComponent.h"
#include "Combat/MMOCombatComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "MMO.h"

namespace MMOCharacterSounds
{
	static const FName Swing(TEXT("Swing"));
	static const FName MeleeImpact(TEXT("MeleeImpact"));
	static const FName Hurt(TEXT("Hurt"));
	static const FName LevelUp(TEXT("LevelUp"));
	static const FName AutoAttackOn(TEXT("AutoAttackOn"));
	static const FName AutoAttackOff(TEXT("AutoAttackOff"));
	static const FName Death(TEXT("Death"));

	static TSoftObjectPtr<USoundBase> Default(const TCHAR* AssetName)
	{
		return TSoftObjectPtr<USoundBase>(FSoftObjectPath(FString::Printf(TEXT("/Game/MMO/Audio/%s.%s"), AssetName, AssetName)));
	}
}

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

	// presentation defaults (soft references: missing assets just mean no sound/effect)
	SwingSound = MMOCharacterSounds::Default(TEXT("S_MMO_Swing"));
	MeleeImpactSound = MMOCharacterSounds::Default(TEXT("S_MMO_MeleeImpact"));
	HurtSound = MMOCharacterSounds::Default(TEXT("S_MMO_PlayerHurt"));
	LevelUpSound = MMOCharacterSounds::Default(TEXT("S_MMO_LevelUp"));
	AutoAttackOnSound = MMOCharacterSounds::Default(TEXT("S_MMO_AutoAttackOn"));
	AutoAttackOffSound = MMOCharacterSounds::Default(TEXT("S_MMO_AutoAttackOff"));
	DeathSound = MMOCharacterSounds::Default(TEXT("S_MMO_PlayerDeath"));
	MeleeImpactEffect = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Variant_Combat/VFX/NS_Damage.NS_Damage")));
	MeleeImpactCameraShake = TSoftClassPtr<UCameraShakeBase>(FSoftObjectPath(TEXT("/Game/Variant_Combat/Blueprints/BP_CameraShake_Hit_Enemy.BP_CameraShake_Hit_Enemy_C")));
	HurtCameraShake = TSoftClassPtr<UCameraShakeBase>(FSoftObjectPath(TEXT("/Game/Variant_Combat/Blueprints/BP_CameraShake_Hit_Player.BP_CameraShake_Hit_Player_C")));

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
	Combat->OnSwingStarted.AddDynamic(this, &AMMOCharacter::HandleSwingStarted);
	Combat->OnAutoAttackChanged.AddDynamic(this, &AMMOCharacter::HandleAutoAttackChanged);

	GetWorldTimerManager().SetTimer(RegenTimer, this, &AMMOCharacter::TickRegeneration, 0.5f, true);

	// MMO camera: a little higher and further back than the template, with smooth follow and wheel zoom
	CameraMaxDistance = FMath::Max(CameraMaxDistance, CameraMinDistance);
	DesiredCameraDistance = FMath::Clamp(CameraDefaultDistance, CameraMinDistance, CameraMaxDistance);
	GetCameraBoom()->TargetArmLength = DesiredCameraDistance;
	GetCameraBoom()->SocketOffset = CameraSocketOffset;
	GetCameraBoom()->bEnableCameraLag = CameraLagSpeed > 0.0f;
	GetCameraBoom()->CameraLagSpeed = CameraLagSpeed;
	GetCameraBoom()->bDoCollisionTest = true;
	if (AController* PlayerController = GetController())
	{
		FRotator ControlRotation = PlayerController->GetControlRotation();
		ControlRotation.Pitch = CameraInitialPitch;
		PlayerController->SetControlRotation(ControlRotation);
	}

	LoadedSounds.Add(MMOCharacterSounds::Swing, SwingSound.LoadSynchronous());
	LoadedSounds.Add(MMOCharacterSounds::MeleeImpact, MeleeImpactSound.LoadSynchronous());
	LoadedSounds.Add(MMOCharacterSounds::Hurt, HurtSound.LoadSynchronous());
	LoadedSounds.Add(MMOCharacterSounds::LevelUp, LevelUpSound.LoadSynchronous());
	LoadedSounds.Add(MMOCharacterSounds::AutoAttackOn, AutoAttackOnSound.LoadSynchronous());
	LoadedSounds.Add(MMOCharacterSounds::AutoAttackOff, AutoAttackOffSound.LoadSynchronous());
	LoadedSounds.Add(MMOCharacterSounds::Death, DeathSound.LoadSynchronous());
	LoadedMeleeImpactEffect = MeleeImpactEffect.LoadSynchronous();
	LoadedMeleeImpactCameraShake = MeleeImpactCameraShake.LoadSynchronous();
	LoadedHurtCameraShake = HurtCameraShake.LoadSynchronous();
}

void AMMOCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// ease toward the requested zoom distance
	USpringArmComponent* Boom = GetCameraBoom();
	if (!FMath::IsNearlyEqual(Boom->TargetArmLength, DesiredCameraDistance, 0.5f))
	{
		Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, DesiredCameraDistance, DeltaSeconds, CameraZoomSpeed);
	}
}

void AMMOCharacter::NotifyMeleeHitFrame()
{
	Combat->NotifyMeleeHitFrame();
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
		EnhancedInputComponent->BindAction(ClearTargetAction, ETriggerEvent::Started, this, &AMMOCharacter::DoClearTarget);

		// Camera zoom
		EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AMMOCharacter::Zoom);
		
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

	auto EnsureAction = [this, bCreateContext](TObjectPtr<UInputAction>& Action, const TCHAR* Name, const FKey& Key, EInputActionValueType ValueType = EInputActionValueType::Boolean)
	{
		if (!Action)
		{
			Action = NewObject<UInputAction>(this, Name);
			Action->ValueType = ValueType;
		}
		if (bCreateContext)
		{
			CombatMappingContext->MapKey(Action, Key);
		}
	};

	EnsureAction(TargetAction, TEXT("IA_MMOTarget_Runtime"), EKeys::LeftMouseButton);
	EnsureAction(CycleTargetAction, TEXT("IA_MMOCycleTarget_Runtime"), EKeys::Tab);
	EnsureAction(BasicAttackAction, TEXT("IA_MMOBasicAttack_Runtime"), EKeys::One);
	EnsureAction(ClearTargetAction, TEXT("IA_MMOClearTarget_Runtime"), EKeys::Escape);
	EnsureAction(ZoomAction, TEXT("IA_MMOZoom_Runtime"), EKeys::MouseWheelAxis, EInputActionValueType::Axis1D);

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
		Combat->ToggleAutoAttack();
	}
}

void AMMOCharacter::DoClearTarget()
{
	Combat->ClearTarget();
}

void AMMOCharacter::DoZoom(float Amount)
{
	DesiredCameraDistance = FMath::Clamp(DesiredCameraDistance - Amount * CameraZoomStep, CameraMinDistance, CameraMaxDistance);
}

void AMMOCharacter::Zoom(const FInputActionValue& Value)
{
	DoZoom(Value.Get<float>());
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
	PlayPresentationSound(MMOCharacterSounds::LevelUp);

	UE_LOG(LogMMO, Log, TEXT("Player reached level %d (Max Health %.0f, Basic Attack %.0f)"), NewLevel, Health->GetMaxHealth(), Combat->BasicAttackDamage);
}

void AMMOCharacter::HandleDamaged(float Amount, AActor* DamageInstigator)
{
	LastDamageTakenTime = GetWorld()->GetTimeSeconds();

	if (!IsDead())
	{
		PlayPresentationSound(MMOCharacterSounds::Hurt);
		PlayCameraShake(LoadedHurtCameraShake, 1.0f);
	}
}

void AMMOCharacter::HandleDeath(AActor* Killer)
{
	UE_LOG(LogMMO, Log, TEXT("Player was killed by %s"), *GetNameSafe(Killer));

	Combat->ClearTarget();
	PlayPresentationSound(MMOCharacterSounds::Death);

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
	// the swing connected: impact sound, effect on the target, and a light camera kick
	if (!Target || Damage <= 0.0f)
	{
		return;
	}

	const FVector ImpactLocation = Target->GetActorLocation() + (GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D() * 35.0f;
	PlayPresentationSound(MMOCharacterSounds::MeleeImpact, &ImpactLocation);

	if (LoadedMeleeImpactEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, LoadedMeleeImpactEffect, ImpactLocation);
	}

	PlayCameraShake(LoadedMeleeImpactCameraShake, 0.35f);
}

void AMMOCharacter::HandleSwingStarted(AActor* Target)
{
	PlayPresentationSound(MMOCharacterSounds::Swing);
}

void AMMOCharacter::HandleAutoAttackChanged(bool bActive)
{
	PlayPresentationSound(bActive ? MMOCharacterSounds::AutoAttackOn : MMOCharacterSounds::AutoAttackOff);
}

void AMMOCharacter::PlayPresentationSound(FName Key, const FVector* Location) const
{
	const TObjectPtr<USoundBase>* Sound = LoadedSounds.Find(Key);
	if (!Sound || !*Sound)
	{
		return;
	}

	if (Location)
	{
		UGameplayStatics::PlaySoundAtLocation(this, *Sound, *Location);
	}
	else
	{
		UGameplayStatics::PlaySound2D(this, *Sound);
	}
}

void AMMOCharacter::PlayCameraShake(TSubclassOf<UCameraShakeBase> Shake, float Scale) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (Shake && PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraShake(Shake, Scale);
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
