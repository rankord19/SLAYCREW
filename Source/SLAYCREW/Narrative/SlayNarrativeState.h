#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Narrative/SlayNarrativeChoice.h"
#include "Narrative/SlayNarrativeSaveData.h"
#include "Narrative/SlayRelationshipTypes.h"
#include "SlayNarrativeState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FSlayRelationshipValueChanged,
	FName, CharacterId, FName, OtherCharacterId, ESlayRelationshipDimension, Dimension, int32, OldValue, int32, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FSlayRelationshipThresholdCrossed,
	FName, CharacterId, FName, OtherCharacterId, FName, ThresholdId, bool, bReached);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FSlayRomanceStateChanged,
	FName, CharacterId, FName, OtherCharacterId, ESlayRomanceState, OldState, ESlayRomanceState, NewState);
/** CharacterId/OtherCharacterId are None for global narrative flags. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FSlayNarrativeFlagChanged,
	FName, CharacterId, FName, OtherCharacterId, FName, Flag, bool, bSet);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSlayNarrativeChoiceApplied, FName, DecisionId, FName, OptionId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSlayChapterChanged, int32, OldChapter, int32, NewChapter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSlayNarrativeStateRestored);

DECLARE_MULTICAST_DELEGATE_FiveParams(FSlayRelationshipValueChangedNative,
	FName, FName, ESlayRelationshipDimension, int32, int32);
DECLARE_MULTICAST_DELEGATE_FourParams(FSlayRelationshipThresholdCrossedNative, FName, FName, FName, bool);
DECLARE_MULTICAST_DELEGATE_FourParams(FSlayRomanceStateChangedNative, FName, FName, ESlayRomanceState, ESlayRomanceState);
DECLARE_MULTICAST_DELEGATE_FourParams(FSlayNarrativeFlagChangedNative, FName, FName, FName, bool);
DECLARE_MULTICAST_DELEGATE_TwoParams(FSlayNarrativeChoiceAppliedNative, FName, FName);
DECLARE_MULTICAST_DELEGATE_TwoParams(FSlayChapterChangedNative, int32, int32);
DECLARE_MULTICAST_DELEGATE(FSlayNarrativeStateRestoredNative);

/**
 * Relationship and narrative state for one playthrough: relationship values and romance
 * states, persistent flags, recorded decisions, seen events and the current chapter.
 *
 * Plain UObject with no world dependency, so it is deterministic and unit-testable.
 * At runtime it is owned by USlayNarrativeSubsystem (game-instance lifetime, survives map travel).
 *
 * Notifications are deferred until an operation finishes, so listeners always observe a
 * consistent state and may safely call back into this object.
 *
 * Not replicated. Intended to be mutated only by the authoritative/local game instance.
 */
UCLASS(BlueprintType)
class SLAYCREW_API USlayNarrativeState : public UObject
{
	GENERATED_BODY()

public:
	/** Character ID used for the playable protagonist in relationship pairs. */
	UFUNCTION(BlueprintPure, Category="Narrative")
	static FName GetProtagonistId() { return FName(TEXT("Protagonist")); }

	// ---- Configuration -------------------------------------------------------------------

	/** Appends human-readable problems to OutErrors. Returns true if the profile is valid. */
	static bool ValidateProfile(const FSlayRelationshipProfile& Profile, TArray<FString>& OutErrors);

	/** Adds a profile and creates its relationship (or sanitizes one restored from a save). */
	UFUNCTION(BlueprintCallable, Category="Narrative|Relationships")
	bool RegisterProfile(const FSlayRelationshipProfile& Profile);

	UFUNCTION(BlueprintPure, Category="Narrative|Relationships")
	bool HasProfile(FName CharacterId, FName OtherCharacterId) const { return FindProfile(CharacterId, OtherCharacterId) != nullptr; }

	const FSlayRelationshipProfile* FindProfile(FName CharacterId, FName OtherCharacterId) const;

	// ---- Relationships -------------------------------------------------------------------

	/** Returns 0 for unknown relationships. Pair order does not matter. */
	UFUNCTION(BlueprintPure, Category="Narrative|Relationships")
	int32 GetRelationshipValue(FName CharacterId, FName OtherCharacterId, ESlayRelationshipDimension Dimension) const;

	UFUNCTION(BlueprintPure, Category="Narrative|Relationships")
	bool GetRelationship(FName CharacterId, FName OtherCharacterId, FSlayRelationshipRecord& OutRecord) const;

	UFUNCTION(BlueprintPure, Category="Narrative|Relationships")
	ESlayRomanceState GetRomanceState(FName CharacterId, FName OtherCharacterId) const;

	UFUNCTION(BlueprintPure, Category="Narrative|Relationships")
	bool HasRelationshipFlag(FName CharacterId, FName OtherCharacterId, FName Flag) const;

	UFUNCTION(BlueprintPure, Category="Narrative|Relationships")
	TArray<FSlayRelationshipRecord> GetAllRelationships() const { return Records; }

	/**
	 * Applies Delta to one dimension, scaled by the profile's Gain/LossPercent and clamped to
	 * its bounds. Returns the change actually applied. Requires a registered profile.
	 */
	UFUNCTION(BlueprintCallable, Category="Narrative|Relationships")
	int32 ModifyRelationship(FName CharacterId, FName OtherCharacterId, ESlayRelationshipDimension Dimension, int32 Delta);

	/** Returns true if the flag changed. */
	UFUNCTION(BlueprintCallable, Category="Narrative|Relationships")
	bool SetRelationshipFlag(FName CharacterId, FName OtherCharacterId, FName Flag, bool bSet = true);

	/** True if the profile allows romance and has a rule from the current state whose requirements pass. */
	UFUNCTION(BlueprintPure, Category="Narrative|Romance")
	bool CanTransitionRomance(FName CharacterId, FName OtherCharacterId, ESlayRomanceState NewState) const;

	/**
	 * Performs an allowed romance transition. Story content should normally request this through
	 * an FSlayNarrativeChoice so the transition is always the result of an explicit player decision.
	 */
	UFUNCTION(BlueprintCallable, Category="Narrative|Romance")
	bool TransitionRomance(FName CharacterId, FName OtherCharacterId, ESlayRomanceState NewState);

	// ---- Flags, decisions, chapters ------------------------------------------------------

	UFUNCTION(BlueprintPure, Category="Narrative|Flags")
	bool HasFlag(FName Flag) const { return NarrativeFlags.Contains(Flag); }

	/** Returns true if the flag changed. */
	UFUNCTION(BlueprintCallable, Category="Narrative|Flags")
	bool SetFlag(FName Flag, bool bSet = true);

	UFUNCTION(BlueprintPure, Category="Narrative|Decisions")
	bool HasMadeDecision(FName DecisionId) const { return FindDecision(DecisionId) != nullptr; }

	UFUNCTION(BlueprintPure, Category="Narrative|Decisions")
	bool GetDecisionOption(FName DecisionId, FName& OutOptionId) const;

	UFUNCTION(BlueprintPure, Category="Narrative|Chapters")
	int32 GetCurrentChapter() const { return CurrentChapter; }

	/** Chapters only move forward. Returns false if NewChapter is not greater than the current one. */
	UFUNCTION(BlueprintCallable, Category="Narrative|Chapters")
	bool AdvanceToChapter(int32 NewChapter);

	// ---- Conditions, choices, events -----------------------------------------------------

	UFUNCTION(BlueprintPure, Category="Narrative|Conditions")
	bool EvaluateCondition(const FSlayNarrativeCondition& Condition) const;

	UFUNCTION(BlueprintPure, Category="Narrative|Conditions")
	bool EvaluateConditionSet(const FSlayNarrativeConditionSet& Conditions) const;

	UFUNCTION(BlueprintPure, Category="Narrative|Choices")
	bool IsChoiceAvailable(const FSlayNarrativeChoice& Choice) const;

	/** Applies every consequence of the choice atomically. Returns false (and changes nothing) if unavailable. */
	UFUNCTION(BlueprintCallable, Category="Narrative|Choices")
	bool ApplyChoice(const FSlayNarrativeChoice& Choice);

	UFUNCTION(BlueprintPure, Category="Narrative|Events")
	bool IsEventAvailable(const FSlayNarrativeEventDefinition& Event) const;

	/** Returns true if the event was not already marked. */
	UFUNCTION(BlueprintCallable, Category="Narrative|Events")
	bool MarkEventSeen(FName EventId);

	UFUNCTION(BlueprintPure, Category="Narrative|Events")
	bool HasSeenEvent(FName EventId) const { return SeenEvents.Contains(EventId); }

	// ---- Persistence ---------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category="Narrative|Save")
	FSlayNarrativeSaveData ExportSaveData() const;

	/**
	 * Validates and restores a snapshot. On error, returns false and leaves the current state
	 * untouched. Out-of-bounds values are clamped and reported in OutMessages as warnings.
	 * Relationships without a saved entry (e.g. added in a later update) get profile defaults.
	 */
	UFUNCTION(BlueprintCallable, Category="Narrative|Save")
	bool ImportSaveData(const FSlayNarrativeSaveData& Data, TArray<FString>& OutMessages);

	/** Clears all progress (flags, decisions, events, chapter) and resets relationships to profile defaults. */
	UFUNCTION(BlueprintCallable, Category="Narrative|Save")
	void ResetState();

	// ---- Events --------------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category="Narrative|Events")
	FSlayRelationshipValueChanged OnRelationshipValueChanged;

	UPROPERTY(BlueprintAssignable, Category="Narrative|Events")
	FSlayRelationshipThresholdCrossed OnRelationshipThresholdCrossed;

	UPROPERTY(BlueprintAssignable, Category="Narrative|Events")
	FSlayRomanceStateChanged OnRomanceStateChanged;

	UPROPERTY(BlueprintAssignable, Category="Narrative|Events")
	FSlayNarrativeFlagChanged OnFlagChanged;

	UPROPERTY(BlueprintAssignable, Category="Narrative|Events")
	FSlayNarrativeChoiceApplied OnChoiceApplied;

	UPROPERTY(BlueprintAssignable, Category="Narrative|Events")
	FSlayChapterChanged OnChapterChanged;

	UPROPERTY(BlueprintAssignable, Category="Narrative|Events")
	FSlayNarrativeStateRestored OnStateRestored;

	FSlayRelationshipValueChangedNative OnRelationshipValueChangedNative;
	FSlayRelationshipThresholdCrossedNative OnRelationshipThresholdCrossedNative;
	FSlayRomanceStateChangedNative OnRomanceStateChangedNative;
	FSlayNarrativeFlagChangedNative OnFlagChangedNative;
	FSlayNarrativeChoiceAppliedNative OnChoiceAppliedNative;
	FSlayChapterChangedNative OnChapterChangedNative;
	FSlayNarrativeStateRestoredNative OnStateRestoredNative;

private:
	UPROPERTY()
	TArray<FSlayRelationshipProfile> Profiles;

	UPROPERTY()
	TArray<FSlayRelationshipRecord> Records;

	UPROPERTY()
	TSet<FName> NarrativeFlags;

	UPROPERTY()
	TArray<FSlayDecisionRecord> Decisions;

	UPROPERTY()
	TSet<FName> SeenEvents;

	int32 CurrentChapter = 0;

	int32 BatchDepth = 0;
	bool bFlushing = false;
	TArray<TFunction<void()>> PendingNotifications;

	int32 FindRecordIndex(FName CharacterId, FName OtherCharacterId) const;
	const FSlayDecisionRecord* FindDecision(FName DecisionId) const;
	static FSlayRelationshipRecord MakeInitialRecord(const FSlayRelationshipProfile& Profile);
	static void SanitizeRecord(FSlayRelationshipRecord& Record, const FSlayRelationshipProfile& Profile, TArray<FString>* OutMessages);

	bool SetRelationshipFlagInternal(int32 RecordIndex, FName Flag, bool bSet);
	void SetRomanceStateInternal(int32 RecordIndex, ESlayRomanceState NewState);
	void RecordDecision(FName DecisionId, FName OptionId);

	void Notify(TFunction<void()>&& Notification);
	void EndBatch();
	void FlushNotifications();
};
