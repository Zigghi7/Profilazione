#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Http.h"
#include "ProbabilityTypes.h"
#include "ProbabilitySubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FOnProbabilityPlayCompleted,
    const FProbabilityPlayResponse&,
    Response
);

UCLASS(BlueprintType)
class PROFILAZIONE_API UProbabilitySubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable, Category = "Probability")
    void ConfigureProbabilitySystem(const FString& InBaseUrl, const FString& InApiKey);

    UFUNCTION(BlueprintCallable, Category = "Probability")
    FString GetProbabilityDebugInfo() const;

    UFUNCTION(BlueprintCallable, Category = "Probability")
    void SubmitPlay(const FProbabilityPlayRequest& PlayRequest);

    UPROPERTY(BlueprintAssignable, Category = "Probability")
    FOnProbabilityPlayCompleted OnProbabilityPlayCompleted;

private:

    UPROPERTY()
    FString BaseUrl;

    UPROPERTY()
    FString ApiKey;

private:

    FString BuildPlayRequestJson(const FProbabilityPlayRequest& PlayRequest) const;

    void OnSubmitPlayResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

    FProbabilityPlayResponse ParsePlayResponseJson(const FString& JsonString, int32 HttpStatusCode) const;

    FProbabilityPlayResponse MakeErrorResponse(
        const FString& Message,
        const FString& Reason,
        int32 HttpStatusCode = 0,
        const FString& RawJson = TEXT("")
    ) const;
};