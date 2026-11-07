#include "Narrative/SlayNarrativeState.h"

#include "Misc/ScopeExit.h"
#include "SLAYCREW.h"

namespace SlayNarrativeStatePrivate
{
	constexpr ESlayRelationshipDimension AllDimensions[] = {
		ESlayRelationshipDimension::Trust,
		ESlayRelationshipDimension::Friendship,
		ESlayRelationshipDimension::Affection,
		ESlayRelationshipDimension::Tension
	};

	FString DimensionName(ESlayRelationshipDimension Dimension)
	{
		return StaticEnum<ESlayRelationshipDimension>()->GetNameStringByValue(static_cast<int64>(Dimension));
	}

	/** Integer percent scaling, rounded half away from zero. Deterministic on every platform. */
	int32 ScaleDelta(int32 Delta, int32 Percent)
	{
		const int64 Scaled = static_cast<int64>(Delta) * FMath::Max(0, Percent);
		const int64 Rounded = Scaled >= 0 ? (Scaled + 50) / 100 : (Scaled - 50) / 100;
		return static_cast<int32>(FMath::Clamp<int64>(Rounded, MIN_int32, MAX_int32));
	}

	bool IsRomanceProgressionState(ESlayRomanceState State)
	{
		return State == ESlayRomanceState::Interested || State == ESlayRomanceState::Committed;
	}

	/** A rule must depend on something beyond numbers: a choice, a flag, a seen event or a custom rule. */
	bool HasStoryRequirement(const FSlayNarrativeConditionSet& Set)
	{
		for (const TObjectPtr<USlayNarrativeCustomCondition>& Custom : Set.Custom)
		{
			if (Custom)
			{
				return true;
			}
		}
		return Set.AllOf.ContainsByPredicate([](const FSlayNarrativeCondition& Condition)
		{
			return !Condition.bNegate && SlayNarrative::IsStoryConditionType(Condition.Type);
		});
	}

	TArray<FName> SortedNames(const TSet<FName>& Names)
	{
		TArray<FName> Result = Names.Array();
		Result.Sort(FNameLexicalLess());
		return Result;
	}
}

using namespace SlayNarrativeStatePrivate;

// ---- Configuration ---------------------------------------------------------------------------

bool USlayNarrativeState::ValidateProfile(const FSlayRelationshipProfile& Profile, TArray<FString>& OutErrors)
{
	const int32 InitialErrorCount = OutErrors.Num();
	const FString Label = FString::Printf(TEXT("Profile %s/%s"),
		*Profile.CharacterId.ToString(), *Profile.OtherCharacterId.ToString());

	if (Profile.CharacterId.IsNone() || Profile.OtherCharacterId.IsNone())
	{
		OutErrors.Add(Label + TEXT(": character IDs must not be None."));
	}
	else if (Profile.CharacterId == Profile.OtherCharacterId)
	{
		OutErrors.Add(Label + TEXT(": a character cannot have a relationship with itself."));
	}

	for (const ESlayRelationshipDimension Dimension : AllDimensions)
	{
		const FSlayDimensionConfig& Config = Profile.Dimensions.Get(Dimension);
		if (Config.Min > Config.Max)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: %s Min is greater than Max."), *Label, *DimensionName(Dimension)));
		}
		else if (Config.Initial < Config.Min || Config.Initial > Config.Max)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: %s Initial is outside [Min, Max]."), *Label, *DimensionName(Dimension)));
		}
		if (Config.GainPercent < 0 || Config.LossPercent < 0)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: %s percent scaling must not be negative."), *Label, *DimensionName(Dimension)));
		}
	}

	TSet<FName> ThresholdIds;
	for (const FSlayRelationshipThreshold& Threshold : Profile.Thresholds)
	{
		bool bDuplicate = false;
		ThresholdIds.Add(Threshold.ThresholdId, &bDuplicate);
		if (Threshold.ThresholdId.IsNone() || bDuplicate)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: threshold IDs must be unique and not None ('%s')."),
				*Label, *Threshold.ThresholdId.ToString()));
		}
	}

	TSet<int32> TransitionKeys;
	for (const FSlayRomanceTransitionRule& Rule : Profile.RomanceTransitions)
	{
		bool bDuplicate = false;
		TransitionKeys.Add((static_cast<int32>(Rule.From) << 8) | static_cast<int32>(Rule.To), &bDuplicate);
		if (Rule.From == Rule.To)
		{
			OutErrors.Add(Label + TEXT(": romance rule From and To must differ."));
		}
		else if (bDuplicate)
		{
			OutErrors.Add(Label + TEXT(": duplicate romance rule."));
		}
		if (IsRomanceProgressionState(Rule.To) && !HasStoryRequirement(Rule.Requirements))
		{
			OutErrors.Add(Label + TEXT(": romance rules entering Interested or Committed must require a flag, decision, ")
				TEXT("seen event or custom condition in AllOf/Custom; numeric values alone cannot unlock romance."));
		}
	}

	return OutErrors.Num() == InitialErrorCount;
}

bool USlayNarrativeState::RegisterProfile(const FSlayRelationshipProfile& Profile)
{
	TArray<FString> Errors;
	if (!ValidateProfile(Profile, Errors))
	{
		for (const FString& Error : Errors)
		{
			UE_LOG(LogSLAYCREW, Warning, TEXT("RegisterProfile: %s"), *Error);
		}
		return false;
	}
	if (HasProfile(Profile.CharacterId, Profile.OtherCharacterId))
	{
		UE_LOG(LogSLAYCREW, Warning, TEXT("RegisterProfile: %s/%s is already registered."),
			*Profile.CharacterId.ToString(), *Profile.OtherCharacterId.ToString());
		return false;
	}

	Profiles.Add(Profile);
	const int32 Index = FindRecordIndex(Profile.CharacterId, Profile.OtherCharacterId);
	if (Index == INDEX_NONE)
	{
		Records.Add(MakeInitialRecord(Profile));
	}
	else
	{
		SanitizeRecord(Records[Index], Profile, nullptr);
	}
	return true;
}

const FSlayRelationshipProfile* USlayNarrativeState::FindProfile(FName CharacterId, FName OtherCharacterId) const
{
	return Profiles.FindByPredicate([&](const FSlayRelationshipProfile& Profile)
	{
		return Profile.Matches(CharacterId, OtherCharacterId);
	});
}

int32 USlayNarrativeState::FindRecordIndex(FName CharacterId, FName OtherCharacterId) const
{
	return Records.IndexOfByPredicate([&](const FSlayRelationshipRecord& Record)
	{
		return Record.Matches(CharacterId, OtherCharacterId);
	});
}

FSlayRelationshipRecord USlayNarrativeState::MakeInitialRecord(const FSlayRelationshipProfile& Profile)
{
	FSlayRelationshipRecord Record;
	Record.CharacterId = Profile.CharacterId;
	Record.OtherCharacterId = Profile.OtherCharacterId;
	for (const ESlayRelationshipDimension Dimension : AllDimensions)
	{
		Record.Values.Set(Dimension, Profile.Dimensions.Get(Dimension).Initial);
	}
	for (const FName Flag : Profile.InitialFlags)
	{
		if (!Flag.IsNone())
		{
			Record.Flags.Add(Flag);
		}
	}
	return Record;
}

void USlayNarrativeState::SanitizeRecord(FSlayRelationshipRecord& Record, const FSlayRelationshipProfile& Profile,
	TArray<FString>* OutMessages)
{
	for (const ESlayRelationshipDimension Dimension : AllDimensions)
	{
		const FSlayDimensionConfig& Config = Profile.Dimensions.Get(Dimension);
		const int32 Value = Record.Values.Get(Dimension);
		const int32 Clamped = FMath::Clamp(Value, Config.Min, Config.Max);
		if (Clamped != Value)
		{
			Record.Values.Set(Dimension, Clamped);
			const FString Message = FString::Printf(TEXT("%s/%s %s clamped from %d to %d."),
				*Record.CharacterId.ToString(), *Record.OtherCharacterId.ToString(), *DimensionName(Dimension), Value, Clamped);
			UE_LOG(LogSLAYCREW, Warning, TEXT("%s"), *Message);
			if (OutMessages)
			{
				OutMessages->Add(Message);
			}
		}
	}
	if (!Profile.bRomanceRouteAvailable && Record.RomanceState != ESlayRomanceState::None)
	{
		Record.RomanceState = ESlayRomanceState::None;
		const FString Message = FString::Printf(TEXT("%s/%s romance reset: route is not available."),
			*Record.CharacterId.ToString(), *Record.OtherCharacterId.ToString());
		UE_LOG(LogSLAYCREW, Warning, TEXT("%s"), *Message);
		if (OutMessages)
		{
			OutMessages->Add(Message);
		}
	}
}

// ---- Relationships ---------------------------------------------------------------------------

int32 USlayNarrativeState::GetRelationshipValue(FName CharacterId, FName OtherCharacterId,
	ESlayRelationshipDimension Dimension) const
{
	const int32 Index = FindRecordIndex(CharacterId, OtherCharacterId);
	return Index != INDEX_NONE ? Records[Index].Values.Get(Dimension) : 0;
}

bool USlayNarrativeState::GetRelationship(FName CharacterId, FName OtherCharacterId, FSlayRelationshipRecord& OutRecord) const
{
	const int32 Index = FindRecordIndex(CharacterId, OtherCharacterId);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	OutRecord = Records[Index];
	return true;
}

ESlayRomanceState USlayNarrativeState::GetRomanceState(FName CharacterId, FName OtherCharacterId) const
{
	const int32 Index = FindRecordIndex(CharacterId, OtherCharacterId);
	return Index != INDEX_NONE ? Records[Index].RomanceState : ESlayRomanceState::None;
}

bool USlayNarrativeState::HasRelationshipFlag(FName CharacterId, FName OtherCharacterId, FName Flag) const
{
	const int32 Index = FindRecordIndex(CharacterId, OtherCharacterId);
	return Index != INDEX_NONE && Records[Index].Flags.Contains(Flag);
}

int32 USlayNarrativeState::ModifyRelationship(FName CharacterId, FName OtherCharacterId,
	ESlayRelationshipDimension Dimension, int32 Delta)
{
	const FSlayRelationshipProfile* Profile = FindProfile(CharacterId, OtherCharacterId);
	const int32 Index = FindRecordIndex(CharacterId, OtherCharacterId);
	if (!Profile || Index == INDEX_NONE)
	{
		UE_LOG(LogSLAYCREW, Warning, TEXT("ModifyRelationship: no profile for %s/%s."),
			*CharacterId.ToString(), *OtherCharacterId.ToString());
		return 0;
	}
	if (Delta == 0)
	{
		return 0;
	}

	const FSlayDimensionConfig& Config = Profile->Dimensions.Get(Dimension);
	const int32 Scaled = ScaleDelta(Delta, Delta > 0 ? Config.GainPercent : Config.LossPercent);
	FSlayRelationshipRecord& Record = Records[Index];
	const int32 OldValue = Record.Values.Get(Dimension);
	const int32 NewValue = static_cast<int32>(FMath::Clamp<int64>(
		static_cast<int64>(OldValue) + Scaled, Config.Min, Config.Max));
	if (NewValue == OldValue)
	{
		return 0;
	}

	++BatchDepth;
	ON_SCOPE_EXIT { EndBatch(); };

	Record.Values.Set(Dimension, NewValue);
	const FName A = Record.CharacterId;
	const FName B = Record.OtherCharacterId;
	Notify([this, A, B, Dimension, OldValue, NewValue]()
	{
		OnRelationshipValueChangedNative.Broadcast(A, B, Dimension, OldValue, NewValue);
		OnRelationshipValueChanged.Broadcast(A, B, Dimension, OldValue, NewValue);
	});

	for (const FSlayRelationshipThreshold& Threshold : Profile->Thresholds)
	{
		if (Threshold.Dimension != Dimension)
		{
			continue;
		}
		const bool bWasReached = SlayNarrative::Compare(OldValue, Threshold.Comparison, Threshold.Value);
		const bool bIsReached = SlayNarrative::Compare(NewValue, Threshold.Comparison, Threshold.Value);
		if (bWasReached == bIsReached)
		{
			continue;
		}
		const FName ThresholdId = Threshold.ThresholdId;
		Notify([this, A, B, ThresholdId, bIsReached]()
		{
			OnRelationshipThresholdCrossedNative.Broadcast(A, B, ThresholdId, bIsReached);
			OnRelationshipThresholdCrossed.Broadcast(A, B, ThresholdId, bIsReached);
		});
		if (bIsReached && !Threshold.FlagOnReached.IsNone())
		{
			SetRelationshipFlagInternal(Index, Threshold.FlagOnReached, true);
		}
	}
	return NewValue - OldValue;
}

bool USlayNarrativeState::SetRelationshipFlag(FName CharacterId, FName OtherCharacterId, FName Flag, bool bSet)
{
	const int32 Index = FindRecordIndex(CharacterId, OtherCharacterId);
	if (Flag.IsNone() || Index == INDEX_NONE)
	{
		return false;
	}
	return SetRelationshipFlagInternal(Index, Flag, bSet);
}

bool USlayNarrativeState::SetRelationshipFlagInternal(int32 RecordIndex, FName Flag, bool bSet)
{
	FSlayRelationshipRecord& Record = Records[RecordIndex];
	const bool bChanged = bSet ? !Record.Flags.Contains(Flag) : Record.Flags.Contains(Flag);
	if (!bChanged)
	{
		return false;
	}
	if (bSet)
	{
		Record.Flags.Add(Flag);
	}
	else
	{
		Record.Flags.Remove(Flag);
	}
	const FName A = Record.CharacterId;
	const FName B = Record.OtherCharacterId;
	Notify([this, A, B, Flag, bSet]()
	{
		OnFlagChangedNative.Broadcast(A, B, Flag, bSet);
		OnFlagChanged.Broadcast(A, B, Flag, bSet);
	});
	return true;
}

bool USlayNarrativeState::CanTransitionRomance(FName CharacterId, FName OtherCharacterId, ESlayRomanceState NewState) const
{
	const FSlayRelationshipProfile* Profile = FindProfile(CharacterId, OtherCharacterId);
	const int32 Index = FindRecordIndex(CharacterId, OtherCharacterId);
	if (!Profile || Index == INDEX_NONE || !Profile->bRomanceRouteAvailable)
	{
		return false;
	}
	const ESlayRomanceState Current = Records[Index].RomanceState;
	if (Current == NewState)
	{
		return false;
	}
	const FSlayRomanceTransitionRule* Rule = Profile->RomanceTransitions.FindByPredicate(
		[Current, NewState](const FSlayRomanceTransitionRule& Candidate)
		{
			return Candidate.From == Current && Candidate.To == NewState;
		});
	return Rule && EvaluateConditionSet(Rule->Requirements);
}

bool USlayNarrativeState::TransitionRomance(FName CharacterId, FName OtherCharacterId, ESlayRomanceState NewState)
{
	if (!CanTransitionRomance(CharacterId, OtherCharacterId, NewState))
	{
		return false;
	}
	SetRomanceStateInternal(FindRecordIndex(CharacterId, OtherCharacterId), NewState);
	return true;
}

void USlayNarrativeState::SetRomanceStateInternal(int32 RecordIndex, ESlayRomanceState NewState)
{
	FSlayRelationshipRecord& Record = Records[RecordIndex];
	const ESlayRomanceState OldState = Record.RomanceState;
	if (OldState == NewState)
	{
		return;
	}
	Record.RomanceState = NewState;
	const FName A = Record.CharacterId;
	const FName B = Record.OtherCharacterId;
	Notify([this, A, B, OldState, NewState]()
	{
		OnRomanceStateChangedNative.Broadcast(A, B, OldState, NewState);
		OnRomanceStateChanged.Broadcast(A, B, OldState, NewState);
	});
}

// ---- Flags, decisions, chapters --------------------------------------------------------------

bool USlayNarrativeState::SetFlag(FName Flag, bool bSet)
{
	if (Flag.IsNone())
	{
		return false;
	}
	const bool bChanged = bSet ? !NarrativeFlags.Contains(Flag) : NarrativeFlags.Remove(Flag) > 0;
	if (!bChanged)
	{
		return false;
	}
	if (bSet)
	{
		NarrativeFlags.Add(Flag);
	}
	Notify([this, Flag, bSet]()
	{
		OnFlagChangedNative.Broadcast(NAME_None, NAME_None, Flag, bSet);
		OnFlagChanged.Broadcast(NAME_None, NAME_None, Flag, bSet);
	});
	return true;
}

const FSlayDecisionRecord* USlayNarrativeState::FindDecision(FName DecisionId) const
{
	return Decisions.FindByPredicate([DecisionId](const FSlayDecisionRecord& Record)
	{
		return Record.DecisionId == DecisionId;
	});
}

bool USlayNarrativeState::GetDecisionOption(FName DecisionId, FName& OutOptionId) const
{
	if (const FSlayDecisionRecord* Record = FindDecision(DecisionId))
	{
		OutOptionId = Record->OptionId;
		return true;
	}
	OutOptionId = NAME_None;
	return false;
}

void USlayNarrativeState::RecordDecision(FName DecisionId, FName OptionId)
{
	if (FSlayDecisionRecord* Existing = const_cast<FSlayDecisionRecord*>(FindDecision(DecisionId)))
	{
		Existing->OptionId = OptionId;
		return;
	}
	FSlayDecisionRecord& Record = Decisions.AddDefaulted_GetRef();
	Record.DecisionId = DecisionId;
	Record.OptionId = OptionId;
}

bool USlayNarrativeState::AdvanceToChapter(int32 NewChapter)
{
	if (NewChapter <= CurrentChapter)
	{
		return false;
	}
	const int32 OldChapter = CurrentChapter;
	CurrentChapter = NewChapter;
	Notify([this, OldChapter, NewChapter]()
	{
		OnChapterChangedNative.Broadcast(OldChapter, NewChapter);
		OnChapterChanged.Broadcast(OldChapter, NewChapter);
	});
	return true;
}

// ---- Conditions, choices, events -------------------------------------------------------------

bool USlayNarrativeState::EvaluateCondition(const FSlayNarrativeCondition& Condition) const
{
	bool bResult = false;
	switch (Condition.Type)
	{
	case ESlayConditionType::RelationshipValue:
		bResult = FindRecordIndex(Condition.CharacterId, Condition.OtherCharacterId) != INDEX_NONE
			&& SlayNarrative::Compare(GetRelationshipValue(Condition.CharacterId, Condition.OtherCharacterId, Condition.Dimension),
				Condition.Comparison, Condition.Value);
		break;
	case ESlayConditionType::RomanceState:
		bResult = FindRecordIndex(Condition.CharacterId, Condition.OtherCharacterId) != INDEX_NONE
			&& GetRomanceState(Condition.CharacterId, Condition.OtherCharacterId) == Condition.RomanceState;
		break;
	case ESlayConditionType::RomanceRouteAvailable:
	{
		const FSlayRelationshipProfile* Profile = FindProfile(Condition.CharacterId, Condition.OtherCharacterId);
		bResult = Profile && Profile->bRomanceRouteAvailable;
		break;
	}
	case ESlayConditionType::NarrativeFlag:
		bResult = HasFlag(Condition.Name);
		break;
	case ESlayConditionType::RelationshipFlag:
		bResult = HasRelationshipFlag(Condition.CharacterId, Condition.OtherCharacterId, Condition.Name);
		break;
	case ESlayConditionType::Decision:
	{
		FName Option;
		bResult = GetDecisionOption(Condition.Name, Option) && (Condition.Option.IsNone() || Option == Condition.Option);
		break;
	}
	case ESlayConditionType::EventSeen:
		bResult = HasSeenEvent(Condition.Name);
		break;
	case ESlayConditionType::Chapter:
		bResult = SlayNarrative::Compare(CurrentChapter, Condition.Comparison, Condition.Value);
		break;
	}
	return bResult != Condition.bNegate;
}

bool USlayNarrativeState::EvaluateConditionSet(const FSlayNarrativeConditionSet& Conditions) const
{
	for (const FSlayNarrativeCondition& Condition : Conditions.AllOf)
	{
		if (!EvaluateCondition(Condition))
		{
			return false;
		}
	}
	if (Conditions.AnyOf.Num() > 0 && !Conditions.AnyOf.ContainsByPredicate(
		[this](const FSlayNarrativeCondition& Condition) { return EvaluateCondition(Condition); }))
	{
		return false;
	}
	for (const TObjectPtr<USlayNarrativeCustomCondition>& Custom : Conditions.Custom)
	{
		if (Custom && !Custom->IsConditionMet(this))
		{
			return false;
		}
	}
	return true;
}

bool USlayNarrativeState::IsChoiceAvailable(const FSlayNarrativeChoice& Choice) const
{
	if (Choice.DecisionId.IsNone() || Choice.OptionId.IsNone())
	{
		return false;
	}
	if (!Choice.bAllowRepeat && HasMadeDecision(Choice.DecisionId))
	{
		return false;
	}
	for (const FSlayRelationshipChange& Change : Choice.RelationshipChanges)
	{
		if (!HasProfile(Change.CharacterId, Change.OtherCharacterId))
		{
			return false;
		}
	}
	for (const FSlayRelationshipFlagChange& Change : Choice.RelationshipFlagChanges)
	{
		if (Change.Flag.IsNone() || !HasProfile(Change.CharacterId, Change.OtherCharacterId))
		{
			return false;
		}
	}
	TSet<int32> RomanceRecords;
	for (const FSlayRomanceTransitionRequest& Request : Choice.RomanceTransitions)
	{
		const int32 Index = FindRecordIndex(Request.CharacterId, Request.OtherCharacterId);
		bool bDuplicate = false;
		RomanceRecords.Add(Index, &bDuplicate);
		if (Index == INDEX_NONE || bDuplicate
			|| !CanTransitionRomance(Request.CharacterId, Request.OtherCharacterId, Request.NewState))
		{
			return false;
		}
	}
	return EvaluateConditionSet(Choice.Requirements);
}

bool USlayNarrativeState::ApplyChoice(const FSlayNarrativeChoice& Choice)
{
	if (!IsChoiceAvailable(Choice))
	{
		return false;
	}

	// Resolve romance targets against the pre-choice state, as validated above.
	TArray<TPair<int32, ESlayRomanceState>> RomanceChanges;
	for (const FSlayRomanceTransitionRequest& Request : Choice.RomanceTransitions)
	{
		RomanceChanges.Emplace(FindRecordIndex(Request.CharacterId, Request.OtherCharacterId), Request.NewState);
	}

	++BatchDepth;
	ON_SCOPE_EXIT { EndBatch(); };

	RecordDecision(Choice.DecisionId, Choice.OptionId);
	for (const FSlayRelationshipChange& Change : Choice.RelationshipChanges)
	{
		ModifyRelationship(Change.CharacterId, Change.OtherCharacterId, Change.Dimension, Change.Delta);
	}
	for (const FSlayRelationshipFlagChange& Change : Choice.RelationshipFlagChanges)
	{
		SetRelationshipFlag(Change.CharacterId, Change.OtherCharacterId, Change.Flag, Change.bSet);
	}
	for (const FName Flag : Choice.FlagsToSet)
	{
		SetFlag(Flag, true);
	}
	for (const FName Flag : Choice.FlagsToClear)
	{
		SetFlag(Flag, false);
	}
	for (const TPair<int32, ESlayRomanceState>& Change : RomanceChanges)
	{
		SetRomanceStateInternal(Change.Key, Change.Value);
	}

	const FName DecisionId = Choice.DecisionId;
	const FName OptionId = Choice.OptionId;
	Notify([this, DecisionId, OptionId]()
	{
		OnChoiceAppliedNative.Broadcast(DecisionId, OptionId);
		OnChoiceApplied.Broadcast(DecisionId, OptionId);
	});
	return true;
}

bool USlayNarrativeState::IsEventAvailable(const FSlayNarrativeEventDefinition& Event) const
{
	return !Event.EventId.IsNone()
		&& (Event.bRepeatable || !HasSeenEvent(Event.EventId))
		&& EvaluateConditionSet(Event.Conditions);
}

bool USlayNarrativeState::MarkEventSeen(FName EventId)
{
	if (EventId.IsNone())
	{
		return false;
	}
	bool bAlreadySeen = false;
	SeenEvents.Add(EventId, &bAlreadySeen);
	return !bAlreadySeen;
}

// ---- Persistence -----------------------------------------------------------------------------

FSlayNarrativeSaveData USlayNarrativeState::ExportSaveData() const
{
	FSlayNarrativeSaveData Data;
	Data.Version = FSlayNarrativeSaveData::CurrentVersion;
	Data.Chapter = CurrentChapter;
	for (const FSlayRelationshipRecord& Record : Records)
	{
		FSlayRelationshipSaveEntry& Entry = Data.Relationships.AddDefaulted_GetRef();
		Entry.CharacterId = Record.CharacterId;
		Entry.OtherCharacterId = Record.OtherCharacterId;
		Entry.Values = Record.Values;
		Entry.RomanceState = Record.RomanceState;
		Entry.Flags = SortedNames(Record.Flags);
	}
	Data.Flags = SortedNames(NarrativeFlags);
	Data.Decisions = Decisions;
	Data.SeenEvents = SortedNames(SeenEvents);
	return Data;
}

bool USlayNarrativeState::ImportSaveData(const FSlayNarrativeSaveData& Data, TArray<FString>& OutMessages)
{
	auto Fail = [&OutMessages](const FString& Message)
	{
		UE_LOG(LogSLAYCREW, Warning, TEXT("ImportSaveData: %s"), *Message);
		OutMessages.Add(Message);
		return false;
	};

	if (Data.Version < 1 || Data.Version > FSlayNarrativeSaveData::CurrentVersion)
	{
		return Fail(FString::Printf(TEXT("unsupported version %d (supported 1..%d)."),
			Data.Version, FSlayNarrativeSaveData::CurrentVersion));
	}
	// Future versions: migrate older layouts here before validation.
	if (Data.Chapter < 0)
	{
		return Fail(TEXT("chapter must not be negative."));
	}

	const UEnum* RomanceEnum = StaticEnum<ESlayRomanceState>();
	TArray<FSlayRelationshipRecord> NewRecords;
	for (const FSlayRelationshipSaveEntry& Entry : Data.Relationships)
	{
		if (Entry.CharacterId.IsNone() || Entry.OtherCharacterId.IsNone() || Entry.CharacterId == Entry.OtherCharacterId)
		{
			return Fail(TEXT("relationship entry has invalid character IDs."));
		}
		if (NewRecords.ContainsByPredicate([&Entry](const FSlayRelationshipRecord& Record)
			{
				return Record.Matches(Entry.CharacterId, Entry.OtherCharacterId);
			}))
		{
			return Fail(FString::Printf(TEXT("duplicate relationship %s/%s."),
				*Entry.CharacterId.ToString(), *Entry.OtherCharacterId.ToString()));
		}
		if (!RomanceEnum->IsValidEnumValue(static_cast<int64>(Entry.RomanceState)))
		{
			return Fail(TEXT("relationship entry has an invalid romance state."));
		}

		FSlayRelationshipRecord& Record = NewRecords.AddDefaulted_GetRef();
		Record.CharacterId = Entry.CharacterId;
		Record.OtherCharacterId = Entry.OtherCharacterId;
		Record.Values = Entry.Values;
		Record.RomanceState = Entry.RomanceState;
		for (const FName Flag : Entry.Flags)
		{
			if (!Flag.IsNone())
			{
				Record.Flags.Add(Flag);
			}
		}
		// Entries without a profile are kept as-is so content added later can pick them up.
		if (const FSlayRelationshipProfile* Profile = FindProfile(Entry.CharacterId, Entry.OtherCharacterId))
		{
			SanitizeRecord(Record, *Profile, &OutMessages);
		}
	}

	TArray<FSlayDecisionRecord> NewDecisions;
	for (const FSlayDecisionRecord& Decision : Data.Decisions)
	{
		if (Decision.DecisionId.IsNone() || Decision.OptionId.IsNone())
		{
			return Fail(TEXT("decision record has a None ID."));
		}
		if (NewDecisions.ContainsByPredicate([&Decision](const FSlayDecisionRecord& Existing)
			{
				return Existing.DecisionId == Decision.DecisionId;
			}))
		{
			return Fail(FString::Printf(TEXT("duplicate decision %s."), *Decision.DecisionId.ToString()));
		}
		NewDecisions.Add(Decision);
	}

	TSet<FName> NewFlags;
	for (const FName Flag : Data.Flags)
	{
		if (!Flag.IsNone())
		{
			NewFlags.Add(Flag);
		}
	}
	TSet<FName> NewSeenEvents;
	for (const FName EventId : Data.SeenEvents)
	{
		if (!EventId.IsNone())
		{
			NewSeenEvents.Add(EventId);
		}
	}

	// Relationships introduced after the save was written start from their profile defaults.
	for (const FSlayRelationshipProfile& Profile : Profiles)
	{
		if (!NewRecords.ContainsByPredicate([&Profile](const FSlayRelationshipRecord& Record)
			{
				return Record.Matches(Profile.CharacterId, Profile.OtherCharacterId);
			}))
		{
			NewRecords.Add(MakeInitialRecord(Profile));
		}
	}

	Records = MoveTemp(NewRecords);
	Decisions = MoveTemp(NewDecisions);
	NarrativeFlags = MoveTemp(NewFlags);
	SeenEvents = MoveTemp(NewSeenEvents);
	CurrentChapter = Data.Chapter;

	Notify([this]()
	{
		OnStateRestoredNative.Broadcast();
		OnStateRestored.Broadcast();
	});
	return true;
}

void USlayNarrativeState::ResetState()
{
	Records.Reset();
	for (const FSlayRelationshipProfile& Profile : Profiles)
	{
		Records.Add(MakeInitialRecord(Profile));
	}
	NarrativeFlags.Reset();
	Decisions.Reset();
	SeenEvents.Reset();
	CurrentChapter = 0;
	Notify([this]()
	{
		OnStateRestoredNative.Broadcast();
		OnStateRestored.Broadcast();
	});
}

// ---- Notifications ---------------------------------------------------------------------------

void USlayNarrativeState::Notify(TFunction<void()>&& Notification)
{
	PendingNotifications.Add(MoveTemp(Notification));
	if (BatchDepth == 0)
	{
		FlushNotifications();
	}
}

void USlayNarrativeState::EndBatch()
{
	if (--BatchDepth == 0)
	{
		FlushNotifications();
	}
}

void USlayNarrativeState::FlushNotifications()
{
	if (bFlushing)
	{
		return; // The outer flush loop picks up notifications queued by listeners.
	}
	TGuardValue<bool> FlushGuard(bFlushing, true);
	while (PendingNotifications.Num() > 0)
	{
		TArray<TFunction<void()>> Batch = MoveTemp(PendingNotifications);
		PendingNotifications.Reset();
		for (TFunction<void()>& Notification : Batch)
		{
			Notification();
		}
	}
}
