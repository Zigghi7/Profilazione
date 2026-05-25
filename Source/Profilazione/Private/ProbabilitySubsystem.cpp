#include "ProbabilitySubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace ProbabilityJson
{
    static FString GetStringSafe(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
    {
        if (!JsonObject.IsValid() || !JsonObject->HasField(FieldName))
        {
            return TEXT("");
        }

        FString StringValue;
        if (JsonObject->TryGetStringField(FieldName, StringValue))
        {
            return StringValue;
        }

        double NumberValue = 0.0;
        if (JsonObject->TryGetNumberField(FieldName, NumberValue))
        {
            const int64 RoundedValue = FMath::RoundToInt64(NumberValue);

            if (FMath::IsNearlyEqual(NumberValue, static_cast<double>(RoundedValue)))
            {
                return LexToString(RoundedValue);
            }

            return FString::SanitizeFloat(NumberValue);
        }

        bool BoolValue = false;
        if (JsonObject->TryGetBoolField(FieldName, BoolValue))
        {
            return BoolValue ? TEXT("true") : TEXT("false");
        }

        return TEXT("");
    }

    static int32 GetIntSafe(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
    {
        if (!JsonObject.IsValid() || !JsonObject->HasField(FieldName))
        {
            return 0;
        }

        double NumberValue = 0.0;
        if (JsonObject->TryGetNumberField(FieldName, NumberValue))
        {
            return FMath::RoundToInt(NumberValue);
        }

        FString StringValue;
        if (JsonObject->TryGetStringField(FieldName, StringValue))
        {
            return FCString::Atoi(*StringValue);
        }

        bool BoolValue = false;
        if (JsonObject->TryGetBoolField(FieldName, BoolValue))
        {
            return BoolValue ? 1 : 0;
        }

        return 0;
    }

    static bool GetBoolSafe(const TSharedPtr<FJsonObject>& JsonObject, const FString& FieldName)
    {
        if (!JsonObject.IsValid() || !JsonObject->HasField(FieldName))
        {
            return false;
        }

        bool BoolValue = false;
        if (JsonObject->TryGetBoolField(FieldName, BoolValue))
        {
            return BoolValue;
        }

        double NumberValue = 0.0;
        if (JsonObject->TryGetNumberField(FieldName, NumberValue))
        {
            return !FMath::IsNearlyZero(NumberValue);
        }

        FString StringValue;
        if (JsonObject->TryGetStringField(FieldName, StringValue))
        {
            FString Normalized = StringValue.ToLower();
            Normalized.TrimStartAndEndInline();

            if (Normalized == TEXT("true"))
            {
                return true;
            }

            if (Normalized == TEXT("1"))
            {
                return true;
            }

            if (Normalized == TEXT("si"))
            {
                return true;
            }

            if (Normalized == TEXT("yes"))
            {
                return true;
            }

            if (Normalized == TEXT("ok"))
            {
                return true;
            }

            if (Normalized == TEXT("vinto"))
            {
                return true;
            }

            if (Normalized == TEXT("winner"))
            {
                return true;
            }

            return false;
        }

        return false;
    }
}

void UProbabilitySubsystem::ConfigureProbabilitySystem(const FString& InBaseUrl, const FString& InApiKey)
{
    BaseUrl = InBaseUrl;
    ApiKey = InApiKey;

    if (!BaseUrl.EndsWith(TEXT("/")))
    {
        BaseUrl += TEXT("/");
    }

    UE_LOG(LogTemp, Warning, TEXT("ProbabilitySubsystem configurato."));
    UE_LOG(LogTemp, Warning, TEXT("BaseUrl: %s"), *BaseUrl);
}

FString UProbabilitySubsystem::GetProbabilityDebugInfo() const
{
    return FString::Printf(
        TEXT("ProbabilitySubsystem attivo | BaseUrl: %s | ApiKey presente: %s"),
        *BaseUrl,
        ApiKey.IsEmpty() ? TEXT("NO") : TEXT("SI")
    );
}

void UProbabilitySubsystem::SubmitPlay(const FProbabilityPlayRequest& PlayRequest)
{
    if (BaseUrl.IsEmpty())
    {
        OnProbabilityPlayCompleted.Broadcast(
            MakeErrorResponse(
                TEXT("BaseUrl non configurato. Chiama prima ConfigureProbabilitySystem."),
                TEXT("CLIENT_BASE_URL_EMPTY")
            )
        );

        return;
    }

    const FString Url = BaseUrl + TEXT("play.php");
    const FString JsonBody = BuildPlayRequestJson(PlayRequest);

    UE_LOG(LogTemp, Warning, TEXT("Probability SubmitPlay URL: %s"), *Url);
    UE_LOG(LogTemp, Warning, TEXT("Probability SubmitPlay JSON: %s"), *JsonBody);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();

    HttpRequest->SetURL(Url);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));

    if (!ApiKey.IsEmpty())
    {
        HttpRequest->SetHeader(TEXT("X-API-KEY"), ApiKey);
    }

    HttpRequest->SetContentAsString(JsonBody);
    HttpRequest->OnProcessRequestComplete().BindUObject(this, &UProbabilitySubsystem::OnSubmitPlayResponse);

    const bool bRequestStarted = HttpRequest->ProcessRequest();

    if (!bRequestStarted)
    {
        OnProbabilityPlayCompleted.Broadcast(
            MakeErrorResponse(
                TEXT("Impossibile avviare la richiesta HTTP."),
                TEXT("HTTP_REQUEST_NOT_STARTED")
            )
        );
    }
}

FString UProbabilitySubsystem::BuildPlayRequestJson(const FProbabilityPlayRequest& PlayRequest) const
{
    TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

    JsonObject->SetStringField(TEXT("idTipoGiocata"), PlayRequest.IdTipoGiocata);

    JsonObject->SetStringField(TEXT("nomeUtente"), PlayRequest.NomeUtente);
    JsonObject->SetStringField(TEXT("cognomeUtente"), PlayRequest.CognomeUtente);
    JsonObject->SetStringField(TEXT("dataNascitaUtente"), PlayRequest.DataNascitaUtente);
    JsonObject->SetStringField(TEXT("codiceFiscale"), PlayRequest.CodiceFiscale);

    JsonObject->SetStringField(TEXT("provinciaNascita"), PlayRequest.ProvinciaNascita);
    JsonObject->SetStringField(TEXT("cittaNascita"), PlayRequest.CittaNascita);
    JsonObject->SetStringField(TEXT("provinciaResidenza"), PlayRequest.ProvinciaResidenza);
    JsonObject->SetStringField(TEXT("cittaResidenza"), PlayRequest.CittaResidenza);

    JsonObject->SetStringField(TEXT("documento"), PlayRequest.Documento);
    JsonObject->SetStringField(TEXT("rilasciatoDa"), PlayRequest.RilasciatoDa);
    JsonObject->SetStringField(TEXT("numeroDocumento"), PlayRequest.NumeroDocumento);

    JsonObject->SetStringField(TEXT("email"), PlayRequest.Email);
    JsonObject->SetStringField(TEXT("telefono"), PlayRequest.Telefono);
    JsonObject->SetStringField(TEXT("informativa"), PlayRequest.Informativa);

    JsonObject->SetStringField(TEXT("scontrino"), PlayRequest.Scontrino);

    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

    return OutputString;
}

void UProbabilitySubsystem::OnSubmitPlayResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
    if (!bWasSuccessful || !Response.IsValid())
    {
        OnProbabilityPlayCompleted.Broadcast(
            MakeErrorResponse(
                TEXT("Connessione al server non riuscita."),
                TEXT("HTTP_CONNECTION_FAILED")
            )
        );

        return;
    }

    const int32 ResponseCode = Response->GetResponseCode();
    const FString ResponseContent = Response->GetContentAsString();

    UE_LOG(LogTemp, Warning, TEXT("Probability HTTP Status Code: %d"), ResponseCode);
    UE_LOG(LogTemp, Warning, TEXT("Probability Response Content: %s"), *ResponseContent);

    if (ResponseCode < 200 || ResponseCode >= 300)
    {
        OnProbabilityPlayCompleted.Broadcast(
            MakeErrorResponse(
                FString::Printf(TEXT("Errore HTTP %d ricevuto dal server."), ResponseCode),
                TEXT("HTTP_STATUS_ERROR"),
                ResponseCode,
                ResponseContent
            )
        );

        return;
    }

    const FProbabilityPlayResponse ParsedResponse = ParsePlayResponseJson(ResponseContent, ResponseCode);
    OnProbabilityPlayCompleted.Broadcast(ParsedResponse);
}

FProbabilityPlayResponse UProbabilitySubsystem::ParsePlayResponseJson(const FString& JsonString, int32 HttpStatusCode) const
{
    TSharedPtr<FJsonObject> JsonObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
    {
        return MakeErrorResponse(
            TEXT("Risposta ricevuta dal server, ma il JSON non e valido."),
            TEXT("INVALID_JSON_RESPONSE"),
            HttpStatusCode,
            JsonString
        );
    }

    FProbabilityPlayResponse Result;

    Result.RawJson = JsonString;
    Result.HttpStatusCode = HttpStatusCode;

    Result.bSuccess = ProbabilityJson::GetBoolSafe(JsonObject, TEXT("success"));
    Result.bGiocataValida = ProbabilityJson::GetBoolSafe(JsonObject, TEXT("giocataValida"));
    Result.bHaVinto = ProbabilityJson::GetBoolSafe(JsonObject, TEXT("haVinto"));

    Result.IdUtente = ProbabilityJson::GetIntSafe(JsonObject, TEXT("idUtente"));
    Result.IdGiocata = ProbabilityJson::GetIntSafe(JsonObject, TEXT("idGiocata"));
    Result.IdLiberatoria = ProbabilityJson::GetIntSafe(JsonObject, TEXT("idLiberatoria"));

    Result.IdVincita = ProbabilityJson::GetStringSafe(JsonObject, TEXT("idVincita"));
    Result.IdPremio = ProbabilityJson::GetStringSafe(JsonObject, TEXT("idPremio"));
    Result.NomePremio = ProbabilityJson::GetStringSafe(JsonObject, TEXT("nomePremio"));
    Result.ValorePremio = ProbabilityJson::GetStringSafe(JsonObject, TEXT("valorePremio"));

    Result.Esito = ProbabilityJson::GetStringSafe(JsonObject, TEXT("esito"));
    Result.MotivazioneEsito = ProbabilityJson::GetStringSafe(JsonObject, TEXT("motivazioneEsito"));
    Result.ProbabilitaCalcolata = ProbabilityJson::GetStringSafe(JsonObject, TEXT("probabilitaCalcolata"));
    Result.LiberatoriaRichiesta = ProbabilityJson::GetStringSafe(JsonObject, TEXT("liberatoriaRichiesta"));
    Result.Messaggio = ProbabilityJson::GetStringSafe(JsonObject, TEXT("messaggio"));

    return Result;
}

FProbabilityPlayResponse UProbabilitySubsystem::MakeErrorResponse(
    const FString& Message,
    const FString& Reason,
    int32 HttpStatusCode,
    const FString& RawJson
) const
{
    FProbabilityPlayResponse Result;

    Result.bSuccess = false;
    Result.bGiocataValida = false;
    Result.bHaVinto = false;

    Result.Esito = TEXT("ERRORE");
    Result.MotivazioneEsito = Reason;
    Result.Messaggio = Message;

    Result.HttpStatusCode = HttpStatusCode;
    Result.RawJson = RawJson;

    return Result;
}