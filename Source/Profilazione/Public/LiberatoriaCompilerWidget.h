#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "TimerManager.h"
#include "LiberatoriaCompilerWidget.generated.h"

class UWidget;
class UWidgetSwitcher;
class UScrollBox;
class UTextBlock;
class UEditableTextBox;
class UComboBoxString;
class USignaturePadWidget;
class FJsonObject;

USTRUCT(BlueprintType)
struct FLiberatoriaPendingItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	int32 IdLiberatoria = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	int32 IdUtente = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	int32 IdGiocata = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	FString NomeLiberatoria;

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	FString CognomeLiberatoria;

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	FString DataNascitaLiberatoria;

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	int32 IdPremio = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	FString NomePremio;

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	FString ValorePremio;

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	FString DataLiberatoria;

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	FString Ora;

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	FString TipologiaGiocata;

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	FString TipoGioco;
};

DECLARE_DELEGATE_OneParam(FOnLiberatoriaRowClickedNative, int32);

UCLASS()
class PROFILAZIONE_API ULiberatoriaRowButton : public UButton
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 ItemIndex = INDEX_NONE;

	FOnLiberatoriaRowClickedNative OnClickedWithIndex;

	UFUNCTION()
	void HandleClicked();
};

UCLASS()
class PROFILAZIONE_API ULiberatoriaCompilerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	ULiberatoriaCompilerWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Liberatoria|Server")
	FString BaseUrl = TEXT("https://www.cubecomunicazione.com/CubeDBphp/2026_Probability_Attivita_Test/");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Liberatoria|Server")
	FString ListEndpoint = TEXT("getLiberatorieDaCompilare.php");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Liberatoria|Server")
	FString CompleteEndpoint = TEXT("completeLiberatoria.php");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Liberatoria|Screenshot")
	FString ScreenshotEndpoint = TEXT("screenshot.php");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Liberatoria|Screenshot")
	FString ScreenshotFolder = TEXT("liberatorie");

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	TArray<FLiberatoriaPendingItem> PendingLiberatorie;

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	FLiberatoriaPendingItem SelectedLiberatoria;

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	bool bHasSelectedLiberatoria = false;

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	bool bIsLoadingList = false;

	UPROPERTY(BlueprintReadOnly, Category = "Liberatoria")
	bool bIsSubmitting = false;

	UFUNCTION(BlueprintCallable, Category = "Liberatoria")
	void LoadPendingLiberatorie();

	UFUNCTION(BlueprintCallable, Category = "Liberatoria")
	void SelectLiberatoriaByIndex(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Liberatoria")
	void SubmitSelectedLiberatoria();

	UFUNCTION(BlueprintCallable, Category = "Liberatoria")
	void ClearSelectedLiberatoria();

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Liberatoria|Widget")
	UWidgetSwitcher* MainSwitcher = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Liberatoria|Widget")
	UWidget* Page_ListaLiberatorie = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Liberatoria|Widget")
	UWidget* Page_FormLiberatoria = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Liberatoria|Widget")
	UScrollBox* ListaLiberatorieScrollBox = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Liberatoria|Widget")
	UTextBlock* TextBlock_Status = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Liberatoria|Widget")
	UTextBlock* TextBlock_Nome = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Liberatoria|Widget")
	UTextBlock* TextBlock_Cognome = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Liberatoria|Widget")
	UTextBlock* TextBlock_DataNascita = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Liberatoria|Widget")
	UComboBoxString* ComboBoxString_TipoDocumento = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Liberatoria|Widget")
	UEditableTextBox* EditableTextBox_NumeroDocumento = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Liberatoria|Widget")
	UEditableTextBox* EditableTextBox_DataDocumento = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Liberatoria|Widget")
	UTextBlock* TextBlock_TestoLiberatoria = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Liberatoria|Widget")
	USignaturePadWidget* WBP_SignatureBox = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Liberatoria|Widget")
	UButton* Button_Invia = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Liberatoria|Widget")
	UButton* Button_Indietro = nullptr;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Liberatoria|Widget")
	UButton* Button_AggiornaLista = nullptr;

private:
	FTimerHandle ReturnToListTimerHandle;

	int32 ListRequestCounter = 0;

	void RenderLiberatorieList();
	void ShowList();
	void ShowForm();
	void SetStatus(const FString& Message);

	void HandleListResponse(const FString& ResponseContent, int32 HttpStatusCode, bool bWasSuccessful);
	void HandleCompleteResponse(const FString& ResponseContent, int32 HttpStatusCode, bool bWasSuccessful);
	void HandleRowClicked(int32 Index);

	void UploadCurrentLiberatoriaScreenshot();
	void FinishSuccessfulSubmit();

	UFUNCTION()
	void HandleSubmitClicked();

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleAggiornaListaClicked();

	FString GetEndpointUrl(const FString& Endpoint) const;
	FString GetNoCacheEndpointUrl(const FString& Endpoint);
	FString BuildCompleteRequestBody() const;
	FString MakeScreenshotFilename() const;

	bool ValidateForm(FString& OutError) const;

	static FString UrlEncode(const FString& Value);
	static FString SanitizeFilename(const FString& Value);

	static FString NormalizeDisplayText(const FString& Value);
	static void AppendFormField(FString& Body, const FString& Key, const FString& Value);
	static FString GetStringField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, const FString& DefaultValue = TEXT(""));
	static int32 GetIntField(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, int32 DefaultValue = 0);
};