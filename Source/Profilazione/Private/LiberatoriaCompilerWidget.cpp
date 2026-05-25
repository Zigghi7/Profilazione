#include "LiberatoriaCompilerWidget.h"
#include "SignaturePadWidget.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "GenericPlatform/GenericPlatformHttp.h"

#include "Json.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "LiberatoriaCompilerWidget"

void ULiberatoriaRowButton::HandleClicked()
{
	if (OnClickedWithIndex.IsBound())
	{
		OnClickedWithIndex.Execute(ItemIndex);
	}
}

ULiberatoriaCompilerWidget::ULiberatoriaCompilerWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULiberatoriaCompilerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Invia)
	{
		Button_Invia->OnClicked.RemoveDynamic(this, &ULiberatoriaCompilerWidget::HandleSubmitClicked);
		Button_Invia->OnClicked.AddDynamic(this, &ULiberatoriaCompilerWidget::HandleSubmitClicked);
	}

	if (Button_Indietro)
	{
		Button_Indietro->OnClicked.RemoveDynamic(this, &ULiberatoriaCompilerWidget::HandleBackClicked);
		Button_Indietro->OnClicked.AddDynamic(this, &ULiberatoriaCompilerWidget::HandleBackClicked);
	}

	if (Button_AggiornaLista)
	{
		Button_AggiornaLista->OnClicked.RemoveDynamic(this, &ULiberatoriaCompilerWidget::HandleAggiornaListaClicked);
		Button_AggiornaLista->OnClicked.AddDynamic(this, &ULiberatoriaCompilerWidget::HandleAggiornaListaClicked);
	}

	if (ComboBoxString_TipoDocumento)
	{
		ComboBoxString_TipoDocumento->ClearOptions();
		ComboBoxString_TipoDocumento->AddOption(TEXT("Carta identita"));
		ComboBoxString_TipoDocumento->AddOption(TEXT("Passaporto"));
		ComboBoxString_TipoDocumento->AddOption(TEXT("Patente"));
		ComboBoxString_TipoDocumento->AddOption(TEXT("Altro"));
		ComboBoxString_TipoDocumento->SetSelectedOption(TEXT("Carta identita"));
	}

	ClearSelectedLiberatoria();
	ShowList();
	LoadPendingLiberatorie();
}

void ULiberatoriaCompilerWidget::LoadPendingLiberatorie()
{
	if (bIsLoadingList)
	{
		return;
	}

	bIsLoadingList = true;

	ClearSelectedLiberatoria();
	PendingLiberatorie.Empty();
	RenderLiberatorieList();
	SetStatus(TEXT("Caricamento liberatorie da compilare..."));

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();

	const FString Url = GetNoCacheEndpointUrl(ListEndpoint);

	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json, text/plain, */*"));

	Request->SetHeader(TEXT("Cache-Control"), TEXT("no-cache, no-store, must-revalidate, max-age=0"));
	Request->SetHeader(TEXT("Pragma"), TEXT("no-cache"));
	Request->SetHeader(TEXT("Expires"), TEXT("0"));

	UE_LOG(LogTemp, Warning, TEXT("LiberatoriaCompilerWidget: GET lista no-cache: %s"), *Url);

	TWeakObjectPtr<ULiberatoriaCompilerWidget> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis](FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			const int32 HttpStatusCode = ResponsePtr.IsValid() ? ResponsePtr->GetResponseCode() : 0;
			const FString ResponseContent = ResponsePtr.IsValid() ? ResponsePtr->GetContentAsString() : TEXT("");

			UE_LOG(LogTemp, Warning, TEXT("LiberatoriaCompilerWidget: risposta lista [%d]: %s"), HttpStatusCode, *ResponseContent);

			WeakThis->HandleListResponse(ResponseContent, HttpStatusCode, bWasSuccessful);
		});

	Request->ProcessRequest();
}

void ULiberatoriaCompilerWidget::HandleListResponse(const FString& ResponseContent, int32 HttpStatusCode, bool bWasSuccessful)
{
	bIsLoadingList = false;

	ClearSelectedLiberatoria();
	PendingLiberatorie.Empty();

	if (!bWasSuccessful || HttpStatusCode < 200 || HttpStatusCode >= 300)
	{
		RenderLiberatorieList();
		SetStatus(TEXT("Errore HTTP durante il caricamento liberatorie. Codice: ") + FString::FromInt(HttpStatusCode));
		return;
	}

	FString CleanResponse = ResponseContent;
	CleanResponse.TrimStartAndEndInline();

	// Rimuove eventuale BOM UTF-8/Unicode all'inizio della risposta.
	if (!CleanResponse.IsEmpty() && CleanResponse[0] == static_cast<TCHAR>(0xFEFF))
	{
		CleanResponse.RightChopInline(1, EAllowShrinking::No);
		CleanResponse.TrimStartAndEndInline();
	}

	// Protezione ulteriore: se il server restituisce warning/testo prima del JSON,
	// prendiamo dal primo carattere utile.
	int32 JsonStartIndex = INDEX_NONE;
	if (CleanResponse.FindChar(TEXT('{'), JsonStartIndex) && JsonStartIndex > 0)
	{
		CleanResponse = CleanResponse.Mid(JsonStartIndex);
		CleanResponse.TrimStartAndEndInline();
	}

	UE_LOG(LogTemp, Warning, TEXT("LiberatoriaCompilerWidget: risposta lista pulita: %s"), *CleanResponse);

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CleanResponse);

	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		RenderLiberatorieList();
		SetStatus(TEXT("Risposta getLiberatorieDaCompilare.php non valida."));
		return;
	}

	bool bServerSuccess = true;
	if (RootObject->TryGetBoolField(TEXT("success"), bServerSuccess) && !bServerSuccess)
	{
		RenderLiberatorieList();
		SetStatus(GetStringField(RootObject, TEXT("message"), TEXT("Il server non ha restituito la lista.")));
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* ArrayValues = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("liberatorie"), ArrayValues))
	{
		RootObject->TryGetArrayField(TEXT("items"), ArrayValues);
	}

	if (!ArrayValues)
	{
		RenderLiberatorieList();
		SetStatus(TEXT("Nessuna lista liberatorie trovata nella risposta PHP."));
		return;
	}

	for (const TSharedPtr<FJsonValue>& Value : *ArrayValues)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Object.IsValid())
		{
			continue;
		}

		FLiberatoriaPendingItem Item;
		Item.IdLiberatoria = GetIntField(Object, TEXT("idLiberatoria"), 0);
		Item.IdUtente = GetIntField(Object, TEXT("idUtente"), 0);
		Item.IdGiocata = GetIntField(Object, TEXT("idGiocata"), 0);
		Item.NomeLiberatoria = GetStringField(Object, TEXT("nomeLiberatoria"), GetStringField(Object, TEXT("nome")));
		Item.CognomeLiberatoria = GetStringField(Object, TEXT("cognomeLiberatoria"), GetStringField(Object, TEXT("cognome")));
		Item.DataNascitaLiberatoria = GetStringField(Object, TEXT("dataNascitaLiberatoria"), GetStringField(Object, TEXT("dataNascita")));
		Item.IdPremio = GetIntField(Object, TEXT("idPremio"), -1);
		Item.NomePremio = GetStringField(Object, TEXT("nomePremio"), GetStringField(Object, TEXT("premio")));
		Item.ValorePremio = GetStringField(Object, TEXT("valorePremio"));
		Item.DataLiberatoria = GetStringField(Object, TEXT("dataLiberatoria"));
		Item.Ora = GetStringField(Object, TEXT("ora"));
		Item.TipologiaGiocata = GetStringField(Object, TEXT("tipologiaGiocata"));
		Item.TipoGioco = GetStringField(Object, TEXT("tipoGioco"));

		if (Item.IdLiberatoria > 0)
		{
			PendingLiberatorie.Add(Item);
		}
	}

	RenderLiberatorieList();

	if (PendingLiberatorie.Num() == 0)
	{
		ClearSelectedLiberatoria();
		SetStatus(TEXT("Nessuna liberatoria da compilare."));
	}
	else
	{
		SetStatus(TEXT("Liberatorie da compilare: ") + FString::FromInt(PendingLiberatorie.Num()));
	}
}

void ULiberatoriaCompilerWidget::RenderLiberatorieList()
{
	if (!ListaLiberatorieScrollBox)
	{
		return;
	}

	ListaLiberatorieScrollBox->ClearChildren();

	for (int32 Index = 0; Index < PendingLiberatorie.Num(); ++Index)
	{
		const FLiberatoriaPendingItem& Item = PendingLiberatorie[Index];

		ULiberatoriaRowButton* RowButton = NewObject<ULiberatoriaRowButton>(this);
		if (!RowButton)
		{
			continue;
		}

		RowButton->ItemIndex = Index;
		RowButton->OnClickedWithIndex.BindUObject(this, &ULiberatoriaCompilerWidget::HandleRowClicked);
		RowButton->OnClicked.AddDynamic(RowButton, &ULiberatoriaRowButton::HandleClicked);

		UTextBlock* RowText = NewObject<UTextBlock>(RowButton);
		if (RowText)
		{
			FString Label = TEXT("#") + FString::FromInt(Item.IdLiberatoria)
				+ TEXT(" - ") + NormalizeDisplayText(Item.NomeLiberatoria) + TEXT(" ") + NormalizeDisplayText(Item.CognomeLiberatoria);

			RowText->SetText(FText::FromString(Label));
			RowText->SetAutoWrapText(true);
			RowText->SetJustification(ETextJustify::Center);
			RowButton->SetContent(RowText);
		}

		ListaLiberatorieScrollBox->AddChild(RowButton);
	}
}

void ULiberatoriaCompilerWidget::SelectLiberatoriaByIndex(int32 Index)
{
	if (!PendingLiberatorie.IsValidIndex(Index))
	{
		SetStatus(TEXT("Liberatoria selezionata non valida. Aggiorna la lista."));
		ClearSelectedLiberatoria();
		ShowList();
		LoadPendingLiberatorie();
		return;
	}

	SelectedLiberatoria = PendingLiberatorie[Index];
	bHasSelectedLiberatoria = true;

	if (TextBlock_Nome)
	{
		TextBlock_Nome->SetText(FText::FromString(TEXT("Nome: ") + SelectedLiberatoria.NomeLiberatoria));
	}

	if (TextBlock_Cognome)
	{
		TextBlock_Cognome->SetText(FText::FromString(TEXT("Cognome: ") + SelectedLiberatoria.CognomeLiberatoria));
	}

	if (TextBlock_DataNascita)
	{
		TextBlock_DataNascita->SetText(FText::FromString(TEXT("Data nascita: ") + SelectedLiberatoria.DataNascitaLiberatoria));
	}

	if (ComboBoxString_TipoDocumento)
	{
		ComboBoxString_TipoDocumento->SetSelectedOption(TEXT("Carta identita"));
	}

	if (EditableTextBox_NumeroDocumento)
	{
		EditableTextBox_NumeroDocumento->SetText(FText::GetEmpty());
	}

	if (EditableTextBox_DataDocumento)
	{
		EditableTextBox_DataDocumento->SetText(FText::GetEmpty());
		EditableTextBox_DataDocumento->SetHintText(FText::FromString(TEXT("Inserisci data documento")));
	}

	if (TextBlock_TestoLiberatoria)
	{
		FString PremioLabel = NormalizeDisplayText(SelectedLiberatoria.NomePremio);
		if (!SelectedLiberatoria.ValorePremio.IsEmpty())
		{
			PremioLabel += TEXT(" del valore di ") + NormalizeDisplayText(SelectedLiberatoria.ValorePremio);
		}

		const FString Testo = TEXT("Il/la sottoscritto/a ")
			+ SelectedLiberatoria.NomeLiberatoria + TEXT(" ") + SelectedLiberatoria.CognomeLiberatoria
			+ TEXT(", nato/a il ") + SelectedLiberatoria.DataNascitaLiberatoria
			+ TEXT(", dichiara di ricevere il premio \"") + PremioLabel
			+ TEXT("\" e conferma l'avvenuta consegna del premio relativo alla giocata indicata.");

		TextBlock_TestoLiberatoria->SetText(FText::FromString(Testo));
		TextBlock_TestoLiberatoria->SetAutoWrapText(true);
		TextBlock_TestoLiberatoria->SetJustification(ETextJustify::Center);
	}

	SetStatus(TEXT("Compila i dati documento e firma la liberatoria."));
	ShowForm();
}

void ULiberatoriaCompilerWidget::ClearSelectedLiberatoria()
{
	SelectedLiberatoria = FLiberatoriaPendingItem();
	bHasSelectedLiberatoria = false;
}

void ULiberatoriaCompilerWidget::SubmitSelectedLiberatoria()
{
	if (bIsSubmitting)
	{
		return;
	}

	FString Error;
	if (!ValidateForm(Error))
	{
		SetStatus(Error);
		return;
	}

	bIsSubmitting = true;
	SetStatus(TEXT("Invio liberatoria in corso..."));

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(GetEndpointUrl(CompleteEndpoint));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded; charset=utf-8"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json, text/plain, */*"));
	Request->SetHeader(TEXT("Cache-Control"), TEXT("no-cache, no-store, must-revalidate, max-age=0"));
	Request->SetHeader(TEXT("Pragma"), TEXT("no-cache"));
	Request->SetHeader(TEXT("Expires"), TEXT("0"));
	Request->SetContentAsString(BuildCompleteRequestBody());

	TWeakObjectPtr<ULiberatoriaCompilerWidget> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis](FHttpRequestPtr RequestPtr, FHttpResponsePtr ResponsePtr, bool bWasSuccessful)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			const int32 HttpStatusCode = ResponsePtr.IsValid() ? ResponsePtr->GetResponseCode() : 0;
			const FString ResponseContent = ResponsePtr.IsValid() ? ResponsePtr->GetContentAsString() : TEXT("");
			WeakThis->HandleCompleteResponse(ResponseContent, HttpStatusCode, bWasSuccessful);
		});

	Request->ProcessRequest();
}

void ULiberatoriaCompilerWidget::HandleCompleteResponse(const FString& ResponseContent, int32 HttpStatusCode, bool bWasSuccessful)
{
	if (!bWasSuccessful || HttpStatusCode < 200 || HttpStatusCode >= 300)
	{
		bIsSubmitting = false;
		SetStatus(TEXT("Errore HTTP invio: ") + FString::FromInt(HttpStatusCode));
		return;
	}

	FString Trimmed = ResponseContent.TrimStartAndEnd();
	bool bSuccess = Trimmed.Equals(TEXT("success"), ESearchCase::IgnoreCase);
	FString Message = Trimmed;

	if (!bSuccess)
	{
		TSharedPtr<FJsonObject> RootObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Trimmed);
		if (FJsonSerializer::Deserialize(Reader, RootObject) && RootObject.IsValid())
		{
			RootObject->TryGetBoolField(TEXT("success"), bSuccess);
			Message = GetStringField(RootObject, TEXT("message"), GetStringField(RootObject, TEXT("error"), Trimmed));
		}
	}

	if (!bSuccess)
	{
		bIsSubmitting = false;
		SetStatus(TEXT("Liberatoria non inviata: ") + Message);
		return;
	}

	UploadCurrentLiberatoriaScreenshot();
	SetStatus(TEXT("Liberatoria aggiornata. Upload screenshot liberatoria in corso..."));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReturnToListTimerHandle);
		World->GetTimerManager().SetTimer(ReturnToListTimerHandle, this, &ULiberatoriaCompilerWidget::FinishSuccessfulSubmit, 1.5f, false);
	}
	else
	{
		FinishSuccessfulSubmit();
	}
}

void ULiberatoriaCompilerWidget::UploadCurrentLiberatoriaScreenshot()
{
	if (!WBP_SignatureBox)
	{
		UE_LOG(LogTemp, Warning, TEXT("LiberatoriaCompilerWidget: WBP_SignatureBox non collegato, screenshot non inviato."));
		return;
	}

	const FString UploadUrl = GetEndpointUrl(ScreenshotEndpoint);
	WBP_SignatureBox->UploadViewportScreenshot(UploadUrl, ScreenshotFolder, MakeScreenshotFilename());
}

void ULiberatoriaCompilerWidget::FinishSuccessfulSubmit()
{
	bIsSubmitting = false;
	ClearSelectedLiberatoria();
	ShowList();
	LoadPendingLiberatorie();
}

void ULiberatoriaCompilerWidget::ShowList()
{
	if (MainSwitcher && Page_ListaLiberatorie)
	{
		MainSwitcher->SetActiveWidget(Page_ListaLiberatorie);
	}
}

void ULiberatoriaCompilerWidget::ShowForm()
{
	if (MainSwitcher && Page_FormLiberatoria)
	{
		MainSwitcher->SetActiveWidget(Page_FormLiberatoria);
	}
}

void ULiberatoriaCompilerWidget::SetStatus(const FString& Message)
{
	UE_LOG(LogTemp, Warning, TEXT("LiberatoriaCompilerWidget: %s"), *Message);

	if (TextBlock_Status)
	{
		TextBlock_Status->SetText(FText::FromString(Message));
	}
}

void ULiberatoriaCompilerWidget::HandleRowClicked(int32 Index)
{
	SelectLiberatoriaByIndex(Index);
}

void ULiberatoriaCompilerWidget::HandleSubmitClicked()
{
	SubmitSelectedLiberatoria();
}

void ULiberatoriaCompilerWidget::HandleBackClicked()
{
	bIsSubmitting = false;
	ClearSelectedLiberatoria();
	ShowList();
	LoadPendingLiberatorie();
}

void ULiberatoriaCompilerWidget::HandleAggiornaListaClicked()
{
	ShowList();
	LoadPendingLiberatorie();
}

FString ULiberatoriaCompilerWidget::GetEndpointUrl(const FString& Endpoint) const
{
	if (Endpoint.StartsWith(TEXT("http://")) || Endpoint.StartsWith(TEXT("https://")))
	{
		return Endpoint;
	}

	FString NormalizedBaseUrl = BaseUrl;
	if (!NormalizedBaseUrl.EndsWith(TEXT("/")))
	{
		NormalizedBaseUrl += TEXT("/");
	}

	return NormalizedBaseUrl + Endpoint;
}

FString ULiberatoriaCompilerWidget::GetNoCacheEndpointUrl(const FString& Endpoint)
{
	FString Url = GetEndpointUrl(Endpoint);

	++ListRequestCounter;

	const int64 Timestamp = FDateTime::UtcNow().ToUnixTimestamp();

	const FString Separator = Url.Contains(TEXT("?")) ? TEXT("&") : TEXT("?");
	Url += Separator
		+ TEXT("_nocache=") + FString::FromInt(static_cast<int32>(Timestamp))
		+ TEXT("_") + FString::FromInt(ListRequestCounter);

	return Url;
}

FString ULiberatoriaCompilerWidget::BuildCompleteRequestBody() const
{
	FString Body;

	const FString TipoDocumento = ComboBoxString_TipoDocumento ? ComboBoxString_TipoDocumento->GetSelectedOption() : TEXT("");
	const FString NumeroDocumento = EditableTextBox_NumeroDocumento ? EditableTextBox_NumeroDocumento->GetText().ToString().TrimStartAndEnd() : TEXT("");
	const FString DataDocumento = EditableTextBox_DataDocumento ? EditableTextBox_DataDocumento->GetText().ToString().TrimStartAndEnd() : TEXT("");

	AppendFormField(Body, TEXT("idLiberatoria"), FString::FromInt(SelectedLiberatoria.IdLiberatoria));
	AppendFormField(Body, TEXT("tipoDocumento"), TipoDocumento);
	AppendFormField(Body, TEXT("documento"), TipoDocumento);
	AppendFormField(Body, TEXT("numeroDocumento"), NumeroDocumento);
	AppendFormField(Body, TEXT("dataDocumento"), DataDocumento);
	AppendFormField(Body, TEXT("consegnata"), TEXT("si"));

	return Body;
}

FString ULiberatoriaCompilerWidget::MakeScreenshotFilename() const
{
	FString Filename = TEXT("liberatoria_")
		+ FString::FromInt(SelectedLiberatoria.IdLiberatoria)
		+ TEXT("_") + SelectedLiberatoria.CognomeLiberatoria
		+ TEXT("_") + SelectedLiberatoria.NomeLiberatoria
		+ TEXT(".png");

	return SanitizeFilename(Filename);
}

bool ULiberatoriaCompilerWidget::ValidateForm(FString& OutError) const
{
	if (!bHasSelectedLiberatoria || SelectedLiberatoria.IdLiberatoria <= 0)
	{
		OutError = TEXT("Nessuna liberatoria selezionata.");
		return false;
	}

	if (!ComboBoxString_TipoDocumento || ComboBoxString_TipoDocumento->GetSelectedOption().TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("Seleziona il tipo documento.");
		return false;
	}

	if (!EditableTextBox_NumeroDocumento || EditableTextBox_NumeroDocumento->GetText().ToString().TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("Inserisci il numero documento.");
		return false;
	}

	if (!EditableTextBox_DataDocumento || EditableTextBox_DataDocumento->GetText().ToString().TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("Inserisci la data documento.");
		return false;
	}

	if (!WBP_SignatureBox)
	{
		OutError = TEXT("Widget firma non collegato.");
		return false;
	}

	return true;
}

FString ULiberatoriaCompilerWidget::UrlEncode(const FString& Value)
{
	return FGenericPlatformHttp::UrlEncode(Value);
}

FString ULiberatoriaCompilerWidget::SanitizeFilename(const FString& Value)
{
	FString Result;
	Result.Reserve(Value.Len());

	for (const TCHAR Character : Value)
	{
		if (FChar::IsAlnum(Character) || Character == TEXT('_') || Character == TEXT('-') || Character == TEXT('.'))
		{
			Result.AppendChar(Character);
		}
		else
		{
			Result.AppendChar(TEXT('_'));
		}
	}

	return Result;
}

FString ULiberatoriaCompilerWidget::NormalizeDisplayText(const FString& Value)
{
	FString Result;
	Result.Reserve(Value.Len() + 8);

	for (const TCHAR Character : Value)
	{
		if (Character == static_cast<TCHAR>(0x0080) || Character == static_cast<TCHAR>(0x20AC))
		{
			Result += TEXT("EUR");
		}
		else
		{
			Result.AppendChar(Character);
		}
	}

	return Result;
}

void ULiberatoriaCompilerWidget::AppendFormField(FString& Body, const FString& Key, const FString& Value)
{
	if (!Body.IsEmpty())
	{
		Body += TEXT("&");
	}

	Body += UrlEncode(Key) + TEXT("=") + UrlEncode(Value);
}

FString ULiberatoriaCompilerWidget::GetStringField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, const FString& DefaultValue)
{
	if (!Object.IsValid())
	{
		return DefaultValue;
	}

	FString StringValue;
	if (Object->TryGetStringField(FieldName, StringValue))
	{
		return NormalizeDisplayText(StringValue);
	}

	double NumberValue = 0.0;
	if (Object->TryGetNumberField(FieldName, NumberValue))
	{
		const int32 RoundedValue = FMath::RoundToInt(NumberValue);
		if (FMath::IsNearlyEqual(NumberValue, static_cast<double>(RoundedValue)))
		{
			return FString::FromInt(RoundedValue);
		}

		return FString::SanitizeFloat(NumberValue);
	}

	bool BoolValue = false;
	if (Object->TryGetBoolField(FieldName, BoolValue))
	{
		return BoolValue ? TEXT("true") : TEXT("false");
	}

	return DefaultValue;
}

int32 ULiberatoriaCompilerWidget::GetIntField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, int32 DefaultValue)
{
	if (!Object.IsValid())
	{
		return DefaultValue;
	}

	double NumberValue = 0.0;
	if (Object->TryGetNumberField(FieldName, NumberValue))
	{
		return FMath::RoundToInt(NumberValue);
	}

	FString StringValue;
	if (Object->TryGetStringField(FieldName, StringValue) && StringValue.IsNumeric())
	{
		return FCString::Atoi(*StringValue);
	}

	return DefaultValue;
}

#undef LOCTEXT_NAMESPACE