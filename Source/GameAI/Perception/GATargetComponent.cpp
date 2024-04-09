#include "GATargetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameAI/Grid/GAGridActor.h"
#include "GAPerceptionSystem.h"
#include "ProceduralMeshComponent.h"



UGATargetComponent::UGATargetComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// A bit of Unreal magic to make TickComponent below get called
	PrimaryComponentTick.bCanEverTick = true;

	SetTickGroup(ETickingGroup::TG_PostUpdateWork);

	// Generate a new guid
	TargetGuid = FGuid::NewGuid();
}


AGAGridActor* UGATargetComponent::GetGridActor() const
{
	AGAGridActor* Result = GridActor.Get();
	if (Result)
	{
		return Result;
	}
	else
	{
		AActor* GenericResult = UGameplayStatics::GetActorOfClass(this, AGAGridActor::StaticClass());
		if (GenericResult)
		{
			Result = Cast<AGAGridActor>(GenericResult);
			if (Result)
			{
				// Cache the result
				// Note, GridActor is marked as mutable in the header, which is why this is allowed in a const method
				GridActor = Result;
			}
		}

		return Result;
	}
}


void UGATargetComponent::OnRegister()
{
	Super::OnRegister();

	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		PerceptionSystem->RegisterTargetComponent(this);
	}

	const AGAGridActor* Grid = GetGridActor();
	if (Grid)
	{
		OccupancyMap = FGAGridMap(Grid, 0.0f);
	}
}

void UGATargetComponent::OnUnregister()
{
	Super::OnUnregister();

	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		PerceptionSystem->UnregisterTargetComponent(this);
	}
}



void UGATargetComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bool isImmediate = false;

	// update my perception state FSM
	UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
	if (PerceptionSystem)
	{
		TArray<TObjectPtr<UGAPerceptionComponent>> &PerceptionComponents = PerceptionSystem->GetAllPerceptionComponents();
		for (UGAPerceptionComponent* PerceptionComponent : PerceptionComponents)
		{
			const FTargetData* TargetData = PerceptionComponent->GetTargetData(TargetGuid);
			if (TargetData && (TargetData->Awareness >= 1.0f))
			{
				isImmediate = true;
				break;
			}
		}
	}

	if (isImmediate)
	{
		AActor* Owner = GetOwner();
		LastKnownState.State = GATS_Immediate;

		// REFRESH MY STATE
		//LastKnownState.Set(Owner->GetActorLocation(), Owner->GetVelocity());
		LastKnownState.Set(Owner->GetActorLocation(), Owner->GetActorForwardVector());

		// Tell the omap to clear out and put all the probability in the observed location
		//OccupancyMapSetPosition(LastKnownState.Position);
	}
	else if (IsKnown())
	{
		LastKnownState.State = GATS_Hidden;
	}

	if (LastKnownState.State == GATS_Hidden)
	{
		//OccupancyMapUpdate();
	}

	// As long as I'm known, whether I'm immediate or not, diffuse the probability in the omap

	if (IsKnown())
	{
		//OccupancyMapDiffuse();
	}

	if (bDebugOccupancyMap)
	{
		AGAGridActor* Grid = GetGridActor();
		Grid->DebugGridMap = OccupancyMap;
		GridActor->RefreshDebugTexture();
		GridActor->DebugMeshComponent->SetVisibility(true);
	}
}


void UGATargetComponent::OccupancyMapSetPosition(const FVector& Position)
{
	AGAGridActor* Grid = GetGridActor();
	FCellRef FCellRefPos = Grid->GetCellRef(Position);
	OccupancyMap.ResetData(0.0f);
	OccupancyMap.SetValue(FCellRefPos, 1.0f);

	
	// TODO PART 4

	// We've been observed to be in a given position
	// Clear out all probability in the omap, and set the appropriate cell to P = 1.0

	//Done.
}


void UGATargetComponent::OccupancyMapUpdate()
{
	const AGAGridActor* Grid = GetGridActor();
	if (Grid)
	{
		// TODO PART 4

		// STEP 1: Build the visibility map, based on the perception components of the AIs in the world

		// STEP 2: Clear out the probability in the visible cells

		// STEP 3: Renormalize the OMap, so that it's still a valid probability distribution

		// STEP 4: Extract the highest-likelihood cell on the omap and refresh the LastKnownState.

		UGAPerceptionSystem* PerceptionSystem = UGAPerceptionSystem::GetPerceptionSystem(this);
		if (PerceptionSystem)
		{
			TArray<TObjectPtr<UGAPerceptionComponent>>& PerceptionComponents = PerceptionSystem->GetAllPerceptionComponents();
			TSet<FCellRef> VisibleSet;
			float harvestedVisibility = 0.0f;
			for (UGAPerceptionComponent* PerceptionComponent : PerceptionComponents)
			{
				//  Find visible cells from this AI
				for (int X = 0; X < Grid->XCount; X++) {
					for (int Y = 0; Y < Grid->YCount; Y++) {
						FCellRef current = FCellRef(X, Y);
						if (PerceptionComponent->checkCellVisibility(Grid->GetCellPosition(current))) 
						{
							if (!VisibleSet.Contains(current)) {
								VisibleSet.Add(current); //Step 1;

								float visVal;//Step 2;
								OccupancyMap.GetValue(current, visVal);
								harvestedVisibility = harvestedVisibility + visVal;
								OccupancyMap.SetValue(current, 0.0f);
							}
							
						}
					}
				}
			}

			//Step 3
			//Distribute every cell to every non
			for (int X = 0; X < Grid->XCount; X++) {
				for (int Y = 0; Y < Grid->YCount; Y++) {
					FCellRef current = FCellRef(X, Y);
					float value;
					OccupancyMap.GetValue(current, value);
					if (value != 0.0f) 
					{
						value = value * (1 / (1 - harvestedVisibility));
					}
				}
			}

			//Step4
			float maxProb;
			OccupancyMap.GetMaxValue(maxProb);
			for (int X = 0; X < Grid->XCount; X++) {
				for (int Y = 0; Y < Grid->YCount; Y++) {
					FCellRef current = FCellRef(X, Y);
					float value;
					OccupancyMap.GetValue(current, value);
					if (value == maxProb)
					{
						AActor* Owner = GetOwner();
						LastKnownState.Set(Grid->GetCellPosition(current), Owner->GetVelocity());
					}
				}
			}
		}
	}

}


void UGATargetComponent::OccupancyMapDiffuse()
{
	// TODO PART 4
	// Diffuse the probability in the OMAP
	const AGAGridActor* Grid = GetGridActor();
	FGAGridMap DiffuseMap(Grid, 0.0f);
	if (Grid)
	{
		for (int X = 0; X < Grid->XCount; X++)
		{
			for (int Y = 0; Y < Grid->YCount; Y++) {
				FCellRef current = FCellRef(X, Y); //For each Cell in the grid
				ECellData flags = Grid->GetCellData(current);
				if (flags == ECellData::CellDataTraversable && Grid->IsValidCell(current))
				{
					//For every Cell
					int distroCount = 0;
					float totalProb = 0.0f;
					TSet<FCellRef> harvestedCells; //Started the harvest
					for (int horiz = X - 1; horiz <= X + 1; horiz++)
					{
						for (int vert = Y - 1; vert <= Y + 1; vert++)
						{
							//For every neighboring FCell
							FCellRef cellToHarvest = FCellRef(horiz, vert);
							if (Grid->IsValidCell(current))//As long as its not OOB
							{
								ECellData flags2 = Grid->GetCellData(cellToHarvest);
								if (flags2 == ECellData::CellDataTraversable) //And as long as its traversable
								{
									distroCount++; //We harvested another cell
									float harvest;
									OccupancyMap.GetValue(cellToHarvest, harvest); //Got its value
									totalProb = totalProb + harvest; //Added it to the total
									//OccupancyMap.SetValue(cellToHarvest, 0.0f); //Set it to 0
									harvestedCells.Add(cellToHarvest); //Now we will go through and redistribute.
								}
							}
						}
					}
					DiffuseMap.SetValue(current, totalProb/distroCount);
					/*
					for (FCellRef cell : harvestedCells) //For each cell we harvested from
					{
						float guaranteedEqual = (totalProb / distroCount);
						OccupancyMap.SetValue(cell, guaranteedEqual); //
					}
					*/
					
				}
			}
		}
		OccupancyMap = DiffuseMap;
	}
}