// Draws VTT callouts: a thick box around the target, a straight line off it, and bold text at the end.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "VTTOverlayWidget.generated.h"

class APlayerController;
struct FVTTActiveCallout;

/**
 * Full-screen, input-transparent overlay. Pure C++: all drawing happens in NativePaint, so no
 * widget Blueprint is required. Style comes from UVTTSettings (Project Settings > Game > VTT);
 * color can be overridden per VTT component.
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
	/** Wrapped text for one callout, measured once when it first appears. */
	struct FCalloutLayout
	{
		TArray<FString> LabelLines;
		TArray<float> LabelLineWidths;
		TArray<FString> BodyLines;
		TArray<float> BodyLineWidths;
		float LabelLineHeight = 0.f;
		float BodyLineHeight = 0.f;
		float BlockWidth = 0.f;
		float BlockHeight = 0.f;
		/** +1 = line/text to the right of the box, -1 = left. Locked so it never flips mid-read. */
		int32 Side = 0;
	};

	struct FRectDraw
	{
		FVector2D Position;
		FVector2D Size;
		FLinearColor Color;
	};

	struct FLineDraw
	{
		FVector2D Start;
		FVector2D End;
		FLinearColor Color;
		float Thickness = 1.f;
	};

	struct FTextDraw
	{
		FVector2D Position;
		FString Text;
		bool bLabel = false;
		FLinearColor Color;
	};

	void RefreshFonts();
	void BuildCallout(const FVTTActiveCallout& Callout, const FVector2D& ViewSize, APlayerController* Controller);
	bool ProjectBounds(const FBox& WorldBounds, APlayerController* Controller, FBox2D& OutRect) const;
	FCalloutLayout& GetOrBuildLayout(const FVTTActiveCallout& Callout);

	/** Rectangle outline made of four filled bars, so corners are clean at any thickness. */
	void AddBox(const FBox2D& Rect, const FLinearColor& Color, float Thickness);

	/** Adds lines of text, revealing at most InOutCharBudget characters (typewriter). */
	void AddTextLines(const TArray<FString>& Lines, const TArray<float>& Widths, float LineHeight, bool bLabel,
		float Left, float Top, float BlockWidth, bool bAlignRight, const FLinearColor& Color, int32& InOutCharBudget);

	TMap<int32, FCalloutLayout> Layouts;

	TArray<FRectDraw> RectDraws;
	TArray<FLineDraw> LineDraws;
	TArray<FTextDraw> TextDraws;

	FSlateFontInfo LabelFont;
	FSlateFontInfo BodyFont;

	/** Solid white brush for bars. Plain FSlateBrush because UObject members must be default-constructible. */
	FSlateBrush WhiteBrush;
};
