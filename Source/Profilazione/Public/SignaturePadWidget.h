#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SignaturePadWidget.generated.h"

USTRUCT(BlueprintType)
struct FSignatureStroke
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TArray<FVector2D> Points;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSignatureScreenshotUploadCompleted,
	bool,
	bSuccess,
	FString,
	Message
);

UCLASS()
class PROFILAZIONE_API USignaturePadWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	USignaturePadWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signature")
	FLinearColor LineColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signature", meta = (ClampMin = "1.0"))
	float LineThickness = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signature", meta = (ClampMin = "0.0"))
	float MinPointDistance = 2.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Signature")
	TArray<FSignatureStroke> Strokes;

	UPROPERTY(BlueprintReadOnly, Category = "Signature")
	bool bIsDrawing = false;

	UPROPERTY(BlueprintReadOnly, Category = "Signature|Screenshot")
	bool bScreenshotUploadInProgress = false;

	UPROPERTY(BlueprintAssignable, Category = "Signature|Screenshot")
	FSignatureScreenshotUploadCompleted OnScreenshotUploadCompleted;

	UFUNCTION(BlueprintCallable, Category = "Signature")
	void ClearSignature();

	UFUNCTION(BlueprintCallable, Category = "Signature")
	bool HasSignature() const;

	UFUNCTION(BlueprintCallable, Category = "Signature|Screenshot")
	FString CaptureViewportAsBase64(bool bAddDataUriPrefix = false) const;

	UFUNCTION(BlueprintCallable, Category = "Signature|Screenshot")
	void UploadViewportScreenshot(
		const FString& UploadUrl,
		const FString& Folder = TEXT("immagini"),
		const FString& Filename = TEXT("screenshot.png")
	);

protected:

	virtual void NativeDestruct() override;

	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent
	) override;

	virtual FReply NativeOnMouseMove(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent
	) override;

	virtual FReply NativeOnMouseButtonUp(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent
	) override;

	virtual FReply NativeOnTouchStarted(
		const FGeometry& InGeometry,
		const FPointerEvent& InGestureEvent
	) override;

	virtual FReply NativeOnTouchMoved(
		const FGeometry& InGeometry,
		const FPointerEvent& InGestureEvent
	) override;

	virtual FReply NativeOnTouchEnded(
		const FGeometry& InGeometry,
		const FPointerEvent& InGestureEvent
	) override;

	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled
	) const override;

private:

	int32 CurrentStrokeIndex = INDEX_NONE;

	FString PendingUploadUrl;
	FString PendingUploadFolder;
	FString PendingUploadFilename;

	FDelegateHandle ScreenshotCapturedHandle;
	FTimerHandle ScreenshotUploadTimeoutHandle;

	bool IsInsideWidget(const FGeometry& InGeometry, const FVector2D& LocalPosition) const;

	void BeginStroke(const FGeometry& InGeometry, const FVector2D& ScreenPosition);

	void AddPoint(const FGeometry& InGeometry, const FVector2D& ScreenPosition);

	void EndStroke();

	void HandleScreenshotCaptured(
		int32 SizeX,
		int32 SizeY,
		const TArray<FColor>& Bitmap
	);

	void HandleScreenshotUploadTimeout();

	void FinishScreenshotUpload(bool bSuccess, const FString& Message);

	FString BuildFinalUploadUrl() const;
};