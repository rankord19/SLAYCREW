#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlayComboComponent.generated.h"
UENUM(BlueprintType)
enum class ESlayOpeningDirection : uint8 { Right, Left, Up, Down };
/** Authority-side finite combo. The legal follow-up window is after full recovery. */
UCLASS(ClassGroup=(SLAYCREW), meta=(BlueprintSpawnableComponent))
class SLAYCREW_API USlayComboComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 USlayComboComponent();
 virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;
 UFUNCTION(BlueprintCallable, Category="Combo") bool Open(ESlayOpeningDirection Direction);
 UFUNCTION(BlueprintCallable, Category="Combo") void Queue();
 UFUNCTION(BlueprintCallable, Category="Combo") void Cancel();
 static ESlayOpeningDirection Recognize(FVector2D Gesture, float Threshold);
 UPROPERTY(EditDefaultsOnly, Category="Combo", meta=(ClampMin="1")) int32 MaxStrikes = 3;
 UPROPERTY(EditDefaultsOnly, Category="Combo", meta=(ClampMin="0.01")) float BufferDuration = .3f;
 UPROPERTY(EditDefaultsOnly, Category="Combo", meta=(ClampMin="0.01")) float FollowupWindow = .2f;
 UPROPERTY(EditDefaultsOnly, Category="Combo", meta=(ClampMin="0")) float GestureThreshold = 12.f;
 UPROPERTY(BlueprintReadOnly, Category="Combo") int32 Strike = 0;
 UPROPERTY(BlueprintReadOnly, Category="Combo") ESlayOpeningDirection Opening = ESlayOpeningDirection::Right;
 UPROPERTY(BlueprintReadOnly, Category="Combo") bool bRejected = false;
protected:
 virtual void BeginPlay() override;
private:
 UPROPERTY() TObjectPtr<class USlayMeleeComponent> Melee;
 UPROPERTY() TObjectPtr<class USlayHealthComponent> Health;
 double BufferedUntil = -1;
 double WindowEnds = -1;
};
