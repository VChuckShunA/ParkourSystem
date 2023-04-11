// Fill out your copyright notice in the Description page of Project Settings.


#include "SphereTrace.h"
#include "Kismet/KismetSystemLibrary.h"

// Sets default values for this component's properties
USphereTrace::USphereTrace()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void USphereTrace::BeginPlay()
{
	Super::BeginPlay();

	// ...
	const FVector Start = GetOwner()->GetActorLocation();
	const FVector End = GetOwner()->GetActorLocation();
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(GetOwner());

	TArray<FHitResult> HitArray;

	const bool Hit = UKismetSystemLibrary::SphereTraceMulti(GetWorld(), Start, End, TraceRadius,
		UEngineTypes::ConvertToTraceType(ECC_Camera), false, ActorsToIgnore,
		EDrawDebugTrace::ForDuration, HitArray, true, FLinearColor::Gray,
		FLinearColor::Blue, 0.001f);

	if (Hit)
	{
		for (const FHitResult HitResult : HitArray)
		{
			GEngine->AddOnScreenDebugMessage(-1, 60.0f, FColor::Orange, FString::Printf(TEXT("Hit: %s"), *HitResult.Actor->GetName()));
		}
	}
	
}


// Called every frame
void USphereTrace::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	
	// ...
}

