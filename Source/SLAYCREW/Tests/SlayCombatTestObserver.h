#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Combat/SlayInjuryComponent.h"
#include "SlayCombatTestObserver.generated.h"

/** Transient delegate receiver for native automation; never instantiated by gameplay. */
UCLASS(Transient)
class USlayCombatTestObserver : public UObject
{
 GENERATED_BODY()
public:
 int32 Deaths = 0;
 int32 Collapses = 0;
 UFUNCTION() void Death() { ++Deaths; }
 UFUNCTION() void Collapse(ESlayInjuryState State, float Severity) { ++Collapses; }
};
