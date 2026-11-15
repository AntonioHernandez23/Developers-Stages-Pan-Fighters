#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "PanFightersCharacter.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Materials/MaterialInterface.h"
#include "PanFightersPlayerController.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGalloGatoMechanicsTest,
	"PanFighters.Characters.GalloGatoMechanics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)

bool FGalloGatoMechanicsTest::RunTest(const FString& Parameters)
{
	// 1. Find the active Game/PIE world containing an APanFightersPlayerController
	UWorld* TargetWorld = nullptr;
	APanFightersPlayerController* PC = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if (UWorld* W = Ctx.World())
			{
				if (APlayerController* FirstPC = W->GetFirstPlayerController())
				{
					if (APanFightersPlayerController* PFPC = Cast<APanFightersPlayerController>(FirstPC))
					{
						TargetWorld = W;
						PC = PFPC;
						break;
					}
				}
			}
		}
	}

	TestNotNull(TEXT("PanFightersPlayerController must exist in loaded game world"), PC);
	TestNotNull(TEXT("Active world must exist"), TargetWorld);
	if (!PC || !TargetWorld)
	{
		return false;
	}

	// 2. Verify GalloGato materials are Opaque and expose EmissiveColor for hit-flash
	const TCHAR* MatPaths[3] = {
		TEXT("/Game/Characters/GalloGato/Materials/M_GalloGato_Body.M_GalloGato_Body"),
		TEXT("/Game/Characters/GalloGato/Materials/M_GalloGato_ChestRosa.M_GalloGato_ChestRosa"),
		TEXT("/Game/Characters/GalloGato/Materials/M_GalloGato_GloveMagenta.M_GalloGato_GloveMagenta")
	};
	for (int32 i = 0; i < 3; ++i)
	{
		UMaterialInterface* Mat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, MatPaths[i]));
		TestNotNull(FString::Printf(TEXT("Material[%d] loads"), i), Mat);
		if (Mat)
		{
			TestEqual(FString::Printf(TEXT("Material[%d] BlendMode is Opaque (no chest cutout hole)"), i), Mat->GetBlendMode(), BLEND_Opaque);
			FLinearColor OutVec;
			const bool bHasEmissive = Mat->GetVectorParameterValue(FMaterialParameterInfo(TEXT("EmissiveColor")), OutVec);
			TestTrue(FString::Printf(TEXT("Material[%d] has EmissiveColor parameter for hit flash"), i), bHasEmissive);
		}
	}

	// 3. Measure PolloBlanco baseline camera height in Stage 1, then select GalloGato & enter Stage 1
	PC->SelectCharacter(EPanFightersCharacter::PolloBlanco);
	PC->SelectStage(EPanFightersStage::Stage1_ElRanchito);
	float BaselineCameraOffsetZ = 110.0f;
	if (APanFightersCharacter* BlancoChar = Cast<APanFightersCharacter>(PC->GetPawn()))
	{
		if (UCameraComponent* BlancoCam = BlancoChar->GetFirstPersonCameraComponent())
		{
			BaselineCameraOffsetZ = BlancoCam->GetComponentLocation().Z - BlancoChar->GetActorLocation().Z;
		}
	}

	PC->SelectCharacter(EPanFightersCharacter::GalloGato);
	PC->SelectStage(EPanFightersStage::Stage1_ElRanchito);
	TestEqual(TEXT("SelectedCharacter is GalloGato"), PC->GetSelectedCharacter(), EPanFightersCharacter::GalloGato);
	TestEqual(TEXT("CurrentScreenState is InGame"), PC->GetCurrentScreenState(), EPanFightersScreenState::InGame);

	APawn* Pawn = PC->GetPawn();
	TestNotNull(TEXT("Player Pawn exists after entering Stage 1"), Pawn);
	if (!Pawn)
	{
		return false;
	}

	ACharacter* CharPawn = Cast<ACharacter>(Pawn);
	USkeletalMeshComponent* Mesh = CharPawn ? CharPawn->GetMesh() : nullptr;
	TestNotNull(TEXT("CharacterMesh0 exists"), Mesh);
	if (!Mesh)
	{
		return false;
	}

	TestEqual(TEXT("Player mesh is SK_GalloGato_Mesh"), Mesh->GetSkeletalMeshAsset() ? Mesh->GetSkeletalMeshAsset()->GetName() : FString(), FString(TEXT("SK_GalloGato_Mesh")));
	TestTrue(TEXT("Player mesh Z-offset is grounded (-95)"), FMath::IsNearlyEqual(Mesh->GetRelativeLocation().Z, -95.0f, 0.5f));
	if (APanFightersCharacter* GalloChar = Cast<APanFightersCharacter>(Pawn))
	{
		if (UCameraComponent* GalloCam = GalloChar->GetFirstPersonCameraComponent())
		{
			const float GalloCameraOffsetZ = GalloCam->GetComponentLocation().Z - GalloChar->GetActorLocation().Z;
			TestTrue(TEXT("GalloGato camera height matches PolloBlanco"), FMath::IsNearlyEqual(GalloCameraOffsetZ, BaselineCameraOffsetZ, 1.0f));
		}
	}

	// 4. Verify all BP_GalloBlanco combat & locomotion state transitions drive GalloGato animations
	FIntProperty* AnimStateProp = FindFProperty<FIntProperty>(Pawn->GetClass(), FName(TEXT("CurrentAnimState")));
	FIntProperty* ComboStepProp = FindFProperty<FIntProperty>(Pawn->GetClass(), FName(TEXT("ComboStep")));
	FBoolProperty* AttackingProp = FindFProperty<FBoolProperty>(Pawn->GetClass(), FName(TEXT("bIsAttacking")));
	FBoolProperty* BlockingProp = FindFProperty<FBoolProperty>(Pawn->GetClass(), FName(TEXT("bIsBlocking")));
	FBoolProperty* HitReactProp = FindFProperty<FBoolProperty>(Pawn->GetClass(), FName(TEXT("bIsHitReacting")));
	FBoolProperty* VictoryProp = FindFProperty<FBoolProperty>(Pawn->GetClass(), FName(TEXT("bIsVictory")));

	TestNotNull(TEXT("BP_GalloBlanco CurrentAnimState exists"), AnimStateProp);
	if (!AnimStateProp)
	{
		return false;
	}

	auto CheckStateAnim = [&](int32 StateVal, const TCHAR* ExpectedAnimName, const TCHAR* Desc)
	{
		// Also simulate BP_GalloBlanco trying to play an incompatible GalloBlanco animation right before PlayerTick
		if (UAnimSequence* WrongBlancoAnim = Cast<UAnimSequence>(StaticLoadObject(UAnimSequence::StaticClass(), nullptr, TEXT("/Game/Characters/GalloBlanco/Animations/Anim_02_Walk.Anim_02_Walk"))))
		{
			Mesh->PlayAnimation(WrongBlancoAnim, true);
		}
		AnimStateProp->SetPropertyValue_InContainer(Pawn, StateVal);
		PC->SyncGalloGatoCombatAnimation(Pawn, false);
		UAnimationAsset* Playing = Mesh->AnimationData.AnimToPlay;
		UAnimationAsset* SingleNodeAsset = Mesh->GetSingleNodeInstance() ? Mesh->GetSingleNodeInstance()->GetCurrentAsset() : nullptr;
		TestNotNull(FString::Printf(TEXT("%s: AnimToPlay non-null"), Desc), Playing);
		if (Playing)
		{
			TestEqual(FString::Printf(TEXT("%s: expected animation %s"), Desc, ExpectedAnimName), Playing->GetName(), FString(ExpectedAnimName));
			TestTrue(FString::Printf(TEXT("%s: anim belongs to GalloGato"), Desc), Playing->GetPathName().Contains(TEXT("/Game/Characters/GalloGato/Animations/")));
		}
		TestNotNull(FString::Printf(TEXT("%s: SingleNodeInstance CurrentAsset non-null"), Desc), SingleNodeAsset);
		if (SingleNodeAsset)
		{
			TestEqual(FString::Printf(TEXT("%s: SingleNode matches %s"), Desc, ExpectedAnimName), SingleNodeAsset->GetName(), FString(ExpectedAnimName));
		}
	};

	CheckStateAnim(0, TEXT("Anim_01_Idle_Guard"), TEXT("State 0 Idle"));
	CheckStateAnim(1, TEXT("Anim_02_Walk"), TEXT("State 1 Walk"));
	CheckStateAnim(2, TEXT("Anim_03_Run"), TEXT("State 2 Run"));
	if (BlockingProp) { BlockingProp->SetPropertyValue_InContainer(Pawn, true); }
	CheckStateAnim(3, TEXT("Anim_09_Block_TwoHands"), TEXT("State 3 Block"));
	if (BlockingProp) { BlockingProp->SetPropertyValue_InContainer(Pawn, false); }

	if (AttackingProp) { AttackingProp->SetPropertyValue_InContainer(Pawn, true); }
	if (ComboStepProp) { ComboStepProp->SetPropertyValue_InContainer(Pawn, 1); }
	CheckStateAnim(4, TEXT("Anim_04_Jab_Left"), TEXT("State 4 Combo1 Jab1"));
	if (ComboStepProp) { ComboStepProp->SetPropertyValue_InContainer(Pawn, 2); }
	CheckStateAnim(5, TEXT("Anim_07_Hook_Left"), TEXT("State 5 Combo2 Hook"));
	if (ComboStepProp) { ComboStepProp->SetPropertyValue_InContainer(Pawn, 3); }
	CheckStateAnim(6, TEXT("Anim_08_Uppercut_Right"), TEXT("State 6 Combo3 Uppercut"));
	if (AttackingProp) { AttackingProp->SetPropertyValue_InContainer(Pawn, false); }
	if (ComboStepProp) { ComboStepProp->SetPropertyValue_InContainer(Pawn, 0); }

	if (HitReactProp) { HitReactProp->SetPropertyValue_InContainer(Pawn, true); }
	CheckStateAnim(7, TEXT("Anim_10_Hit_React"), TEXT("State 7 HitReact Chin"));
	CheckStateAnim(9, TEXT("Anim_10_Hit_React"), TEXT("State 9 HitReact Gut"));
	if (HitReactProp) { HitReactProp->SetPropertyValue_InContainer(Pawn, false); }

	if (VictoryProp) { VictoryProp->SetPropertyValue_InContainer(Pawn, true); }
	CheckStateAnim(8, TEXT("Anim_11_Victory"), TEXT("State 8 Victory"));
	if (VictoryProp) { VictoryProp->SetPropertyValue_InContainer(Pawn, false); }

	// Verify Jab re-trigger cycles Anim_04 -> Anim_05 -> Anim_06 without needing to return to Idle
	PC->LastGalloGatoStateTimer = 0.10f;
	PC->LastGalloGatoAnimState = 4;
	if (FDoubleProperty* TimerProp = FindFProperty<FDoubleProperty>(Pawn->GetClass(), FName(TEXT("StateTimer"))))
	{
		TimerProp->SetPropertyValue_InContainer(Pawn, 0.55);
	}
	if (AttackingProp) { AttackingProp->SetPropertyValue_InContainer(Pawn, true); }
	if (ComboStepProp) { ComboStepProp->SetPropertyValue_InContainer(Pawn, 1); }
	AnimStateProp->SetPropertyValue_InContainer(Pawn, 4);
	PC->SyncGalloGatoCombatAnimation(Pawn, false);
	TestEqual(TEXT("Jab re-trigger advances to Anim_05_Jab_Right"), Mesh->AnimationData.AnimToPlay ? Mesh->AnimationData.AnimToPlay->GetName() : FString(), FString(TEXT("Anim_05_Jab_Right")));
	if (AttackingProp) { AttackingProp->SetPropertyValue_InContainer(Pawn, false); }
	if (ComboStepProp) { ComboStepProp->SetPropertyValue_InContainer(Pawn, 0); }

	// Verify Defeat animation triggers when Health <= 0
	PC->SetActorHealth(Pawn, 0.0f);
	PC->SyncGalloGatoCombatAnimation(Pawn, false);
	TestEqual(TEXT("Zero health triggers Anim_12_Defeat"), Mesh->AnimationData.AnimToPlay ? Mesh->AnimationData.AnimToPlay->GetName() : FString(), FString(TEXT("Anim_12_Defeat")));
	PC->SetActorHealth(Pawn, PC->MaxPlayerHealth);

	// 5. Verify PowerUp Z-pivot preservation after ApplyPowerUpBuff & ClearPowerUpEffects
	if (UFunction* ClearFn = Pawn->FindFunction(FName(TEXT("ClearPowerUpEffects"))))
	{
		Pawn->ProcessEvent(ClearFn, nullptr); // BP resets Mesh->RelativeLocation to (0,0,0)
		PC->SyncGalloGatoCombatAnimation(Pawn, false);
		TestTrue(TEXT("Floor Z-offset stays -95 after ClearPowerUpEffects"), FMath::IsNearlyEqual(Mesh->GetRelativeLocation().Z, -95.0f, 0.5f));
	}

	// Simulate Aji Chombo scale (1.2x) + BP's (0,0,19) offset
	Mesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 1.2f));
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 19.0f));
	PC->SyncGalloGatoCombatAnimation(Pawn, false);
	TestTrue(TEXT("Floor Z-offset stays -95 even when scaled by Aji Chombo (1.2x)"), FMath::IsNearlyEqual(Mesh->GetRelativeLocation().Z, -95.0f, 0.5f));

	// 6. Verify Winner Showcase plays Anim_11_Victory and stays grounded
	PC->ShowMatchResult(true);
	AActor* ShowcaseActor = PC->GetOrCreatePreviewRooster();
	TestNotNull(TEXT("Winner Showcase preview rooster spawned"), ShowcaseActor);
	if (ShowcaseActor)
	{
		PC->SyncGalloGatoCombatAnimation(ShowcaseActor, true);
		USkeletalMeshComponent* ShowcaseMesh = Cast<ACharacter>(ShowcaseActor) ? Cast<ACharacter>(ShowcaseActor)->GetMesh() : nullptr;
		if (ShowcaseMesh)
		{
			TestEqual(TEXT("Winner showcase uses SK_GalloGato_Mesh"), ShowcaseMesh->GetSkeletalMeshAsset() ? ShowcaseMesh->GetSkeletalMeshAsset()->GetName() : FString(), FString(TEXT("SK_GalloGato_Mesh")));
			TestTrue(TEXT("Winner showcase stays grounded at Z=-95"), FMath::IsNearlyEqual(ShowcaseMesh->GetRelativeLocation().Z, -95.0f, 0.5f));
			TestEqual(TEXT("Winner showcase plays Anim_11_Victory"), ShowcaseMesh->AnimationData.AnimToPlay ? ShowcaseMesh->AnimationData.AnimToPlay->GetName() : FString(), FString(TEXT("Anim_11_Victory")));
			UAnimationAsset* ShowcaseSingle = ShowcaseMesh->GetSingleNodeInstance() ? ShowcaseMesh->GetSingleNodeInstance()->GetCurrentAsset() : nullptr;
			TestEqual(TEXT("Winner showcase SingleNode plays Anim_11_Victory"), ShowcaseSingle ? ShowcaseSingle->GetName() : FString(), FString(TEXT("Anim_11_Victory")));
		}
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
