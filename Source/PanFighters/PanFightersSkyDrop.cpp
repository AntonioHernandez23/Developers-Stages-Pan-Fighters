// Copyright Epic Games, Inc. All Rights Reserved.

#include "PanFightersSkyDrop.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

APanFightersDropBeacon::APanFightersDropBeacon()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BeaconMatAsset(TEXT("/Game/Items/PowerUps/M_DropBeacon.M_DropBeacon"));

	if (BeaconMatAsset.Succeeded())
	{
		BeaconBaseMaterial = BeaconMatAsset.Object;
	}

	auto ConfigureBeaconMesh = [](UStaticMeshComponent* MeshComp, UStaticMesh* MeshAsset)
	{
		if (MeshAsset)
		{
			MeshComp->SetStaticMesh(MeshAsset);
		}
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
		MeshComp->SetCastShadow(false);
		MeshComp->bReceivesDecals = false;
	};

	GroundDiscMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundDiscMesh"));
	GroundDiscMesh->SetupAttachment(SceneRoot);
	ConfigureBeaconMesh(GroundDiscMesh, CylinderAsset.Succeeded() ? CylinderAsset.Object : nullptr);
	GroundDiscMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 2.5f));
	GroundDiscMesh->SetRelativeScale3D(FVector(0.80f, 0.80f, 0.018f));

	SkyBeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SkyBeamMesh"));
	SkyBeamMesh->SetupAttachment(SceneRoot);
	ConfigureBeaconMesh(SkyBeamMesh, CylinderAsset.Succeeded() ? CylinderAsset.Object : nullptr);
	SkyBeamMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	SkyBeamMesh->SetRelativeScale3D(FVector(0.18f, 0.18f, 2.2f));

	GuideOrbMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GuideOrbMesh"));
	GuideOrbMesh->SetupAttachment(SceneRoot);
	ConfigureBeaconMesh(GuideOrbMesh, SphereAsset.Succeeded() ? SphereAsset.Object : nullptr);
	GuideOrbMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 18.0f));
	GuideOrbMesh->SetRelativeScale3D(FVector(0.16f, 0.16f, 0.16f));

	BeaconLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BeaconLight"));
	BeaconLight->SetupAttachment(SceneRoot);
	BeaconLight->SetRelativeLocation(FVector(0.0f, 0.0f, 55.0f));
	BeaconLight->SetIntensity(360.0f);
	BeaconLight->SetAttenuationRadius(145.0f);
	BeaconLight->SetCastShadows(false);
}

void APanFightersDropBeacon::InitializeDrop(AActor* InItemActor, const FVector& InLandingLocation, const FLinearColor& InColor, float InStartHeight, float InFallSpeed, float InSpawnDelay)
{
	TrackedItem = InItemActor;
	LandingLocation = InLandingLocation;
	BeaconColor = InColor;
	StartHeight = FMath::Max(220.0f, InStartHeight);
	CurrentHeight = StartHeight;
	FallSpeed = FMath::Max(80.0f, InFallSpeed);
	SpawnDelay = FMath::Max(0.0f, InSpawnDelay);
	ElapsedTime = 0.0f;
	PostLandFadeTimer = 0.0f;
	bDropStarted = (SpawnDelay <= KINDA_SMALL_NUMBER);
	bBeaconRevealed = false;
	bHasLanded = false;
	bCollisionEnabled = false;

	SetActorLocation(LandingLocation);

	if (BeaconBaseMaterial)
	{
		GroundDynMat = UMaterialInstanceDynamic::Create(BeaconBaseMaterial, this);
		BeamDynMat = UMaterialInstanceDynamic::Create(BeaconBaseMaterial, this);

		if (GroundDynMat)
		{
			GroundDynMat->SetVectorParameterValue(TEXT("BeaconColor"), BeaconColor);
			GroundDynMat->SetScalarParameterValue(TEXT("BeaconIntensity"), 0.85f);
			GroundDynMat->SetScalarParameterValue(TEXT("BeaconOpacity"), 0.0f);
			GroundDiscMesh->SetMaterial(0, GroundDynMat);
			GuideOrbMesh->SetMaterial(0, GroundDynMat);
		}
		if (BeamDynMat)
		{
			BeamDynMat->SetVectorParameterValue(TEXT("BeaconColor"), BeaconColor);
			BeamDynMat->SetScalarParameterValue(TEXT("BeaconIntensity"), 0.65f);
			BeamDynMat->SetScalarParameterValue(TEXT("BeaconOpacity"), 0.0f);
			SkyBeamMesh->SetMaterial(0, BeamDynMat);
		}
	}

	if (BeaconLight)
	{
		BeaconLight->SetLightColor(BeaconColor);
		BeaconLight->SetIntensity(0.0f);
	}

	// Keep the ground beacon hidden until the item is actively descending and close to the floor
	SetActorHiddenInGame(true);

	if (InItemActor)
	{
		InItemActor->SetActorEnableCollision(false);
		InItemActor->SetActorHiddenInGame(!bDropStarted);
		InItemActor->SetActorLocation(LandingLocation + FVector(0.0f, 0.0f, CurrentHeight));
	}
}

bool APanFightersDropBeacon::HasActiveItem() const
{
	AActor* Item = TrackedItem.Get();
	return IsValid(Item) && !Item->IsActorBeingDestroyed();
}

void APanFightersDropBeacon::DestroyDropAndItem()
{
	if (AActor* Item = TrackedItem.Get())
	{
		if (IsValid(Item) && !Item->IsActorBeingDestroyed())
		{
			Item->Destroy();
		}
	}
	TrackedItem.Reset();
	if (!IsActorBeingDestroyed())
	{
		Destroy();
	}
}

void APanFightersDropBeacon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasActiveItem())
	{
		Destroy();
		return;
	}

	ElapsedTime += DeltaTime;
	AActor* Item = TrackedItem.Get();

	// Wait silently during the random staggered match delay (up to 30s)
	if (!bDropStarted)
	{
		if (ElapsedTime < SpawnDelay)
		{
			return;
		}
		bDropStarted = true;
		Item->SetActorHiddenInGame(false);
	}

	const float ActiveFallTime = FMath::Max(0.0f, ElapsedTime - SpawnDelay);

	if (!bHasLanded)
	{
		CurrentHeight = FMath::Max(LandedHoverHeight, CurrentHeight - FallSpeed * DeltaTime);
		if (CurrentHeight <= LandedHoverHeight + KINDA_SMALL_NUMBER)
		{
			bHasLanded = true;
			CurrentHeight = LandedHoverHeight;
			PostLandFadeTimer = 0.0f;
		}
	}

	// Only reveal the ground indicator shortly before landing (when height <= 280 cm, ~0.85s before touchdown)
	if (!bBeaconRevealed && CurrentHeight <= 280.0f)
	{
		bBeaconRevealed = true;
		SetActorHiddenInGame(false);
	}

	// Enable item pickup collision once it drops into reachable height
	if (!bCollisionEnabled && CurrentHeight <= 165.0f)
	{
		bCollisionEnabled = true;
		Item->SetActorEnableCollision(true);
	}

	// Compute smooth item transform (descent -> gentle hover + steady spin)
	const float HoverOffset = bHasLanded ? (8.0f * FMath::Sin(ActiveFallTime * 2.6f)) : 0.0f;
	const FVector DesiredItemLoc = LandingLocation + FVector(0.0f, 0.0f, CurrentHeight + HoverOffset);
	Item->SetActorLocation(DesiredItemLoc, false, nullptr, ETeleportType::TeleportPhysics);

	FRotator ItemRot = Item->GetActorRotation();
	ItemRot.Yaw += (bHasLanded ? 65.0f : 110.0f) * DeltaTime;
	Item->SetActorRotation(ItemRot);

	if (!bBeaconRevealed)
	{
		return;
	}

	// Soft, non-blinding landing indicator
	const float Pulse = 0.5f + 0.5f * FMath::Sin(ActiveFallTime * 5.5f);
	if (!bHasLanded)
	{
		const float RevealAlpha = FMath::Clamp((280.0f - CurrentHeight) / FMath::Max(1.0f, 280.0f - LandedHoverHeight), 0.0f, 1.0f);
		const float RingScale = 0.68f + 0.16f * (1.0f - RevealAlpha) + 0.08f * Pulse;
		GroundDiscMesh->SetRelativeScale3D(FVector(RingScale, RingScale, 0.018f));
		GuideOrbMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 16.0f + 8.0f * Pulse));
		GuideOrbMesh->SetRelativeScale3D(FVector(0.14f + 0.04f * Pulse));

		if (GroundDynMat)
		{
			GroundDynMat->SetScalarParameterValue(TEXT("BeaconOpacity"), (0.12f + 0.06f * Pulse) * RevealAlpha);
		}
		if (BeamDynMat)
		{
			BeamDynMat->SetScalarParameterValue(TEXT("BeaconOpacity"), (0.04f + 0.04f * Pulse) * RevealAlpha);
		}
		if (BeaconLight)
		{
			BeaconLight->SetIntensity(360.0f * RevealAlpha);
		}
	}
	else
	{
		PostLandFadeTimer += DeltaTime;
		const float BeamFade = FMath::Clamp(1.0f - (PostLandFadeTimer / 0.45f), 0.0f, 1.0f);
		SkyBeamMesh->SetVisibility(BeamFade > 0.02f);
		GuideOrbMesh->SetVisibility(BeamFade > 0.02f);
		if (BeamDynMat)
		{
			BeamDynMat->SetScalarParameterValue(TEXT("BeaconOpacity"), 0.06f * BeamFade);
		}
		if (BeaconLight)
		{
			BeaconLight->SetIntensity(FMath::Lerp(140.0f, 320.0f, BeamFade));
		}

		const float SubtleRing = 0.72f + 0.05f * Pulse;
		GroundDiscMesh->SetRelativeScale3D(FVector(SubtleRing, SubtleRing, 0.015f));
		if (GroundDynMat)
		{
			GroundDynMat->SetScalarParameterValue(TEXT("BeaconOpacity"), 0.09f + 0.04f * Pulse);
		}
	}
}
