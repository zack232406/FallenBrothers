// Fallen Brothers - What ordinary townspeople say when the brothers question them.
//
// Every person gets a name, a job, a temper (helpful, scared, rude) and maybe something they saw, all from one
// seed, so the same person always says the same things. Questions unlock as the investigation goes on (the mill,
// the 1952 fire, Elias Crane) and a few witnesses give real leads that go into the journal.

#pragma once

#include "CoreMinimal.h"
#include "Dialogue/FBDialogueTypes.h"

namespace FBTownTalk
{
	struct FPerson
	{
		FText Name;
		FText Role;
		FText Greeting;
		FText Farewell;
		TArray<FFBDialogueOption> Options;
	};

	/** The conversation for townsperson number Seed. bFemale picks first names; Hour (0-24) picks the greeting. */
	FALLENBROTHERS_API FPerson Build(int32 Seed, bool bFemale, float Hour = 12.f);
}
