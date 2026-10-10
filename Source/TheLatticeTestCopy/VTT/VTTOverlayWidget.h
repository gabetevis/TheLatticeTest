// Draws VTT callouts: a bracketed frame around the target, a leader line, and translated text.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Fonts/SlateFontInfo.h"
#include "VTTOverlayWidget.generated.h"

class APlayerController;
struct FVTTActiveCallout;

/**
 * Full-screen, input-transparent overlay. Pure C++: all drawing happens in NativePaint, so no
 * widget Blueprint is required. Visual style comes from UVTTSettings (Project Settings > Game > VTT).
 *
 * Each frame NativeTick projects every active callout to the screen and builds a small draw
 * list (rects, lines, text); NativePaint just emits that list.
 */
UCLASS()
class THELATTICETESTCOPY_API UVTTOverlayWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit UVTTOverlayWidget(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	/** Wrapped text and placement for one callout, built once when it first appears. */
	struct FCalloutTextLayout
	{
		TArray<FString> LabelLines;
		TArray<FString> BodyLines;
		float LabelLineHeight = 0.f;
		float BodyLineHeight = 0.f;
		float BlockWidth = 0.f;
		/** Which side the text sits on (+1 right / -1 left, +1 down / -1 up). Locked so it never flips mid-read. */
		int32 SideX = 0;
		int32 SideY = 0;
	};

	struct FLineDraw
	{
		TArray<FVector2f> Points;
		FLinearColor Color;
		float Thickness = 1.f;
	};

	struct FRectDraw
	{
		FVector2D Position;
		FVector2D Size;
		FLinearColor Color;
	};

	struct FTextDraw
	{
		FVector2D Position;
		FVector2D Size;
		FString Text;
		bool bLabelFont = false;
		FLinearColor Color;
	};

	void RefreshFonts();
	void BuildCallout(const FVTTActiveCallout& Callout, int32 StackIndex, const FGeometry& Geometry, APlayerController* Controller);
	bool ProjectBounds(const FBox& WorldBounds, APlayerController* Controller, FBox2D& OutRect) const;
	FCalloutTextLayout& GetOrBuildTextLayout(const FVTTActiveCallout& Callout);

	void AddPolyline(TArray<FVector2D> Points, const FLinearColor& Color, float Thickness);
	void AddFrame(const FBox2D& Rect, float Alpha);
	void AddTextLines(const TArray<FString>& Lines, const FVector2D& TopLeft, float LineHeight, bool bLabelFont,
		const FLinearColor& Color, float Alpha, int32& InOutCharBudget, bool bFullyRevealed);

	TMap<int32, FCalloutTextLayout> TextLayouts;

	TArray<FRectDraw> RectDraws;
	TArray<FLineDraw> LineDraws;
	TArray<FTextDraw> TextDraws;

	FSlateFontInfo LabelFont;
	FSlateFontInfo BodyFont;
	/** Solid white brush for boxes. Plain FSlateBrush because UObject members must be default-constructible. */
	FSlateBrush WhiteBrush;
};
