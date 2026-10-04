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
#include "Creatures/MMOCreature.h"
#include "Items/MMOEquipmentComponent.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "Items/MMOLootContainerComponent.h"
#include "UI/MMOHUD.h"
#include "World/MMOExplorationComponent.h"
#include "World/MMOInteractable.h"
#include "Quests/MMOQuestLogComponent.h"
#include "Combat/MMOCooldownComponent.h"
#include "Items/MMOActionBarComponent.h"
#include "Combat/MMOAbilityComponent.h"
#include "Combat/MMOAbilityDefinition.h"
#include "Professions/MMOProfessionComponent.h"
#include "UI/MMOHUDWidget.h"
#include "NPC/MMONPC.h"
#include "Save/MMOSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
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
	static const FName Loot(TEXT("Loot"));
	static const FName RareLoot(TEXT("RareLoot"));
	static const FName Coin(TEXT("Coin"));
	static const FName Equip(TEXT("Equip"));
	static const FName Error(TEXT("Error"));
	static const FName Drink(TEXT("Drink"));
	static const FName Eat(TEXT("Eat"));
	static const FName Ability(TEXT("Ability"));
	static const FName HealSpell(TEXT("Heal"));

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

	// items
	Inventory = CreateDefaultSubobject<UMMOInventoryComponent>(TEXT("Inventory"));
	Equipment = CreateDefaultSubobject<UMMOEquipmentComponent>(TEXT("Equipment"));
	Exploration = CreateDefaultSubobject<UMMOExplorationComponent>(TEXT("Exploration"));
	QuestLog = CreateDefaultSubobject<UMMOQuestLogComponent>(TEXT("QuestLog"));
	Cooldowns = CreateDefaultSubobject<UMMOCooldownComponent>(TEXT("Cooldowns"));
	ActionBar = CreateDefaultSubobject<UMMOActionBarComponent>(TEXT("ActionBar"));
	Abilities = CreateDefaultSubobject<UMMOAbilityComponent>(TEXT("Abilities"));
	Professions = CreateDefaultSubobject<UMMOProfessionComponent>(TEXT("Professions"));

	MainHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MainHandMesh"));
	MainHandMesh->SetupAttachment(GetMesh(), MainHandSocket);
	MainHandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MainHandMesh->SetCastShadow(true);

	// presentation defaults (soft references: missing assets just mean no sound/effect)
	SwingSound = MMOCharacterSounds::Default(TEXT("S_MMO_Swing"));
	MeleeImpactSound = MMOCharacterSounds::Default(TEXT("S_MMO_MeleeImpact"));
	HurtSound = MMOCharacterSounds::Default(TEXT("S_MMO_PlayerHurt"));
	LevelUpSound = MMOCharacterSounds::Default(TEXT("S_MMO_LevelUp"));
	AutoAttackOnSound = MMOCharacterSounds::Default(TEXT("S_MMO_AutoAttackOn"));
	AutoAttackOffSound = MMOCharacterSounds::Default(TEXT("S_MMO_AutoAttackOff"));
	DeathSound = MMOCharacterSounds::Default(TEXT("S_MMO_PlayerDeath"));
	LootSound = MMOCharacterSounds::Default(TEXT("S_MMO_LootPickup"));
	RareLootSound = MMOCharacterSounds::Default(TEXT("S_MMO_RareLoot"));
	CoinSound = MMOCharacterSounds::Default(TEXT("S_MMO_Coins"));
	EquipSound = MMOCharacterSounds::Default(TEXT("S_MMO_Equip"));
	ErrorSound = MMOCharacterSounds::Default(TEXT("S_MMO_Error"));
	DrinkSound = MMOCharacterSounds::Default(TEXT("S_MMO_Drink"));
	EatSound = MMOCharacterSounds::Default(TEXT("S_MMO_Eat"));
	AbilitySound = MMOCharacterSounds::Default(TEXT("S_MMO_AbilityHit"));
	HealSound = MMOCharacterSounds::Default(TEXT("S_MMO_Heal"));
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
	RespawnTransform = GetActorTransform();
	MeshRelativeTransform = GetMesh()->GetRelativeTransform();
	MeshCollisionProfile = GetMesh()->GetCollisionProfileName();

	// make sure the weapon visual follows the configured socket even if the Blueprint changed the mesh
	MainHandMesh->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, MainHandSocket);

	Equipment->OnEquipmentChanged.AddDynamic(this, &AMMOCharacter::HandleEquipmentChanged);
	Inventory->OnItemsReceived.AddDynamic(this, &AMMOCharacter::HandleItemsReceived);
	Inventory->OnCurrencyReceived.AddDynamic(this, &AMMOCharacter::HandleCurrencyReceived);

	// starting gear was equipped during component BeginPlay, before these bindings existed
	RecalculateStats(true);
	RefreshEquipmentVisuals();

	Health->OnDamaged.AddDynamic(this, &AMMOCharacter::HandleDamaged);
	Health->OnDeath.AddDynamic(this, &AMMOCharacter::HandleDeath);
	Progression->OnLevelUp.AddDynamic(this, &AMMOCharacter::HandleLevelUp);
	Combat->OnBasicAttack.AddDynamic(this, &AMMOCharacter::HandleBasicAttack);
	Combat->OnSwingStarted.AddDynamic(this, &AMMOCharacter::HandleSwingStarted);
	Combat->OnAutoAttackChanged.AddDynamic(this, &AMMOCharacter::HandleAutoAttackChanged);

	GetWorldTimerManager().SetTimer(RegenTimer, this, &AMMOCharacter::TickRegeneration, 0.5f, true);

	// persistence: load once possessed, then autosave on a timer and soon after important events
	GetWorldTimerManager().SetTimerForNextTick(this, &AMMOCharacter::LoadSavedGame);
	GetWorldTimerManager().SetTimer(SaveTimer, this, &AMMOCharacter::TickAutoSave, 2.0f, true);
	Progression->OnLevelUp.AddDynamic(this, &AMMOCharacter::HandleLevelUpForSave);
	QuestLog->OnQuestLogChanged.AddDynamic(this, &AMMOCharacter::RequestSave);
	Equipment->OnEquipmentChanged.AddDynamic(this, &AMMOCharacter::RequestSave);
	Exploration->OnLocationDiscovered.AddDynamic(this, &AMMOCharacter::HandleLocationDiscoveredForSave);

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
	LoadedSounds.Add(MMOCharacterSounds::Loot, LootSound.LoadSynchronous());
	LoadedSounds.Add(MMOCharacterSounds::RareLoot, RareLootSound.LoadSynchronous());
	LoadedSounds.Add(MMOCharacterSounds::Coin, CoinSound.LoadSynchronous());
	LoadedSounds.Add(MMOCharacterSounds::Equip, EquipSound.LoadSynchronous());
	LoadedSounds.Add(MMOCharacterSounds::Error, ErrorSound.LoadSynchronous());
	LoadedSounds.Add(MMOCharacterSounds::Drink, DrinkSound.LoadSynchronous());
	LoadedSounds.Add(MMOCharacterSounds::Eat, EatSound.LoadSynchronous());
	LoadedSounds.Add(MMOCharacterSounds::Ability, AbilitySound.LoadSynchronous());
	LoadedSounds.Add(MMOCharacterSounds::HealSpell, HealSound.LoadSynchronous());

	// abilities known at the starting level; later ones arrive with level ups
	Abilities->RefreshKnown(Progression->GetLevel(), false);
	Abilities->OnAbilityLearned.AddDynamic(this, &AMMOCharacter::HandleAbilityLearned);
	LoadedMeleeImpactEffect = MeleeImpactEffect.LoadSynchronous();
	LoadedMeleeImpactCameraShake = MeleeImpactCameraShake.LoadSynchronous();
	LoadedHurtCameraShake = HurtCameraShake.LoadSynchronous();
}

void AMMOCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateHoverCursor();

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
		EnhancedInputComponent->BindAction(TargetAction, ETriggerEvent::Started, this, &AMMOCharacter::OnLeftMousePressed);
		EnhancedInputComponent->BindAction(TargetAction, ETriggerEvent::Completed, this, &AMMOCharacter::OnLeftMouseReleased);
		EnhancedInputComponent->BindAction(RightClickAction, ETriggerEvent::Started, this, &AMMOCharacter::OnRightMousePressed);
		EnhancedInputComponent->BindAction(RightClickAction, ETriggerEvent::Completed, this, &AMMOCharacter::OnRightMouseReleased);
		EnhancedInputComponent->BindAction(CycleTargetAction, ETriggerEvent::Started, this, &AMMOCharacter::DoCycleTarget);
		EnhancedInputComponent->BindAction(BasicAttackAction, ETriggerEvent::Started, this, &AMMOCharacter::DoBasicAttack);
		EnhancedInputComponent->BindAction(ClearTargetAction, ETriggerEvent::Started, this, &AMMOCharacter::DoClearTarget);

		// Items
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AMMOCharacter::DoInteract);
		EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Started, this, &AMMOCharacter::DoToggleInventory);
		EnhancedInputComponent->BindAction(CharacterAction, ETriggerEvent::Started, this, &AMMOCharacter::DoToggleCharacter);
		EnhancedInputComponent->BindAction(QuestLogAction, ETriggerEvent::Started, this, &AMMOCharacter::DoToggleQuestLog);
		EnhancedInputComponent->BindAction(AbilitiesAction, ETriggerEvent::Started, this, &AMMOCharacter::DoToggleAbilities);
		for (int32 Index = 0; Index < ActionSlotActions.Num(); ++Index)
		{
			EnhancedInputComponent->BindActionValueLambda(ActionSlotActions[Index], ETriggerEvent::Started, [this, Index](const FInputActionValue&) { UseActionSlot(Index); });
		}

		// Camera zoom
		EnhancedInputComponent->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AMMOCharacter::Zoom);
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMMOCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AMMOCharacter::MouseLook);

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

	auto EnsureAction = [this, bCreateContext](TObjectPtr<UInputAction>& Action, const TCHAR* Name, const FKey& Key, EInputActionValueType ValueType = EInputActionValueType::Boolean, FKey AltKey = FKey())
	{
		if (!Action)
		{
			Action = NewObject<UInputAction>(this, Name);
			Action->ValueType = ValueType;
		}
		if (bCreateContext)
		{
			CombatMappingContext->MapKey(Action, Key);
			if (AltKey.IsValid())
			{
				CombatMappingContext->MapKey(Action, AltKey);
			}
		}
	};

	EnsureAction(TargetAction, TEXT("IA_MMOTarget_Runtime"), EKeys::LeftMouseButton);
	EnsureAction(CycleTargetAction, TEXT("IA_MMOCycleTarget_Runtime"), EKeys::Tab);
	EnsureAction(BasicAttackAction, TEXT("IA_MMOBasicAttack_Runtime"), EKeys::One);
	EnsureAction(ClearTargetAction, TEXT("IA_MMOClearTarget_Runtime"), EKeys::Escape);
	EnsureAction(ZoomAction, TEXT("IA_MMOZoom_Runtime"), EKeys::MouseWheelAxis, EInputActionValueType::Axis1D);
	EnsureAction(InteractAction, TEXT("IA_MMOInteract_Runtime"), EKeys::F);
	EnsureAction(RightClickAction, TEXT("IA_MMORightClick_Runtime"), EKeys::RightMouseButton);
	EnsureAction(InventoryAction, TEXT("IA_MMOInventory_Runtime"), EKeys::B, EInputActionValueType::Boolean, EKeys::I);
	EnsureAction(CharacterAction, TEXT("IA_MMOCharacter_Runtime"), EKeys::C);
	EnsureAction(QuestLogAction, TEXT("IA_MMOQuestLog_Runtime"), EKeys::L);
	EnsureAction(AbilitiesAction, TEXT("IA_MMOAbilities_Runtime"), EKeys::K);

	// hotbar keys 2-9
	static const FKey SlotKeys[] = { EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	if (ActionSlotActions.Num() != UE_ARRAY_COUNT(SlotKeys))
	{
		ActionSlotActions.Reset();
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(SlotKeys); ++Index)
		{
			TObjectPtr<UInputAction> Action;
			EnsureAction(Action, *FString::Printf(TEXT("IA_MMOActionSlot%d_Runtime"), Index + 2), SlotKeys[Index]);
			ActionSlotActions.Add(Action);
		}
	}

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
	if (IsDead())
	{
		return;
	}

	// select what is under the mouse cursor; clicking empty ground clears the target
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && PC->bShowMouseCursor)
	{
		AMMOCreature* Creature = GetCreatureUnderCursor();
		if (Creature && Creature->IsTargetable())
		{
			Combat->SetTarget(Creature);
		}
		else if (!GetInteractableUnderCursor())
		{
			Combat->ClearTarget();
		}
		return;
	}

	// no cursor (e.g. gamepad): pick what the camera is aimed at
	Combat->TargetFromView();
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
	// Escape closes open windows first, like most MMOs; otherwise it clears the target
	AMMOHUD* HUD = Cast<AMMOHUD>(Cast<APlayerController>(GetController()) ? Cast<APlayerController>(GetController())->GetHUD() : nullptr);
	if (HUD && HUD->CloseAllWindows())
	{
		return;
	}

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

void AMMOCharacter::RecalculateStats(bool bFillHealth)
{
	const int32 LevelsAboveFirst = FMath::Max(0, Progression->GetLevel() - 1);
	const FMMOStatModifiers Gear = Equipment->GetTotalStats();

	Health->SetMaxHealth(BaseMaxHealth + MaxHealthPerLevel * LevelsAboveFirst + Gear.MaxHealth, bFillHealth);
	Health->SetArmor(Gear.Armor);

	// swing damage = weapon roll + level bonus + gear bonus
	Equipment->GetWeaponDamage(Combat->WeaponDamageMin, Combat->WeaponDamageMax);
	Combat->BonusDamage = AttackDamagePerLevel * LevelsAboveFirst + Gear.AttackDamage;
}

void AMMOCharacter::HandleEquipmentChanged()
{
	RecalculateStats(false);
	RefreshEquipmentVisuals();
}

void AMMOCharacter::RefreshEquipmentVisuals()
{
	const FMMOItemStack& Weapon = Equipment->GetEquipped(EMMOEquipmentSlot::MainHand);
	UStaticMesh* WeaponMesh = Weapon.IsEmpty() ? nullptr : Weapon.Item->EquippedMesh.Get();

	MainHandMesh->SetStaticMesh(WeaponMesh);
	MainHandMesh->SetVisibility(WeaponMesh != nullptr);
	if (WeaponMesh)
	{
		MainHandMesh->SetRelativeTransform(Weapon.Item->EquippedMeshTransform);
		if (UMaterialInstanceDynamic* Material = MainHandMesh->CreateDynamicMaterialInstance(0))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Weapon.Item->EquippedMeshColor);
		}
	}
}

void AMMOCharacter::HandleItemsReceived(UMMOItemDefinition* Item, int32 Quantity)
{
	// new potions / food go straight onto the hotbar, like most MMOs
	if (Item && Item->IsUsable())
	{
		ActionBar->AutoPlace(UMMOActionBarComponent::MakeItem(Item->ItemId));
	}

	PlayPresentationSound(Item && Item->Rarity >= EMMOItemRarity::Rare ? MMOCharacterSounds::RareLoot : MMOCharacterSounds::Loot);
}

void AMMOCharacter::HandleCurrencyReceived(int32 Amount)
{
	PlayPresentationSound(MMOCharacterSounds::Coin);
}

void AMMOCharacter::ShowPlayerMessage(const FText& Message, bool bPlayErrorSound)
{
	OnPlayerMessage.Broadcast(Message);
	if (bPlayErrorSound)
	{
		PlayPresentationSound(MMOCharacterSounds::Error);
	}
}

EMMOEquipResult AMMOCharacter::EquipInventorySlot(int32 SlotIndex)
{
	const EMMOEquipResult Result = Equipment->EquipFromInventory(Inventory, SlotIndex);
	if (Result == EMMOEquipResult::Success)
	{
		PlayPresentationSound(MMOCharacterSounds::Equip);
	}
	else if (Result == EMMOEquipResult::NotEquippable)
	{
		ShowPlayerMessage(NSLOCTEXT("MMOItems", "CannotEquip", "You can't equip that."));
	}
	return Result;
}

EMMOEquipResult AMMOCharacter::UnequipSlot(EMMOEquipmentSlot Slot, int32 PreferredInventorySlot)
{
	const EMMOEquipResult Result = Equipment->Unequip(Slot, Inventory, PreferredInventorySlot);
	if (Result == EMMOEquipResult::Success)
	{
		PlayPresentationSound(MMOCharacterSounds::Equip);
	}
	else if (Result == EMMOEquipResult::InventoryFull)
	{
		ShowPlayerMessage(NSLOCTEXT("MMOItems", "InventoryFull", "Inventory Full"));
	}
	return Result;
}

bool AMMOCharacter::MoveInventorySlot(int32 FromIndex, int32 ToIndex)
{
	return Inventory->MoveSlot(FromIndex, ToIndex);
}

EMMOLootResult AMMOCharacter::LootItem(UMMOLootContainerComponent* Container, const FGuid& InstanceId)
{
	if (!CanReachLoot(Container))
	{
		return EMMOLootResult::NotFound;
	}

	const EMMOLootResult Result = Container->TakeItem(InstanceId, Inventory);
	if (Result == EMMOLootResult::InventoryFull || Result == EMMOLootResult::Partial)
	{
		ShowPlayerMessage(NSLOCTEXT("MMOItems", "InventoryFull", "Inventory Full"));
	}
	return Result;
}

EMMOLootResult AMMOCharacter::LootCurrency(UMMOLootContainerComponent* Container)
{
	return CanReachLoot(Container) ? Container->TakeCurrency(Inventory) : EMMOLootResult::NotFound;
}

EMMOLootResult AMMOCharacter::LootAll(UMMOLootContainerComponent* Container)
{
	if (!CanReachLoot(Container))
	{
		return EMMOLootResult::NotFound;
	}

	const EMMOLootResult Result = Container->TakeAll(Inventory);
	if (Result == EMMOLootResult::InventoryFull || Result == EMMOLootResult::Partial)
	{
		ShowPlayerMessage(NSLOCTEXT("MMOItems", "InventoryFull", "Inventory Full"));
	}
	return Result;
}

bool AMMOCharacter::CanReachLoot(const UMMOLootContainerComponent* Container) const
{
	const AMMOCreature* Corpse = Container ? Cast<AMMOCreature>(Container->GetOwner()) : nullptr;
	return Corpse && !IsDead() && Corpse->IsLootable() && FVector::Dist2D(Corpse->GetActorLocation(), GetActorLocation()) <= InteractRange + 150.0f;
}

AMMOCreature* AMMOCharacter::FindLootableCorpse(bool& bOutTooFar) const
{
	bOutTooFar = false;

	// prefer the corpse under the mouse cursor
	if (AMMOCreature* Corpse = GetCreatureUnderCursor())
	{
		if (Corpse->IsLootable())
		{
			bOutTooFar = FVector::Dist2D(Corpse->GetActorLocation(), GetActorLocation()) > InteractRange;
			return bOutTooFar ? nullptr : Corpse;
		}
	}

	// otherwise: the nearest lootable corpse in reach
	AMMOCreature* Best = nullptr;
	float BestDistance = InteractRange;
	for (TActorIterator<AMMOCreature> It(GetWorld()); It; ++It)
	{
		if (!It->IsLootable())
		{
			continue;
		}

		const float Distance = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
		if (Distance <= BestDistance)
		{
			BestDistance = Distance;
			Best = *It;
		}
		else if (Distance <= InteractRange * 3.0f)
		{
			bOutTooFar = true;
		}
	}

	if (Best)
	{
		bOutTooFar = false;
	}
	return Best;
}

void AMMOCharacter::MouseLook(const FInputActionValue& Value)
{
	// classic MMO controls: the cursor is free; holding a mouse button and dragging turns the camera
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (PC && PC->bShowMouseCursor && !bLeftMouseHeld && !bRightMouseHeld)
	{
		return;
	}

	const FVector2D Delta = Value.Get<FVector2D>();
	const float Moved = FMath::Abs(Delta.X) + FMath::Abs(Delta.Y);
	if (bLeftMouseHeld)
	{
		LeftMouseDrag += Moved;
	}
	if (bRightMouseHeld)
	{
		RightMouseDrag += Moved;
	}

	DoLook(Delta.X, Delta.Y);
}

void AMMOCharacter::OnLeftMousePressed()
{
	bLeftMouseHeld = true;
	LeftMouseDrag = 0.0f;
}

void AMMOCharacter::OnLeftMouseReleased()
{
	// a click (not a camera drag) selects what is under the cursor
	const bool bWasClick = bLeftMouseHeld && LeftMouseDrag < ClickDragThreshold;
	bLeftMouseHeld = false;
	if (bWasClick)
	{
		DoTarget();
	}
}

void AMMOCharacter::OnRightMousePressed()
{
	bRightMouseHeld = true;
	RightMouseDrag = 0.0f;
}

void AMMOCharacter::OnRightMouseReleased()
{
	const bool bWasClick = bRightMouseHeld && RightMouseDrag < ClickDragThreshold;
	bRightMouseHeld = false;
	if (bWasClick && !IsDead())
	{
		// whichever is nearer along the cursor ray: an NPC / object, or a creature
		float InteractableDistance = 0.0f;
		AActor* Interactable = GetInteractableUnderCursor(&InteractableDistance);
		AMMOCreature* Creature = GetCreatureUnderCursor();
		const APlayerController* PC = Cast<APlayerController>(GetController());
		FVector Origin, Direction;
		const bool bCreatureNearer = Creature && PC && PC->DeprojectMousePositionToWorld(Origin, Direction)
			&& FVector::DotProduct(Creature->GetActorLocation() - Origin, Direction.GetSafeNormal()) < InteractableDistance;

		if (Interactable && !bCreatureNearer)
		{
			TryInteract(Interactable);
		}
		else
		{
			InteractWith(Creature);
		}
	}
}

AActor* AMMOCharacter::GetInteractableUnderCursor(float* OutDistance) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	FVector Origin, Direction;
	if (!PC || !PC->bShowMouseCursor || !PC->DeprojectMousePositionToWorld(Origin, Direction))
	{
		return nullptr;
	}
	const FVector Dir = Direction.GetSafeNormal();

	AActor* Best = nullptr;
	float BestAlong = 6000.0f;
	for (AActor* Actor : MMOInteraction::GetAll(GetWorld()))
	{
		const IMMOInteractable* Interactable = Cast<IMMOInteractable>(Actor);
		const FVector Center = Interactable->GetInteractLocation();
		const float Along = FVector::DotProduct(Center - Origin, Dir);
		if (Along <= 0.0f || Along >= BestAlong || FVector::Dist(Origin + Dir * Along, Center) > Interactable->GetInteractPickRadius())
		{
			continue;
		}

		FCollisionQueryParams Params(SCENE_QUERY_STAT(MMOInteractPick), false, this);
		Params.AddIgnoredActor(Actor);
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Center, ECC_Visibility, Params))
		{
			continue;
		}
		BestAlong = Along;
		Best = Actor;
	}

	if (OutDistance)
	{
		*OutDistance = BestAlong;
	}
	return Best;
}

AActor* AMMOCharacter::FindNearestInteractable() const
{
	AActor* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (AActor* Actor : MMOInteraction::GetAll(GetWorld()))
	{
		const IMMOInteractable* Interactable = Cast<IMMOInteractable>(Actor);
		const float Distance = FVector::Dist2D(Interactable->GetInteractLocation(), GetActorLocation());
		if (Distance <= Interactable->GetInteractRange() && Distance < BestDistance && Interactable->CanInteract(this))
		{
			BestDistance = Distance;
			Best = Actor;
		}
	}
	return Best;
}

bool AMMOCharacter::TryInteract(AActor* Target)
{
	IMMOInteractable* Interactable = Cast<IMMOInteractable>(Target);
	if (!Interactable || IsDead())
	{
		return false;
	}
	if (FVector::Dist2D(Interactable->GetInteractLocation(), GetActorLocation()) > Interactable->GetInteractRange())
	{
		ShowPlayerMessage(NSLOCTEXT("MMOItems", "TooFar", "You are too far away."));
		return false;
	}
	if (!Interactable->CanInteract(this))
	{
		return false;
	}
	Interactable->Interact(this);
	return true;
}

void AMMOCharacter::InteractWith(AMMOCreature* Creature)
{
	if (!Creature || IsDead())
	{
		return;
	}

	// corpse: loot it
	if (Creature->IsLootable())
	{
		if (FVector::Dist2D(Creature->GetActorLocation(), GetActorLocation()) > InteractRange)
		{
			ShowPlayerMessage(NSLOCTEXT("MMOItems", "TooFar", "You are too far away."));
			return;
		}
		if (AMMOHUD* HUD = Cast<AMMOHUD>(Cast<APlayerController>(GetController())->GetHUD()))
		{
			HUD->OpenLoot(Creature->GetLoot());
		}
		return;
	}

	// living enemy: target it and start auto-attacking
	if (Creature->IsTargetable())
	{
		Combat->SetTarget(Creature);
		if (!Combat->IsAutoAttacking())
		{
			Combat->StartAutoAttack();
		}
	}
}

AMMOCreature* AMMOCharacter::GetCreatureUnderCursor() const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	FVector Origin, Direction;
	if (!PC || !PC->bShowMouseCursor || !PC->DeprojectMousePositionToWorld(Origin, Direction))
	{
		return nullptr;
	}
	return FindCreatureAlongRay(GetWorld(), Origin, Direction, 6000.0f, this);
}

AMMOCreature* AMMOCharacter::FindCreatureAlongRay(const UWorld* World, const FVector& Origin, const FVector& Direction, float MaxDistance, const AActor* Ignore)
{
	if (!World)
	{
		return nullptr;
	}

	// test against each creature's body directly, so clicks work regardless of collision settings (corpses included)
	AMMOCreature* Best = nullptr;
	float BestAlong = MaxDistance;
	const FVector Dir = Direction.GetSafeNormal();

	for (TActorIterator<AMMOCreature> It(const_cast<UWorld*>(World)); It; ++It)
	{
		AMMOCreature* Creature = *It;
		if (Creature->IsHidden())
		{
			continue;
		}

		const FVector Center = Creature->GetActorLocation();
		const float Along = FVector::DotProduct(Center - Origin, Dir);
		if (Along <= 0.0f || Along >= BestAlong)
		{
			continue;
		}

		const float Radius = FMath::Max(60.0f, Creature->GetCapsuleComponent()->GetScaledCapsuleRadius() * 1.3f);
		if (FVector::Dist(Origin + Dir * Along, Center) > Radius)
		{
			continue;
		}

		// not through walls
		FCollisionQueryParams Params(SCENE_QUERY_STAT(MMOCursorPick), false, Ignore);
		Params.AddIgnoredActor(Creature);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Origin, Center, ECC_Visibility, Params))
		{
			continue;
		}

		BestAlong = Along;
		Best = Creature;
	}
	return Best;
}

void AMMOCharacter::UpdateHoverCursor()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController() || !PC->bShowMouseCursor)
	{
		return;
	}

	const AMMOCreature* Hovered = GetCreatureUnderCursor();
	EMouseCursor::Type Cursor = EMouseCursor::Default;
	if (GetInteractableUnderCursor())
	{
		Cursor = EMouseCursor::Hand;
	}
	else if (Hovered && Hovered->IsLootable())
	{
		Cursor = EMouseCursor::Hand;
	}
	else if (Hovered && Hovered->IsTargetable())
	{
		Cursor = EMouseCursor::Crosshairs;
	}
	PC->CurrentMouseCursor = Cursor;
}

void AMMOCharacter::DoInteract()
{
	if (IsDead())
	{
		return;
	}

	// an NPC / object under the cursor wins, then the nearest corpse, then the nearest NPC / object
	if (AActor* Hovered = GetInteractableUnderCursor())
	{
		TryInteract(Hovered);
		return;
	}

	bool bTooFar = false;
	AMMOCreature* Corpse = FindLootableCorpse(bTooFar);
	if (!Corpse)
	{
		if (AActor* Nearby = FindNearestInteractable())
		{
			TryInteract(Nearby);
		}
		else if (bTooFar)
		{
			ShowPlayerMessage(NSLOCTEXT("MMOItems", "TooFar", "You are too far away."));
		}
		return;
	}

	if (AMMOHUD* HUD = Cast<AMMOHUD>(Cast<APlayerController>(GetController())->GetHUD()))
	{
		HUD->OpenLoot(Corpse->GetLoot());
	}
}

void AMMOCharacter::DoToggleInventory()
{
	if (AMMOHUD* HUD = Cast<AMMOHUD>(Cast<APlayerController>(GetController()) ? Cast<APlayerController>(GetController())->GetHUD() : nullptr))
	{
		HUD->ToggleInventory();
	}
}

void AMMOCharacter::DoToggleCharacter()
{
	if (AMMOHUD* HUD = Cast<AMMOHUD>(Cast<APlayerController>(GetController()) ? Cast<APlayerController>(GetController())->GetHUD() : nullptr))
	{
		HUD->ToggleCharacter();
	}
}

void AMMOCharacter::LoadSavedGame()
{
	UMMOSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMMOSaveSubsystem>() : nullptr;
	if (!Saves || !IsPlayerControlled() || !Saves->IsPersistenceEnabled())
	{
		return;
	}
	Saves->BeginCharacterSession();

	if (!Saves->HasSave())
	{
		bSaveReady = true;
		UE_LOG(LogMMO, Log, TEXT("No saved character yet; starting fresh"));
		return;
	}
	bSaveReady = Saves->LoadCharacter(this);
	if (!bSaveReady)
	{
		UE_LOG(LogMMO, Warning, TEXT("The saved character could not be loaded; autosave is off this session so it isn't overwritten"));
	}
	LastSaveTime = GetWorld()->GetTimeSeconds();
}

void AMMOCharacter::RefreshAfterLoad(float SavedHealth)
{
	Abilities->RefreshKnown(Progression->GetLevel(), false);
	RecalculateStats(false);
	RefreshEquipmentVisuals();
	Health->RestoreHealth(SavedHealth > 0.0f ? SavedHealth : Health->GetMaxHealth());
	Combat->ClearTarget();
}

void AMMOCharacter::RequestSave()
{
	bSaveRequested = true;
}

void AMMOCharacter::HandleLevelUpForSave(int32 NewLevel)
{
	RequestSave();
}

void AMMOCharacter::HandleLocationDiscoveredForSave(AMMODiscoveryZone* Zone, int32 XPAwarded)
{
	RequestSave();
}

void AMMOCharacter::TickAutoSave()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (bSaveReady && (bSaveRequested || Now - LastSaveTime >= AutoSaveInterval))
	{
		SaveNow();
	}
}

bool AMMOCharacter::SaveNow()
{
	UMMOSaveSubsystem* Saves = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMMOSaveSubsystem>() : nullptr;
	if (!bSaveReady || !Saves || !Saves->IsPersistenceEnabled())
	{
		return false;
	}
	bSaveRequested = false;
	LastSaveTime = GetWorld()->GetTimeSeconds();
	return Saves->SaveCharacter(this);
}

void AMMOCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// leaving the game (or the map) keeps progress
	SaveNow();
	Super::EndPlay(EndPlayReason);
}

bool AMMOCharacter::IsInCombat() const
{
	const double Now = GetWorld()->GetTimeSeconds();
	return Now - FMath::Max3(LastDamageTakenTime, Combat->GetLastAttackTime(), LastAbilityTime) < OutOfCombatDelay;
}

float AMMOCharacter::GetFoodRemaining() const
{
	return FMath::Max(0.0f, static_cast<float>(FoodEndTime - GetWorld()->GetTimeSeconds()));
}

EMMOUseItemResult AMMOCharacter::UseItem(UMMOItemDefinition* Item)
{
	if (IsDead())
	{
		return EMMOUseItemResult::Dead;
	}
	if (!Item || !Item->IsUsable())
	{
		return EMMOUseItemResult::NotUsable;
	}
	if (Inventory->CountItem(Item) <= 0)
	{
		ShowPlayerMessage(FText::Format(NSLOCTEXT("MMOItems", "NoneLeft", "You have no {0} left."), Item->DisplayName));
		return EMMOUseItemResult::NotInBackpack;
	}
	if (!Cooldowns->IsReady(Item->GetCooldownKey()))
	{
		ShowPlayerMessage(NSLOCTEXT("MMOItems", "NotReady", "That item is not ready yet."));
		return EMMOUseItemResult::OnCooldown;
	}
	if (!Item->bUsableInCombat && IsInCombat())
	{
		ShowPlayerMessage(NSLOCTEXT("MMOItems", "InCombat", "You can't do that while in combat."));
		return EMMOUseItemResult::InCombat;
	}
	if (Health->GetCurrentHealth() >= Health->GetMaxHealth())
	{
		ShowPlayerMessage(NSLOCTEXT("MMOItems", "FullHealth", "You are already at full health."));
		return EMMOUseItemResult::FullHealth;
	}

	Inventory->RemoveItem(Item, 1);
	Cooldowns->StartCooldown(Item->GetCooldownKey(), Item->Cooldown);
	if (Item->HealAmount > 0.0f)
	{
		Health->Heal(Item->HealAmount);
	}
	if (Item->HealOverTime > 0.0f && Item->EffectDuration > 0.0f)
	{
		FoodHealPerSecond = Item->HealOverTime / Item->EffectDuration;
		FoodEndTime = GetWorld()->GetTimeSeconds() + Item->EffectDuration;
	}
	PlayPresentationSound(Item->bUsableInCombat ? MMOCharacterSounds::Drink : MMOCharacterSounds::Eat);
	UE_LOG(LogMMO, Log, TEXT("Used %s (%d left)"), *Item->DisplayName.ToString(), Inventory->CountItem(Item));
	return EMMOUseItemResult::Success;
}

bool AMMOCharacter::UseActionSlot(int32 Index)
{
	const FMMOActionSlot& Action = ActionBar->GetSlot(Index);
	if (Action.Type == EMMOActionType::Item)
	{
		return UseItem(UMMOItemDefinition::FindById(Action.Id)) == EMMOUseItemResult::Success;
	}
	if (Action.Type == EMMOActionType::Ability)
	{
		// fighting takes priority over gathering or crafting
		Professions->Interrupt(false);
		const EMMOAbilityResult Result = Abilities->UseAbility(UMMOAbilityDefinition::FindById(Action.Id));
		return Result == EMMOAbilityResult::Success || Result == EMMOAbilityResult::CastStarted;
	}
	return false;
}

void AMMOCharacter::UseOrEquipInventorySlot(int32 SlotIndex)
{
	const FMMOItemStack& Stack = Inventory->GetSlot(SlotIndex);
	if (Stack.IsEmpty())
	{
		return;
	}
	if (CanTradeWith(ActiveVendor.Get()))
	{
		SellInventorySlot(SlotIndex);
	}
	else if (Stack.Item->IsUsable())
	{
		UseItem(Stack.Item);
	}
	else if (Stack.Item->IsEquippable())
	{
		EquipInventorySlot(SlotIndex);
	}
}

void AMMOCharacter::SetActiveVendor(AMMONPC* Vendor)
{
	ActiveVendor = Vendor;
}

bool AMMOCharacter::CanTradeWith(const AMMONPC* Vendor) const
{
	return Vendor && Vendor->IsVendor() && !IsDead() && FVector::Dist2D(Vendor->GetActorLocation(), GetActorLocation()) <= Vendor->GetInteractRange() + 250.0f;
}

EMMOVendorResult AMMOCharacter::BuyFromVendor(int32 EntryIndex, int32 Quantity)
{
	AMMONPC* Vendor = ActiveVendor.Get();
	if (!CanTradeWith(Vendor))
	{
		ShowPlayerMessage(MMOVendor::GetResultText(EMMOVendorResult::TooFar));
		return EMMOVendorResult::TooFar;
	}
	if (!Vendor->VendorStock.IsValidIndex(EntryIndex))
	{
		return EMMOVendorResult::InvalidItem;
	}

	const FMMOVendorEntry& Entry = Vendor->VendorStock[EntryIndex];
	const EMMOVendorResult Result = MMOVendor::Buy(Inventory, Entry.Item, Entry.GetPrice(), Quantity);
	if (Result == EMMOVendorResult::Success)
	{
		PlayPresentationSound(MMOCharacterSounds::Coin);
		UE_LOG(LogMMO, Log, TEXT("Bought %d x %s for %s"), Quantity, *Entry.Item->DisplayName.ToString(), *MMOItems::FormatCurrency(Entry.GetPrice() * Quantity));
	}
	else
	{
		ShowPlayerMessage(MMOVendor::GetResultText(Result));
	}
	OnTradeChanged.Broadcast();
	return Result;
}

EMMOVendorResult AMMOCharacter::SellInventorySlot(int32 SlotIndex)
{
	if (!CanTradeWith(ActiveVendor.Get()))
	{
		ShowPlayerMessage(MMOVendor::GetResultText(EMMOVendorResult::TooFar));
		return EMMOVendorResult::TooFar;
	}

	FMMOItemStack Sold;
	int32 Copper = 0;
	const EMMOVendorResult Result = MMOVendor::Sell(Inventory, SlotIndex, Sold, Copper);
	if (Result == EMMOVendorResult::Success)
	{
		// the merchant keeps the last few sales so mistakes can be undone
		Buyback.Insert(Sold, 0);
		if (Buyback.Num() > 6)
		{
			Buyback.SetNum(6);
		}
		PlayPresentationSound(MMOCharacterSounds::Coin);
		UE_LOG(LogMMO, Log, TEXT("Sold %d x %s for %s"), Sold.Quantity, *Sold.Item->DisplayName.ToString(), *MMOItems::FormatCurrency(Copper));
	}
	else
	{
		ShowPlayerMessage(MMOVendor::GetResultText(Result));
	}
	OnTradeChanged.Broadcast();
	return Result;
}

EMMOVendorResult AMMOCharacter::BuybackItem(int32 BuybackIndex)
{
	if (!CanTradeWith(ActiveVendor.Get()))
	{
		ShowPlayerMessage(MMOVendor::GetResultText(EMMOVendorResult::TooFar));
		return EMMOVendorResult::TooFar;
	}
	if (!Buyback.IsValidIndex(BuybackIndex))
	{
		return EMMOVendorResult::InvalidItem;
	}

	const FMMOItemStack Stack = Buyback[BuybackIndex];
	const int32 Cost = MMOVendor::GetSellPrice(Stack);
	EMMOVendorResult Result = EMMOVendorResult::Success;
	if (Inventory->GetCurrency() < Cost)
	{
		Result = EMMOVendorResult::NotEnoughMoney;
	}
	else if (Inventory->GetAddableQuantity(Stack.Item, Stack.Quantity) < Stack.Quantity)
	{
		Result = EMMOVendorResult::InventoryFull;
	}

	if (Result == EMMOVendorResult::Success)
	{
		Inventory->SpendCurrency(Cost);
		Inventory->AddStack(Stack);
		Buyback.RemoveAt(BuybackIndex);
		PlayPresentationSound(MMOCharacterSounds::Coin);
	}
	else
	{
		ShowPlayerMessage(MMOVendor::GetResultText(Result));
	}
	OnTradeChanged.Broadcast();
	return Result;
}

void AMMOCharacter::DoToggleAbilities()
{
	if (AMMOHUD* HUD = Cast<AMMOHUD>(Cast<APlayerController>(GetController()) ? Cast<APlayerController>(GetController())->GetHUD() : nullptr))
	{
		HUD->ToggleAbilities();
	}
}

void AMMOCharacter::HandleAbilityLearned(UMMOAbilityDefinition* Ability)
{
	if (Ability)
	{
		ActionBar->AutoPlace(UMMOActionBarComponent::MakeAbility(Ability->AbilityId));
		UE_LOG(LogMMO, Log, TEXT("Learned %s"), *Ability->DisplayName.ToString());
	}
}

void AMMOCharacter::FaceActor(const AActor* Other)
{
	if (!Other)
	{
		return;
	}
	FVector To = Other->GetActorLocation() - GetActorLocation();
	To.Z = 0.0f;
	if (!To.IsNearlyZero())
	{
		SetActorRotation(To.Rotation());
	}
}

void AMMOCharacter::ShowWorldText(const FVector& Location, const FString& Text, const FLinearColor& Color) const
{
	const APlayerController* PC = Cast<APlayerController>(GetController());
	const AMMOHUD* HUD = PC ? Cast<AMMOHUD>(PC->GetHUD()) : nullptr;
	if (HUD && HUD->GetHUDWidget())
	{
		HUD->GetHUDWidget()->AddFloatingText(Location, Text, Color, 20, 1.5f);
	}
}

void AMMOCharacter::NotifyAbilityExecuted(UMMOAbilityDefinition* Ability, AActor* Target)
{
	LastAbilityTime = GetWorld()->GetTimeSeconds();
	if (Ability->IsOffensive())
	{
		Combat->PlayAbilityAnimation();
		PlayPresentationSound(MMOCharacterSounds::Ability);

		// like most MMOs, an attack ability also starts auto-attack on the target
		if (Target && Target == Combat->GetCurrentTarget() && !Combat->IsAutoAttacking())
		{
			Combat->StartAutoAttack();
		}
	}
	if (Ability->SelfHealFraction > 0.0f)
	{
		PlayPresentationSound(MMOCharacterSounds::HealSpell);
	}
	UE_LOG(LogMMO, Log, TEXT("Used %s"), *Ability->DisplayName.ToString());
}

bool AMMOCharacter::IsCasting() const
{
	return Abilities->IsCasting();
}

bool AMMOCharacter::GetActiveCast(FText& OutName, float& OutProgress) const
{
	if (const UMMOAbilityDefinition* Casting = Abilities->GetCastingAbility())
	{
		OutName = Casting->DisplayName;
		OutProgress = Abilities->GetCastProgress();
		return true;
	}
	return Professions->GetActivity(OutName, OutProgress);
}

void AMMOCharacter::DoToggleQuestLog()
{
	if (AMMOHUD* HUD = Cast<AMMOHUD>(Cast<APlayerController>(GetController()) ? Cast<APlayerController>(GetController())->GetHUD() : nullptr))
	{
		HUD->ToggleQuestLog();
	}
}

void AMMOCharacter::HandleLevelUp(int32 NewLevel)
{
	// levelling up fully heals: a classic, satisfying reward that also keeps the test loop moving
	RecalculateStats(true);
	Abilities->RefreshKnown(NewLevel, true);
	PlayPresentationSound(MMOCharacterSounds::LevelUp);

	float DamageMin, DamageMax;
	Combat->GetDamageRange(DamageMin, DamageMax);
	UE_LOG(LogMMO, Log, TEXT("Player reached level %d (Max Health %.0f, Damage %.0f-%.0f)"), NewLevel, Health->GetMaxHealth(), DamageMin, DamageMax);
}

void AMMOCharacter::HandleDamaged(float Amount, AActor* DamageInstigator)
{
	LastDamageTakenTime = GetWorld()->GetTimeSeconds();

	// so do gathering and crafting
	Professions->Interrupt(true);

	// eating stops when you get hit
	if (FoodEndTime > LastDamageTakenTime)
	{
		FoodEndTime = 0.0;
		ShowPlayerMessage(NSLOCTEXT("MMOItems", "FoodInterrupted", "You stop eating."), false);
	}

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
	const double Now = GetWorld()->GetTimeSeconds();
	if (IsDead() || Health->GetCurrentHealth() >= Health->GetMaxHealth())
	{
		return;
	}

	if (FoodEndTime > Now)
	{
		Health->Heal(FoodHealPerSecond * 0.5f);
	}

	const double LastCombat = FMath::Max3(LastDamageTakenTime, Combat->GetLastAttackTime(), LastAbilityTime);
	if (Now - LastCombat >= OutOfCombatDelay)
	{
		Health->Heal(OutOfCombatRegenPerSecond * 0.5f);
	}
}
