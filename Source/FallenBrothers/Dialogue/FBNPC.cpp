// Fallen Brothers - Talkable NPC.

#include "Dialogue/FBNPC.h"

#include "Investigation/FBCaseJournalComponent.h"
#include "Inventory/FBInventoryComponent.h"
#include "Core/FBRealModels.h"
#include "Components/SkeletalMeshComponent.h"
#include "UI/FBHUD.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AFBNPC::AFBNPC()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// Capsule: blocks the player and is what the interaction trace hits. Actor origin = capsule centre.
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SetRootComponent(Capsule);
	Capsule->InitCapsuleSize(30.f, 88.f);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));
	Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// Placeholder body: cylinder torso/legs + sphere head (~176 cm tall).
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Capsule);
	Body->SetStaticMesh(Cylinder.Object);
	Body->SetRelativeScale3D(FVector(0.45f, 0.35f, 1.5f));
	Body->SetRelativeLocation(FVector(0.f, 0.f, -13.f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Capsule);
	Head->SetStaticMesh(Sphere.Object);
	Head->SetRelativeScale3D(FVector(0.24f));
	Head->SetRelativeLocation(FVector(0.f, 0.f, 75.f));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (BasicMaterial.Succeeded())
	{
		Body->SetMaterial(0, BasicMaterial.Object);
		Head->SetMaterial(0, BasicMaterial.Object);
	}

	BuildDemoNPC();
}

void AFBNPC::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyPreset();

	if (UMaterialInstanceDynamic* MID = Body->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), ClothingColor);
	}
	if (UMaterialInstanceDynamic* MID = Head->CreateAndSetMaterialInstanceDynamic(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.75f, 0.55f, 0.45f));
	}
}

void AFBNPC::BeginPlay()
{
	Super::BeginPlay();

	// A rigged body (e.g. the motel clerk's): imported rigs come in at any size - fit it to a real person's height.
	TInlineComponentArray<USkeletalMeshComponent*> Skins(this);
	const float FeetZ = GetActorLocation().Z - Capsule->GetScaledCapsuleHalfHeight();
	for (USkeletalMeshComponent* Skin : Skins)
	{
		FBRealModels::FitSkeletonToHeight(Skin, Skin, 178.f, [FeetZ]() { return FeetZ; });
	}
}

FText AFBNPC::GetInteractionText_Implementation(AActor* InteractingActor) const
{
	return FText::Format(NSLOCTEXT("FB", "TalkTo", "Talk to {0}"), NPCName);
}

void AFBNPC::Interact_Implementation(AActor* InteractingActor)
{
	const APawn* Pawn = Cast<APawn>(InteractingActor);
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (AFBHUD* HUD = PC ? Cast<AFBHUD>(PC->GetHUD()) : nullptr)
	{
		HUD->OpenDialogue(this);
	}
}

void AFBNPC::BeginConversation(AActor* Player)
{
	ConversationPartner = Player;
}

void AFBNPC::EndConversation()
{
	ConversationPartner.Reset();
}

FVector AFBNPC::GetHeadLocation() const
{
	return Head->GetComponentLocation();
}

void AFBNPC::ConfigureProxy(const FText& InName, const FText& InRole, const FText& InGreeting, const FText& InFarewell, const TArray<FFBDialogueOption>& InOptions)
{
	Preset = TEXT("Proxy");
	AppliedPreset = Preset;
	NPCName = InName;
	NPCRole = InRole;
	Greeting = InGreeting;
	Farewell = InFarewell;
	Options = InOptions;
	Body->SetVisibility(false);
	Head->SetVisibility(false);
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorTickEnabled(false);
}

void AFBNPC::ConfigureStoryProxy(FName InPreset)
{
	Preset = InPreset;
	ApplyPreset();
	TInlineComponentArray<UPrimitiveComponent*> Parts(this);
	for (UPrimitiveComponent* P : Parts)
	{
		P->SetVisibility(false, true);
		P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetActorTickEnabled(false);
}

void AFBNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Turn to face whoever we're talking to.
	if (const AActor* Partner = ConversationPartner.Get())
	{
		FVector ToPartner = Partner->GetActorLocation() - GetActorLocation();
		ToPartner.Z = 0.f;
		if (!ToPartner.IsNearlyZero())
		{
			const FRotator Target(0.f, ToPartner.Rotation().Yaw, 0.f);
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), Target, DeltaSeconds, TurnSpeed));
		}
	}
}

void AFBNPC::GetAvailableOptions(const UFBCaseJournalComponent* Journal, TArray<int32>& OutIndices) const
{
	OutIndices.Reset();
	for (int32 i = 0; i < Options.Num(); ++i)
	{
		if (Journal && !Journal->HasAllClues(Options[i].RequiredClues))
		{
			continue;
		}
		if (!Options[i].RequiredItemId.IsNone())
		{
			const AActor* JournalOwner = Journal ? Journal->GetOwner() : nullptr;
			const UFBInventoryComponent* Inventory = JournalOwner ? JournalOwner->FindComponentByClass<UFBInventoryComponent>() : nullptr;
			if (!Inventory || !Inventory->HasItem(Options[i].RequiredItemId))
			{
				continue;
			}
		}
		OutIndices.Add(i);
	}
}

// ---------------------------------------------------------------------------
// Demo NPC for "The Weeping Mill": Martha Kell, runs the diner on Main Street.
// ---------------------------------------------------------------------------
void AFBNPC::BuildDemoNPC()
{
	NPCName = FText::FromString(TEXT("Martha Kell"));
	NPCRole = FText::FromString(TEXT("Diner owner, Harlow Falls"));
	Greeting = FText::FromString(TEXT("You boys aren't from around here. Coffee's on the house if you're not reporters."));
	Farewell = FText::FromString(TEXT("Stay off Old Mill Road after dark. I mean it."));

	auto Add = [this](FName Id, const TCHAR* Ask, const TCHAR* Answer, TArray<FName> Required, TArray<FName> Grants, const TCHAR* Note)
	{
		FFBDialogueOption O;
		O.Id = Id;
		O.PlayerLine = FText::FromString(Ask);
		O.NpcResponse = FText::FromString(Answer);
		O.RequiredClues = MoveTemp(Required);
		O.GrantsClues = MoveTemp(Grants);
		O.JournalNote = FText::FromString(Note);
		Options.Add(O);
	};

	Add(TEXT("Martha_Disappearances"),
		TEXT("We're looking into the people who went missing."),
		TEXT("Dale Whitmore ate breakfast right where you're standing, every morning for ten years. "
		     "Then one night he goes up to that old mill on a dare and never comes back. "
		     "Folks say they hear a woman crying up there. I say folks should mind their business and stay away."),
		{}, { TEXT("Clue_OldMill") },
		TEXT("Martha: Dale Whitmore went to the old mill on a dare and vanished. People hear crying there."));

	Add(TEXT("Martha_Fire"),
		TEXT("What do you know about the mill fire in 1952?"),
		TEXT("My grandmother was one of the fourteen. She was nineteen.\n\n"
		     "Mama said Grandma used to come home with bruises on her arms. Said the foreman, Crane, "
		     "liked to lock the girls in the cutting room if they talked back. That night he locked all of them in."),
		{ TEXT("Clue_MillFire") }, {},
		TEXT("Martha's grandmother died in the fire. Crane used to lock workers in as punishment."));

	Add(TEXT("Martha_Grave"),
		TEXT("Do you know where Elias Crane was buried?"),
		TEXT("...You don't ask that around here.\n\n"
		     "Fine. My granddad dug that hole. Behind St. Agnes, along the north wall, no stone. "
		     "He said the ground was so cold it cracked his shovel. He never went back."),
		{ TEXT("Clue_Crane") }, { TEXT("Clue_CraneGrave") },
		TEXT("Martha: Crane buried behind St. Agnes Chapel, north wall, no headstone. Her grandfather dug it."));

	Add(TEXT("Martha_Iron"),
		TEXT("Anyone in town keep iron or salt around the house?"),
		TEXT("Old Hollis at the hardware store nails horseshoes over every door. Pours salt on his windowsills too. "
		     "Everybody laughs at him.\n\nHe's also the only one on Mill Road who still sleeps through the night."),
		{ TEXT("Clue_Weakness") }, {},
		TEXT("Hollis (hardware store) protects his house with iron horseshoes and salt - and sleeps fine."));
}

void AFBNPC::ApplyPreset()
{
	if (Preset == AppliedPreset)
	{
		return;
	}
	if (Preset == TEXT("Proxy"))
	{
		AppliedPreset = Preset; // a townsperson's conversation, filled in by ConfigureProxy - keep it
		return;
	}
	AppliedPreset = Preset;
	Options.Reset();
	if (Preset == TEXT("MotelClerk"))
	{
		BuildMotelClerk();
	}
	else if (Preset == TEXT("Whitmore") || Preset == TEXT("Librarian") || Preset == TEXT("Decker") || Preset == TEXT("Barlow") || Preset == TEXT("Crane"))
	{
		BuildFamilyAndLibrary(Preset);
	}
	else
	{
		BuildDemoNPC();
	}
}

// ---------------------------------------------------------------------------
// The Weeping Mill, the families and the library: Ellen Whitmore (Dale's wife) and the town librarian.
// The grave isn't in any book (the register line is scratched out) - only someone whose family was there knows.
// ---------------------------------------------------------------------------
void AFBNPC::BuildFamilyAndLibrary(FName Who)
{
	auto Add = [this](FName Id, const TCHAR* Ask, const TCHAR* Answer, TArray<FName> Required, TArray<FName> Grants, const TCHAR* Note)
	{
		FFBDialogueOption O;
		O.Id = Id;
		O.PlayerLine = FText::FromString(Ask);
		O.NpcResponse = FText::FromString(Answer);
		O.RequiredClues = MoveTemp(Required);
		O.GrantsClues = MoveTemp(Grants);
		O.JournalNote = FText::FromString(Note);
		Options.Add(O);
	};

	if (Who == TEXT("Whitmore"))
	{
		NPCName = FText::FromString(TEXT("Ellen Whitmore"));
		NPCRole = FText::FromString(TEXT("Dale Whitmore's wife"));
		Greeting = FText::FromString(TEXT("If you're from the paper, I've got nothing to say.\n\n...Federal? The sheriff stopped returning my calls on the second day."));
		Farewell = FText::FromString(TEXT("Find him. Or find out. Either one."));
		ClothingColor = FLinearColor(0.3f, 0.28f, 0.32f);

		Add(TEXT("Ellen_Dale"),
			TEXT("We're looking into your husband's disappearance."),
			TEXT("Dale went up to that mill on a Thursday night. Said he had to see it for himself. His truck was still there in the morning, "
			     "keys in it, radio on.\n\nThe search party wouldn't go past the stairs. Grown men. Said it was too cold to breathe up there."),
			{}, { TEXT("Clue_OldMill"), TEXT("Clue_FamWhitmore") },
			TEXT("Ellen Whitmore: Dale drove to the mill on a Thursday night and vanished. Truck left running. Searchers wouldn't climb the stairs - too cold."));

		Add(TEXT("Ellen_Week"),
			TEXT("Was he acting strange before he went up there?"),
			TEXT("He stopped sleeping. Spent every afternoon at the library, reading old newspapers about the fire. "
			     "He'd come home smelling of smoke when there wasn't a fire anywhere.\n\n"
			     "The last night he woke me up. Said a man in a foreman's coat was standing at the foot of our bed, holding a ring of keys."),
			{}, { TEXT("Clue_LibraryLead"), TEXT("Clue_ColdSpots") },
			TEXT("Ellen: before he vanished Dale read about the 1952 fire at the library every day. Saw a man in a foreman's coat with a ring of keys."));

		Add(TEXT("Ellen_Crane"),
			TEXT("Did he ever say the name Crane?"),
			TEXT("...How do you know that name?\n\nAt the end he wrote it on everything. The grocery list. The wall by the phone. CRANE, CRANE, over and over. "
			     "I painted over it twice. It's still there under the paint. I can feel it when I walk past."),
			{ TEXT("Clue_Crane") }, {},
			TEXT("Ellen: Dale wrote CRANE all over the house in his last days."));
	}
	else if (Who == TEXT("Decker"))
	{
		NPCName = FText::FromString(TEXT("Tom Decker"));
		NPCRole = FText::FromString(TEXT("Father of Ruth Ann Decker (missing since March)"));
		Greeting = FText::FromString(TEXT("You're the first badge to knock on this door since April.\n\nShe's nineteen. Was. Is. I don't know which word to use anymore."));
		Farewell = FText::FromString(TEXT("Her room's the way she left it. I can't make myself shut the door."));
		ClothingColor = FLinearColor(0.25f, 0.22f, 0.18f);

		Add(TEXT("Decker_Ruth"),
			TEXT("Tell us about the night Ruth Ann disappeared."),
			TEXT("She was the first. March. Took pictures for a school paper - 'forgotten places', she called it. Went up to the mill before dark, promised she'd be home for supper.\n\n"
			     "They found her camera on the second floor. Every picture on it was black. Except the last one. A door, with a chain on it. From the inside."),
			{}, { TEXT("Clue_OldMill"), TEXT("Clue_FamDecker") },
			TEXT("Tom Decker: Ruth Ann (19) was the first to vanish, in March, photographing the mill. Her last photo: a chained door, taken from inside."));

		Add(TEXT("Decker_Before"),
			TEXT("Anything strange in the days before?"),
			TEXT("She kept saying the house smelled like smoke. Kept checking the stove.\n\nAnd she asked me about my grandfather. Out of nowhere. "
			     "Whether it was true what the old men used to say, that he was 'down at the river that night in fifty-three'. I told her to leave it alone.\n\nI should've asked her why she wanted to know."),
			{}, { TEXT("Clue_ColdSpots"), TEXT("Clue_RiverNight") },
			TEXT("Decker: Ruth Ann smelled smoke for days, and asked about her great-grandfather being 'down at the river that night in 1953'."));
	}
	else if (Who == TEXT("Barlow"))
	{
		NPCName = FText::FromString(TEXT("June Barlow"));
		NPCRole = FText::FromString(TEXT("Daughter of Hank Barlow (missing since June)"));
		Greeting = FText::FromString(TEXT("If this is about Dad's pension, talk to the county.\n\n...Oh. You're actually looking for him."));
		Farewell = FText::FromString(TEXT("If you find his thermos up there, I'd like it back."));
		ClothingColor = FLinearColor(0.2f, 0.26f, 0.34f);

		Add(TEXT("Barlow_Hank"),
			TEXT("Your father was the second to go missing."),
			TEXT("Dad was night watchman on that property for thirty years. Never scared of it. Called the stories 'kid stuff'.\n\n"
			     "Then in June he comes home at four in the morning, white as milk, and nails a horseshoe over the back door. Wouldn't say why. "
			     "Two nights later he went back for his thermos. That's it. That's the whole story."),
			{}, { TEXT("Clue_OldMill"), TEXT("Clue_FamBarlow") },
			TEXT("June Barlow: Hank (62), the mill's night watchman, came home terrified in June, nailed up a horseshoe, vanished two nights later."));

		Add(TEXT("Barlow_Box"),
			TEXT("Did he leave anything behind? Papers, notes?"),
			TEXT("A cigar box. Grandpa's things. There's a photograph in it, nineteen fifty-three, six men standing on the river bank with lanterns. Nobody's smiling.\n\n"
			     "Dad wrote the names on the back the week before he went. Barlow. Decker. Whitmore. Kell. Two I can't read.\n\nHe underlined three of them."),
			{}, { TEXT("Clue_RiverNight"), TEXT("Clue_RiverPhoto") },
			TEXT("June Barlow: a 1953 photo of six men with lanterns at the river - Barlow, Decker, Whitmore, Kell and two more. Hank underlined three names."));
	}
	else if (Who == TEXT("Crane"))
	{
		NPCName = FText::FromString(TEXT("Walter Crane"));
		NPCRole = FText::FromString(TEXT("Grand-nephew of Elias Crane"));
		Greeting = FText::FromString(TEXT("Whatever you're selling, the answer's no.\n\nAnd if you're here about the name - I've been spat on for it since grade school. Get in line."));
		Farewell = FText::FromString(TEXT("Seventy years this town kept its mouth shut. Don't expect a thank-you for opening it."));
		ClothingColor = FLinearColor(0.18f, 0.18f, 0.2f);

		Add(TEXT("Crane_Elias"),
			TEXT("We want to know about Elias."),
			TEXT("Uncle Elias was a hard man and a mean one. Nobody in my family ever said different. He chained those doors, far as anyone knows.\n\n"
			     "But the town tells it like he ran, and the river took him, and God settled it. That's the Sunday-school version."),
			{ TEXT("Clue_Crane") }, {},
			TEXT("Walter Crane, Elias's grand-nephew: the family admits Elias was cruel - but says the town's story of his death is a lie."));

		Add(TEXT("Crane_Death"),
			TEXT("How did he really die?"),
			TEXT("He didn't run. He hid in my grandmother's root cellar for five months. March of fifty-three, six men came with lanterns. Fathers and brothers of the girls who burned.\n\n"
			     "They walked him to the river above the falls, wired his own key ring round his wrist, and held him under. Grandma watched from the trees.\n\n"
			     "Coroner wrote 'drowned'. Same men carried the box. Same men picked a hole with no stone, so nobody'd ever have to look at it.\n\n"
			     "The keys never went in the box. One of them hung the ring back on the foreman's hook up at the mill. A trophy."),
			{ TEXT("Clue_Crane") }, { TEXT("Clue_CraneDeath"), TEXT("Clue_RiverNight") },
			TEXT("TRUTH: Elias Crane didn't drown by accident. In March 1953 six townsmen - relatives of the fire's victims - drowned him in the river and buried him unmarked."));

		Add(TEXT("Crane_Why"),
			TEXT("Whitmore. Decker. Barlow. Why those three?"),
			TEXT("...You saw the photograph.\n\nThen you already know. He's not haunting a building, son. He's collecting. One name at a time, off the back of that picture.\n\n"
			     "There's three names left. One of them pours your coffee at the diner."),
			{ TEXT("Clue_RiverPhoto"), TEXT("Clue_CraneDeath") }, { TEXT("Clue_Bloodline") },
			TEXT("PATTERN: Crane is taking the descendants of the six men who drowned him. Three are gone. Three names remain - one of them is Martha Kell."));
	}
	else
	{
		NPCName = FText::FromString(TEXT("Agnes Pell"));
		NPCRole = FText::FromString(TEXT("Librarian, Harlow Falls Public Library"));
		Greeting = FText::FromString(TEXT("We close at five. Keep your voices down - Mr. Tolliver is asleep in periodicals and I'd like him to stay that way."));
		Farewell = FText::FromString(TEXT("Put the reels back the way you found them."));
		ClothingColor = FLinearColor(0.22f, 0.3f, 0.24f);

		Add(TEXT("Agnes_Fire"),
			TEXT("We're looking for anything on the 1952 mill fire."),
			TEXT("The Gazette's on microfilm, back room, reel fifty-two. Dale Whitmore wore the crank smooth this summer.\n\n"
			     "Funny thing. Every time he finished, the reader was cold to the touch. Like it had been sitting outside all night."),
			{}, { TEXT("Clue_OldMill") },
			TEXT("Librarian Agnes Pell: the 1952 Gazette is on microfilm in the back room (reel 52). Dale read it all summer."));

		Add(TEXT("Agnes_Register"),
			TEXT("Is there a burial record for Elias Crane?"),
			TEXT("The parish register's here, the church gave it to us after the flood.\n\n"
			     "Nineteen fifty-three... There. Fourteen names from the fire, all in the churchyard. And one line scratched out so hard the pen went through the page.\n\n"
			     "Whoever kept this book didn't want him found. If anybody knows, it's the old families. The ones who lost someone in that fire."),
			{ TEXT("Clue_Crane") }, { TEXT("Clue_GraveScratched") },
			TEXT("Agnes: Crane's burial line in the parish register was scratched out. Ask the old families who lost someone in the fire."));
	}
}

// ---------------------------------------------------------------------------
// The motel's night clerk: suspicious of two young men with no luggage. The fake FBI badge gets them room 7.
// ---------------------------------------------------------------------------
void AFBNPC::BuildMotelClerk()
{
	NPCName = FText::FromString(TEXT("Wendell Price"));
	NPCRole = FText::FromString(TEXT("Desk clerk, Harlow Falls Motor Inn"));
	Greeting = FText::FromString(TEXT("Forty a night, cash or card, checkout's at eleven.\n\nYou two don't look like you're here for the fishing."));
	Farewell = FText::FromString(TEXT("Ice machine's busted. Don't bother complaining."));
	ClothingColor = FLinearColor(0.2f, 0.24f, 0.3f);

	auto Add = [this](FName Id, const TCHAR* Ask, const TCHAR* Answer, TArray<FName> Required, TArray<FName> Grants, const TCHAR* Note) -> FFBDialogueOption&
	{
		FFBDialogueOption O;
		O.Id = Id;
		O.PlayerLine = FText::FromString(Ask);
		O.NpcResponse = FText::FromString(Answer);
		O.RequiredClues = MoveTemp(Required);
		O.GrantsClues = MoveTemp(Grants);
		O.JournalNote = FText::FromString(Note);
		return Options.Add_GetRef(O);
	};

	Add(TEXT("Clerk_Room"),
		TEXT("We need a room. A few nights, maybe a week."),
		TEXT("A week. In Harlow Falls.\n\nNobody stays a week here. Not anymore. Last folks who did left at three in the morning "
		     "and didn't even take their suitcase.\n\nI'm gonna need some ID. And a reason."),
		{}, { TEXT("Clue_ClerkAsked") }, TEXT(""));

	FFBDialogueOption& Badge = Add(TEXT("Clerk_Badge"),
		TEXT("[Show the FBI badge] Agents Harlan and Wade. Federal Bureau of Investigation."),
		TEXT("...FBI.\n\nYou boys look kinda young for feds.\n\n"
		     "Huh. Okay. Okay, sure. Room 7, end of the row, it's the only one with a working heater. "
		     "Here's the key card. Don't lose it, the reader eats 'em and I only got the one spare."),
		{ TEXT("Clue_ClerkAsked") }, { TEXT("Clue_MotelRoom") },
		TEXT("Checked into the motel as FBI agents Harlan and Wade. Room 7, key card from the night clerk, Wendell."));
	Badge.RequiredItemId = TEXT("FBIBadge");
	Badge.GrantsItemId = TEXT("Room7Card");
	Badge.GrantsItemName = FText::FromString(TEXT("Room 7 key card"));
	Badge.GrantsItemDescription = FText::FromString(TEXT("White plastic card, HARLOW FALLS MOTOR INN. Someone wrote a 7 on it in marker."));
	Badge.bShowsBadge = true;

	Add(TEXT("Clerk_Suitcase"),
		TEXT("The people who left at three in the morning. What happened?"),
		TEXT("Couple from Ohio, room 4. I heard the car start, went out, engine's running, doors wide open, nobody in it.\n\n"
		     "Came back an hour later, said the room got so cold their breath was smoking. Somebody wrote COLD on the bathroom mirror. "
		     "From the inside. I told the sheriff. Sheriff told me to quit drinking on shift."),
		{ TEXT("Clue_ClerkAsked") }, { TEXT("Clue_ColdSpots") },
		TEXT("Wendell (motel clerk): guests in room 4 fled at 3 a.m. - freezing room, COLD written on the mirror from the inside."));

	Add(TEXT("Clerk_Mill"),
		TEXT("Has anybody else come asking questions? About the old mill?"),
		TEXT("Only the ones who went missing.\n\nDale Whitmore sat right where you're standing and asked me how to get up Old Mill Road. "
		     "I told him. I think about that a lot."),
		{ TEXT("Clue_MotelRoom") }, { TEXT("Clue_OldMill") },
		TEXT("Wendell gave Dale Whitmore directions to Old Mill Road the night he vanished."));
}
