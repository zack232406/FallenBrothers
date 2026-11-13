// Fallen Brothers - Paranormal world subsystem.

#include "Paranormal/FBParanormalSubsystem.h"

#include "Paranormal/FBParanormalSourceComponent.h"
#include "Paranormal/FBUVTrace.h"
#include "Paranormal/FBSaltLine.h"

void UFBParanormalSubsystem::RegisterSource(UFBParanormalSourceComponent* Source)
{
	if (Source)
	{
		Sources.AddUnique(Source);
	}
}

void UFBParanormalSubsystem::UnregisterSource(UFBParanormalSourceComponent* Source)
{
	Sources.Remove(Source);
}

void UFBParanormalSubsystem::RegisterUVTrace(AFBUVTrace* Trace)
{
	if (Trace)
	{
		UVTraces.AddUnique(Trace);
	}
}

void UFBParanormalSubsystem::UnregisterUVTrace(AFBUVTrace* Trace)
{
	UVTraces.Remove(Trace);
}

float UFBParanormalSubsystem::GetEMFLevelAt(const FVector& Location) const
{
	float Best = 0.f;
	for (const TWeakObjectPtr<UFBParanormalSourceComponent>& Source : Sources)
	{
		if (const UFBParanormalSourceComponent* S = Source.Get())
		{
			Best = FMath::Max(Best, S->GetEMFAt(Location));
		}
	}
	return FMath::Clamp(Best, 0.f, 5.f);
}

void UFBParanormalSubsystem::RegisterSaltLine(AFBSaltLine* Line)
{
	if (Line)
	{
		SaltLines.AddUnique(Line);
	}
}

void UFBParanormalSubsystem::UnregisterSaltLine(AFBSaltLine* Line)
{
	SaltLines.Remove(Line);
}

bool UFBParanormalSubsystem::IsBlockedBySalt(const FVector& A, const FVector& B) const
{
	for (const TWeakObjectPtr<AFBSaltLine>& Line : SaltLines)
	{
		if (const AFBSaltLine* L = Line.Get())
		{
			if (L->BlocksPath(A, B))
			{
				return true;
			}
		}
	}
	return false;
}
