// Copyright Epic Games, Inc. All Rights Reserved.

#include "Settings/MMOSettingsSubsystem.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "MMO.h"

#define LOCTEXT_NAMESPACE "MMOSettings"

const FString UMMOSettingsSubsystem::SlotName = TEXT("MMO_Settings");

void UMMOSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		Settings = Cast<UMMOSettingsSave>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
		bHasSavedSettings = Settings != nullptr;
	}
	if (!Settings)
	{
		Settings = NewObject<UMMOSettingsSave>(this);
	}

	ApplyAudio();
	if (bHasSavedSettings)
	{
		ApplyGraphics();
	}
}

void UMMOSettingsSubsystem::ApplyAudio() const
{
	if (FAudioDeviceHandle Device = GEngine ? GEngine->GetMainAudioDevice() : FAudioDeviceHandle())
	{
		Device->SetTransientPrimaryVolume(FMath::Clamp(Settings->MasterVolume, 0.0f, 1.0f));
	}
}

void UMMOSettingsSubsystem::ApplyGraphics() const
{
	UGameUserSettings* User = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!User || IsRunningCommandlet())
	{
		return;
	}
	User->SetOverallScalabilityLevel(FMath::Clamp(Settings->GraphicsQuality, 0, 3));
	User->SetFullscreenMode(Settings->bFullscreen ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
	User->SetVSyncEnabled(Settings->bVSync);
	User->SetFrameRateLimit(static_cast<float>(FMath::Max(0, Settings->FrameRateLimit)));
	User->ApplySettings(false);
}

void UMMOSettingsSubsystem::ApplyAndSave()
{
	ApplyAudio();
	ApplyGraphics();
	bHasSavedSettings = UGameplayStatics::SaveGameToSlot(Settings, SlotName, 0);
	UE_LOG(LogMMO, Log, TEXT("Settings saved (volume %.0f%%, sensitivity %.1f, quality %d, fullscreen %d, vsync %d, fps limit %d)"),
		Settings->MasterVolume * 100.0f, Settings->MouseSensitivity, Settings->GraphicsQuality, Settings->bFullscreen, Settings->bVSync, Settings->FrameRateLimit);
}

bool UMMOSettingsSubsystem::ShouldShowTitle() const
{
	return !bTitleShown && !FApp::IsUnattended() && !FParse::Param(FCommandLine::Get(), TEXT("MMONoTitle"));
}

FText UMMOSettingsSubsystem::GetQualityName(int32 Quality)
{
	switch (Quality)
	{
	case 0: return LOCTEXT("Low", "Low");
	case 1: return LOCTEXT("Medium", "Medium");
	case 2: return LOCTEXT("High", "High");
	default: return LOCTEXT("Epic", "Epic");
	}
}

#undef LOCTEXT_NAMESPACE
