// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParkourSystemCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h" 
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Math/Vector.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h" // Include the World.h header for accessing GetWorld()
#include "TimerManager.h" // Include the TimerManager.h header for accessing the timer manager
#include "DrawDebugHelpers.h" // For drawing debug lines, optional
//////////////////////////////////////////////////////////////////////////
// AParkourSystemCharacter

AParkourSystemCharacter::AParkourSystemCharacter():
IsClimbingLedge(true)
{
	PrimaryActorTick.bCanEverTick = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)
}

//////////////////////////////////////////////////////////////////////////
// Input

void AParkourSystemCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &AParkourSystemCharacter::HandleJump);
	//PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &AParkourSystemCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AParkourSystemCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AParkourSystemCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AParkourSystemCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AParkourSystemCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AParkourSystemCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AParkourSystemCharacter::OnResetVR);


	PlayerInputComponent->BindAction("SKey", IE_Pressed, this, &AParkourSystemCharacter::ExitLedge);
}

void AParkourSystemCharacter::CanGrab_Implementation(bool canGrabBL)
{
	
}

void AParkourSystemCharacter::ClimbLedge_Implementation(bool isClimbing)
{
	if (!IsClimbingLedge)
	{
		IParkourInterface::Execute_ClimbLedge(this,true);
		GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
		IsClimbingLedge = true;
		IsHanging = false;
	}

}



void AParkourSystemCharacter::OnResetVR()
{
	// If ParkourSystem is added to a project via 'Add Feature' in the Unreal Editor the dependency on HeadMountedDisplay in ParkourSystem.Build.cs is not automatically propagated
	// and a linker error will result.
	// You will need to either:
	//		Add "HeadMountedDisplay" to [YourProject].Build.cs PublicDependencyModuleNames in order to build successfully (appropriate if supporting VR).
	// or:
	//		Comment or delete the call to ResetOrientationAndPosition below (appropriate if not supporting VR)
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AParkourSystemCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void AParkourSystemCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void AParkourSystemCharacter::ForwardTracer()
{
	FVector Start = GetActorLocation();
	FRotator ActorRotation = GetActorRotation();
	FVector ForwardVector = UKismetMathLibrary::GetForwardVector(ActorRotation);
	FVector End =(FVector(ForwardVector.X * 150, ForwardVector.Y*150, ForwardVector.Z)+GetActorLocation());
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(GetOwner());

	TArray<FHitResult> HitArray;


	const bool Hit = UKismetSystemLibrary::SphereTraceMulti(GetWorld(), Start, End, 20,
		UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel1), false, ActorsToIgnore,
		EDrawDebugTrace::ForDuration, HitArray, true, FLinearColor::Gray,
		FLinearColor::Blue, 0.1f);


	if (Hit)
	{
		for (const FHitResult& HitResult : HitArray)
		{
			WallLocation = HitResult.Location;
			WallNormal = HitResult.Normal;
			// Use Location as needed
		}
	}


}

void AParkourSystemCharacter::HeightTracer()
{
	FVector StartHeight = (FVector(GetActorLocation().X,GetActorLocation().Y,GetActorLocation().Z+500)+(
	UKismetMathLibrary::GetForwardVector(GetActorRotation()) * 70));
	FVector EndHeight = (FVector(StartHeight.X, StartHeight.Y, StartHeight.Z-500));

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(GetOwner());

	TArray<FHitResult> HitArray;

	const bool Hit = UKismetSystemLibrary::SphereTraceMulti(GetWorld(), StartHeight, EndHeight, 20,
		UEngineTypes::ConvertToTraceType(ECC_GameTraceChannel1), false, ActorsToIgnore,
		EDrawDebugTrace::ForDuration, HitArray, true, FLinearColor::Gray,
		FLinearColor::Blue, 0.1f);

	if (Hit)
	{
		for (const FHitResult& HitResult : HitArray)
		{
			HeightLocation = HitResult.Location;

			float Value = GetMesh()->GetSocketLocation(PelvisSocket).Z - HeightLocation.Z;
			float Min = -50;
			float Max = 0;

			if (Value >= Min && Value <= Max)
			{
				if (!IsClimbingLedge)
				{
					GrabLedge();
				}
			}
		}
	}
}

void AParkourSystemCharacter::HandleJump()
{
	if (IsHanging)
	{

		//IParkourInterface::Execute_ClimbLedge(this,true);
	}
	else
	{
		Jump();
	}
}


void AParkourSystemCharacter::GrabLedge()
{
	IParkourInterface::Execute_CanGrab(GetMesh()->GetAnimInstance(),true);
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Flying);
	IsHanging = true;
	FLatentActionInfo LatentInfo;
	LatentInfo.CallbackTarget = this;
	FTimerHandle TimerHandle;
	FVector TargetLocation=FVector(WallNormal.X * 22 + WallLocation.X, WallNormal.Y * 22 + WallLocation.Y, HeightLocation.Z - 135);
	FRotator TargetRotation = FRotator(WallNormal.X,WallNormal.Y,WallNormal.Z);
	UKismetSystemLibrary::MoveComponentTo(GetCapsuleComponent(), TargetLocation,
	TargetRotation, false, false,  0.13f,false,EMoveComponentAction::Move, LatentInfo);
	
	GetWorld()->GetTimerManager().SetTimer(
		TimerHandle, // The timer handle
		nullptr, // The delegate to the function to be called after the delay
		0.13f, // The delay in seconds
		false // Whether the timer should loop
	);
	GetCharacterMovement()->StopMovementImmediately();
	
}

void AParkourSystemCharacter::ExitLedge()
{
	GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
	IParkourInterface::Execute_CanGrab(GetMesh()->GetAnimInstance(), false);
	IsHanging = false;
}



void AParkourSystemCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AParkourSystemCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AParkourSystemCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AParkourSystemCharacter::MoveRight(float Value)
{
	if ( (Controller != nullptr) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

// Called every frame
void AParkourSystemCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ForwardTracer();
	HeightTracer();
	// ...
}

