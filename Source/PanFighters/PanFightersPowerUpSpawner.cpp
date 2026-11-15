#include "PanFightersPowerUpSpawner.h"
#include "Engine/World.h"
#include "TimerManager.h"

APanFightersPowerUpSpawner::APanFightersPowerUpSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
}

void APanFightersPowerUpSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		SpawnRandomPowerUp();
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			SpawnTimerHandle,
			this,
			&APanFightersPowerUpSpawner::SpawnRandomPowerUp,
			SpawnInterval,
			true
		);
	}
}

void APanFightersPowerUpSpawner::CleanupDestroyedItems()
{
	ActiveSpawnedItems.RemoveAll([](const TWeakObjectPtr<AActor>& Ptr)
	{
		return !Ptr.IsValid();
	});
}

void APanFightersPowerUpSpawner::SpawnRandomPowerUp()
{
	if (!GetWorld() || PowerUpClasses.Num() == 0)
	{
		return;
	}

	CleanupDestroyedItems();
	if (ActiveSpawnedItems.Num() >= MaxActivePowerUps)
	{
		return;
	}

	const int32 RandomIndex = FMath::RandRange(0, PowerUpClasses.Num() - 1);
	TSubclassOf<AActor> ChosenClass = PowerUpClasses[RandomIndex];
	if (!ChosenClass)
	{
		return;
	}

	const float Angle = FMath::FRandRange(0.0f, 2.0f * PI);
	const float Dist = FMath::FRandRange(0.0f, SpawnRadius);
	const FVector Offset(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, SpawnHeightOffset);
	const FVector SpawnLocation = GetActorLocation() + Offset;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	if (AActor* Spawned = GetWorld()->SpawnActor<AActor>(ChosenClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams))
	{
		ActiveSpawnedItems.Add(Spawned);
	}
}
