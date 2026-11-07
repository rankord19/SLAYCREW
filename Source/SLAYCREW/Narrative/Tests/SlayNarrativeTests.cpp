#if WITH_DEV_AUTOMATION_TESTS

#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Narrative/SlayNarrativeSaveData.h"
#include "Narrative/SlayNarrativeState.h"

// Placeholder IDs only; no story characters are defined by these tests.
namespace SlayNarrativeTests
{
	const TCHAR* const CompanionA = TEXT("CompanionA");
	const TCHAR* const CompanionB = TEXT("CompanionB");
	const TCHAR* const CompanionC = TEXT("CompanionC");
	const TCHAR* const CompanionD = TEXT("CompanionD");

	FName Player() { return USlayNarrativeState::GetProtagonistId(); }

	USlayNarrativeState* NewState() { return NewObject<USlayNarrativeState>(); }

	FSlayRelationshipProfile Profile(FName Id, FName Other = USlayNarrativeState::GetProtagonistId())
	{
		FSlayRelationshipProfile Result;
		Result.CharacterId = Id;
		Result.OtherCharacterId = Other;
		return Result;
	}

	FSlayNarrativeCondition FlagCondition(FName Flag, bool bNegate = false)
	{
		FSlayNarrativeCondition Condition;
		Condition.Type = ESlayConditionType::NarrativeFlag;
		Condition.Name = Flag;
		Condition.bNegate = bNegate;
		return Condition;
	}

	FSlayNarrativeCondition RelationshipFlagCondition(FName Id, FName Flag)
	{
		FSlayNarrativeCondition Condition;
		Condition.Type = ESlayConditionType::RelationshipFlag;
		Condition.CharacterId = Id;
		Condition.Name = Flag;
		return Condition;
	}

	FSlayNarrativeCondition ValueCondition(FName Id, ESlayRelationshipDimension Dimension, ESlayComparison Op, int32 Value)
	{
		FSlayNarrativeCondition Condition;
		Condition.Type = ESlayConditionType::RelationshipValue;
		Condition.CharacterId = Id;
		Condition.Dimension = Dimension;
		Condition.Comparison = Op;
		Condition.Value = Value;
		return Condition;
	}

	FSlayNarrativeCondition ChapterCondition(ESlayComparison Op, int32 Value)
	{
		FSlayNarrativeCondition Condition;
		Condition.Type = ESlayConditionType::Chapter;
		Condition.Comparison = Op;
		Condition.Value = Value;
		return Condition;
	}

	FSlayNarrativeCondition DecisionCondition(FName DecisionId, FName OptionId = NAME_None)
	{
		FSlayNarrativeCondition Condition;
		Condition.Type = ESlayConditionType::Decision;
		Condition.Name = DecisionId;
		Condition.Option = OptionId;
		return Condition;
	}

	FSlayRomanceTransitionRule Rule(ESlayRomanceState From, ESlayRomanceState To,
		TArray<FSlayNarrativeCondition> AllOf = {})
	{
		FSlayRomanceTransitionRule Result;
		Result.From = From;
		Result.To = To;
		Result.Requirements.AllOf = MoveTemp(AllOf);
		return Result;
	}

	FSlayNarrativeChoice Choice(FName DecisionId, FName OptionId)
	{
		FSlayNarrativeChoice Result;
		Result.DecisionId = DecisionId;
		Result.OptionId = OptionId;
		return Result;
	}

	FSlayRelationshipChange Change(FName Id, ESlayRelationshipDimension Dimension, int32 Delta)
	{
		FSlayRelationshipChange Result;
		Result.CharacterId = Id;
		Result.Dimension = Dimension;
		Result.Delta = Delta;
		return Result;
	}

	FSlayRomanceTransitionRequest RomanceRequest(FName Id, ESlayRomanceState NewState)
	{
		FSlayRomanceTransitionRequest Result;
		Result.CharacterId = Id;
		Result.NewState = NewState;
		return Result;
	}
}

using namespace SlayNarrativeTests;
using EDim = ESlayRelationshipDimension;
using ERomance = ESlayRomanceState;

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlayRelationshipDimensionsTest, "SLAYCREW.Narrative.RelationshipDimensions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlayRelationshipDimensionsTest::RunTest(const FString& Parameters)
{
	USlayNarrativeState* State = NewState();

	// Character-specific initial states, bounds and responses.
	FSlayRelationshipProfile A = Profile(CompanionA);
	A.Dimensions.Trust.Initial = 10;
	A.Dimensions.Friendship.Initial = 30;
	A.Dimensions.Affection.Min = 0;
	A.Dimensions.Affection.Max = 50;
	FSlayRelationshipThreshold TrustHigh;
	TrustHigh.ThresholdId = TEXT("TrustHigh");
	TrustHigh.Dimension = EDim::Trust;
	TrustHigh.Value = 40;
	TrustHigh.FlagOnReached = TEXT("TrustedOnce");
	A.Thresholds.Add(TrustHigh);

	FSlayRelationshipProfile B = Profile(CompanionB);
	B.Dimensions.Trust.Initial = -5;
	B.Dimensions.Trust.GainPercent = 50;
	B.Dimensions.Friendship.LossPercent = 200;
	B.InitialFlags.Add(TEXT("KnownBeforeAcademy"));

	TestTrue(TEXT("Register A"), State->RegisterProfile(A));
	TestTrue(TEXT("Register B"), State->RegisterProfile(B));
	TestFalse(TEXT("Duplicate profile rejected"), State->RegisterProfile(A));
	TestFalse(TEXT("Reversed duplicate rejected"), State->RegisterProfile(Profile(Player(), CompanionA)));
	TestFalse(TEXT("Self relationship rejected"), State->RegisterProfile(Profile(CompanionC, CompanionC)));
	FSlayRelationshipProfile Bad = Profile(CompanionC);
	Bad.Dimensions.Trust.Min = 10;
	Bad.Dimensions.Trust.Max = 0;
	TestFalse(TEXT("Invalid bounds rejected"), State->RegisterProfile(Bad));

	TestEqual(TEXT("A initial trust"), State->GetRelationshipValue(CompanionA, Player(), EDim::Trust), 10);
	TestEqual(TEXT("A initial friendship"), State->GetRelationshipValue(CompanionA, Player(), EDim::Friendship), 30);
	TestEqual(TEXT("B initial trust"), State->GetRelationshipValue(CompanionB, Player(), EDim::Trust), -5);
	TestTrue(TEXT("B initial flag"), State->HasRelationshipFlag(CompanionB, Player(), TEXT("KnownBeforeAcademy")));

	TArray<TTuple<FName, EDim, int32, int32>> ValueEvents;
	State->OnRelationshipValueChangedNative.AddLambda([&](FName Id, FName, EDim Dim, int32 Old, int32 New)
	{
		ValueEvents.Emplace(Id, Dim, Old, New);
	});
	TArray<TPair<FName, bool>> ThresholdEvents;
	State->OnRelationshipThresholdCrossedNative.AddLambda([&](FName, FName, FName Id, bool bReached)
	{
		ThresholdEvents.Emplace(Id, bReached);
	});

	// Independent dimensions.
	TestEqual(TEXT("Positive trust change"), State->ModifyRelationship(CompanionA, Player(), EDim::Trust, 15), 15);
	TestEqual(TEXT("Trust updated"), State->GetRelationshipValue(CompanionA, Player(), EDim::Trust), 25);
	TestEqual(TEXT("Friendship untouched"), State->GetRelationshipValue(CompanionA, Player(), EDim::Friendship), 30);
	TestEqual(TEXT("Affection untouched"), State->GetRelationshipValue(CompanionA, Player(), EDim::Affection), 0);
	TestEqual(TEXT("Tension untouched"), State->GetRelationshipValue(CompanionA, Player(), EDim::Tension), 0);
	TestEqual(TEXT("B untouched"), State->GetRelationshipValue(CompanionB, Player(), EDim::Trust), -5);
	TestEqual(TEXT("Negative friendship change"), State->ModifyRelationship(CompanionA, Player(), EDim::Friendship, -40), -40);
	TestEqual(TEXT("Friendship can go negative"), State->GetRelationshipValue(CompanionA, Player(), EDim::Friendship), -10);
	TestEqual(TEXT("Tension rises independently"), State->ModifyRelationship(CompanionA, Player(), EDim::Tension, 20), 20);
	TestEqual(TEXT("Trust unaffected by tension"), State->GetRelationshipValue(CompanionA, Player(), EDim::Trust), 25);

	// Bounds.
	TestEqual(TEXT("Affection clamps at character max"), State->ModifyRelationship(CompanionA, Player(), EDim::Affection, 100), 50);
	TestEqual(TEXT("Affection clamps at character min"), State->ModifyRelationship(CompanionA, Player(), EDim::Affection, -100), -50);
	TestEqual(TEXT("No-op at bound"), State->ModifyRelationship(CompanionA, Player(), EDim::Affection, -1), 0);
	TestEqual(TEXT("Zero delta"), State->ModifyRelationship(CompanionA, Player(), EDim::Trust, 0), 0);

	// Character-specific response scaling.
	TestEqual(TEXT("B gains trust at half rate"), State->ModifyRelationship(CompanionB, Player(), EDim::Trust, 10), 5);
	TestEqual(TEXT("B loses friendship at double rate"), State->ModifyRelationship(CompanionB, Player(), EDim::Friendship, -10), -20);
	TestEqual(TEXT("Rounding half away from zero"), State->ModifyRelationship(CompanionB, Player(), EDim::Trust, 3), 2);

	// Pair order and unknown relationships.
	TestEqual(TEXT("Reversed lookup"), State->GetRelationshipValue(Player(), CompanionA, EDim::Trust), 25);
	TestEqual(TEXT("Unknown pair ignored"), State->ModifyRelationship(CompanionD, Player(), EDim::Trust, 5), 0);

	TestEqual(TEXT("Value events"), ValueEvents.Num(), 8);
	if (ValueEvents.Num() > 0)
	{
		TestTrue(TEXT("First event character"), ValueEvents[0].Get<0>() == FName(CompanionA));
		TestEqual(TEXT("First event old value"), ValueEvents[0].Get<2>(), 10);
		TestEqual(TEXT("First event new value"), ValueEvents[0].Get<3>(), 25);
	}

	// Thresholds and persistent relationship flags.
	TestEqual(TEXT("No threshold yet"), ThresholdEvents.Num(), 0);
	State->ModifyRelationship(CompanionA, Player(), EDim::Trust, 20);
	TestEqual(TEXT("Threshold reached event"), ThresholdEvents.Num(), 1);
	TestTrue(TEXT("Threshold flag set"), State->HasRelationshipFlag(CompanionA, Player(), TEXT("TrustedOnce")));
	State->ModifyRelationship(CompanionA, Player(), EDim::Trust, -30);
	TestEqual(TEXT("Threshold lost event"), ThresholdEvents.Num(), 2);
	if (ThresholdEvents.Num() == 2)
	{
		TestFalse(TEXT("Lost flag on event"), ThresholdEvents[1].Value);
	}
	TestTrue(TEXT("Threshold flag persists"), State->HasRelationshipFlag(CompanionA, Player(), TEXT("TrustedOnce")));

	// Companion-to-companion relationships are independent of protagonist relationships.
	FSlayRelationshipProfile Pair = Profile(CompanionA, CompanionB);
	TestTrue(TEXT("Register NPC pair"), State->RegisterProfile(Pair));
	TestEqual(TEXT("NPC pair change"), State->ModifyRelationship(CompanionB, CompanionA, EDim::Tension, 5), 5);
	TestEqual(TEXT("NPC pair value"), State->GetRelationshipValue(CompanionA, CompanionB, EDim::Tension), 5);
	TestEqual(TEXT("Protagonist pair unaffected"), State->GetRelationshipValue(CompanionB, Player(), EDim::Tension), 0);
	return true;
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlayMultiCharacterChoiceTest, "SLAYCREW.Narrative.MultiCharacterChoice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlayMultiCharacterChoiceTest::RunTest(const FString& Parameters)
{
	USlayNarrativeState* State = NewState();
	for (const TCHAR* Id : { CompanionA, CompanionB, CompanionC, CompanionD })
	{
		State->RegisterProfile(Profile(Id));
	}

	TArray<FString> Order;
	State->OnRelationshipValueChangedNative.AddLambda([&](FName Id, FName, EDim, int32, int32)
	{
		Order.Add(TEXT("Value:") + Id.ToString());
	});
	State->OnFlagChangedNative.AddLambda([&](FName Id, FName, FName Flag, bool)
	{
		Order.Add(TEXT("Flag:") + Flag.ToString());
	});
	State->OnChoiceAppliedNative.AddLambda([&](FName, FName Option)
	{
		Order.Add(TEXT("Choice:") + Option.ToString());
	});

	FSlayNarrativeChoice Decision = Choice(TEXT("Test.Decision"), TEXT("OptionX"));
	Decision.RelationshipChanges.Add(Change(CompanionA, EDim::Trust, 10));
	Decision.RelationshipChanges.Add(Change(CompanionB, EDim::Friendship, -5));
	Decision.RelationshipChanges.Add(Change(CompanionC, EDim::Tension, 15));
	Decision.FlagsToSet.Add(TEXT("Test.GlobalFlag"));
	FSlayRelationshipFlagChange Unlock;
	Unlock.CharacterId = CompanionD;
	Unlock.Flag = TEXT("PersonalEventUnlocked");
	Decision.RelationshipFlagChanges.Add(Unlock);

	FSlayNarrativeEventDefinition PersonalEvent;
	PersonalEvent.EventId = TEXT("Test.PersonalEvent");
	PersonalEvent.Conditions.AllOf.Add(RelationshipFlagCondition(CompanionD, TEXT("PersonalEventUnlocked")));
	PersonalEvent.Conditions.AllOf.Add(ChapterCondition(ESlayComparison::GreaterOrEqual, 2));

	TestFalse(TEXT("Event locked before choice"), State->IsEventAvailable(PersonalEvent));
	TestTrue(TEXT("Choice available"), State->IsChoiceAvailable(Decision));
	TestTrue(TEXT("Choice applied"), State->ApplyChoice(Decision));

	TestEqual(TEXT("A trust up"), State->GetRelationshipValue(CompanionA, Player(), EDim::Trust), 10);
	TestEqual(TEXT("B friendship down"), State->GetRelationshipValue(CompanionB, Player(), EDim::Friendship), -5);
	TestEqual(TEXT("C tension up"), State->GetRelationshipValue(CompanionC, Player(), EDim::Tension), 15);
	TestEqual(TEXT("D values untouched"), State->GetRelationshipValue(CompanionD, Player(), EDim::Trust), 0);
	TestTrue(TEXT("Global flag set"), State->HasFlag(TEXT("Test.GlobalFlag")));
	FName Option;
	TestTrue(TEXT("Decision recorded"), State->GetDecisionOption(TEXT("Test.Decision"), Option));
	TestTrue(TEXT("Decision option"), Option == FName(TEXT("OptionX")));

	const TArray<FString> ExpectedOrder = {
		TEXT("Value:CompanionA"), TEXT("Value:CompanionB"), TEXT("Value:CompanionC"),
		TEXT("Flag:PersonalEventUnlocked"), TEXT("Flag:Test.GlobalFlag"), TEXT("Choice:OptionX") };
	TestTrue(TEXT("Deterministic notification order"), Order == ExpectedOrder);

	// Chapter progression gates the unlocked event.
	TestFalse(TEXT("Event still needs chapter"), State->IsEventAvailable(PersonalEvent));
	TestTrue(TEXT("Advance chapter"), State->AdvanceToChapter(2));
	TestFalse(TEXT("Chapters do not go back"), State->AdvanceToChapter(1));
	TestTrue(TEXT("Event unlocked"), State->IsEventAvailable(PersonalEvent));
	TestTrue(TEXT("Mark seen"), State->MarkEventSeen(PersonalEvent.EventId));
	TestFalse(TEXT("One-shot event consumed"), State->IsEventAvailable(PersonalEvent));
	PersonalEvent.bRepeatable = true;
	TestTrue(TEXT("Repeatable event still available"), State->IsEventAvailable(PersonalEvent));

	// Decisions are not repeatable by default.
	Order.Reset();
	TestFalse(TEXT("Decision already made"), State->IsChoiceAvailable(Decision));
	TestFalse(TEXT("Repeat apply rejected"), State->ApplyChoice(Decision));
	TestEqual(TEXT("Values unchanged after rejection"), State->GetRelationshipValue(CompanionA, Player(), EDim::Trust), 10);

	// Unmet requirements apply nothing.
	FSlayNarrativeChoice Gated = Choice(TEXT("Test.Gated"), TEXT("OptionY"));
	Gated.Requirements.AllOf.Add(FlagCondition(TEXT("Test.Missing")));
	Gated.RelationshipChanges.Add(Change(CompanionA, EDim::Trust, 50));
	Gated.FlagsToSet.Add(TEXT("Test.ShouldNotSet"));
	TestFalse(TEXT("Gated choice rejected"), State->ApplyChoice(Gated));
	TestFalse(TEXT("Gated flag not set"), State->HasFlag(TEXT("Test.ShouldNotSet")));
	TestFalse(TEXT("Gated decision not recorded"), State->HasMadeDecision(TEXT("Test.Gated")));
	TestEqual(TEXT("No notifications on rejection"), Order.Num(), 0);

	// Unknown characters make a choice unavailable instead of partially applying.
	FSlayNarrativeChoice Unknown = Choice(TEXT("Test.Unknown"), TEXT("OptionZ"));
	Unknown.RelationshipChanges.Add(Change(CompanionA, EDim::Trust, 5));
	Unknown.RelationshipChanges.Add(Change(TEXT("NotRegistered"), EDim::Trust, 5));
	TestFalse(TEXT("Unknown character rejected"), State->ApplyChoice(Unknown));
	TestEqual(TEXT("No partial change"), State->GetRelationshipValue(CompanionA, Player(), EDim::Trust), 10);

	// Listeners may re-enter safely and observe a consistent state.
	int32 ObservedTrust = INDEX_NONE;
	State->OnChoiceAppliedNative.AddLambda([&](FName DecisionId, FName)
	{
		if (DecisionId == TEXT("Test.Reentrant"))
		{
			ObservedTrust = State->GetRelationshipValue(CompanionA, Player(), EDim::Trust);
			State->SetFlag(TEXT("Test.FromListener"));
		}
	});
	FSlayNarrativeChoice Reentrant = Choice(TEXT("Test.Reentrant"), TEXT("Option"));
	Reentrant.RelationshipChanges.Add(Change(CompanionA, EDim::Trust, 1));
	TestTrue(TEXT("Re-entrant choice applied"), State->ApplyChoice(Reentrant));
	TestEqual(TEXT("Listener saw final value"), ObservedTrust, 11);
	TestTrue(TEXT("Listener mutation applied"), State->HasFlag(TEXT("Test.FromListener")));
	return true;
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlayNarrativeConditionsTest, "SLAYCREW.Narrative.Conditions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlayNarrativeConditionsTest::RunTest(const FString& Parameters)
{
	USlayNarrativeState* State = NewState();
	FSlayRelationshipProfile A = Profile(CompanionA);
	A.Dimensions.Trust.Initial = 20;
	State->RegisterProfile(A);

	FSlayNarrativeChoice Decision = Choice(TEXT("Test.Path"), TEXT("Left"));
	State->ApplyChoice(Decision);

	TestTrue(TEXT("Value >= passes"), State->EvaluateCondition(ValueCondition(CompanionA, EDim::Trust, ESlayComparison::GreaterOrEqual, 20)));
	TestFalse(TEXT("Value > fails"), State->EvaluateCondition(ValueCondition(CompanionA, EDim::Trust, ESlayComparison::Greater, 20)));
	TestTrue(TEXT("Value < passes"), State->EvaluateCondition(ValueCondition(CompanionA, EDim::Tension, ESlayComparison::Less, 1)));
	TestFalse(TEXT("Unknown relationship fails"), State->EvaluateCondition(ValueCondition(CompanionB, EDim::Trust, ESlayComparison::GreaterOrEqual, -1000)));

	TestTrue(TEXT("Any option"), State->EvaluateCondition(DecisionCondition(TEXT("Test.Path"))));
	TestTrue(TEXT("Specific option"), State->EvaluateCondition(DecisionCondition(TEXT("Test.Path"), TEXT("Left"))));
	TestFalse(TEXT("Other option"), State->EvaluateCondition(DecisionCondition(TEXT("Test.Path"), TEXT("Right"))));
	TestFalse(TEXT("Unmade decision"), State->EvaluateCondition(DecisionCondition(TEXT("Test.Other"))));

	TestFalse(TEXT("Flag missing"), State->EvaluateCondition(FlagCondition(TEXT("Test.Flag"))));
	TestTrue(TEXT("Negated missing flag"), State->EvaluateCondition(FlagCondition(TEXT("Test.Flag"), true)));
	TestTrue(TEXT("Set flag"), State->SetFlag(TEXT("Test.Flag")));
	TestFalse(TEXT("Setting again is no-op"), State->SetFlag(TEXT("Test.Flag")));
	TestTrue(TEXT("Flag present"), State->EvaluateCondition(FlagCondition(TEXT("Test.Flag"))));
	TestTrue(TEXT("Clear flag"), State->SetFlag(TEXT("Test.Flag"), false));
	TestFalse(TEXT("Flag cleared"), State->HasFlag(TEXT("Test.Flag")));

	TestTrue(TEXT("Chapter 0"), State->EvaluateCondition(ChapterCondition(ESlayComparison::Equal, 0)));
	State->AdvanceToChapter(3);
	TestTrue(TEXT("Chapter >= 3"), State->EvaluateCondition(ChapterCondition(ESlayComparison::GreaterOrEqual, 3)));
	TestFalse(TEXT("Chapter < 3"), State->EvaluateCondition(ChapterCondition(ESlayComparison::Less, 3)));

	FSlayNarrativeCondition Seen;
	Seen.Type = ESlayConditionType::EventSeen;
	Seen.Name = TEXT("Test.Event");
	TestFalse(TEXT("Event unseen"), State->EvaluateCondition(Seen));
	State->MarkEventSeen(Seen.Name);
	TestTrue(TEXT("Event seen"), State->EvaluateCondition(Seen));

	FSlayNarrativeCondition Route;
	Route.Type = ESlayConditionType::RomanceRouteAvailable;
	Route.CharacterId = CompanionA;
	TestFalse(TEXT("Route disabled by default"), State->EvaluateCondition(Route));

	FSlayNarrativeConditionSet Set;
	TestTrue(TEXT("Empty set passes"), State->EvaluateConditionSet(Set));
	Set.AnyOf.Add(FlagCondition(TEXT("Test.Missing")));
	TestFalse(TEXT("AnyOf with no match fails"), State->EvaluateConditionSet(Set));
	Set.AnyOf.Add(DecisionCondition(TEXT("Test.Path"), TEXT("Left")));
	TestTrue(TEXT("AnyOf with one match passes"), State->EvaluateConditionSet(Set));
	Set.AllOf.Add(ChapterCondition(ESlayComparison::GreaterOrEqual, 4));
	TestFalse(TEXT("AllOf failure dominates"), State->EvaluateConditionSet(Set));
	return true;
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlayRomanceTest, "SLAYCREW.Narrative.Romance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlayRomanceTest::RunTest(const FString& Parameters)
{
	USlayNarrativeState* State = NewState();

	// Validation: romance can never be unlocked by numbers alone.
	FSlayRelationshipProfile NumericOnly = Profile(CompanionD);
	NumericOnly.bRomanceRouteAvailable = true;
	NumericOnly.RomanceTransitions.Add(Rule(ERomance::None, ERomance::Interested,
		{ ValueCondition(CompanionD, EDim::Affection, ESlayComparison::GreaterOrEqual, 50) }));
	TArray<FString> Errors;
	TestFalse(TEXT("Numeric-only rule invalid"), USlayNarrativeState::ValidateProfile(NumericOnly, Errors));
	TestFalse(TEXT("Numeric-only rule rejected"), State->RegisterProfile(NumericOnly));
	NumericOnly.RomanceTransitions[0].Requirements.AllOf.Add(FlagCondition(TEXT("Test.Flag"), true));
	TestFalse(TEXT("Negated story condition does not count"), State->RegisterProfile(NumericOnly));

	// A: a full route with reconciliation.
	FSlayRelationshipProfile A = Profile(CompanionA);
	A.bRomanceRouteAvailable = true;
	A.RomanceTransitions.Add(Rule(ERomance::None, ERomance::Interested, {
		ValueCondition(CompanionA, EDim::Affection, ESlayComparison::GreaterOrEqual, 20),
		RelationshipFlagCondition(CompanionA, TEXT("InterestAcknowledged")) }));
	A.RomanceTransitions.Add(Rule(ERomance::Interested, ERomance::Committed, {
		ValueCondition(CompanionA, EDim::Trust, ESlayComparison::GreaterOrEqual, 30),
		RelationshipFlagCondition(CompanionA, TEXT("AgreedToCommit")) }));
	A.RomanceTransitions.Add(Rule(ERomance::Interested, ERomance::None));
	A.RomanceTransitions.Add(Rule(ERomance::Committed, ERomance::Strained));
	A.RomanceTransitions.Add(Rule(ERomance::Strained, ERomance::Committed, {
		RelationshipFlagCondition(CompanionA, TEXT("Reconciled")) }));
	A.RomanceTransitions.Add(Rule(ERomance::Strained, ERomance::Separated));

	// B: a different progression without reconciliation.
	FSlayRelationshipProfile B = Profile(CompanionB);
	B.bRomanceRouteAvailable = true;
	B.RomanceTransitions.Add(Rule(ERomance::None, ERomance::Committed, { DecisionCondition(TEXT("Test.BPromise"), TEXT("Accept")) }));
	B.RomanceTransitions.Add(Rule(ERomance::Committed, ERomance::Separated));

	// C: route unavailable (character boundary).
	FSlayRelationshipProfile C = Profile(CompanionC);
	C.RomanceTransitions = A.RomanceTransitions;

	TestTrue(TEXT("Register A"), State->RegisterProfile(A));
	TestTrue(TEXT("Register B"), State->RegisterProfile(B));
	TestTrue(TEXT("Register C"), State->RegisterProfile(C));

	TArray<TPair<ERomance, ERomance>> RomanceEvents;
	State->OnRomanceStateChangedNative.AddLambda([&](FName, FName, ERomance Old, ERomance New)
	{
		RomanceEvents.Emplace(Old, New);
	});

	// Maximum affection alone does not unlock romance.
	State->ModifyRelationship(CompanionA, Player(), EDim::Affection, 100);
	TestFalse(TEXT("Affection alone insufficient"), State->CanTransitionRomance(CompanionA, Player(), ERomance::Interested));
	// The story flag alone is not enough either when the profile also requires affection.
	State->SetRelationshipFlag(CompanionB, Player(), TEXT("InterestAcknowledged"));
	TestFalse(TEXT("Flag on another relationship irrelevant"), State->CanTransitionRomance(CompanionA, Player(), ERomance::Interested));
	TestFalse(TEXT("Cannot skip to Committed"), State->CanTransitionRomance(CompanionA, Player(), ERomance::Committed));

	// Romance progresses through an explicit choice once character conditions are met.
	FSlayNarrativeChoice Acknowledge = Choice(TEXT("Test.AAcknowledge"), TEXT("Yes"));
	FSlayRelationshipFlagChange AckFlag;
	AckFlag.CharacterId = CompanionA;
	AckFlag.Flag = TEXT("InterestAcknowledged");
	Acknowledge.RelationshipFlagChanges.Add(AckFlag);
	Acknowledge.RomanceTransitions.Add(RomanceRequest(CompanionA, ERomance::Interested));
	TestFalse(TEXT("Requests validate against pre-choice state"), State->ApplyChoice(Acknowledge));
	TestFalse(TEXT("Rejected choice set no flag"), State->HasRelationshipFlag(CompanionA, Player(), TEXT("InterestAcknowledged")));

	State->SetRelationshipFlag(CompanionA, Player(), TEXT("InterestAcknowledged"));
	TestTrue(TEXT("Choice with romance request applied"), State->ApplyChoice(Acknowledge));
	TestEqual(TEXT("A interested"), State->GetRomanceState(CompanionA, Player()), ERomance::Interested);

	// Friendship and romance develop independently.
	TestEqual(TEXT("Friendship unaffected by romance"), State->GetRelationshipValue(CompanionA, Player(), EDim::Friendship), 0);
	State->ModifyRelationship(CompanionA, Player(), EDim::Friendship, -30);
	TestEqual(TEXT("Romance unaffected by friendship"), State->GetRomanceState(CompanionA, Player()), ERomance::Interested);

	// Mutual agreement requires both trust and the companion-specific agreement flag.
	State->ModifyRelationship(CompanionA, Player(), EDim::Trust, 30);
	TestFalse(TEXT("Trust without agreement insufficient"), State->TransitionRomance(CompanionA, Player(), ERomance::Committed));
	State->SetRelationshipFlag(CompanionA, Player(), TEXT("AgreedToCommit"));
	TestTrue(TEXT("Committed"), State->TransitionRomance(CompanionA, Player(), ERomance::Committed));

	// Conflict and reconciliation.
	TestTrue(TEXT("Strained"), State->TransitionRomance(CompanionA, Player(), ERomance::Strained));
	TestFalse(TEXT("Reconcile needs story event"), State->TransitionRomance(CompanionA, Player(), ERomance::Committed));
	State->SetRelationshipFlag(CompanionA, Player(), TEXT("Reconciled"));
	TestTrue(TEXT("Reconciled"), State->TransitionRomance(CompanionA, Player(), ERomance::Committed));
	TestFalse(TEXT("No direct separation configured for A from Committed"), State->TransitionRomance(CompanionA, Player(), ERomance::Separated));
	State->TransitionRomance(CompanionA, Player(), ERomance::Strained);
	TestTrue(TEXT("Separated"), State->TransitionRomance(CompanionA, Player(), ERomance::Separated));
	TestFalse(TEXT("No rekindle rule for A"), State->CanTransitionRomance(CompanionA, Player(), ERomance::Interested));
	TestEqual(TEXT("Romance events"), RomanceEvents.Num(), 6);

	// B follows its own progression.
	TestFalse(TEXT("B has no Interested stage"), State->CanTransitionRomance(CompanionB, Player(), ERomance::Interested));
	FSlayNarrativeChoice Promise = Choice(TEXT("Test.BPromise"), TEXT("Accept"));
	State->ApplyChoice(Promise);
	TestTrue(TEXT("B committed via decision"), State->TransitionRomance(CompanionB, Player(), ERomance::Committed));
	TestFalse(TEXT("B has no Strained stage"), State->CanTransitionRomance(CompanionB, Player(), ERomance::Strained));

	// C's boundary holds regardless of values and flags.
	State->ModifyRelationship(CompanionC, Player(), EDim::Affection, 100);
	State->ModifyRelationship(CompanionC, Player(), EDim::Trust, 100);
	State->SetRelationshipFlag(CompanionC, Player(), TEXT("InterestAcknowledged"));
	TestFalse(TEXT("Unavailable route blocks romance"), State->CanTransitionRomance(CompanionC, Player(), ERomance::Interested));
	FSlayNarrativeChoice CChoice = Choice(TEXT("Test.CChoice"), TEXT("Option"));
	CChoice.RelationshipChanges.Add(Change(CompanionC, EDim::Friendship, 10));
	CChoice.RomanceTransitions.Add(RomanceRequest(CompanionC, ERomance::Interested));
	TestFalse(TEXT("Choice with blocked romance unavailable"), State->ApplyChoice(CChoice));
	TestEqual(TEXT("Blocked choice changed nothing"), State->GetRelationshipValue(CompanionC, Player(), EDim::Friendship), 0);
	return true;
}

// ---------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlayNarrativeSaveLoadTest, "SLAYCREW.Narrative.SaveLoad",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlayNarrativeSaveLoadTest::RunTest(const FString& Parameters)
{
	auto MakeProfiles = []()
	{
		TArray<FSlayRelationshipProfile> Result;
		FSlayRelationshipProfile A = Profile(CompanionA);
		A.bRomanceRouteAvailable = true;
		A.RomanceTransitions.Add(Rule(ERomance::None, ERomance::Interested, { FlagCondition(TEXT("Test.Romance")) }));
		Result.Add(A);
		FSlayRelationshipProfile B = Profile(CompanionB);
		B.Dimensions.Trust.Max = 40;
		Result.Add(B);
		Result.Add(Profile(CompanionA, CompanionB));
		return Result;
	};

	USlayNarrativeState* Source = NewState();
	for (const FSlayRelationshipProfile& P : MakeProfiles())
	{
		Source->RegisterProfile(P);
	}
	Source->ModifyRelationship(CompanionA, Player(), EDim::Trust, 12);
	Source->ModifyRelationship(CompanionA, Player(), EDim::Affection, 34);
	Source->ModifyRelationship(CompanionB, Player(), EDim::Tension, -7);
	Source->ModifyRelationship(CompanionB, CompanionA, EDim::Friendship, 9);
	Source->SetRelationshipFlag(CompanionA, Player(), TEXT("Test.RelFlag"));
	Source->SetFlag(TEXT("Test.Romance"));
	Source->TransitionRomance(CompanionA, Player(), ERomance::Interested);
	Source->ApplyChoice(Choice(TEXT("Test.First"), TEXT("A")));
	Source->ApplyChoice(Choice(TEXT("Test.Second"), TEXT("B")));
	Source->MarkEventSeen(TEXT("Test.Event"));
	Source->AdvanceToChapter(2);

	// Real serialization round trip through the engine SaveGame path.
	USlayNarrativeSaveGame* SaveGame = NewObject<USlayNarrativeSaveGame>();
	SaveGame->Narrative = Source->ExportSaveData();
	TestEqual(TEXT("Export version"), SaveGame->Narrative.Version, FSlayNarrativeSaveData::CurrentVersion);
	TArray<uint8> Bytes;
	TestTrue(TEXT("Serialize"), UGameplayStatics::SaveGameToMemory(SaveGame, Bytes));
	USlayNarrativeSaveGame* Loaded = Cast<USlayNarrativeSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("Deserialize"), Loaded))
	{
		return false;
	}

	// Restore into a later build that also contains a newly added companion relationship.
	USlayNarrativeState* Target = NewState();
	for (const FSlayRelationshipProfile& P : MakeProfiles())
	{
		Target->RegisterProfile(P);
	}
	FSlayRelationshipProfile NewCompanion = Profile(CompanionD);
	NewCompanion.Dimensions.Trust.Initial = 5;
	Target->RegisterProfile(NewCompanion);

	int32 RestoredEvents = 0;
	int32 ValueEvents = 0;
	Target->OnStateRestoredNative.AddLambda([&]() { ++RestoredEvents; });
	Target->OnRelationshipValueChangedNative.AddLambda([&](FName, FName, EDim, int32, int32) { ++ValueEvents; });

	TArray<FString> Messages;
	TestTrue(TEXT("Import succeeds"), Target->ImportSaveData(Loaded->Narrative, Messages));
	TestEqual(TEXT("No warnings"), Messages.Num(), 0);
	TestEqual(TEXT("Restored event"), RestoredEvents, 1);
	TestEqual(TEXT("No per-value events on restore"), ValueEvents, 0);
	TestEqual(TEXT("Trust restored"), Target->GetRelationshipValue(CompanionA, Player(), EDim::Trust), 12);
	TestEqual(TEXT("Affection restored"), Target->GetRelationshipValue(CompanionA, Player(), EDim::Affection), 34);
	TestEqual(TEXT("Tension restored"), Target->GetRelationshipValue(CompanionB, Player(), EDim::Tension), -7);
	TestEqual(TEXT("NPC pair restored"), Target->GetRelationshipValue(CompanionA, CompanionB, EDim::Friendship), 9);
	TestEqual(TEXT("Romance restored"), Target->GetRomanceState(CompanionA, Player()), ERomance::Interested);
	TestTrue(TEXT("Relationship flag restored"), Target->HasRelationshipFlag(CompanionA, Player(), TEXT("Test.RelFlag")));
	TestTrue(TEXT("Global flag restored"), Target->HasFlag(TEXT("Test.Romance")));
	FName Option;
	TestTrue(TEXT("Decision restored"), Target->GetDecisionOption(TEXT("Test.Second"), Option) && Option == TEXT("B"));
	TestTrue(TEXT("Seen event restored"), Target->HasSeenEvent(TEXT("Test.Event")));
	TestEqual(TEXT("Chapter restored"), Target->GetCurrentChapter(), 2);
	TestEqual(TEXT("New companion gets defaults"), Target->GetRelationshipValue(CompanionD, Player(), EDim::Trust), 5);
	TestFalse(TEXT("Restored decisions block repeats"), Target->IsChoiceAvailable(Choice(TEXT("Test.First"), TEXT("A"))));

	const FSlayNarrativeSaveData ReExport = Target->ExportSaveData();
	TestTrue(TEXT("Decision order preserved"), ReExport.Decisions.Num() == 2 && ReExport.Decisions[0].DecisionId == TEXT("Test.First"));

	// Validation: unsupported versions and malformed data leave state untouched.
	FSlayNarrativeSaveData Invalid = Loaded->Narrative;
	Invalid.Version = 0;
	TestFalse(TEXT("Unwritten version rejected"), Target->ImportSaveData(Invalid, Messages));
	Invalid.Version = FSlayNarrativeSaveData::CurrentVersion + 1;
	TestFalse(TEXT("Future version rejected"), Target->ImportSaveData(Invalid, Messages));
	Invalid = Loaded->Narrative;
	FSlayRelationshipSaveEntry Duplicate = Invalid.Relationships[0];
	Swap(Duplicate.CharacterId, Duplicate.OtherCharacterId);
	Invalid.Relationships.Add(Duplicate);
	TestFalse(TEXT("Duplicate relationship rejected"), Target->ImportSaveData(Invalid, Messages));
	Invalid = Loaded->Narrative;
	Invalid.Decisions.Add(Invalid.Decisions[0]);
	TestFalse(TEXT("Duplicate decision rejected"), Target->ImportSaveData(Invalid, Messages));
	Invalid = Loaded->Narrative;
	Invalid.Chapter = -1;
	TestFalse(TEXT("Negative chapter rejected"), Target->ImportSaveData(Invalid, Messages));
	TestEqual(TEXT("State untouched after rejections"), Target->GetRelationshipValue(CompanionA, Player(), EDim::Trust), 12);
	TestEqual(TEXT("Restored event count unchanged"), RestoredEvents, 1);

	// Sanitizing: out-of-range values clamp; unknown relationships are preserved for later content.
	FSlayNarrativeSaveData Edited = Loaded->Narrative;
	for (FSlayRelationshipSaveEntry& Entry : Edited.Relationships)
	{
		if (Entry.CharacterId == CompanionB && Entry.OtherCharacterId == Player())
		{
			Entry.Values.Trust = 999;
		}
	}
	FSlayRelationshipSaveEntry Future;
	Future.CharacterId = TEXT("FutureCompanion");
	Future.OtherCharacterId = Player();
	Future.Values.Trust = 3;
	Edited.Relationships.Add(Future);
	Messages.Reset();
	TestTrue(TEXT("Edited import succeeds"), Target->ImportSaveData(Edited, Messages));
	TestEqual(TEXT("Clamp reported"), Messages.Num(), 1);
	TestEqual(TEXT("Clamped to character max"), Target->GetRelationshipValue(CompanionB, Player(), EDim::Trust), 40);
	TestTrue(TEXT("Unknown relationship preserved"),
		Target->ExportSaveData().Relationships.ContainsByPredicate([](const FSlayRelationshipSaveEntry& Entry)
		{
			return Entry.CharacterId == TEXT("FutureCompanion") && Entry.Values.Trust == 3;
		}));

	// Restoring before profiles are registered sanitizes on registration (boundary enforcement).
	USlayNarrativeState* Early = NewState();
	FSlayNarrativeSaveData RomanceOnClosedRoute = Loaded->Narrative;
	for (FSlayRelationshipSaveEntry& Entry : RomanceOnClosedRoute.Relationships)
	{
		if (Entry.CharacterId == CompanionB && Entry.OtherCharacterId == Player())
		{
			Entry.RomanceState = ERomance::Committed;
		}
	}
	TestTrue(TEXT("Import without profiles"), Early->ImportSaveData(RomanceOnClosedRoute, Messages));
	for (const FSlayRelationshipProfile& P : MakeProfiles())
	{
		Early->RegisterProfile(P);
	}
	TestEqual(TEXT("Closed route romance reset"), Early->GetRomanceState(CompanionB, Player()), ERomance::None);
	TestEqual(TEXT("Open route romance kept"), Early->GetRomanceState(CompanionA, Player()), ERomance::Interested);

	// Reset keeps configuration but clears progress.
	Target->ResetState();
	TestEqual(TEXT("Reset chapter"), Target->GetCurrentChapter(), 0);
	TestFalse(TEXT("Reset flags"), Target->HasFlag(TEXT("Test.Romance")));
	TestEqual(TEXT("Reset values"), Target->GetRelationshipValue(CompanionA, Player(), EDim::Trust), 0);
	TestTrue(TEXT("Profiles kept"), Target->HasProfile(CompanionD, Player()));
	return true;
}

#endif
