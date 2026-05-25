#include "SignaturePadWidget.h"

#include "Rendering/DrawElements.h"
#include "Input/Events.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"

#include "Misc/Base64.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "GenericPlatform/GenericPlatformHttp.h"

#include "TimerManager.h"
#include "HAL/IConsoleManager.h"

USignaturePadWidget::USignaturePadWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsFocusable = true;
}

void USignaturePadWidget::NativeDestruct()
{
	if (ScreenshotCapturedHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotCapturedHandle);
		ScreenshotCapturedHandle.Reset();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScreenshotUploadTimeoutHandle);
	}

	Super::NativeDestruct();
}

bool USignaturePadWidget::IsInsideWidget(
	const FGeometry& InGeometry,
	const FVector2D& LocalPosition
) const
{
	const FVector2D Size = InGeometry.GetLocalSize();

	return LocalPosition.X >= 0.0f &&
		LocalPosition.Y >= 0.0f &&
		LocalPosition.X <= Size.X &&
		LocalPosition.Y <= Size.Y;
}

void USignaturePadWidget::BeginStroke(
	const FGeometry& InGeometry,
	const FVector2D& ScreenPosition
)
{
	const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(ScreenPosition);

	if (!IsInsideWidget(InGeometry, LocalPosition))
	{
		return;
	}

	FSignatureStroke NewStroke;
	NewStroke.Points.Add(LocalPosition);

	CurrentStrokeIndex = Strokes.Add(NewStroke);
	bIsDrawing = true;

	InvalidateLayoutAndVolatility();
}

void USignaturePadWidget::AddPoint(
	const FGeometry& InGeometry,
	const FVector2D& ScreenPosition
)
{
	if (!bIsDrawing || !Strokes.IsValidIndex(CurrentStrokeIndex))
	{
		return;
	}

	const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(ScreenPosition);

	if (!IsInsideWidget(InGeometry, LocalPosition))
	{
		return;
	}

	TArray<FVector2D>& CurrentPoints = Strokes[CurrentStrokeIndex].Points;

	if (CurrentPoints.Num() > 0)
	{
		const float Distance = FVector2D::Distance(CurrentPoints.Last(), LocalPosition);

		if (Distance < MinPointDistance)
		{
			return;
		}
	}

	CurrentPoints.Add(LocalPosition);

	InvalidateLayoutAndVolatility();
}

void USignaturePadWidget::EndStroke()
{
	bIsDrawing = false;
	CurrentStrokeIndex = INDEX_NONE;

	InvalidateLayoutAndVolatility();
}

void USignaturePadWidget::ClearSignature()
{
	Strokes.Empty();
	CurrentStrokeIndex = INDEX_NONE;
	bIsDrawing = false;

	InvalidateLayoutAndVolatility();
}

bool USignaturePadWidget::HasSignature() const
{
	for (const FSignatureStroke& Stroke : Strokes)
	{
		if (Stroke.Points.Num() > 1)
		{
			return true;
		}
	}

	return false;
}

FString USignaturePadWidget::CaptureViewportAsBase64(bool bAddDataUriPrefix) const
{
	if (!GEngine)
	{
		UE_LOG(LogTemp, Warning, TEXT("CaptureViewportAsBase64 failed: GEngine non valido."));
		return FString();
	}

	UGameViewportClient* GameViewportClient = GEngine->GameViewport;

	if (!GameViewportClient)
	{
		UE_LOG(LogTemp, Warning, TEXT("CaptureViewportAsBase64 failed: GameViewportClient non valido."));
		return FString();
	}

	FViewport* Viewport = GameViewportClient->Viewport;

	if (!Viewport)
	{
		UE_LOG(LogTemp, Warning, TEXT("CaptureViewportAsBase64 failed: Viewport non valida."));
		return FString();
	}

	const FIntPoint ViewportSize = Viewport->GetSizeXY();

	if (ViewportSize.X <= 0 || ViewportSize.Y <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("CaptureViewportAsBase64 failed: dimensioni viewport non valide."));
		return FString();
	}

	TArray<FColor> Bitmap;
	Bitmap.SetNumUninitialized(ViewportSize.X * ViewportSize.Y);

	FReadSurfaceDataFlags ReadPixelFlags(RCM_UNorm);
	ReadPixelFlags.SetLinearToGamma(true);

	const bool bReadSuccess = Viewport->ReadPixels(Bitmap, ReadPixelFlags);

	if (!bReadSuccess || Bitmap.Num() <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("CaptureViewportAsBase64 failed: ReadPixels fallito."));
		return FString();
	}

	for (FColor& Pixel : Bitmap)
	{
		Pixel.A = 255;
	}

	IImageWrapperModule& ImageWrapperModule =
		FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));

	TSharedPtr<IImageWrapper> ImageWrapper =
		ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	if (!ImageWrapper.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("CaptureViewportAsBase64 failed: ImageWrapper PNG non valido."));
		return FString();
	}

	const bool bSetRawSuccess = ImageWrapper->SetRaw(
		Bitmap.GetData(),
		Bitmap.Num() * sizeof(FColor),
		ViewportSize.X,
		ViewportSize.Y,
		ERGBFormat::BGRA,
		8
	);

	if (!bSetRawSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("CaptureViewportAsBase64 failed: SetRaw fallito."));
		return FString();
	}

	const TArray64<uint8>& CompressedPng64 = ImageWrapper->GetCompressed(100);

	if (CompressedPng64.Num() <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("CaptureViewportAsBase64 failed: compressione PNG fallita."));
		return FString();
	}

	TArray<uint8> CompressedPng;
	CompressedPng.Append(CompressedPng64.GetData(), CompressedPng64.Num());

	FString Base64String = FBase64::Encode(CompressedPng);

	if (bAddDataUriPrefix)
	{
		Base64String = TEXT("data:image/png;base64,") + Base64String;
	}

	return Base64String;
}

void USignaturePadWidget::UploadViewportScreenshot(
	const FString& UploadUrl,
	const FString& Folder,
	const FString& Filename
)
{
	if (bScreenshotUploadInProgress)
	{
		UE_LOG(LogTemp, Warning, TEXT("UploadViewportScreenshot ignorato: upload gia in corso."));
		return;
	}

	if (UploadUrl.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("UploadViewportScreenshot fallito: UploadUrl vuoto."));
		OnScreenshotUploadCompleted.Broadcast(false, TEXT("UploadUrl vuoto."));
		return;
	}

	if (!GEngine || !GEngine->GameViewport || !GEngine->GameViewport->Viewport)
	{
		UE_LOG(LogTemp, Warning, TEXT("UploadViewportScreenshot fallito: viewport non disponibile."));
		OnScreenshotUploadCompleted.Broadcast(false, TEXT("Viewport non disponibile."));
		return;
	}

	PendingUploadUrl = UploadUrl;
	PendingUploadFolder = Folder.IsEmpty() ? TEXT("immagini") : Folder;
	PendingUploadFilename = Filename.IsEmpty() ? TEXT("screenshot.png") : Filename;

	bScreenshotUploadInProgress = true;

	if (ScreenshotCapturedHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotCapturedHandle);
		ScreenshotCapturedHandle.Reset();
	}

	IConsoleVariable* ScreenshotDelegateCVar =
		IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenshotDelegate"));

	if (ScreenshotDelegateCVar)
	{
		ScreenshotDelegateCVar->Set(1, ECVF_SetByCode);
	}

	ScreenshotCapturedHandle =
		UGameViewportClient::OnScreenshotCaptured().AddUObject(
			this,
			&USignaturePadWidget::HandleScreenshotCaptured
		);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ScreenshotUploadTimeoutHandle,
			this,
			&USignaturePadWidget::HandleScreenshotUploadTimeout,
			60.0f,
			false
		);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("UploadViewportScreenshot: richiesta screenshot con UI. Url=%s Folder=%s Filename=%s"),
		*PendingUploadUrl,
		*PendingUploadFolder,
		*PendingUploadFilename
	);

	FScreenshotRequest::RequestScreenshot(
		TEXT("ScreenShot"),
		true,
		false,
		false
	);
}

void USignaturePadWidget::HandleScreenshotCaptured(
	int32 SizeX,
	int32 SizeY,
	const TArray<FColor>& Bitmap
)
{
	if (!bScreenshotUploadInProgress)
	{
		return;
	}

	if (ScreenshotCapturedHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotCapturedHandle);
		ScreenshotCapturedHandle.Reset();
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("HandleScreenshotCaptured: screenshot ricevuto. Size=%dx%d Pixels=%d"),
		SizeX,
		SizeY,
		Bitmap.Num()
	);

	if (SizeX <= 0 || SizeY <= 0 || Bitmap.Num() <= 0)
	{
		FinishScreenshotUpload(false, TEXT("Screenshot vuoto o non valido."));
		return;
	}

	TArray<FColor> BitmapCopy = Bitmap;

	for (FColor& Pixel : BitmapCopy)
	{
		Pixel.A = 255;
	}

	IImageWrapperModule& ImageWrapperModule =
		FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));

	TSharedPtr<IImageWrapper> ImageWrapper =
		ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);

	if (!ImageWrapper.IsValid())
	{
		FinishScreenshotUpload(false, TEXT("ImageWrapper PNG non valido."));
		return;
	}

	const bool bSetRawSuccess = ImageWrapper->SetRaw(
		BitmapCopy.GetData(),
		BitmapCopy.Num() * sizeof(FColor),
		SizeX,
		SizeY,
		ERGBFormat::BGRA,
		8
	);

	if (!bSetRawSuccess)
	{
		FinishScreenshotUpload(false, TEXT("SetRaw PNG fallito."));
		return;
	}

	const TArray64<uint8>& CompressedPng64 = ImageWrapper->GetCompressed(100);

	if (CompressedPng64.Num() <= 0)
	{
		FinishScreenshotUpload(false, TEXT("Compressione PNG fallita."));
		return;
	}

	TArray<uint8> CompressedPng;
	CompressedPng.Append(CompressedPng64.GetData(), CompressedPng64.Num());

	const FString FinalUrl = BuildFinalUploadUrl();

	UE_LOG(
		LogTemp,
		Display,
		TEXT("Upload HTTP in partenza. Url=%s Bytes=%d"),
		*FinalUrl,
		CompressedPng.Num()
	);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(FinalUrl);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("image/png"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetContent(CompressedPng);

	TWeakObjectPtr<USignaturePadWidget> WeakThis(this);

	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis](FHttpRequestPtr RequestPtr, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			if (!WeakThis.IsValid())
			{
				return;
			}

			USignaturePadWidget* Widget = WeakThis.Get();

			if (!Widget->bScreenshotUploadInProgress)
			{
				return;
			}

			const int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
			const FString ResponseBody = Response.IsValid()
				? Response->GetContentAsString()
				: TEXT("Nessuna risposta HTTP.");

			const bool bHttpOk =
				bWasSuccessful &&
				Response.IsValid() &&
				ResponseCode >= 200 &&
				ResponseCode < 300;

			const FString Message = FString::Printf(
				TEXT("HTTP %d - %s"),
				ResponseCode,
				*ResponseBody
			);

			if (bHttpOk)
			{
				UE_LOG(LogTemp, Display, TEXT("Upload screenshot completato: %s"), *Message);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Upload screenshot fallito: %s"), *Message);
			}

			Widget->FinishScreenshotUpload(bHttpOk, Message);
		}
	);

	const bool bStarted = Request->ProcessRequest();

	if (!bStarted)
	{
		FinishScreenshotUpload(false, TEXT("ProcessRequest HTTP fallito."));
	}
}

void USignaturePadWidget::HandleScreenshotUploadTimeout()
{
	if (!bScreenshotUploadInProgress)
	{
		return;
	}

	FinishScreenshotUpload(false, TEXT("Timeout: screenshot/upload non completato entro 60 secondi."));
}

void USignaturePadWidget::FinishScreenshotUpload(bool bSuccess, const FString& Message)
{
	if (ScreenshotCapturedHandle.IsValid())
	{
		UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotCapturedHandle);
		ScreenshotCapturedHandle.Reset();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScreenshotUploadTimeoutHandle);
	}

	bScreenshotUploadInProgress = false;

	UE_LOG(
		LogTemp,
		Display,
		TEXT("FinishScreenshotUpload: Success=%s Message=%s"),
		bSuccess ? TEXT("true") : TEXT("false"),
		*Message
	);

	OnScreenshotUploadCompleted.Broadcast(bSuccess, Message);
}

FString USignaturePadWidget::BuildFinalUploadUrl() const
{
	const FString Separator = PendingUploadUrl.Contains(TEXT("?")) ? TEXT("&") : TEXT("?");

	const FString EncodedFolder =
		FGenericPlatformHttp::UrlEncode(PendingUploadFolder);

	const FString EncodedFilename =
		FGenericPlatformHttp::UrlEncode(PendingUploadFilename);

	return FString::Printf(
		TEXT("%s%sfolder=%s&filename=%s"),
		*PendingUploadUrl,
		*Separator,
		*EncodedFolder,
		*EncodedFilename
	);
}

FReply USignaturePadWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent
)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		BeginStroke(InGeometry, InMouseEvent.GetScreenSpacePosition());

		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply USignaturePadWidget::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent
)
{
	if (bIsDrawing)
	{
		AddPoint(InGeometry, InMouseEvent.GetScreenSpacePosition());

		return FReply::Handled();
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply USignaturePadWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent
)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		EndStroke();

		return FReply::Handled().ReleaseMouseCapture();
	}

	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply USignaturePadWidget::NativeOnTouchStarted(
	const FGeometry& InGeometry,
	const FPointerEvent& InGestureEvent
)
{
	BeginStroke(InGeometry, InGestureEvent.GetScreenSpacePosition());

	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply USignaturePadWidget::NativeOnTouchMoved(
	const FGeometry& InGeometry,
	const FPointerEvent& InGestureEvent
)
{
	if (bIsDrawing)
	{
		AddPoint(InGeometry, InGestureEvent.GetScreenSpacePosition());
	}

	return FReply::Handled();
}

FReply USignaturePadWidget::NativeOnTouchEnded(
	const FGeometry& InGeometry,
	const FPointerEvent& InGestureEvent
)
{
	EndStroke();

	return FReply::Handled().ReleaseMouseCapture();
}

int32 USignaturePadWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled
) const
{
	const int32 NewLayerId = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		LayerId,
		InWidgetStyle,
		bParentEnabled
	);

	const int32 DrawLayerId = NewLayerId + 1;

	for (const FSignatureStroke& Stroke : Strokes)
	{
		if (Stroke.Points.Num() < 2)
		{
			continue;
		}

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			DrawLayerId,
			AllottedGeometry.ToPaintGeometry(),
			Stroke.Points,
			ESlateDrawEffect::None,
			LineColor,
			true,
			LineThickness
		);
	}

	return DrawLayerId;
}