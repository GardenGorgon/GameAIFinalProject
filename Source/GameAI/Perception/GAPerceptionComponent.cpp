#include "GAPerceptionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GAPerceptionSystem.h"

UGAPerceptionComponent::UGAPerceptionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A bit of Unreal magic to make TickComponent below get called
	PrimaryComponentTick.bCanEverTick = true;

	// Default vision parameters
	VisionParameters.VisionAngle = 90.0f;
	VisionParameters.VisionDistance = 8000.0;
}


void UGAPerceptionComponent::OnRegister()
{
	Super::OnRegister();

	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		PerceptionSystem->RegisterPerceptionComponent(this);
	}
}

void UGAPerceptionComponent::OnUnregister()
{
	Super::OnUnregister();

	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		PerceptionSystem->UnregisterPerceptionComponent(this);
	}
}


APawn* UGAPerceptionComponent::GetOwnerPawn() const
{
	AActor* Owner = GetOwner();
	if (Owner)
	{
		APawn* Pawn = Cast<APawn>(Owner);
		if (Pawn)
		{
			return Pawn;
		}
		else
		{
			AController* Controller = Cast<AController>(Owner);
			if (Controller)
			{
				return Controller->GetPawn();
			}
		}
	}

	return NULL;
}



// Returns the Target this AI is attending to right now.

UGATargetComponent* UGAPerceptionComponent::GetCurrentTarget() const
{
	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);

	if (PerceptionSystem && PerceptionSystem->TargetComponents.Num() > 0)
	{
		UGATargetComponent* TargetComponent = PerceptionSystem->TargetComponents[0];
		if (TargetComponent->IsKnown())
		{
			return PerceptionSystem->TargetComponents[0];
		}
	}

	return NULL;
}

bool UGAPerceptionComponent::HasTarget() const
{
	return GetCurrentTarget() != NULL;
}


bool UGAPerceptionComponent::GetCurrentTargetState(FTargetCache& TargetStateOut, FTargetData& TargetDataOut) const
{
	UGATargetComponent* Target = GetCurrentTarget();
	if (Target)
	{
		const FTargetData* TargetData = TargetMap.Find(Target->TargetGuid);
		if (TargetData)
		{
			TargetStateOut = Target->LastKnownState;
			TargetDataOut = *TargetData;
			return true;
		}

	}
	return false;
}


void UGAPerceptionComponent::GetAllTargetStates(bool OnlyKnown, TArray<FTargetCache>& TargetCachesOut, TArray<FTargetData>& TargetDatasOut) const
{
	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		TArray<TObjectPtr<UGATargetComponent>>& TargetComponents = PerceptionSystem->GetAllTargetComponents();
		for (UGATargetComponent* TargetComponent : TargetComponents)
		{
			const FTargetData* TargetData = TargetMap.Find(TargetComponent->TargetGuid);
			if (TargetData)
			{
				if (!OnlyKnown || TargetComponent->IsKnown())
				{
					TargetCachesOut.Add(TargetComponent->LastKnownState);
					TargetDatasOut.Add(*TargetData);
				}
			}
		}
	}
}


void UGAPerceptionComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateAllTargetData();
}


void UGAPerceptionComponent::UpdateAllTargetData()
{
	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		TArray<TObjectPtr<UGATargetComponent>>& TargetComponents = PerceptionSystem->GetAllTargetComponents();
		for (UGATargetComponent* TargetComponent : TargetComponents)
		{
			UpdateTargetData(TargetComponent);
		}
	}
}

void UGAPerceptionComponent::UpdateTargetData(UGATargetComponent* TargetComponent)
{
	// REMEMBER: the UGAPerceptionComponent is going to be attached to the controller, not the pawn. So we call this special accessor to 
	// get the pawn that our controller is controlling
	APawn* OwnerPawn = GetOwnerPawn();
	// need to get the rocket info
	AActor* TargetOwner = TargetComponent->GetOwner();
	
	//APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	//APawn* TargetPawn = Cast<APawn>(TargetOwner);

	FTargetData *TargetData = TargetMap.Find(TargetComponent->TargetGuid);
	if (TargetData == NULL)		// If we don't already have a target data for the given target component, add it
	{
		FTargetData NewTargetData;
		FGuid TargetGuid = TargetComponent->TargetGuid;
		TargetData = &TargetMap.Add(TargetGuid, NewTargetData);
	}

	if (TargetData)
	{
		// TODO PART 3
		// 
		// - Update TargetData->bClearLOS
		//		Use this.VisionParameters to determine whether the target is within the vision cone or not 
		//		(and ideally do so before you case a ray towards it)
		// - Update TargetData->Awareness
		//		On ticks when the AI has a clear LOS, the Awareness should grow
		//		On ticks when the AI does not have a clear LOS, the Awareness should decay
		//
		// Awareness should be clamped to the range [0, 1]
		// You can add parameters to the UGAPerceptionComponent to control the speed at which awareness rises and falls

		// YOUR CODE HERE

		FVector OwnerPawnForward = OwnerPawn->GetActorForwardVector();

		// Calculate the vector between both pawns
		FVector DirectionToTarget = TargetOwner->GetActorLocation() - OwnerPawn->GetActorLocation();
		DirectionToTarget.Z = 0.0f; // Ignore height difference

		// Normalize both vectors
		OwnerPawnForward.Normalize();
		DirectionToTarget.Normalize();

		// Calculate the dot product between the two vectors
		float DotProduct = FVector::DotProduct(OwnerPawnForward, DirectionToTarget);

		// Calculate the angle between the vectors in degrees
		float AngleInDegrees = FMath::RadiansToDegrees(FMath::Acos(DotProduct));
		float Distance = FVector::Dist(OwnerPawn->GetActorLocation(), TargetOwner->GetActorLocation());
		bool clearLosRes = false;

		if (AngleInDegrees <= VisionParameters.VisionAngle/2 && Distance <= VisionParameters.VisionDistance)
		//if (Distance <= VisionParameters.VisionDistance)
		{
			UWorld* World = GetWorld();
			FHitResult HitResult;
			FCollisionQueryParams Params;
			FVector Start = OwnerPawn->GetActorLocation();		// need a ray start
			FVector End = TargetOwner->GetActorLocation();
			Params.AddIgnoredActor(OwnerPawn);
			Params.AddIgnoredActor(TargetOwner);
			bool bHitSomething = World->LineTraceSingleByChannel(HitResult, Start, End, ECollisionChannel::ECC_Visibility, Params);
			if (!bHitSomething) { // If bHitSomething is false, then we have a clear LOS
				clearLosRes = true;
			}
		}
		TargetData->bClearLos = clearLosRes;
		
		float oldAwareness = TargetData->Awareness;
		if (TargetData->bClearLos == true) {
			oldAwareness = oldAwareness + 0.1;
		}
		else {
			oldAwareness = oldAwareness - 0.1;
		}
		TargetData->Awareness = FMath::Clamp(oldAwareness, 0.0f, 1.0f);
	}
}


const FTargetData* UGAPerceptionComponent::GetTargetData(FGuid TargetGuid) const
{
	return TargetMap.Find(TargetGuid);
}

const bool UGAPerceptionComponent::checkCellVisibility(FVector Target)
{
	bool retVal = false;
	APawn* OwnerPawn = GetOwnerPawn();
	FVector OwnerPawnForward = OwnerPawn->GetActorForwardVector();

	// Calculate the vector between both pawns
	Target.Z = OwnerPawn->GetActorLocation().Z; //Hmm, this aint great LOS.
	FVector DirectionToTarget = Target - OwnerPawn->GetActorLocation();
	DirectionToTarget.Z = 0.0f; // Ignore height difference

	// Normalize both vectors
	OwnerPawnForward.Normalize();
	DirectionToTarget.Normalize();

	// Calculate the dot product between the two vectors
	float DotProduct = FVector::DotProduct(OwnerPawnForward, DirectionToTarget);

	// Calculate the angle between the vectors in degrees
	float AngleInDegrees = FMath::RadiansToDegrees(FMath::Acos(DotProduct));
	float Distance = FVector::Dist(OwnerPawn->GetActorLocation(), Target);

	if (AngleInDegrees <= VisionParameters.VisionAngle / 2 && Distance <= VisionParameters.VisionDistance)
		//if (Distance <= VisionParameters.VisionDistance)
	{
		UWorld* World = GetWorld();
		FHitResult HitResult;
		FCollisionQueryParams Params;
		FVector Start = OwnerPawn->GetActorLocation();		// need a ray start
		FVector End = Target;
		Params.AddIgnoredActor(OwnerPawn);
		bool bHitSomething = World->LineTraceSingleByChannel(HitResult, Start, End, ECollisionChannel::ECC_Visibility, Params);
		if (!bHitSomething) { // If bHitSomething is false, then we have a clear LOS
			retVal = true;
		}
	}

	return retVal;
}