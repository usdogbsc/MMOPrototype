// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MMOSaveSubsystem.generated.h"

class AMMOCharacter;
class UMMOSaveGame;

/**
 *  Local save/load of the player character (single player for now; a server would own this later).
 *  The character asks for autosaves; this subsystem writes them to Saved/SaveGames/<slot>.sav.
 *  Launch with -MMONoSave to play without loading or writing the save (used by automated tests).
 */
UCLASS()
class UMMOSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	static const FString DefaultSlot;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** False with -MMONoSave or after DisablePersistence (the self-test turns it off so it can't overwrite real progress) */
	bool IsPersistenceEnabled() const { return bPersistenceEnabled; }
	void DisablePersistence() { bPersistenceEnabled = false; }

	/** Blocks saves until the next character load (used by mmo.newgame while the old map unloads) */
	void SuppressSavesUntilNextLoad() { bSuppressSaves = true; }

	/** Called by a newly spawned player character before it loads */
	void BeginCharacterSession() { bSuppressSaves = false; }

	/** Builds a save object from the character's current state */
	static UMMOSaveGame* Capture(const AMMOCharacter* Character, UObject* Outer);

	/** Applies a save object to the character. Returns false if the save is from a newer, unknown version */
	static bool Apply(const UMMOSaveGame* Save, AMMOCharacter* Character);

	bool SaveCharacter(const AMMOCharacter* Character, const FString& Slot = DefaultSlot);
	bool LoadCharacter(AMMOCharacter* Character, const FString& Slot = DefaultSlot);
	bool HasSave(const FString& Slot = DefaultSlot) const;
	bool DeleteSave(const FString& Slot = DefaultSlot);

	/** Seconds played including earlier sessions (for the save file) */
	double GetPlayTime() const;

protected:

	bool bPersistenceEnabled = true;
	bool bSuppressSaves = false;

	/** Play time loaded from the save, plus this session */
	double LoadedPlayTime = 0.0;
	double SessionStart = 0.0;
};
