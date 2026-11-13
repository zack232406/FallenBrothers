// Fallen Brothers - What ordinary townspeople say when questioned.

#include "Town/FBTownTalk.h"

#include "Math/RandomStream.h"

namespace
{
	const TCHAR* GMen[] = { TEXT("Walt"), TEXT("Earl"), TEXT("Doug"), TEXT("Ray"), TEXT("Hank"), TEXT("Gary"), TEXT("Tom"), TEXT("Frank"), TEXT("Lyle"), TEXT("Dennis"), TEXT("Carl"), TEXT("Bobby"), TEXT("Marty"), TEXT("Vern") };
	const TCHAR* GWomen[] = { TEXT("Peggy"), TEXT("Darlene"), TEXT("Judy"), TEXT("Carol"), TEXT("Linda"), TEXT("Rose"), TEXT("Barb"), TEXT("Nancy"), TEXT("Joanne"), TEXT("Tammy"), TEXT("Lorraine"), TEXT("Kathy") };
	const TCHAR* GLast[] = { TEXT("Hollis"), TEXT("Pruitt"), TEXT("Danvers"), TEXT("Kowalski"), TEXT("Brandt"), TEXT("McCready"), TEXT("Tolliver"), TEXT("Sumner"), TEXT("Akers"), TEXT("Lindqvist"), TEXT("Garber"), TEXT("Whitcomb"), TEXT("Ostrowski"), TEXT("Pell") };
	const TCHAR* GJobs[] = { TEXT("Works at the hardware store"), TEXT("Retired mill-town schoolteacher"), TEXT("Truck driver"), TEXT("Waitress at the diner"), TEXT("Mechanic"), TEXT("Lives on Elm Street"), TEXT("Volunteer firefighter"), TEXT("Postal carrier"), TEXT("Retired"), TEXT("Farmhand"), TEXT("Bank teller"), TEXT("Church organist") };

	enum class ETemper : uint8 { Helpful, Scared, Rude };

	FFBDialogueOption Opt(const TCHAR* Id, const FString& Ask, const FString& Answer)
	{
		FFBDialogueOption O;
		O.Id = FName(Id);
		O.PlayerLine = FText::FromString(Ask);
		O.NpcResponse = FText::FromString(Answer);
		return O;
	}
	template <int32 N>
	const TCHAR* Pick(FRandomStream& R, const TCHAR* (&List)[N]) { return List[R.RandRange(0, N - 1)]; }
}

FBTownTalk::FPerson FBTownTalk::Build(int32 Seed, bool bFemale, float Hour)
{
	FRandomStream R(Seed);
	FPerson P;
	const FString First = bFemale ? Pick(R, GWomen) : Pick(R, GMen);
	const FString Last = Pick(R, GLast);
	P.Name = FText::FromString(First + TEXT(" ") + Last);
	P.Role = FText::FromString(Pick(R, GJobs));
	const ETemper Temper = R.FRand() < 0.5f ? ETemper::Helpful : (R.FRand() < 0.6f ? ETemper::Scared : ETemper::Rude);
	// What this person knows (most know nothing useful - that's real life).
	const bool bSawTruck = R.FRand() < 0.18f;    // Dale's truck on Old Mill Road
	const bool bColdStory = R.FRand() < 0.22f;   // cold spots / dead lights
	const bool bOldTimer = R.FRand() < 0.25f;    // family remembers the 1952 fire
	const bool bKnowsCrane = bOldTimer && R.FRand() < 0.5f;

	const bool bNight = Hour >= 21.f || Hour < 5.f;
	const TCHAR* Hello = Hour >= 5.f && Hour < 12.f ? TEXT("Morning.") : Hour >= 12.f && Hour < 18.f ? TEXT("Afternoon.") : TEXT("Evening.");
	switch (Temper)
	{
	case ETemper::Helpful:
		P.Greeting = FText::FromString(bNight ? FString(TEXT("Bit late to be out, isn't it? You boys aren't from around here."))
			: FString(Hello) + TEXT(" You boys aren't from around here, are you?"));
		P.Farewell = FText::FromString(TEXT("You find whoever's doing this. People are scared."));
		break;
	case ETemper::Scared:
		P.Greeting = FText::FromString(TEXT("Oh! Sorry - you startled me. Everybody's jumpy lately."));
		P.Farewell = FText::FromString(bNight ? TEXT("I need to get home. Nobody should be out this late anymore.")
			: TEXT("I should get home before dark. You should too."));
		break;
	default:
		P.Greeting = FText::FromString(TEXT("What do you want?"));
		P.Farewell = FText::FromString(TEXT("Yeah, yeah. Good luck with that."));
		break;
	}

	// 1) The badge.
	{
		FFBDialogueOption O = Opt(TEXT("T_Badge"), TEXT("Agent Kessler, FBI. We're looking into the disappearances."),
			Temper == ETemper::Rude ? TEXT("FBI, huh? Took you long enough. Sheriff couldn't find his own hat.")
			: Temper == ETemper::Scared ? TEXT("The FBI? So it's... it's that bad, then. Okay. Ask me anything.")
			: TEXT("About time somebody took this seriously. Three people, Agent. Three."));
		O.RequiredItemId = TEXT("FBIBadge");
		O.bShowsBadge = true;
		P.Options.Add(O);
	}
	// 2) Dale Whitmore.
	{
		FString A;
		if (bSawTruck)
		{
			A = TEXT("Dale? I saw his truck the night he went missing. Parked out on Old Mill Road, lights off, door hanging open. I figured he was hunting. I should've stopped.");
		}
		else if (Temper == ETemper::Rude)
		{
			A = TEXT("Everybody knew Dale. Drank too much, talked too much. Doesn't mean he deserved... whatever this is.");
		}
		else
		{
			A = R.FRand() < 0.5f ? TEXT("Dale fixed my porch two summers ago. Good man. His wife hasn't slept since.")
			                     : TEXT("Only to say hello. Folks say he went out toward the old mill. Nobody goes out there.");
		}
		FFBDialogueOption O = Opt(TEXT("T_Dale"), TEXT("Did you know Dale Whitmore?"), A);
		if (bSawTruck)
		{
			O.GrantsClues = { TEXT("Clue_OldMill") };
			O.JournalNote = FText::FromString(FString::Printf(TEXT("%s saw Dale Whitmore's truck abandoned on Old Mill Road the night he vanished."), *P.Name.ToString()));
		}
		P.Options.Add(O);
	}
	// 3) Anything strange.
	{
		FString A;
		if (bColdStory)
		{
			A = TEXT("My dog won't walk past the mill anymore. And the cold - you go by there on a warm night and your breath fogs. My flashlight died out there. Brand new batteries.");
		}
		else if (Temper == ETemper::Scared)
		{
			A = TEXT("Strange? I hear crying some nights. My husband says it's the wind off the falls. It's not the wind.");
		}
		else
		{
			A = R.FRand() < 0.5f ? TEXT("Besides three people vanishing? No. Quiet town. Was, anyway.")
			                     : TEXT("Power keeps flickering on the east side. Electric company says there's nothing wrong.");
		}
		FFBDialogueOption O = Opt(TEXT("T_Strange"), TEXT("Noticed anything strange around town lately?"), A);
		if (bColdStory)
		{
			O.GrantsClues = { TEXT("Clue_ColdSpots") };
			O.JournalNote = FText::FromString(FString::Printf(TEXT("%s: sudden cold near the mill, flashlight died with fresh batteries."), *P.Name.ToString()));
		}
		P.Options.Add(O);
	}
	// 4) The mill (once the mill is a lead).
	{
		FString A = bOldTimer
			? TEXT("My grandfather worked there. Burned in '52 - fourteen dead. They said the doors upstairs were chained. Grandpa never talked about it sober.")
			: (Temper == ETemper::Rude ? TEXT("It's a burned-out wreck. Kids dare each other to go in. I'd stay out if I were you.")
			                           : TEXT("Old textile mill by the river. Closed after a fire, long before my time."));
		FFBDialogueOption O = Opt(TEXT("T_Mill"), TEXT("What can you tell me about the old textile mill?"), A);
		O.RequiredClues = { TEXT("Clue_OldMill") };
		if (bOldTimer)
		{
			O.GrantsClues = { TEXT("Clue_MillFire") };
			O.JournalNote = FText::FromString(FString::Printf(TEXT("%s: mill fire in 1952, fourteen dead, upstairs doors chained."), *P.Name.ToString()));
		}
		P.Options.Add(O);
	}
	// 5) Elias Crane (once the fire is known).
	{
		FString A = bKnowsCrane
			? TEXT("Crane. The foreman with the keys. They fished him out of the river the next spring. Council wouldn't give him a stone - buried him behind St. Agnes, by the north wall.")
			: TEXT("Crane? Can't say I know the name.");
		FFBDialogueOption O = Opt(TEXT("T_Crane"), TEXT("Ever heard the name Elias Crane?"), A);
		O.RequiredClues = { TEXT("Clue_MillFire") };
		if (bKnowsCrane)
		{
			O.GrantsClues = { TEXT("Clue_Crane") };
			O.JournalNote = FText::FromString(FString::Printf(TEXT("%s: Crane found in the river, buried unmarked behind St. Agnes Chapel, north wall."), *P.Name.ToString()));
		}
		P.Options.Add(O);
	}
	return P;
}
