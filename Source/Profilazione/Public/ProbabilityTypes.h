#pragma once

#include "CoreMinimal.h"
#include "ProbabilityTypes.generated.h"

USTRUCT(BlueprintType)
struct PROFILAZIONE_API FProbabilityPlayRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probability|Giocata")
    FString IdTipoGiocata;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probability|Utente")
    FString NomeUtente;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probability|Utente")
    FString CognomeUtente;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probability|Utente")
    FString DataNascitaUtente;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probability|Utente")
    FString CodiceFiscale;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probability|Utente")
    FString ProvinciaNascita;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probability|Utente")
    FString CittaNascita;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probability|Utente")
    FString ProvinciaResidenza;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probability|Utente")
    FString CittaResidenza;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probability|Documento")
    FString Documento;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probability|Documento")
    FString RilasciatoDa;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probability|Documento")
    FString NumeroDocumento;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probability|Contatti")
    FString Email;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probability|Contatti")
    FString Telefono;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probability|Privacy")
    FString Informativa;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Probability|Giocata")
    FString Scontrino;
};

USTRUCT(BlueprintType)
struct PROFILAZIONE_API FProbabilityPlayResponse
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    bool bGiocataValida = false;

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    bool bHaVinto = false;

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    int32 IdUtente = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    int32 IdGiocata = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    int32 IdLiberatoria = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    FString IdVincita;

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    FString IdPremio;

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    FString NomePremio;

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    FString ValorePremio;

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    FString Esito;

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    FString MotivazioneEsito;

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    FString ProbabilitaCalcolata;

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    FString LiberatoriaRichiesta;

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    FString Messaggio;

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    FString RawJson;

    UPROPERTY(BlueprintReadOnly, Category = "Probability|Response")
    int32 HttpStatusCode = 0;
};