// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PanFightersSkyDrop.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * Visual sky-drop telegraph and descent controller for arena PowerUp items.
 * Supports a random match spawn delay (up to 30s) during which both item and indicator
 * remain hidden, followed by a subtle, non-blinding landing indicator shortly before touchdown.
 */
UCLASS()
class PANFIGHTERS_API APanFightersDropBeacon : public AActor
{
	GENERATED_BODY()

public:
	APanFightersDropBeacon();

	virtual void Tick(float DeltaTime) override;

	/** Pairs this beacon with a spawned PowerUp actor at the validated floor landing location */
	void InitializeDrop(AActor* InItemActor, const FVector& InLandingLocation, const FLinearColor& InColor, float InStartHeight = 560.0f, float InFallSpeed = 250.0f, float InSpawnDelay = 0.0f);

	/** Returns true while the paired PowerUp actor still exists in the world */
	UFUNCTION(BlueprintPure, Category = "DropBeacon")
	bool HasActiveItem() const;

	/** Returns the paired PowerUp actor, if still valid */
	UFUNCTION(BlueprintPure, Category = "DropBeacon")
	AActor* GetTrackedItem() const { return TrackedItem.Get(); }

	/** Returns remaining seconds before this sky drop begins descending */
	UFUNCTION(BlueprintPure, Category = "DropBeacon")
	float GetRemainingSpawnDelay() const { return FMath::Max(0.0f, SpawnDelay - ElapsedTime); }

	/** Returns true once the sky drop has started descending */
	UFUNCTION(BlueprintPure, Category = "DropBeacon")
	bool HasDropStarted() const { return bDropStarted; }

	/** Destroys both the paired PowerUp actor (if alive) and this beacon */
	void DestroyDropAndItem();

protected:
	UPROPERTY(VisibleAnywhere, Category = "DropBeacon")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "DropBeacon")
	TObjectPtr<UStaticMeshComponent> GroundDiscMesh;

	UPROPERTY(VisibleAnywhere, Category = "DropBeacon")
	TObjectPtr<UStaticMeshComponent> SkyBeamMesh;

	UPROPERTY(VisibleAnywhere, Category = "DropBeacon")
	TObjectPtr<UStaticMeshComponent> GuideOrbMesh;

	UPROPERTY(VisibleAnywhere, Category = "DropBeacon")
	TObjectPtr<UPointLightComponent> BeaconLight;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GroundDynMat;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BeamDynMat;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BeaconBaseMaterial;

	TWeakObjectPtr<AActor> TrackedItem;
	FVector LandingLocation = FVector::ZeroVector;
	FLinearColor BeaconColor = FLinearColor(1.0f, 0.72f, 0.12f, 1.0f);

	float CurrentHeight = 560.0f;
	float StartHeight = 560.0f;
	float FallSpeed = 250.0f;
	float SpawnDelay = 0.0f;
	float LandedHoverHeight = 68.0f;
	float ElapsedTime = 0.0f;
	float PostLandFadeTimer = 0.0f;
	bool bDropStarted = false;
	bool bBeaconRevealed = false;
	bool bHasLanded = false;
	bool bCollisionEnabled = false;
};
