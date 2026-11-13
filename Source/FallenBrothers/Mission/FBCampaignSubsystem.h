// Fallen Brothers - Campaign subsystem.
//
// Lives for the whole game session (survives level changes).
// Remembers which of the 13 cases are finished. The save system (step 12) will write this to disk.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FBCampaignSubsystem.generated.h"

UCLASS()
class FALLENBROTHERS_API UFBCampaignSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Campaign")
	void MarkMissionComplete(FName MissionId) { CompletedMissions.Add(MissionId); }

	UFUNCTION(BlueprintPure, Category = "Campaign")
	bool IsMissionComplete(FName MissionId) const { return CompletedMissions.Contains(MissionId); }

	UFUNCTION(BlueprintPure, Category = "Campaign")
	int32 GetCompletedCount() const { return CompletedMissions.Num(); }

	const TSet<FName>& GetCompletedMissions() const { return CompletedMissions; }
	void SetCompletedMissions(const TSet<FName>& InMissions) { CompletedMissions = InMissions; }

private:
	UPROPERTY()
	TSet<FName> CompletedMissions;
};
