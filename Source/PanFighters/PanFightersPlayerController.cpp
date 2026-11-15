// Copyright Epic Games, Inc. All Rights Reserved.

#include "PanFightersPlayerController.h"
#include "PanFightersCharacter.h"
#include "PanFightersSkyDrop.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "PanFightersCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "PanFighters.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "UI/SPanFightersMasterWidget.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "ImageUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UObject/UObjectIterator.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/ConstructorHelpers.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"

namespace
{
	FText GetRoosterDisplayName(EPanFightersCharacter Character)
	{
		switch (Character)
		{
		case EPanFightersCharacter::PolloChocolate:
			return FText::FromString(TEXT("POLLO CHOCOLATE"));
		case EPanFightersCharacter::PolloAmarillo:
			return FText::FromString(TEXT("POLLO AMARILLO"));
		case EPanFightersCharacter::GalloGato:
			return FText::FromString(TEXT("GALLO GATO"));
		case EPanFightersCharacter::PolloBlanco:
		default:
			return FText::FromString(TEXT("POLLO BLANCO"));
		}
	}

	USkeletalMeshComponent* ResolvePrimaryRoosterMesh(AActor* RoosterActor)
	{
		if (!RoosterActor)
		{
			return nullptr;
		}
		if (ACharacter* CharActor = Cast<ACharacter>(RoosterActor))
		{
			if (USkeletalMeshComponent* MainMesh = CharActor->GetMesh())
			{
				return MainMesh;
			}
		}
		TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes(RoosterActor);
		for (USkeletalMeshComponent* MeshComp : SkeletalMeshes)
		{
			if (MeshComp && !MeshComp->bOnlyOwnerSee)
			{
				return MeshComp;
			}
		}
		return nullptr;
	}

	void SyncRoosterCameraRigOffset(AActor* RoosterActor, USkeletalMeshComponent* MainMesh, float TargetCameraCompensationZ)
	{
		APanFightersCharacter* PanChar = Cast<APanFightersCharacter>(RoosterActor);
		USkeletalMeshComponent* FirstPersonMesh = PanChar ? PanChar->GetFirstPersonMesh() : nullptr;
		if (!FirstPersonMesh || !MainMesh)
		{
			return;
		}

		// FirstPersonMesh (parent of FirstPersonCamera) is attached to CharacterMesh0 in BP_GalloBlanco,
		// so compensating its local Z keeps the third-person camera at identical height when MainMesh drops -95cm.
		if (FirstPersonMesh->GetAttachParent() == MainMesh)
		{
			FVector FPRelLoc = FirstPersonMesh->GetRelativeLocation();
			if (!FMath::IsNearlyEqual(FPRelLoc.Z, TargetCameraCompensationZ, 0.5f))
			{
				FPRelLoc.Z = TargetCameraCompensationZ;
				FirstPersonMesh->SetRelativeLocation(FPRelLoc);
			}
		}
	}

	void ApplyExplicitRoosterMaterials(AActor* RoosterActor, UMaterialInterface* BodyMat, UMaterialInterface* ChestCapMat, UMaterialInterface* GloveMat = nullptr)
	{
		USkeletalMeshComponent* MainMesh = ResolvePrimaryRoosterMesh(RoosterActor);
		if (!MainMesh)
		{
			return;
		}

		if (USkeletalMesh* BaseMesh = Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, TEXT("/Game/Characters/GalloBlanco/Textures/SK_GalloBoxeador_Mesh.SK_GalloBoxeador_Mesh"))))
		{
			if (MainMesh->GetSkeletalMeshAsset() != BaseMesh)
			{
				MainMesh->SetSkeletalMesh(BaseMesh);
			}
		}
		MainMesh->SetRelativeLocation(FVector::ZeroVector);
		SyncRoosterCameraRigOffset(RoosterActor, MainMesh, 0.0f);

		if (BodyMat)
		{
			MainMesh->SetMaterial(0, BodyMat);
		}
		if (ChestCapMat)
		{
			MainMesh->SetMaterial(1, ChestCapMat);
		}
		if (!GloveMat)
		{
			GloveMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Characters/GalloBlanco/Textures/Glove_Red_Cap.Glove_Red_Cap")));
		}
		if (GloveMat)
		{
			MainMesh->SetMaterial(2, GloveMat);
		}
	}

	int32 ReadBpIntProperty(const AActor* Target, FName PropName, int32 DefaultVal = 0)
	{
		if (!Target)
		{
			return DefaultVal;
		}
		if (FIntProperty* Prop = FindFProperty<FIntProperty>(Target->GetClass(), PropName))
		{
			return Prop->GetPropertyValue_InContainer(Target);
		}
		return DefaultVal;
	}

	bool ReadBpBoolProperty(const AActor* Target, FName PropName, bool DefaultVal = false)
	{
		if (!Target)
		{
			return DefaultVal;
		}
		if (FBoolProperty* Prop = FindFProperty<FBoolProperty>(Target->GetClass(), PropName))
		{
			return Prop->GetPropertyValue_InContainer(Target);
		}
		return DefaultVal;
	}

	float ReadBpFloatProperty(const AActor* Target, FName PropName, float DefaultVal = 0.0f)
	{
		if (!Target)
		{
			return DefaultVal;
		}
		if (FDoubleProperty* DProp = FindFProperty<FDoubleProperty>(Target->GetClass(), PropName))
		{
			return static_cast<float>(DProp->GetPropertyValue_InContainer(Target));
		}
		if (FFloatProperty* FProp = FindFProperty<FFloatProperty>(Target->GetClass(), PropName))
		{
			return FProp->GetPropertyValue_InContainer(Target);
		}
		return DefaultVal;
	}

	struct FRoosterAnimSpec
	{
		const TCHAR* AnimPath;
		bool bLoop;
	};

	FRoosterAnimSpec ResolveGalloGatoAnimSpec(int32 StateKey, int32& InOutJabCounter, bool bAdvanceJabCycle)
	{
		switch (StateKey)
		{
		case 1:  return { TEXT("/Game/Characters/GalloGato/Animations/Anim_02_Walk.Anim_02_Walk"), true };
		case 2:  return { TEXT("/Game/Characters/GalloGato/Animations/Anim_03_Run.Anim_03_Run"), true };
		case 3:  return { TEXT("/Game/Characters/GalloGato/Animations/Anim_09_Block_TwoHands.Anim_09_Block_TwoHands"), true };
		case 4:
		{
			static const TCHAR* JabCycle[3] = {
				TEXT("/Game/Characters/GalloGato/Animations/Anim_04_Jab_Left.Anim_04_Jab_Left"),
				TEXT("/Game/Characters/GalloGato/Animations/Anim_05_Jab_Right.Anim_05_Jab_Right"),
				TEXT("/Game/Characters/GalloGato/Animations/Anim_06_Cross_Right.Anim_06_Cross_Right")
			};
			const TCHAR* Selected = JabCycle[InOutJabCounter % 3];
			if (bAdvanceJabCycle)
			{
				InOutJabCounter++;
			}
			return { Selected, false };
		}
		case 5:  return { TEXT("/Game/Characters/GalloGato/Animations/Anim_07_Hook_Left.Anim_07_Hook_Left"), false };
		case 6:  return { TEXT("/Game/Characters/GalloGato/Animations/Anim_08_Uppercut_Right.Anim_08_Uppercut_Right"), false };
		case 7:
		case 9:  return { TEXT("/Game/Characters/GalloGato/Animations/Anim_10_Hit_React.Anim_10_Hit_React"), false };
		case 8:  return { TEXT("/Game/Characters/GalloGato/Animations/Anim_11_Victory.Anim_11_Victory"), true };
		case 12: return { TEXT("/Game/Characters/GalloGato/Animations/Anim_12_Defeat.Anim_12_Defeat"), false };
		case 0:
		default: return { TEXT("/Game/Characters/GalloGato/Animations/Anim_01_Idle_Guard.Anim_01_Idle_Guard"), true };
		}
	}

	UAnimationAsset* GetActiveSingleNodeAnimAsset(const USkeletalMeshComponent* Mesh)
	{
		if (!Mesh)
		{
			return nullptr;
		}
		if (UAnimSingleNodeInstance* SingleNode = Mesh->GetSingleNodeInstance())
		{
			return SingleNode->GetCurrentAsset();
		}
		return Mesh->AnimationData.AnimToPlay;
	}

	void PlayRoosterSingleNodeAnim(USkeletalMeshComponent* Mesh, UAnimationAsset* Anim, bool bLoop)
	{
		if (!Mesh || !Anim)
		{
			return;
		}
		Mesh->AnimationData.AnimToPlay = Anim;
		Mesh->AnimationData.bSavedLooping = bLoop;
		Mesh->AnimationData.bSavedPlaying = true;
		Mesh->AnimationData.SavedPosition = 0.0f;
		Mesh->AnimationData.SavedPlayRate = 1.0f;
		Mesh->PlayAnimation(Anim, bLoop);
	}
}

APanFightersPlayerController::APanFightersPlayerController()
{
	PlayerCameraManagerClass = APanFightersCameraManager::StaticClass();
	SplashDuration = 5.0f;
	bEnableStartupSplash = true;
	// Faster rival chase speed (+5% over original 245) relative to player's 300 walk speed for brisk combat pacing
	DefaultRivalWalkSpeed = 257.25f;

	static ConstructorHelpers::FObjectFinder<USoundBase> MainMenuMusicFinder(TEXT("/Game/Audio/BGM/Mus_MainMenu.Mus_MainMenu"));
	if (MainMenuMusicFinder.Succeeded())
	{
		MainMenuMusic = MainMenuMusicFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> SelectionMusicFinder(TEXT("/Game/Audio/BGM/Mus_Selection.Mus_Selection"));
	if (SelectionMusicFinder.Succeeded())
	{
		SelectionMenuMusic = SelectionMusicFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> FightMusicFinder(TEXT("/Game/Audio/BGM/Mus_Fight.Mus_Fight"));
	if (FightMusicFinder.Succeeded())
	{
		FightMusic = FightMusicFinder.Object;
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<USoundBase> FightFallbackFinder(TEXT("/Game/Audio/BGM/kf029904-at-war.kf029904-at-war"));
		if (FightFallbackFinder.Succeeded())
		{
			FightMusic = FightFallbackFinder.Object;
		}
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> VictoryMusicFinder(TEXT("/Game/Audio/BGM/Mus_Victory.Mus_Victory"));
	if (VictoryMusicFinder.Succeeded())
	{
		VictoryMusic = VictoryMusicFinder.Object;
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<USoundBase> VictoryFallbackFinder(TEXT("/Game/Audio/BGM/kf017812-jingle-win.kf017812-jingle-win"));
		if (VictoryFallbackFinder.Succeeded())
		{
			VictoryMusic = VictoryFallbackFinder.Object;
		}
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> BoxingBellFinder(TEXT("/Game/Audio/SFX/SFX_BoxingBell.SFX_BoxingBell"));
	if (BoxingBellFinder.Succeeded())
	{
		BoxingBellSound = BoxingBellFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> PunchHitFinder(TEXT("/Game/Audio/SFX/SFX_Punch_Hit.SFX_Punch_Hit"));
	if (PunchHitFinder.Succeeded())
	{
		PunchHitSound = PunchHitFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> RoosterHurtFinder(TEXT("/Game/Audio/SFX/SFX_Rooster_Hurt.SFX_Rooster_Hurt"));
	if (RoosterHurtFinder.Succeeded())
	{
		RoosterHurtSound = RoosterHurtFinder.Object;
	}
}

void APanFightersPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Initialize master UI (Splash screen -> Main Menu -> Stage Selection -> InGame / Pause)
	if (IsLocalPlayerController() && GEngine && GEngine->GameViewport)
	{
		MasterMenuWidget = SNew(SPanFightersMasterWidget)
			.OwnerPlayerController(this)
			.SplashDuration(SplashDuration);

		GEngine->GameViewport->AddViewportWidgetContent(MasterMenuWidget.ToSharedRef(), 100);

		if (bEnableStartupSplash)
		{
			SetScreenState(EPanFightersScreenState::Splash);
		}
		else
		{
			SetScreenState(EPanFightersScreenState::MainMenu);
		}
	}

	// Only spawn touch controls on local player controllers if needed
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
		if (MobileControlsWidget)
		{
			MobileControlsWidget->AddToPlayerScreen(0);
		}
		else
		{
			UE_LOG(LogPanFighters, Error, TEXT("Could not spawn mobile controls widget."));
		}
	}
}

void APanFightersPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}

		// Bind standard keys for Pause and Menu toggle (ESC, P, M) with bExecuteWhenPaused = true
		if (InputComponent)
		{
			FInputKeyBinding& EscBinding = InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &APanFightersPlayerController::ToggleInGameMenu);
			EscBinding.bExecuteWhenPaused = true;

			FInputKeyBinding& PBinding = InputComponent->BindKey(EKeys::P, IE_Pressed, this, &APanFightersPlayerController::ToggleInGameMenu);
			PBinding.bExecuteWhenPaused = true;

			FInputKeyBinding& MBinding = InputComponent->BindKey(EKeys::M, IE_Pressed, this, &APanFightersPlayerController::ToggleInGameMenu);
			MBinding.bExecuteWhenPaused = true;

			// Debug hotkeys to test Victory / Defeat screens in-game (V = Victory, B = Defeat)
			FInputKeyBinding& VBinding = InputComponent->BindKey(EKeys::V, IE_Pressed, this, &APanFightersPlayerController::TestVictoryAction);
			VBinding.bExecuteWhenPaused = true;

			FInputKeyBinding& BBinding = InputComponent->BindKey(EKeys::B, IE_Pressed, this, &APanFightersPlayerController::TestDefeatAction);
			BBinding.bExecuteWhenPaused = true;

			// Combat reaction and damage test hotkeys (H = Hit reaction, K = Damage player, J = Damage rival)
			FInputKeyBinding& HBinding = InputComponent->BindKey(EKeys::H, IE_Pressed, this, &APanFightersPlayerController::TestHitReactionAction);
			HBinding.bExecuteWhenPaused = true;

			FInputKeyBinding& KBinding = InputComponent->BindKey(EKeys::K, IE_Pressed, this, &APanFightersPlayerController::TestDamagePlayerAction);
			KBinding.bExecuteWhenPaused = true;

			FInputKeyBinding& JBinding = InputComponent->BindKey(EKeys::J, IE_Pressed, this, &APanFightersPlayerController::TestDamageRivalAction);
			JBinding.bExecuteWhenPaused = true;
		}
	}
}

void APanFightersPlayerController::SetScreenState(EPanFightersScreenState NewState)
{
	if (MasterMenuWidget.IsValid())
	{
		MasterMenuWidget->SetScreenState(NewState);
	}
}

void APanFightersPlayerController::PlayBGM(USoundBase* NewMusic, float FadeTime)
{
	if (!NewMusic)
	{
		return;
	}

	if (USoundWave* Wave = Cast<USoundWave>(NewMusic))
	{
		Wave->bLooping = true;
	}

	if (BGMComponent && BGMComponent->Sound == NewMusic && BGMComponent->IsPlaying())
	{
		return;
	}

	if (BGMComponent && BGMComponent->IsPlaying())
	{
		BGMComponent->bAutoDestroy = true;
		BGMComponent->FadeOut(FadeTime, 0.0f);
	}

	float SavedMusicVolume = 0.8f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("PanFighters.Audio"), TEXT("MusicVolume"), SavedMusicVolume, GGameUserSettingsIni);
		SavedMusicVolume = FMath::Clamp(SavedMusicVolume, 0.0f, 1.0f);
	}

	// Keep bAutoDestroy false while active so muting to 0% and raising volume back up resumes cleanly
	BGMComponent = UGameplayStatics::SpawnSound2D(this, NewMusic, SavedMusicVolume, 1.0f, 0.0f, nullptr, true, false);
	if (BGMComponent)
	{
		BGMComponent->bIsUISound = true;
		BGMComponent->FadeIn(FadeTime, 1.0f);
	}
}

void APanFightersPlayerController::StopBGM(float FadeTime)
{
	if (BGMComponent && BGMComponent->IsPlaying())
	{
		BGMComponent->FadeOut(FadeTime, 0.0f);
	}
}

void APanFightersPlayerController::PlaySFX2D(USoundBase* SoundToPlay, float VolumeMultiplier, float PitchMultiplier)
{
	if (!SoundToPlay)
	{
		return;
	}

	float SavedSFXVolume = 1.0f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("PanFighters.Audio"), TEXT("SFXVolume"), SavedSFXVolume, GGameUserSettingsIni);
		SavedSFXVolume = FMath::Clamp(SavedSFXVolume, 0.0f, 1.0f);
	}

	UGameplayStatics::PlaySound2D(this, SoundToPlay, SavedSFXVolume * VolumeMultiplier, PitchMultiplier);
}

void APanFightersPlayerController::PlaySFXAtLocation(USoundBase* SoundToPlay, FVector Location, float VolumeMultiplier, float PitchMultiplier)
{
	if (!SoundToPlay || !GetWorld())
	{
		return;
	}

	float SavedSFXVolume = 1.0f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("PanFighters.Audio"), TEXT("SFXVolume"), SavedSFXVolume, GGameUserSettingsIni);
		SavedSFXVolume = FMath::Clamp(SavedSFXVolume, 0.0f, 1.0f);
	}

	UGameplayStatics::PlaySoundAtLocation(this, SoundToPlay, Location, SavedSFXVolume * VolumeMultiplier, PitchMultiplier);
}

void APanFightersPlayerController::OnScreenStateChanged(EPanFightersScreenState NewState)
{
	const EPanFightersScreenState PreviousState = CurrentScreenState;
	CurrentScreenState = NewState;

	if ((PreviousState == EPanFightersScreenState::CharacterSelect || PreviousState == EPanFightersScreenState::MatchResult) &&
		NewState != PreviousState)
	{
		CleanupCharacterSelectScene();
	}

	if (NewState == EPanFightersScreenState::Splash)
	{
		StopBGM(0.0f);
	}
	else if (NewState == EPanFightersScreenState::MainMenu)
	{
		PlayBGM(MainMenuMusic, 0.8f);

		// Reinicio de Cámara: Force view target back to initial overview camera (Stage 1 Ranchito)
		ACameraActor* DefaultCam = FindStageCamera(EPanFightersStage::Stage1_ElRanchito);
		if (DefaultCam)
		{
			SetViewTarget(DefaultCam);
		}
	}
	else if (NewState == EPanFightersScreenState::CharacterSelect)
	{
		PlayBGM(SelectionMenuMusic, 0.8f);

		SetupRoosterShowcaseScene(false, true);
	}
	else if (NewState == EPanFightersScreenState::StageSelect)
	{
		PlayBGM(SelectionMenuMusic, 0.8f);

		ACameraActor* StageCam = FindStageCamera(CurrentStage != EPanFightersStage::None ? CurrentStage : EPanFightersStage::Stage1_ElRanchito);
		if (StageCam)
		{
			SetViewTarget(StageCam);
		}
	}
	else if (NewState == EPanFightersScreenState::InGame)
	{
		if (!FightMusic)
		{
			FightMusic = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Audio/BGM/Mus_Fight.Mus_Fight")));
			if (!FightMusic)
			{
				FightMusic = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Audio/BGM/kf029904-at-war.kf029904-at-war")));
			}
		}

		if (FightMusic)
		{
			PlayBGM(FightMusic, 0.8f);
		}
		else
		{
			StopBGM(1.0f);
		}
	}
	else if (NewState == EPanFightersScreenState::MatchResult)
	{
		if (bLastMatchVictory)
		{
			if (!VictoryMusic)
			{
				VictoryMusic = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Audio/BGM/Mus_Victory.Mus_Victory")));
				if (!VictoryMusic)
				{
					VictoryMusic = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Audio/BGM/kf017812-jingle-win.kf017812-jingle-win")));
				}
			}
			if (VictoryMusic)
			{
				PlayBGM(VictoryMusic, 0.4f);
			}
			else
			{
				StopBGM(1.2f);
			}
		}
		else
		{
			StopBGM(1.2f);
		}
		ClearStagePowerUps();
		if (MasterMenuWidget.IsValid())
		{
			MasterMenuWidget->HideInGameBanner();
		}
		SetupRoosterShowcaseScene(true, bLastMatchVictory);
	}

	if (NewState == EPanFightersScreenState::InGame)
	{
		// Unpause world physics and gameplay
		UGameplayStatics::SetGamePaused(GetWorld(), false);

		// First-Person gameplay input mode: capture mouse, hide cursor
		FInputModeGameOnly GameInputMode;
		SetInputMode(GameInputMode);
		bShowMouseCursor = false;
	}
	else if (NewState == EPanFightersScreenState::InGameMenu)
	{
		// Pause the game world physics and actors
		UGameplayStatics::SetGamePaused(GetWorld(), true);

		// Pause menu interactive UI mode: show mouse cursor for clicking buttons
		FInputModeGameAndUI PauseInputMode;
		PauseInputMode.SetHideCursorDuringCapture(false);
		PauseInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(PauseInputMode);
		bShowMouseCursor = true;
	}
	else
	{
		// Main Menu, Options, Character Select, Stage Select, Splash, MatchResult (live 3D victory anim)
		UGameplayStatics::SetGamePaused(GetWorld(), false);

		FInputModeUIOnly UIInputMode;
		UIInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(UIInputMode);
		bShowMouseCursor = true;
	}
}

void APanFightersPlayerController::SelectStage(EPanFightersStage Stage)
{
	CleanupCharacterSelectScene();

	CurrentStage = Stage;
	CurrentRound = 1;
	PlayerRoundWins = 0;
	RivalRoundWins = 0;
	bWaitingNextRound = false;
	RoundTransitionTimer = 0.0f;
	bMatchCompletedNormally = false;

	if (AActor* StageRival = GetActiveRival())
	{
		if (!CachedStageRivalSpawns.Contains(Stage))
		{
			CachedStageRivalSpawns.Add(Stage, StageRival->GetActorTransform());
		}
		CacheRivalAppearance(StageRival);
	}

	StartRound(true);
}

void APanFightersPlayerController::StartRound(bool bInitialStageEntry)
{
	if (CurrentStage == EPanFightersStage::None)
	{
		return;
	}

	// Reset round combat telemetry
	bMatchResolved = false;
	MatchFinishTimer = 0.0f;
	bWaitingNextRound = false;
	RoundTransitionTimer = 0.0f;
	SkyDropRespawnTimer = 0.0f;
	PunchTraumaPitch = 0.0f;
	PunchTraumaRoll = 0.0f;
	CurrentPlayerHealth = 100.0f;
	PreviousPlayerHealth = 100.0f;
	CurrentRivalHealth = 100.0f;
	PreviousRivalHealth = 100.0f;

	// Suppress any legacy WBP_ArenaHUD widget
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* W = *It;
		if (W && W->GetWorld() == GetWorld() && W->IsInViewport())
		{
			if (W->GetClass()->GetName().Contains(TEXT("WBP_ArenaHUD")))
			{
				W->SetVisibility(ESlateVisibility::Collapsed);
				W->RemoveFromParent();
			}
		}
	}

	FText StageNameText;
	FText StageSubtitleText;

	switch (CurrentStage)
	{
	case EPanFightersStage::Stage1_ElRanchito:
		StageNameText = FText::FromString(TEXT("El Ranchito"));
		StageSubtitleText = FText::FromString(TEXT("Bohío Campesino Tradicional y Ruedo de las Aves"));
		break;

	case EPanFightersStage::Stage2_ElCampoLlano:
		StageNameText = FText::FromString(TEXT("El Campo Llano"));
		StageSubtitleText = FText::FromString(TEXT("La Sabana y Potrero a Campo Abierto"));
		break;

	case EPanFightersStage::Stage3_ElSarten:
		StageNameText = FText::FromString(TEXT("El Sartén"));
		StageSubtitleText = FText::FromString(TEXT("La Cocina Campesina al Fuego Vivo"));
		break;

	default:
		break;
	}

	// 1. Ensure world is unpaused and clear any active input ignores
	UGameplayStatics::SetGamePaused(GetWorld(), false);
	ResetIgnoreMoveInput();
	ResetIgnoreLookInput();

	// 2. Reset all character & rival effects, buffs, animation states, and speeds before positioning
	ResetCharacterAndRivalStates();

	// 3. Find stage references (Camera and/or PlayerStart)
	ACameraActor* StageCam = FindStageCamera(CurrentStage);
	AActor* PlayerStartActor = FindStagePlayerStart(CurrentStage);

	// 4. Teleport character pawn to stage location in FIRST PERSON view
	APawn* MyPawn = GetPawn();
	if (MyPawn)
	{
		FVector TargetLocation = MyPawn->GetActorLocation();
		FRotator TargetRotation = MyPawn->GetActorRotation();

		if (PlayerStartActor)
		{
			TargetLocation = PlayerStartActor->GetActorLocation();
			TargetRotation = PlayerStartActor->GetActorRotation();
		}
		else if (StageCam)
		{
			TargetLocation = StageCam->GetActorLocation();
			TargetRotation = StageCam->GetActorRotation();

			// Ground trace so character stands securely on stage floor
			if (UWorld* World = GetWorld())
			{
				FHitResult GroundHit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(StageGroundTrace), false, MyPawn);
				if (World->LineTraceSingleByChannel(GroundHit, TargetLocation, TargetLocation - FVector(0.0f, 0.0f, 2500.0f), ECC_Visibility, Params))
				{
					TargetLocation.Z = GroundHit.Location.Z + 100.0f;
				}
				// Advance slightly in front of the camera to avoid any clipping
				TargetLocation += TargetRotation.Vector() * 50.0f;
			}
		}

		TargetRotation.Pitch = 0.0f;
		TargetRotation.Roll = 0.0f;

		MyPawn->TeleportTo(TargetLocation, TargetRotation, false, true);
		SetControlRotation(TargetRotation);
		SetViewTarget(MyPawn);

		// Restore full internal health & baseline walk speed to player
		SetActorHealth(MyPawn, MaxPlayerHealth);
		if (ACharacter* MyChar = Cast<ACharacter>(MyPawn))
		{
			if (UCharacterMovementComponent* MoveComp = MyChar->GetCharacterMovement())
			{
				MoveComp->StopMovementImmediately();
				MoveComp->MaxWalkSpeed = DefaultPlayerWalkSpeed;
			}
		}

		// Apply selected character mesh, ground pivot offset, and materials
		ApplyRoosterAppearance(MyPawn, SelectedCharacter);
		SyncGalloGatoCombatAnimation(MyPawn);
	}

	// Restore stage rival position, health, and baseline walk speed
	if (AActor* ActiveRival = GetActiveRival())
	{
		if (const FTransform* InitialTransform = CachedStageRivalSpawns.Find(CurrentStage))
		{
			FRotator RivalRot = InitialTransform->Rotator();
			RivalRot.Pitch = 0.0f;
			RivalRot.Roll = 0.0f;
			ActiveRival->SetActorLocationAndRotation(InitialTransform->GetLocation(), RivalRot, false, nullptr, ETeleportType::TeleportPhysics);
		}

		SetActorHealth(ActiveRival, MaxRivalHealth);
		if (ACharacter* RivalChar = Cast<ACharacter>(ActiveRival))
		{
			if (UCharacterMovementComponent* RivalMove = RivalChar->GetCharacterMovement())
			{
				RivalMove->StopMovementImmediately();
				RivalMove->MaxWalkSpeed = DefaultRivalWalkSpeed;
			}
		}
	}

	// 5. Regenerate stage items falling slowly from the sky onto the playable arena platform
	RegenerateStagePowerUps(CurrentStage);

	// 6. Notify widget and switch/refresh InGame HUD
	if (MasterMenuWidget.IsValid())
	{
		if (bInitialStageEntry)
		{
			MasterMenuWidget->NotifyStageEntered(CurrentStage, StageNameText, StageSubtitleText);
			MasterMenuWidget->SetScreenState(EPanFightersScreenState::InGame);
		}
		else
		{
			MasterMenuWidget->NotifyRoundBanner(
				FText::FromString(FString::Printf(TEXT("✦  ASALTO %d DE %d  ✦"), CurrentRound, MaxMatchRounds)),
				FText::FromString(FString::Printf(TEXT("Marcador: %d - %d  •  El primero en 2 victorias gana"), PlayerRoundWins, RivalRoundWins)),
				FText::FromString(TEXT("¡A pelear!")),
				2.5f
			);
		}
	}

	// 7. Set input mode to first-person gameplay
	FInputModeGameOnly GameInputMode;
	SetInputMode(GameInputMode);
	bShowMouseCursor = false;

	// 8. Play Boxing Bell sound to announce the start of the round!
	if (!BoxingBellSound)
	{
		BoxingBellSound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Audio/SFX/SFX_BoxingBell.SFX_BoxingBell")));
	}
	PlaySFX2D(BoxingBellSound, 1.0f, 1.0f);
}

void APanFightersPlayerController::HandleRoundEnded(bool bPlayerWonRound)
{
	if (bPlayerWonRound)
	{
		PlayerRoundWins = FMath::Min(RoundsToWinMatch, PlayerRoundWins + 1);
	}
	else
	{
		RivalRoundWins = FMath::Min(RoundsToWinMatch, RivalRoundWins + 1);
	}

	// First rooster to win 2 rounds wins the match immediately (2-0 or 2-1)
	if (PlayerRoundWins >= RoundsToWinMatch || RivalRoundWins >= RoundsToWinMatch || CurrentRound >= MaxMatchRounds)
	{
		bMatchResolved = true;
		bMatchCompletedNormally = true;
		ShowMatchResult(PlayerRoundWins >= RivalRoundWins);
		return;
	}

	// Otherwise schedule next round smoothly
	bWaitingNextRound = true;
	RoundTransitionTimer = 2.4f;
	MatchFinishTimer = 0.0f;

	if (MasterMenuWidget.IsValid())
	{
		const FText Headline = bPlayerWonRound
			? FText::FromString(FString::Printf(TEXT("✦  ASALTO %d GANADO  ✦"), CurrentRound))
			: FText::FromString(FString::Printf(TEXT("✦  ASALTO %d PARA EL RIVAL  ✦"), CurrentRound));
		const FText Subline = FText::FromString(FString::Printf(TEXT("Marcador: %d - %d  •  Preparando Asalto %d"), PlayerRoundWins, RivalRoundWins, CurrentRound + 1));
		MasterMenuWidget->NotifyRoundBanner(Headline, Subline, FText::FromString(TEXT("Recuperando energías...")), 2.3f);
	}
}

void APanFightersPlayerController::CacheRivalAppearance(AActor* RivalActor)
{
	if (!RivalActor)
	{
		return;
	}

	TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes(RivalActor);
	for (USkeletalMeshComponent* MeshComp : SkeletalMeshes)
	{
		if (MeshComp && MeshComp->GetNumMaterials() > 0)
		{
			CachedRivalBodyMat = MeshComp->GetMaterial(0);
			CachedRivalChestMat = (MeshComp->GetNumMaterials() > 1) ? MeshComp->GetMaterial(1) : nullptr;
			break;
		}
	}
}

void APanFightersPlayerController::ToggleInGameMenu()
{
	if (CurrentScreenState == EPanFightersScreenState::InGame)
	{
		// Play Boxing Bell sound when pausing the match
		if (!BoxingBellSound)
		{
			BoxingBellSound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Audio/SFX/SFX_BoxingBell.SFX_BoxingBell")));
		}
		PlaySFX2D(BoxingBellSound, 0.9f, 1.0f);

		SetScreenState(EPanFightersScreenState::InGameMenu);
	}
	else if (CurrentScreenState == EPanFightersScreenState::InGameMenu)
	{
		ResumeGame();
	}
}

void APanFightersPlayerController::ResumeGame()
{
	SetScreenState(EPanFightersScreenState::InGame);
}

void APanFightersPlayerController::ReturnToMainMenu()
{
	ResetCharacterAndRivalStates();
	ClearStagePowerUps();
	SetScreenState(EPanFightersScreenState::MainMenu);
}

void APanFightersPlayerController::ReturnToStageSelect()
{
	ResetCharacterAndRivalStates();
	ClearStagePowerUps();
	SetScreenState(EPanFightersScreenState::StageSelect);
}

void APanFightersPlayerController::QuitGameAction()
{
	UKismetSystemLibrary::QuitGame(GetWorld(), this, EQuitPreference::Quit, false);
}

void APanFightersPlayerController::SelectCharacter(EPanFightersCharacter InCharacter)
{
	SelectedCharacter = InCharacter;
	UpdatePreviewRoosterVisuals();
	SetScreenState(EPanFightersScreenState::StageSelect);
}

void APanFightersPlayerController::SetPreviewCharacter(EPanFightersCharacter InCharacter)
{
	SelectedCharacter = InCharacter;
	UpdatePreviewRoosterVisuals();
}

void APanFightersPlayerController::ConfirmCharacterAndProceed()
{
	SetScreenState(EPanFightersScreenState::StageSelect);
}

AActor* APanFightersPlayerController::GetOrCreatePreviewRooster()
{
	if (PreviewRoosterActor && IsValid(PreviewRoosterActor))
	{
		return PreviewRoosterActor;
	}
	return nullptr;
}

void APanFightersPlayerController::ApplyRoosterAppearance(AActor* RoosterActor, EPanFightersCharacter InCharacter)
{
	USkeletalMeshComponent* MainMesh = ResolvePrimaryRoosterMesh(RoosterActor);
	if (!MainMesh)
	{
		return;
	}

	LastSyncedGalloGatoActor.Reset();
	LastGalloGatoAnimState = -1;
	LastGalloGatoStateTimer = 0.0f;

	if (InCharacter == EPanFightersCharacter::GalloGato)
	{
		if (USkeletalMesh* GalloGatoMesh = Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, TEXT("/Game/Characters/GalloGato/SK_GalloGato_Mesh.SK_GalloGato_Mesh"))))
		{
			if (MainMesh->GetSkeletalMeshAsset() != GalloGatoMesh)
			{
				MainMesh->SetSkeletalMesh(GalloGatoMesh);
			}
		}
		// SK_GalloGato_Mesh origin is at the soles of the feet (Z=0), while SK_GalloBoxeador_Mesh is centered at Z=95
		MainMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -95.0f));
		SyncRoosterCameraRigOffset(RoosterActor, MainMesh, 95.0f);

		if (UMaterialInterface* BodyMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Characters/GalloGato/Materials/M_GalloGato_Body.M_GalloGato_Body"))))
		{
			MainMesh->SetMaterial(0, BodyMat);
		}
		if (UMaterialInterface* ChestMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Characters/GalloGato/Materials/M_GalloGato_ChestRosa.M_GalloGato_ChestRosa"))))
		{
			MainMesh->SetMaterial(1, ChestMat);
		}
		if (UMaterialInterface* GloveMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Characters/GalloGato/Materials/M_GalloGato_GloveMagenta.M_GalloGato_GloveMagenta"))))
		{
			MainMesh->SetMaterial(2, GloveMat);
		}
		return;
	}

	const bool bChocolate = (InCharacter == EPanFightersCharacter::PolloChocolate);
	const TCHAR* BodyMatPath = bChocolate
		? TEXT("/Game/Characters/GalloBlanco/Textures/M_GalloChocolate.M_GalloChocolate")
		: TEXT("/Game/Characters/GalloBlanco/Textures/Material_001.Material_001");
	const TCHAR* ChestCapMatPath = bChocolate
		? TEXT("/Game/Characters/GalloBlanco/Textures/MI_Chest_Chocolate_Cap.MI_Chest_Chocolate_Cap")
		: TEXT("/Game/Characters/GalloBlanco/Textures/Chest_White_Cap.Chest_White_Cap");

	UMaterialInterface* BodyMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, BodyMatPath));
	UMaterialInterface* ChestCapMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, ChestCapMatPath));
	ApplyExplicitRoosterMaterials(RoosterActor, BodyMat, ChestCapMat);
}

void APanFightersPlayerController::SyncGalloGatoCombatAnimation(AActor* TargetActor, bool bForceVictory)
{
	if (SelectedCharacter != EPanFightersCharacter::GalloGato || !IsValid(TargetActor))
	{
		return;
	}

	USkeletalMeshComponent* MainMesh = ResolvePrimaryRoosterMesh(TargetActor);
	if (!MainMesh)
	{
		return;
	}

	if (USkeletalMesh* ExpectedMesh = Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(), nullptr, TEXT("/Game/Characters/GalloGato/SK_GalloGato_Mesh.SK_GalloGato_Mesh"))))
	{
		if (MainMesh->GetSkeletalMeshAsset() != ExpectedMesh)
		{
			ApplyRoosterAppearance(TargetActor, SelectedCharacter);
		}
	}

	// Keep foot-origin SK_GalloGato_Mesh grounded at capsule floor even after BP_GalloBlanco power-up scale/location resets
	FVector RelLoc = MainMesh->GetRelativeLocation();
	if (!FMath::IsNearlyEqual(RelLoc.Z, -95.0f, 0.5f))
	{
		RelLoc.Z = -95.0f;
		MainMesh->SetRelativeLocation(RelLoc);
	}
	SyncRoosterCameraRigOffset(TargetActor, MainMesh, 95.0f);
	MainMesh->bPauseAnims = false;

	if (LastSyncedGalloGatoActor.Get() != TargetActor)
	{
		LastSyncedGalloGatoActor = TargetActor;
		LastGalloGatoAnimState = -1;
		LastGalloGatoStateTimer = 0.0f;
	}

	const bool bIsVictory = bForceVictory || ReadBpBoolProperty(TargetActor, FName(TEXT("bIsVictory")), false);
	const bool bIsHitReacting = ReadBpBoolProperty(TargetActor, FName(TEXT("bIsHitReacting")), false);
	const bool bIsAttacking = ReadBpBoolProperty(TargetActor, FName(TEXT("bIsAttacking")), false);
	const bool bIsBlocking = ReadBpBoolProperty(TargetActor, FName(TEXT("bIsBlocking")), false);
	const int32 BpAnimState = ReadBpIntProperty(TargetActor, FName(TEXT("CurrentAnimState")), 0);
	const int32 ComboStep = ReadBpIntProperty(TargetActor, FName(TEXT("ComboStep")), 0);
	const float StateTimer = ReadBpFloatProperty(TargetActor, FName(TEXT("StateTimer")), 0.0f);
	const float ActorHealth = GetActorHealth(TargetActor);

	int32 TargetStateKey = BpAnimState;
	if (bIsVictory || BpAnimState == 8)
	{
		TargetStateKey = 8;
	}
	else if (ActorHealth <= 0.0f && CurrentScreenState == EPanFightersScreenState::InGame)
	{
		TargetStateKey = 12;
	}
	else if (bIsHitReacting && TargetStateKey != 7 && TargetStateKey != 9)
	{
		TargetStateKey = 7;
	}
	else if (bIsAttacking && (TargetStateKey < 4 || TargetStateKey > 6))
	{
		TargetStateKey = (ComboStep == 2) ? 5 : ((ComboStep == 3) ? 6 : 4);
	}
	else if (bIsBlocking && TargetStateKey != 3)
	{
		TargetStateKey = 3;
	}

	UAnimationAsset* CurrentAnim = GetActiveSingleNodeAnimAsset(MainMesh);
	const bool bHasValidGalloGatoAnim = CurrentAnim && CurrentAnim->GetPathName().Contains(TEXT("/Game/Characters/GalloGato/Animations/"));
	const bool bStateChanged = (TargetStateKey != LastGalloGatoAnimState);
	const bool bLoopState = (TargetStateKey == 0 || TargetStateKey == 1 || TargetStateKey == 2 || TargetStateKey == 3 || TargetStateKey == 8);
	const bool bOneShotRetriggered = (!bLoopState && StateTimer > LastGalloGatoStateTimer + 0.08f);

	if (!bHasValidGalloGatoAnim || bStateChanged || bOneShotRetriggered)
	{
		const bool bAdvanceJab = (bStateChanged || bOneShotRetriggered);
		const FRoosterAnimSpec Spec = ResolveGalloGatoAnimSpec(TargetStateKey, GalloGatoJabCounter, bAdvanceJab);
		if (UAnimSequence* GalloAnim = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, Spec.AnimPath)))
		{
			PlayRoosterSingleNodeAnim(MainMesh, GalloAnim, Spec.bLoop);
		}
	}

	LastGalloGatoAnimState = TargetStateKey;
	LastGalloGatoStateTimer = StateTimer;
}

void APanFightersPlayerController::UpdatePreviewRoosterVisuals()
{
	AActor* Rooster = GetOrCreatePreviewRooster();
	ApplyRoosterAppearance(Rooster, SelectedCharacter);

	if (USkeletalMeshComponent* MainMesh = ResolvePrimaryRoosterMesh(Rooster))
	{
		const TCHAR* IdleAnimPath = (SelectedCharacter == EPanFightersCharacter::GalloGato)
			? TEXT("/Game/Characters/GalloGato/Animations/Anim_01_Idle_Guard.Anim_01_Idle_Guard")
			: TEXT("/Game/Characters/GalloBlanco/Animations/Anim_01_Idle_Guard.Anim_01_Idle_Guard");
		if (UAnimSequence* IdleAnim = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, IdleAnimPath)))
		{
			PlayRoosterSingleNodeAnim(MainMesh, IdleAnim, true);
		}
	}
}

void APanFightersPlayerController::RotatePreviewRooster(float DeltaYaw)
{
	AActor* Rooster = GetOrCreatePreviewRooster();
	if (Rooster)
	{
		Rooster->AddActorWorldRotation(FRotator(0.0f, DeltaYaw, 0.0f));
	}
}

void APanFightersPlayerController::UpdatePreviewRoosterHover(FVector2D ScreenPos)
{
	AActor* Rooster = GetOrCreatePreviewRooster();
	if (!Rooster)
	{
		return;
	}

	int32 ViewportSizeX = 1920;
	int32 ViewportSizeY = 1080;
	GetViewportSize(ViewportSizeX, ViewportSizeY);

	if (ViewportSizeX > 0 && GetWorld())
	{
		const float NormalizedX = (ScreenPos.X / (float)ViewportSizeX) - 0.5f;
		const float SubtleYawOffset = NormalizedX * 30.0f;

		FRotator TargetRot = Rooster->GetActorRotation();
		TargetRot.Yaw = FMath::FInterpTo(TargetRot.Yaw, PreviewRoosterBaseYaw + SubtleYawOffset, GetWorld()->GetDeltaSeconds(), 6.0f);
		Rooster->SetActorRotation(TargetRot);
	}
}

void APanFightersPlayerController::ReturnToCharacterSelect()
{
	ResetCharacterAndRivalStates();
	ClearStagePowerUps();
	SetScreenState(EPanFightersScreenState::CharacterSelect);
}

FText APanFightersPlayerController::GetWinnerCharacterName() const
{
	if (bLastMatchVictory)
	{
		return GetRoosterDisplayName(SelectedCharacter);
	}
	return GetRivalCharacterName();
}

void APanFightersPlayerController::ShowMatchResult(bool bVictory)
{
	if (!bVictory && !CachedRivalBodyMat)
	{
		CacheRivalAppearance(GetActiveRival());
	}
	bLastMatchVictory = bVictory;
	SetScreenState(EPanFightersScreenState::MatchResult);
}

void APanFightersPlayerController::RetryMatch()
{
	if (CurrentStage != EPanFightersStage::None)
	{
		SelectStage(CurrentStage);
	}
	else
	{
		ReturnToStageSelect();
	}
}

void APanFightersPlayerController::TestVictoryAction()
{
	if (CurrentScreenState == EPanFightersScreenState::InGame || CurrentScreenState == EPanFightersScreenState::InGameMenu)
	{
		ShowMatchResult(true);
	}
}

void APanFightersPlayerController::TestDefeatAction()
{
	if (CurrentScreenState == EPanFightersScreenState::InGame || CurrentScreenState == EPanFightersScreenState::InGameMenu)
	{
		ShowMatchResult(false);
	}
}

void APanFightersPlayerController::TestHitReactionAction()
{
	TriggerPlayerHitReaction(25.0f);
}

void APanFightersPlayerController::TestDamagePlayerAction()
{
	ApplyDamageToPlayer(20.0f);
}

void APanFightersPlayerController::TestDamageRivalAction()
{
	ApplyDamageToRival(25.0f);
}

void APanFightersPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (CurrentScreenState == EPanFightersScreenState::InGame)
	{
		// 1. Suppress any legacy WBP_ArenaHUD in viewport so it doesn't conflict with our UI
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			UUserWidget* W = *It;
			if (W && W->GetWorld() == GetWorld() && W->IsInViewport())
			{
				if (W->GetClass()->GetName().Contains(TEXT("WBP_ArenaHUD")))
				{
					W->SetVisibility(ESlateVisibility::Collapsed);
					W->RemoveFromParent();
				}
			}
		}

		// 2. Sync GalloGato animations (if active), read player health (normalized 0..100 for HUD), and detect hit
		APawn* MyPawn = GetPawn();
		if (MyPawn)
		{
			SyncGalloGatoCombatAnimation(MyPawn);
			CurrentPlayerHealth = GetActorNormalizedHealth(MyPawn, MaxPlayerHealth);
			if (CurrentPlayerHealth < PreviousPlayerHealth - 0.01f)
			{
				const float DamageTaken = PreviousPlayerHealth - CurrentPlayerHealth;
				TriggerPlayerHitReaction(DamageTaken);
			}
			PreviousPlayerHealth = CurrentPlayerHealth;
		}

		// 3. Read active rival health (normalized 0..100 for HUD)
		AActor* ActiveRival = GetActiveRival();
		if (ActiveRival)
		{
			CurrentRivalHealth = GetActorNormalizedHealth(ActiveRival, MaxRivalHealth);
			if (CurrentRivalHealth < PreviousRivalHealth - 0.01f)
			{
				if (!PunchHitSound)
				{
					PunchHitSound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Audio/SFX/SFX_Punch_Hit.SFX_Punch_Hit")));
				}
				if (!RoosterHurtSound)
				{
					RoosterHurtSound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Audio/SFX/SFX_Rooster_Hurt.SFX_Rooster_Hurt")));
				}

				PlaySFXAtLocation(PunchHitSound, ActiveRival->GetActorLocation(), 1.0f, FMath::RandRange(0.92f, 1.08f));
				PlaySFXAtLocation(RoosterHurtSound, ActiveRival->GetActorLocation(), 0.95f, FMath::RandRange(0.90f, 1.10f));
			}
			PreviousRivalHealth = CurrentRivalHealth;
		}

		// 4. Advance round transition countdown if waiting to start Round 2/3
		if (bWaitingNextRound)
		{
			RoundTransitionTimer -= DeltaTime;
			if (RoundTransitionTimer <= 0.0f)
			{
				bWaitingNextRound = false;
				CurrentRound = FMath::Min(MaxMatchRounds, CurrentRound + 1);
				StartRound(false);
				return;
			}
		}

		// 5. Prune completed/collected sky-drop beacons and replenish items continuously during active combat
		if (!bMatchResolved && !bWaitingNextRound && CurrentStage != EPanFightersStage::None)
		{
			TryReplenishStagePowerUp(CurrentStage, DeltaTime);
		}

		// 6. Check round knockout condition (Best of 3 rounds: first to 2 wins takes the match)
		if (!bMatchResolved && !bWaitingNextRound)
		{
			if (CurrentRivalHealth <= 0.0f || CurrentPlayerHealth <= 0.0f)
			{
				const bool bPlayerWonRound = (CurrentRivalHealth <= 0.0f);
				MatchFinishTimer += DeltaTime;
				if (MatchFinishTimer >= 0.85f)
				{
					HandleRoundEnded(bPlayerWonRound);
				}
			}
			else
			{
				MatchFinishTimer = 0.0f;
			}
		}
	}
	else if (CurrentScreenState == EPanFightersScreenState::MatchResult && bLastMatchVictory && PreviewRoosterActor && IsValid(PreviewRoosterActor))
	{
		SyncGalloGatoCombatAnimation(PreviewRoosterActor, true);
	}
	else if (CurrentScreenState == EPanFightersScreenState::CharacterSelect && PreviewRoosterActor && IsValid(PreviewRoosterActor))
	{
		SyncGalloGatoCombatAnimation(PreviewRoosterActor, false);
	}

	// Smooth camera punch recoil decay
	if (PunchTraumaPitch != 0.0f || PunchTraumaRoll != 0.0f)
	{
		PunchTraumaPitch = FMath::FInterpTo(PunchTraumaPitch, 0.0f, DeltaTime, 14.0f);
		PunchTraumaRoll = FMath::FInterpTo(PunchTraumaRoll, 0.0f, DeltaTime, 14.0f);
	}
}

void APanFightersPlayerController::TriggerPlayerHitReaction(float DamageAmount)
{
	// 1. Notify master widget to flash the crimson screen vignette and shake HUD bar
	if (MasterMenuWidget.IsValid())
	{
		MasterMenuWidget->TriggerHitReaction(DamageAmount);
	}

	// 2. Play punch impact and hurt vocalization for player
	if (!PunchHitSound)
	{
		PunchHitSound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Audio/SFX/SFX_Punch_Hit.SFX_Punch_Hit")));
	}
	if (!RoosterHurtSound)
	{
		RoosterHurtSound = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Audio/SFX/SFX_Rooster_Hurt.SFX_Rooster_Hurt")));
	}
	PlaySFX2D(PunchHitSound, 1.0f, FMath::RandRange(0.95f, 1.05f));
	PlaySFX2D(RoosterHurtSound, 0.9f, FMath::RandRange(0.95f, 1.05f));

	// 3. Camera punch impulse / head snap
	PunchTraumaPitch = FMath::RandRange(1.2f, 2.2f);
	PunchTraumaRoll = FMath::RandRange(-2.0f, 2.0f);
	AddPitchInput(PunchTraumaPitch * 0.4f);
	AddRollInput(PunchTraumaRoll * 0.4f);
}

void APanFightersPlayerController::ApplyDamageToPlayer(float DamageAmount)
{
	APawn* MyPawn = GetPawn();
	if (MyPawn)
	{
		const float RawHealth = FMath::Clamp(GetActorHealth(MyPawn) - DamageAmount, 0.0f, MaxPlayerHealth);
		SetActorHealth(MyPawn, RawHealth);
		CurrentPlayerHealth = FMath::Clamp((RawHealth / FMath::Max(1.0f, MaxPlayerHealth)) * 100.0f, 0.0f, 100.0f);
		TriggerPlayerHitReaction(DamageAmount);
		PreviousPlayerHealth = CurrentPlayerHealth;
	}
}

void APanFightersPlayerController::ApplyDamageToRival(float DamageAmount)
{
	AActor* ActiveRival = GetActiveRival();
	if (ActiveRival)
	{
		const float RawHealth = FMath::Clamp(GetActorHealth(ActiveRival) - DamageAmount, 0.0f, MaxRivalHealth);
		SetActorHealth(ActiveRival, RawHealth);
		CurrentRivalHealth = FMath::Clamp((RawHealth / FMath::Max(1.0f, MaxRivalHealth)) * 100.0f, 0.0f, 100.0f);
	}
}

AActor* APanFightersPlayerController::GetActiveRival() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TArray<AActor*> AllRivals;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetClass()->GetName().Contains(TEXT("GalloRival")))
		{
			AllRivals.Add(Actor);
		}
	}

	if (AllRivals.Num() == 0)
	{
		return nullptr;
	}

	const FString StageKeyword = GetStageKeyword(CurrentStage);
	if (!StageKeyword.IsEmpty())
	{
		for (AActor* Rival : AllRivals)
		{
#if WITH_EDITOR
			if (Rival->GetActorLabel().Contains(StageKeyword, ESearchCase::IgnoreCase))
			{
				return Rival;
			}
#endif
			if (Rival->GetName().Contains(StageKeyword, ESearchCase::IgnoreCase))
			{
				return Rival;
			}
			for (const FName& Tag : Rival->Tags)
			{
				if (Tag.ToString().Contains(StageKeyword, ESearchCase::IgnoreCase))
				{
					return Rival;
				}
			}
		}
	}

	// Fallback by XY proximity to stage anchor (also works in packaged builds where ActorLabel is stripped)
	FVector ReferencePoint = GetStageFallbackCenter(CurrentStage);
	if (AActor* StageStart = FindStagePlayerStart(CurrentStage))
	{
		ReferencePoint = StageStart->GetActorLocation();
	}
	else if (ACameraActor* StageCam = FindStageCamera(CurrentStage))
	{
		ReferencePoint = StageCam->GetActorLocation();
	}
	else if (APawn* MyPawn = GetPawn())
	{
		ReferencePoint = MyPawn->GetActorLocation();
	}

	AActor* ClosestRival = AllRivals[0];
	float BestDistSq = FVector::DistSquared2D(AllRivals[0]->GetActorLocation(), ReferencePoint);
	for (int32 i = 1; i < AllRivals.Num(); ++i)
	{
		const float DistSq = FVector::DistSquared2D(AllRivals[i]->GetActorLocation(), ReferencePoint);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			ClosestRival = AllRivals[i];
		}
	}
	return ClosestRival;
}

FText APanFightersPlayerController::GetRivalCharacterName() const
{
	return FText::FromString(TEXT("GALLO RIVAL"));
}

float APanFightersPlayerController::GetActorHealth(AActor* InActor) const
{
	if (!InActor) return 0.0f;
	if (FProperty* Prop = InActor->GetClass()->FindPropertyByName(TEXT("Health")))
	{
		if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
		{
			return FloatProp->GetPropertyValue_InContainer(InActor);
		}
		if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
		{
			return (float)DoubleProp->GetPropertyValue_InContainer(InActor);
		}
	}
	return 100.0f;
}

void APanFightersPlayerController::SetActorHealth(AActor* InActor, float NewHealth)
{
	if (!InActor) return;
	if (FProperty* Prop = InActor->GetClass()->FindPropertyByName(TEXT("Health")))
	{
		if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
		{
			FloatProp->SetPropertyValue_InContainer(InActor, NewHealth);
		}
		else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
		{
			DoubleProp->SetPropertyValue_InContainer(InActor, (double)NewHealth);
		}
	}
}

float APanFightersPlayerController::GetActorNormalizedHealth(AActor* InActor, float MaxHealth) const
{
	if (!InActor)
	{
		return 0.0f;
	}
	const float RawHealth = GetActorHealth(InActor);
	return FMath::Clamp((RawHealth / FMath::Max(1.0f, MaxHealth)) * 100.0f, 0.0f, 100.0f);
}

FString APanFightersPlayerController::GetStageKeyword(EPanFightersStage Stage)
{
	switch (Stage)
	{
	case EPanFightersStage::Stage1_ElRanchito:
		return TEXT("Ranchito");
	case EPanFightersStage::Stage2_ElCampoLlano:
		return TEXT("CampoLlano");
	case EPanFightersStage::Stage3_ElSarten:
		return TEXT("Sarten");
	default:
		return FString();
	}
}

FVector APanFightersPlayerController::GetStageFallbackCenter(EPanFightersStage Stage)
{
	switch (Stage)
	{
	case EPanFightersStage::Stage1_ElRanchito:
		return FVector(0.0f, 0.0f, 212.0f);
	case EPanFightersStage::Stage2_ElCampoLlano:
		return FVector(12000.0f, 0.0f, 213.0f);
	case EPanFightersStage::Stage3_ElSarten:
		return FVector(36000.0f, 0.0f, 213.0f);
	default:
		return FVector::ZeroVector;
	}
}

void APanFightersPlayerController::ResetCharacterAndRivalStates()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	auto InvokeBpEvent = [](AActor* Target, FName FuncName)
	{
		if (IsValid(Target))
		{
			if (UFunction* Fn = Target->FindFunction(FuncName))
			{
				Target->ProcessEvent(Fn, nullptr);
			}
		}
	};

	if (APawn* MyPawn = GetPawn())
	{
		InvokeBpEvent(MyPawn, FName(TEXT("ClearPowerUpEffects")));
		InvokeBpEvent(MyPawn, FName(TEXT("ResetMatch")));
		SetActorHealth(MyPawn, MaxPlayerHealth);

		if (ACharacter* MyChar = Cast<ACharacter>(MyPawn))
		{
			if (UCharacterMovementComponent* MoveComp = MyChar->GetCharacterMovement())
			{
				MoveComp->StopMovementImmediately();
				MoveComp->MaxWalkSpeed = DefaultPlayerWalkSpeed;
			}
		}

		// Deactivate any persistent particle systems or transient FX left on the pawn
		TArray<UParticleSystemComponent*> ParticleComps;
		MyPawn->GetComponents<UParticleSystemComponent>(ParticleComps);
		for (UParticleSystemComponent* PSC : ParticleComps)
		{
			if (PSC && PSC->IsActive())
			{
				PSC->DeactivateImmediate();
			}
		}

		LastSyncedGalloGatoActor.Reset();
		LastGalloGatoAnimState = -1;
		LastGalloGatoStateTimer = 0.0f;
		ApplyRoosterAppearance(MyPawn, SelectedCharacter);
		SyncGalloGatoCombatAnimation(MyPawn, false);
	}

	// Reset EVERY rival rooster in the level so no stale state lingers when changing stages
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor && Actor->GetClass()->GetName().Contains(TEXT("GalloRival")))
		{
			InvokeBpEvent(Actor, FName(TEXT("ResetRival")));
			SetActorHealth(Actor, MaxRivalHealth);

			if (ACharacter* RivalChar = Cast<ACharacter>(Actor))
			{
				if (UCharacterMovementComponent* RivalMove = RivalChar->GetCharacterMovement())
				{
					RivalMove->StopMovementImmediately();
					RivalMove->MaxWalkSpeed = DefaultRivalWalkSpeed;
				}
			}
		}
	}
}

void APanFightersPlayerController::ClearStagePowerUps()
{
	SkyDropRespawnTimer = 0.0f;

	for (const TWeakObjectPtr<APanFightersDropBeacon>& BeaconPtr : ActiveDropBeacons)
	{
		if (APanFightersDropBeacon* Beacon = BeaconPtr.Get())
		{
			if (IsValid(Beacon) && !Beacon->IsActorBeingDestroyed())
			{
				Beacon->DestroyDropAndItem();
			}
		}
	}
	ActiveDropBeacons.Empty();

	if (UWorld* World = GetWorld())
	{
		TArray<AActor*> Leftovers;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (!IsValid(Actor) || Actor->IsActorBeingDestroyed())
			{
				continue;
			}
			const FString ClassName = Actor->GetClass()->GetName();
			if (ClassName.Contains(TEXT("BP_PowerUp_")) || Actor->IsA(APanFightersDropBeacon::StaticClass()))
			{
				Leftovers.Add(Actor);
			}
		}
		for (AActor* Actor : Leftovers)
		{
			Actor->Destroy();
		}
	}
}

AActor* APanFightersPlayerController::FindStagePlatformActor(EPanFightersStage Stage) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FString PlatformKeyword;
	switch (Stage)
	{
	case EPanFightersStage::Stage1_ElRanchito:
		PlatformKeyword = TEXT("SM_Cylinder");
		break;
	case EPanFightersStage::Stage2_ElCampoLlano:
		PlatformKeyword = TEXT("CL_Potrero_Y_Arena_Rojiza");
		break;
	case EPanFightersStage::Stage3_ElSarten:
		PlatformKeyword = TEXT("SR_Sarten_Gigante_Hierro");
		break;
	default:
		return nullptr;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}
#if WITH_EDITOR
		if (Actor->GetActorLabel().Equals(PlatformKeyword, ESearchCase::IgnoreCase) ||
			Actor->GetActorLabel().Contains(PlatformKeyword, ESearchCase::IgnoreCase))
		{
			return Actor;
		}
#endif
		if (Actor->GetName().Contains(PlatformKeyword, ESearchCase::IgnoreCase))
		{
			return Actor;
		}
	}

	// Fallback for packaged builds where ActorLabel is stripped: trace down near the arena center
	const FVector Center = GetStageFallbackCenter(Stage);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FindStagePlatformFallback), false);
	if (World->LineTraceSingleByChannel(Hit, Center + FVector(220.0f, 0.0f, 650.0f), Center + FVector(220.0f, 0.0f, -500.0f), ECC_Visibility, Params))
	{
		return Hit.GetActor();
	}

	return nullptr;
}

bool APanFightersPlayerController::FindRandomDropPointOnPlatform(EPanFightersStage Stage, AActor* PlatformActor, const TArray<FVector>& AvoidLocations, FVector& OutFloorPoint) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector ArenaCenter = GetStageFallbackCenter(Stage);

	float MinRadius = 170.0f;
	float MaxRadius = 860.0f;
	if (Stage == EPanFightersStage::Stage1_ElRanchito)
	{
		MinRadius = 150.0f;
		MaxRadius = 520.0f; // Inside SM_Cylinder radius 760 and Colision_Invisible_Arena 762
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(PlatformDropTrace), false);
	if (APawn* MyPawn = GetPawn())
	{
		Params.AddIgnoredActor(MyPawn);
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (IsValid(Actor) && (Actor->GetClass()->GetName().Contains(TEXT("GalloRival")) ||
			Actor->GetClass()->GetName().Contains(TEXT("BP_PowerUp_")) ||
			Actor->IsA(APanFightersDropBeacon::StaticClass())))
		{
			Params.AddIgnoredActor(Actor);
		}
	}

	FBox PlatformBounds(EForceInit::ForceInit);
	if (PlatformActor)
	{
		PlatformBounds = PlatformActor->GetComponentsBoundingBox(true);
	}

	for (int32 Attempt = 0; Attempt < 36; ++Attempt)
	{
		const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const float Radius = FMath::Lerp(MinRadius, MaxRadius, FMath::Sqrt(FMath::FRand()));
		const float CandX = ArenaCenter.X + FMath::Cos(Angle) * Radius;
		const float CandY = ArenaCenter.Y + FMath::Sin(Angle) * Radius;

		const FVector TraceStart(CandX, CandY, ArenaCenter.Z + 650.0f);
		const FVector TraceEnd(CandX, CandY, ArenaCenter.Z - 450.0f);

		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
		{
			continue;
		}
		if (!Hit.bBlockingHit || Hit.ImpactNormal.Z < 0.75f)
		{
			continue;
		}
		if (PlatformActor && Hit.GetActor() != PlatformActor)
		{
			// Allow Stage floor collider / oil surface directly inside the platform actor's XY bounds
			const bool bInsidePlatformXY = PlatformBounds.IsValid &&
				Hit.ImpactPoint.X >= PlatformBounds.Min.X && Hit.ImpactPoint.X <= PlatformBounds.Max.X &&
				Hit.ImpactPoint.Y >= PlatformBounds.Min.Y && Hit.ImpactPoint.Y <= PlatformBounds.Max.Y;
			if (!bInsidePlatformXY)
			{
				continue;
			}
		}
		// Reject elevated ledges / rims higher than playable floor
		if (Hit.ImpactPoint.Z > ArenaCenter.Z + 80.0f || Hit.ImpactPoint.Z < ArenaCenter.Z - 80.0f)
		{
			continue;
		}

		bool bTooClose = false;
		for (const FVector& Avoid : AvoidLocations)
		{
			if (FVector::DistSquared2D(Avoid, Hit.ImpactPoint) < FMath::Square(210.0f))
			{
				bTooClose = true;
				break;
			}
		}
		if (bTooClose)
		{
			continue;
		}

		OutFloorPoint = Hit.ImpactPoint;
		return true;
	}

	// Safe randomized fallback strictly within the inner playable circle of the platform
	for (int32 FallbackAttempt = 0; FallbackAttempt < 16; ++FallbackAttempt)
	{
		const float Angle = FMath::FRandRange(0.0f, UE_TWO_PI);
		const float Radius = FMath::FRandRange(MinRadius, MaxRadius * 0.8f);
		const FVector Candidate(ArenaCenter.X + FMath::Cos(Angle) * Radius, ArenaCenter.Y + FMath::Sin(Angle) * Radius, ArenaCenter.Z);
		bool bTooClose = false;
		for (const FVector& Avoid : AvoidLocations)
		{
			if (FVector::DistSquared2D(Avoid, Candidate) < FMath::Square(190.0f))
			{
				bTooClose = true;
				break;
			}
		}
		if (!bTooClose)
		{
			OutFloorPoint = Candidate;
			return true;
		}
	}

	OutFloorPoint = ArenaCenter + FVector(220.0f, 0.0f, 0.0f);
	return true;
}

void APanFightersPlayerController::SpawnSkyDropItem(EPanFightersStage Stage, AActor* PlatformActor, UClass* PowerUpClass, const FLinearColor& BeaconColor, TArray<FVector>& InOutUsedPoints, float SpawnDelaySeconds)
{
	UWorld* World = GetWorld();
	if (!World || !PowerUpClass)
	{
		return;
	}

	FVector FloorPoint;
	if (!FindRandomDropPointOnPlatform(Stage, PlatformActor, InOutUsedPoints, FloorPoint))
	{
		return;
	}

	InOutUsedPoints.Add(FloorPoint);

	const float StartHeight = FMath::FRandRange(480.0f, 540.0f);
	const float FallSpeed = FMath::FRandRange(260.0f, 300.0f);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector SpawnLoc = FloorPoint + FVector(0.0f, 0.0f, StartHeight);
	AActor* ItemActor = World->SpawnActor<AActor>(PowerUpClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);
	if (!ItemActor)
	{
		return;
	}

	APanFightersDropBeacon* Beacon = World->SpawnActor<APanFightersDropBeacon>(APanFightersDropBeacon::StaticClass(), FloorPoint, FRotator::ZeroRotator, SpawnParams);
	if (Beacon)
	{
		Beacon->InitializeDrop(ItemActor, FloorPoint, BeaconColor, StartHeight, FallSpeed, SpawnDelaySeconds);
		ActiveDropBeacons.Add(Beacon);
	}
}

namespace
{
	struct FPowerUpDef
	{
		const TCHAR* Path;
		FLinearColor Color;
	};

	const TArray<FPowerUpDef>& GetStagePowerUpDefs()
	{
		static const TArray<FPowerUpDef> ItemDefs = {
			{ TEXT("/Game/Items/PowerUps/BP_PowerUp_AjiChombo.BP_PowerUp_AjiChombo_C"), FLinearColor(1.0f, 0.25f, 0.06f, 1.0f) },
			{ TEXT("/Game/Items/PowerUps/BP_PowerUp_PataconDorado.BP_PowerUp_PataconDorado_C"), FLinearColor(1.0f, 0.85f, 0.10f, 1.0f) },
			{ TEXT("/Game/Items/PowerUps/BP_PowerUp_TazaCafe.BP_PowerUp_TazaCafe_C"), FLinearColor(0.20f, 0.88f, 1.0f, 1.0f) }
		};
		return ItemDefs;
	}
}

TArray<FVector> APanFightersPlayerController::BuildPowerUpAvoidPoints(EPanFightersStage Stage) const
{
	TArray<FVector> AvoidPoints;
	if (const APawn* MyPawn = GetPawn())
	{
		AvoidPoints.Add(MyPawn->GetActorLocation());
	}
	else if (AActor* PlayerStart = FindStagePlayerStart(Stage))
	{
		AvoidPoints.Add(PlayerStart->GetActorLocation());
	}
	if (AActor* Rival = GetActiveRival())
	{
		AvoidPoints.Add(Rival->GetActorLocation());
	}
	for (const TWeakObjectPtr<APanFightersDropBeacon>& BeaconPtr : ActiveDropBeacons)
	{
		if (const APanFightersDropBeacon* Beacon = BeaconPtr.Get())
		{
			if (IsValid(Beacon) && !Beacon->IsActorBeingDestroyed() && Beacon->HasActiveItem())
			{
				AvoidPoints.Add(Beacon->GetActorLocation());
			}
		}
	}
	return AvoidPoints;
}

void APanFightersPlayerController::RegenerateStagePowerUps(EPanFightersStage Stage)
{
	ClearStagePowerUps();
	SkyDropRespawnTimer = 0.0f;
	NextSkyDropRespawnInterval = FMath::FRandRange(4.0f, 6.5f);

	if (Stage == EPanFightersStage::None)
	{
		return;
	}

	AActor* PlatformActor = FindStagePlatformActor(Stage);
	TArray<FVector> AvoidPoints = BuildPowerUpAvoidPoints(Stage);
	TArray<FPowerUpDef> ItemDefs = GetStagePowerUpDefs();

	// Shuffle item order so which item drops 1st, 2nd, and 3rd is random each match
	for (int32 i = ItemDefs.Num() - 1; i > 0; --i)
	{
		const int32 j = FMath::RandRange(0, i);
		ItemDefs.Swap(i, j);
	}

	// Schedule each unique item with brisk staggered timing at match start
	const float StaggeredDelays[3] = {
		FMath::FRandRange(1.2f, 2.8f),
		FMath::FRandRange(4.8f, 7.8f),
		FMath::FRandRange(9.5f, 13.5f)
	};

	for (int32 Index = 0; Index < ItemDefs.Num(); ++Index)
	{
		const FPowerUpDef& Def = ItemDefs[Index];
		if (UClass* ItemClass = StaticLoadClass(AActor::StaticClass(), nullptr, Def.Path))
		{
			SpawnSkyDropItem(Stage, PlatformActor, ItemClass, Def.Color, AvoidPoints, StaggeredDelays[Index]);
		}
	}
}

void APanFightersPlayerController::TryReplenishStagePowerUp(EPanFightersStage Stage, float DeltaTime)
{
	ActiveDropBeacons.RemoveAll([](const TWeakObjectPtr<APanFightersDropBeacon>& Ptr)
	{
		return !Ptr.IsValid() || Ptr->IsActorBeingDestroyed() || !Ptr->HasActiveItem();
	});

	const TArray<FPowerUpDef>& AllDefs = GetStagePowerUpDefs();
	if (AllDefs.Num() == 0)
	{
		return;
	}

	// Cap concurrent active/pending drops on the arena to 3; replenish whenever an item is collected
	if (ActiveDropBeacons.Num() >= AllDefs.Num())
	{
		SkyDropRespawnTimer = 0.0f;
		return;
	}

	SkyDropRespawnTimer += DeltaTime;
	if (SkyDropRespawnTimer < NextSkyDropRespawnInterval)
	{
		return;
	}

	SkyDropRespawnTimer = 0.0f;
	NextSkyDropRespawnInterval = FMath::FRandRange(4.0f, 6.5f);

	TSet<UClass*> ActiveClasses;
	for (const TWeakObjectPtr<APanFightersDropBeacon>& BeaconPtr : ActiveDropBeacons)
	{
		if (const APanFightersDropBeacon* Beacon = BeaconPtr.Get())
		{
			if (const AActor* Item = Beacon->GetTrackedItem())
			{
				ActiveClasses.Add(Item->GetClass());
			}
		}
	}

	// Prefer power-up classes not currently present on the arena
	TArray<const FPowerUpDef*> Candidates;
	for (const FPowerUpDef& Def : AllDefs)
	{
		if (UClass* ItemClass = StaticLoadClass(AActor::StaticClass(), nullptr, Def.Path))
		{
			if (!ActiveClasses.Contains(ItemClass))
			{
				Candidates.Add(&Def);
			}
		}
	}
	if (Candidates.Num() == 0)
	{
		for (const FPowerUpDef& Def : AllDefs)
		{
			Candidates.Add(&Def);
		}
	}

	const FPowerUpDef* ChosenDef = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	if (UClass* ChosenClass = StaticLoadClass(AActor::StaticClass(), nullptr, ChosenDef->Path))
	{
		AActor* PlatformActor = FindStagePlatformActor(Stage);
		TArray<FVector> AvoidPoints = BuildPowerUpAvoidPoints(Stage);
		SpawnSkyDropItem(Stage, PlatformActor, ChosenClass, ChosenDef->Color, AvoidPoints, 0.0f);
	}
}

ACameraActor* APanFightersPlayerController::FindStageCamera(EPanFightersStage Stage) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FString StageKeyword = GetStageKeyword(Stage);
	if (StageKeyword.IsEmpty())
	{
		return nullptr;
	}

	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		ACameraActor* Cam = *It;
#if WITH_EDITOR
		if (Cam->GetActorLabel().Contains(StageKeyword, ESearchCase::IgnoreCase))
		{
			return Cam;
		}
#endif
		if (Cam->GetName().Contains(StageKeyword, ESearchCase::IgnoreCase))
		{
			return Cam;
		}
		for (const FName& Tag : Cam->Tags)
		{
			if (Tag.ToString().Contains(StageKeyword, ESearchCase::IgnoreCase))
			{
				return Cam;
			}
		}
	}

	// Fallback by index
	TArray<ACameraActor*> AllCameras;
	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		AllCameras.Add(*It);
	}

	if (AllCameras.Num() > 0)
	{
		int32 Index = (int32)Stage - 1;
		if (AllCameras.IsValidIndex(Index))
		{
			return AllCameras[Index];
		}
		return AllCameras[0];
	}

	return nullptr;
}

AActor* APanFightersPlayerController::FindStagePlayerStart(EPanFightersStage Stage) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const FString StageKeyword = GetStageKeyword(Stage);
	if (StageKeyword.IsEmpty())
	{
		return nullptr;
	}

	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* Start = *It;
#if WITH_EDITOR
		if (Start->GetActorLabel().Contains(StageKeyword, ESearchCase::IgnoreCase))
		{
			return Start;
		}
#endif
		if (Start->GetName().Contains(StageKeyword, ESearchCase::IgnoreCase))
		{
			return Start;
		}
		if (Start->PlayerStartTag.ToString().Contains(StageKeyword, ESearchCase::IgnoreCase))
		{
			return Start;
		}
	}

	return nullptr;
}

bool APanFightersPlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void APanFightersPlayerController::SetupRoosterShowcaseScene(bool bIsVictoryShowcase, bool bPlayerWon)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 1. SINGLETON / INSTANCE CHECK: Destroy any existing preview rooster on pedestal
	if (PreviewRoosterActor && IsValid(PreviewRoosterActor))
	{
		PreviewRoosterActor->Destroy();
		PreviewRoosterActor = nullptr;
	}
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && A->ActorHasTag(TEXT("PreviewRooster")))
		{
			A->Destroy();
		}
	}

	// 2. Hide stage characters, player pawn, and powerup props in Ranchito so they never overlap
	HiddenPowerupActors.Empty();

	// Hide player pawn if spawned
	if (APawn* MyPawn = GetPawn())
	{
		if (!MyPawn->IsHidden())
		{
			MyPawn->SetActorHiddenInGame(true);
			MyPawn->SetActorEnableCollision(false);
			HiddenPowerupActors.Add(MyPawn);
		}
	}

	// Hide stage rivals and powerups
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A) continue;
		const FString Name = A->GetName();
		if (Name.Contains(TEXT("TazaCafe"), ESearchCase::IgnoreCase) ||
			Name.Contains(TEXT("Patacon"), ESearchCase::IgnoreCase) ||
			Name.Contains(TEXT("AjiChombo"), ESearchCase::IgnoreCase) ||
			Name.Contains(TEXT("PowerUp"), ESearchCase::IgnoreCase) ||
			Name.Contains(TEXT("GalloRival"), ESearchCase::IgnoreCase))
		{
			if (!A->IsHidden() && !A->ActorHasTag(TEXT("PreviewRooster")))
			{
				A->SetActorHiddenInGame(true);
				A->SetActorEnableCollision(false);
				HiddenPowerupActors.Add(A);
			}
		}
	}

	// 3. Find base anchor point in Ranchito (near stage center or Ranchito camera target)
	FVector AnchorLoc = FVector(-350.0f, 150.0f, 100.0f);
	ACameraActor* RanchitoCam = FindStageCamera(EPanFightersStage::Stage1_ElRanchito);
	if (RanchitoCam)
	{
		AnchorLoc = RanchitoCam->GetActorLocation() + RanchitoCam->GetActorRotation().Vector() * 550.0f;
	}
	for (TWeakObjectPtr<AActor>& WeakA : HiddenPowerupActors)
	{
		if (WeakA.IsValid() && WeakA->GetName().Contains(TEXT("Ranchito"), ESearchCase::IgnoreCase) && WeakA->GetName().Contains(TEXT("Gallo"), ESearchCase::IgnoreCase))
		{
			AnchorLoc = WeakA->GetActorLocation();
			break;
		}
	}

	// Trace down to stage ground
	FVector MatLoc = AnchorLoc;
	FHitResult GroundHit;
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(PedestalGroundTrace), false);
	if (World->LineTraceSingleByChannel(GroundHit, AnchorLoc + FVector(0.0f, 0.0f, 200.0f), AnchorLoc - FVector(0.0f, 0.0f, 500.0f), ECC_Visibility, TraceParams))
	{
		MatLoc = GroundHit.Location + FVector(0.0f, 0.0f, 1.2f);
	}
	PreviewMatLocation = MatLoc;

	// 4. Spawn the flat circular ring canvas mat
	if (RingCanvasPedestalActor && IsValid(RingCanvasPedestalActor))
	{
		RingCanvasPedestalActor->Destroy();
		RingCanvasPedestalActor = nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), MatLoc + FVector(0.0f, 0.0f, 1.2f), FRotator::ZeroRotator, SpawnParams);
	if (MeshActor)
	{
		RingCanvasPedestalActor = MeshActor;
		UStaticMeshComponent* SMC = MeshActor->GetStaticMeshComponent();
		if (SMC)
		{
			// Explicitly set mobility to Movable so SetStaticMesh does not emit any runtime warnings
			SMC->SetMobility(EComponentMobility::Movable);

			UStaticMesh* CylinderMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
			if (CylinderMesh)
			{
				SMC->SetStaticMesh(CylinderMesh);
				SMC->SetRelativeScale3D(FVector(2.8f, 2.8f, 0.008f));
			}

			UTexture2D* RingTex = FImageUtils::ImportFileAsTexture2D(FPaths::ProjectContentDir() / TEXT("UI/T_LonaRingBoxeo.png"));
			if (RingTex)
			{
				RingTex->AddToRoot();
				UMaterialInterface* BaseMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Characters/GalloBlanco/Textures/Material_001.Material_001")));
				if (!BaseMat)
				{
					BaseMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
				}
				if (BaseMat)
				{
					UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, MeshActor);
					if (DynMat)
					{
						DynMat->SetTextureParameterValue(FName(TEXT("DiffuseColorMap")), RingTex);
						DynMat->SetTextureParameterValue(FName(TEXT("Texture")), RingTex);
						DynMat->SetTextureParameterValue(FName(TEXT("BaseColor")), RingTex);
						DynMat->SetTextureParameterValue(FName(TEXT("Base Color")), RingTex);
						DynMat->SetVectorParameterValue(FName(TEXT("Color")), FLinearColor::White);
						SMC->SetMaterial(0, DynMat);
					}
				}
			}
			SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	// 5. Spawn the dedicated 3D Preview Rooster actor squarely on the pedestal (full body visible, feet touching mat!)
	UClass* RoosterClass = StaticLoadClass(AActor::StaticClass(), nullptr, TEXT("/Game/Characters/GalloBlanco/BP_GalloBlanco.BP_GalloBlanco_C"));
	if (!RoosterClass)
	{
		RoosterClass = StaticLoadClass(AActor::StaticClass(), nullptr, TEXT("/Game/Characters/GalloBlanco/BP_GalloRival.BP_GalloRival_C"));
	}

	// Orientation: Face towards the camera
	FRotator RoosterSpawnRot = FRotator(0.0f, 180.0f, 0.0f);
	if (RanchitoCam)
	{
		RoosterSpawnRot = (RanchitoCam->GetActorLocation() - MatLoc).Rotation();
		RoosterSpawnRot.Pitch = 0.0f;
		RoosterSpawnRot.Roll = 0.0f;
	}
	PreviewRoosterBaseYaw = RoosterSpawnRot.Yaw;

	if (RoosterClass)
	{
		FActorSpawnParameters RoosterSpawnParams;
		RoosterSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// Initial spawn at approximate height
		AActor* SpawnedRooster = World->SpawnActor<AActor>(RoosterClass, MatLoc + FVector(0.0f, 0.0f, 95.0f), RoosterSpawnRot, RoosterSpawnParams);
		if (SpawnedRooster)
		{
			SpawnedRooster->Tags.Add(FName(TEXT("PreviewRooster")));
			SpawnedRooster->SetActorEnableCollision(false);

			float HalfHeight = 88.0f;
			if (ACharacter* CharActor = Cast<ACharacter>(SpawnedRooster))
			{
				if (UCapsuleComponent* Capsule = CharActor->GetCapsuleComponent())
				{
					HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
				}
				if (CharActor->GetCharacterMovement())
				{
					CharActor->GetCharacterMovement()->DisableMovement();
				}
			}

			// Position rooster stably so feet rest squarely on the circular ring mat (lifted slightly so claws never penetrate the surface)
			const FVector FinalRoosterLoc = MatLoc + FVector(0.0f, 0.0f, HalfHeight + 3.8f);
			SpawnedRooster->SetActorLocation(FinalRoosterLoc);

			PreviewRoosterActor = SpawnedRooster;
			if (bIsVictoryShowcase && !bPlayerWon)
			{
				UMaterialInterface* RivalBody = CachedRivalBodyMat.Get();
				if (!RivalBody)
				{
					RivalBody = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Characters/GalloBlanco/Textures/Material_001.Material_001")));
				}
				ApplyExplicitRoosterMaterials(SpawnedRooster, RivalBody, CachedRivalChestMat.Get());
			}
			else
			{
				UpdatePreviewRoosterVisuals();
			}

			if (bIsVictoryShowcase)
			{
				if (UFunction* VictoryFn = SpawnedRooster->FindFunction(FName(TEXT("TriggerVictory"))))
				{
					SpawnedRooster->ProcessEvent(VictoryFn, nullptr);
				}

				const TCHAR* VictoryAnimPath = (bPlayerWon && SelectedCharacter == EPanFightersCharacter::GalloGato)
					? TEXT("/Game/Characters/GalloGato/Animations/Anim_11_Victory.Anim_11_Victory")
					: TEXT("/Game/Characters/GalloBlanco/Animations/Anim_09_Victory.Anim_09_Victory");

				if (UAnimSequence* VictoryAnim = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, VictoryAnimPath)))
				{
					if (USkeletalMeshComponent* MeshComp = ResolvePrimaryRoosterMesh(SpawnedRooster))
					{
						PlayRoosterSingleNodeAnim(MeshComp, VictoryAnim, true);
					}
				}
			}
		}
	}

	// 6. Camera Setup: Heroic full-body view framing rooster and ring mat
	// Character Select shifts slightly for right grid; Winner Showcase frames centered above bottom WINNER banner
	const FVector ForwardDir = RoosterSpawnRot.Vector();
	const FVector RightDir = FRotationMatrix(RoosterSpawnRot).GetScaledAxis(EAxis::Y);

	const float CamDist = bIsVictoryShowcase ? 640.0f : 610.0f;
	const FVector CamRightOffset = bIsVictoryShowcase ? FVector::ZeroVector : (RightDir * 50.0f);
	const FVector AimRightOffset = bIsVictoryShowcase ? FVector::ZeroVector : (RightDir * (-30.0f));
	const float CamHeightOffset = bIsVictoryShowcase ? 102.0f : 96.0f;
	const float AimHeightOffset = bIsVictoryShowcase ? 62.0f : 65.0f;

	const FVector CamLoc = MatLoc + ForwardDir * CamDist + CamRightOffset + FVector(0.0f, 0.0f, CamHeightOffset);
	const FVector AimTarget = MatLoc + AimRightOffset + FVector(0.0f, 0.0f, AimHeightOffset);
	const FRotator CamRot = (AimTarget - CamLoc).Rotation();

	if (CharacterSelectCameraActor && IsValid(CharacterSelectCameraActor))
	{
		CharacterSelectCameraActor->Destroy();
		CharacterSelectCameraActor = nullptr;
	}

	FActorSpawnParameters CamSpawnParams;
	CamSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	CharacterSelectCameraActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CamLoc, CamRot, CamSpawnParams);

	if (CharacterSelectCameraActor)
	{
		UCameraComponent* CamComp = CharacterSelectCameraActor->GetCameraComponent();
		if (CamComp)
		{
			CamComp->FieldOfView = 58.0f;

			// Soft cinematic depth of field on the background farm
			const float FocalDist = (CamLoc - (MatLoc + FVector(0.0f, 0.0f, 65.0f))).Size();
			CamComp->PostProcessSettings.bOverride_DepthOfFieldFstop = true;
			CamComp->PostProcessSettings.DepthOfFieldFstop = 2.0f;
			CamComp->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
			CamComp->PostProcessSettings.DepthOfFieldFocalDistance = FocalDist;
			CamComp->PostProcessSettings.bOverride_DepthOfFieldSensorWidth = true;
			CamComp->PostProcessSettings.DepthOfFieldSensorWidth = 56.0f;
		}

		SetViewTarget(CharacterSelectCameraActor);
	}
}

void APanFightersPlayerController::CleanupCharacterSelectScene()
{
	PreviewMatLocation = FVector::ZeroVector;

	// 1. Destroy Preview Rooster actor completely (no leftover duplicate models!)
	if (PreviewRoosterActor && IsValid(PreviewRoosterActor))
	{
		PreviewRoosterActor->Destroy();
		PreviewRoosterActor = nullptr;
	}
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* A = *It;
			if (A && A->ActorHasTag(TEXT("PreviewRooster")))
			{
				A->Destroy();
			}
		}
	}

	// 2. Destroy ring canvas pedestal actor completely
	if (RingCanvasPedestalActor && IsValid(RingCanvasPedestalActor))
	{
		RingCanvasPedestalActor->Destroy();
		RingCanvasPedestalActor = nullptr;
	}

	// 3. Destroy close-up camera actor completely
	if (CharacterSelectCameraActor && IsValid(CharacterSelectCameraActor))
	{
		CharacterSelectCameraActor->Destroy();
		CharacterSelectCameraActor = nullptr;
	}

	// 4. Restore hidden stage actors and player pawn
	for (TWeakObjectPtr<AActor>& WeakA : HiddenPowerupActors)
	{
		if (WeakA.IsValid())
		{
			WeakA->SetActorHiddenInGame(false);
			WeakA->SetActorEnableCollision(true);
		}
	}
	HiddenPowerupActors.Empty();
}
