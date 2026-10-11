#include "VTT/VTTOverlayWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Font.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "VTT/VTTSettings.h"
#include "VTT/VTTSubsystem.h"

namespace VTTDraw
{
	static FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= FMath::Clamp(Alpha, 0.f, 1.f);
		return Color;
	}

	static float EaseOutCubic(float T)
	{
		return 1.f - FMath::Pow(1.f - FMath::Clamp(T, 0.f, 1.f), 3.f);
	}

	static void WrapText(const FString& Source, const FSlateFontInfo& Font, float MaxWidth,
		const TSharedRef<FSlateFontMeasure>& Measure, TArray<FString>& OutLines, TArray<float>& OutWidths)
	{
		const FString Clean = Source.Replace(TEXT("\r"), TEXT(""));
		if (Clean.IsEmpty())
		{
			return;
		}

		TArray<FString> Paragraphs;
		Clean.ParseIntoArray(Paragraphs, TEXT("\n"), /*bCullEmpty*/ false);

		auto Commit = [&](const FString& Line)
		{
			OutLines.Add(Line);
			OutWidths.Add(static_cast<float>(Measure->Measure(Line, Font).X));
		};

		for (const FString& Paragraph : Paragraphs)
		{
			TArray<FString> Words;
			Paragraph.ParseIntoArrayWS(Words);
			if (Words.Num() == 0)
			{
				Commit(FString());
				continue;
			}

			FString Line;
			for (const FString& Word : Words)
			{
				const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
				if (!Line.IsEmpty() && Measure->Measure(Candidate, Font).X > MaxWidth)
				{
					Commit(Line);
					Line = Word;
				}
				else
				{
					Line = Candidate;
				}
			}
			Commit(Line);
		}
	}
}

UVTTOverlayWidget::UVTTOverlayWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WhiteBrush = FSlateColorBrush(FLinearColor::White);
}

void UVTTOverlayWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	RefreshFonts();
}

void UVTTOverlayWidget::RefreshFonts()
{
	const UVTTSettings* Settings = UVTTSettings::Get();

	if (const UFont* Font = Settings->Font.LoadSynchronous())
	{
		LabelFont = FSlateFontInfo(Font, static_cast<float>(Settings->LabelFontSize), Settings->Typeface);
		BodyFont = FSlateFontInfo(Font, static_cast<float>(Settings->FontSize), Settings->Typeface);
	}
	else
	{
		LabelFont = FCoreStyle::GetDefaultFontStyle(Settings->Typeface, Settings->LabelFontSize);
		BodyFont = FCoreStyle::GetDefaultFontStyle(Settings->Typeface, Settings->FontSize);
	}

	for (FSlateFontInfo* FontInfo : { &LabelFont, &BodyFont })
	{
		FontInfo->LetterSpacing = Settings->LetterSpacing;
		FontInfo->OutlineSettings.OutlineSize = Settings->TextOutlineSize;
		FontInfo->OutlineSettings.OutlineColor = Settings->TextOutlineColor;
	}
}

// ---------------------------------------------------------------------------------------------
// Tick: build the draw list
// ---------------------------------------------------------------------------------------------

void UVTTOverlayWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RectDraws.Reset();
	LineDraws.Reset();
	TextDraws.Reset();

	APlayerController* Controller = GetOwningPlayer();
	const UVTTSubsystem* Subsystem = UVTTSubsystem::Get(this);
	if (!Controller || !Subsystem || !FSlateApplication::IsInitialized())
	{
		return;
	}

	const TArray<FVTTActiveCallout>& Callouts = Subsystem->GetActiveCallouts();

	// Forget layouts of callouts that are gone.
	for (auto It = Layouts.CreateIterator(); It; ++It)
	{
		const int32 Id = It.Key();
		if (!Callouts.ContainsByPredicate([Id](const FVTTActiveCallout& Callout) { return Callout.Id == Id; }))
		{
			It.RemoveCurrent();
		}
	}

	for (const FVTTActiveCallout& Callout : Callouts)
	{
		BuildCallout(Callout, MyGeometry.GetLocalSize(), Controller);
	}
}

bool UVTTOverlayWidget::ProjectBounds(const FBox& WorldBounds, APlayerController* Controller, FBox2D& OutRect) const
{
	if (!WorldBounds.IsValid)
	{
		return false;
	}

	FVector Corners[8];
	WorldBounds.GetVertices(Corners);

	OutRect = FBox2D(ForceInit);
	int32 NumProjected = 0;
	for (const FVector& Corner : Corners)
	{
		FVector2D ScreenPosition;
		// Widget-space (DPI-scaled) position, matching this full-screen widget's local space.
		if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(Controller, Corner, ScreenPosition, false))
		{
			OutRect += ScreenPosition;
			++NumProjected;
		}
	}
	return NumProjected >= 2 && OutRect.bIsValid;
}

UVTTOverlayWidget::FCalloutLayout& UVTTOverlayWidget::GetOrBuildLayout(const FVTTActiveCallout& Callout)
{
	if (FCalloutLayout* Existing = Layouts.Find(Callout.Id))
	{
		return *Existing;
	}

	const UVTTSettings* Settings = UVTTSettings::Get();
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	FCalloutLayout& Layout = Layouts.Add(Callout.Id);
	VTTDraw::WrapText(Callout.Label.ToString(), LabelFont, Settings->TextMaxWidth, Measure, Layout.LabelLines, Layout.LabelLineWidths);
	VTTDraw::WrapText(Callout.Body.ToString(), BodyFont, Settings->TextMaxWidth, Measure, Layout.BodyLines, Layout.BodyLineWidths);
	Layout.LabelLineHeight = Measure->GetMaxCharacterHeight(LabelFont);
	Layout.BodyLineHeight = Measure->GetMaxCharacterHeight(BodyFont);

	for (const float Width : Layout.LabelLineWidths) { Layout.BlockWidth = FMath::Max(Layout.BlockWidth, Width); }
	for (const float Width : Layout.BodyLineWidths) { Layout.BlockWidth = FMath::Max(Layout.BlockWidth, Width); }
	Layout.BlockHeight = Layout.LabelLines.Num() * Layout.LabelLineHeight + Layout.BodyLines.Num() * Layout.BodyLineHeight;
	return Layout;
}

void UVTTOverlayWidget::BuildCallout(const FVTTActiveCallout& Callout, const FVector2D& ViewSize, APlayerController* Controller)
{
	using namespace VTTDraw;

	const UVTTSettings* Settings = UVTTSettings::Get();

	FBox2D TargetRect;
	if (!ProjectBounds(Callout.LastWorldBounds, Controller, TargetRect))
	{
		return; // target behind the camera (e.g. during outro): nothing to anchor to
	}

	// Pad, enforce a minimum size, keep on screen.
	FVector2D Center = TargetRect.GetCenter();
	FVector2D HalfSize = TargetRect.GetExtent() + FVector2D(Settings->BoxPadding);
	const double MinHalfSize = Settings->MinBoxSize * 0.5;
	HalfSize.X = FMath::Max(HalfSize.X, MinHalfSize);
	HalfSize.Y = FMath::Max(HalfSize.Y, MinHalfSize);
	FBox2D Rect(Center - HalfSize, Center + HalfSize);
	Rect.Min = FVector2D::Max(Rect.Min, FVector2D::ZeroVector);
	Rect.Max = FVector2D::Min(Rect.Max, ViewSize);
	Center = Rect.GetCenter();
	HalfSize = Rect.GetExtent();

	// --- Timing ---
	const float Age = Callout.Age;
	const float BoxIn = Settings->BoxIntroDuration;
	const float LineIn = Settings->LineIntroDuration;
	const float TypeStart = BoxIn + LineIn;
	const float BoxT = BoxIn > 0.f ? Age / BoxIn : 1.f;
	const float LineT = LineIn > 0.f ? (Age - BoxIn) / LineIn : (Age >= BoxIn ? 1.f : 0.f);
	const float Alpha = Callout.bDismissing
		? 1.f - FMath::Clamp((Age - Callout.DismissStartAge) / Settings->OutroDuration, 0.f, 1.f)
		: 1.f;
	const FLinearColor Color = WithAlpha(Callout.Color, Alpha);

	// --- Box: grows out from the center ---
	const double BoxScale = Callout.bDismissing ? 1.0 : EaseOutCubic(BoxT);
	AddBox(FBox2D(Center - HalfSize * BoxScale, Center + HalfSize * BoxScale), Color, Settings->BoxThickness);
	if (Age < BoxIn && !Callout.bDismissing)
	{
		return;
	}

	// --- Side: toward the emptier half of the screen, unless the text wouldn't fit there ---
	FCalloutLayout& Layout = GetOrBuildLayout(Callout);
	const float AngleRad = FMath::DegreesToRadians(Settings->LineAngleDegrees);
	const double Reach = Settings->LineLength * FMath::Cos(AngleRad) + Settings->TextGap + Layout.BlockWidth;
	if (Layout.Side == 0)
	{
		Layout.Side = Center.X < ViewSize.X * 0.5 ? 1 : -1;
		const bool bFitsRight = Rect.Max.X + Reach <= ViewSize.X;
		const bool bFitsLeft = Rect.Min.X - Reach >= 0.0;
		if (Layout.Side > 0 && !bFitsRight && bFitsLeft) { Layout.Side = -1; }
		if (Layout.Side < 0 && !bFitsLeft && bFitsRight) { Layout.Side = 1; }
	}
	const double Side = static_cast<double>(Layout.Side);

	// --- Line: straight off the middle of the box's side ---
	const FVector2D LineStart(Layout.Side > 0 ? Rect.Max.X : Rect.Min.X, Center.Y);
	const FVector2D LineDirection(Side * FMath::Cos(AngleRad), -FMath::Sin(AngleRad));
	const FVector2D LineEnd = LineStart + LineDirection * Settings->LineLength;
	const FVector2D DrawnEnd = Callout.bDismissing ? LineEnd : FMath::Lerp(LineStart, LineEnd, FMath::Clamp(LineT, 0.f, 1.f));
	LineDraws.Add(FLineDraw{ LineStart, DrawnEnd, Color, Settings->LineThickness });

	if (Age < TypeStart && !Callout.bDismissing)
	{
		return;
	}

	// --- Text: vertically centered on the line end, kept on screen ---
	constexpr double ScreenMargin = 8.0;
	double Left = Layout.Side > 0 ? LineEnd.X + Settings->TextGap : LineEnd.X - Settings->TextGap - Layout.BlockWidth;
	double Top = LineEnd.Y - Layout.BlockHeight * 0.5;
	Left = FMath::Clamp(Left, ScreenMargin, FMath::Max(ScreenMargin, ViewSize.X - ScreenMargin - Layout.BlockWidth));
	Top = FMath::Clamp(Top, ScreenMargin, FMath::Max(ScreenMargin, ViewSize.Y - ScreenMargin - Layout.BlockHeight));

	int32 CharBudget = (Settings->bTypewriter && !Callout.bDismissing)
		? FMath::FloorToInt((Age - TypeStart) * Settings->CharactersPerSecond)
		: MAX_int32;

	const bool bAlignRight = Layout.Side < 0; // text hugs the line end on either side
	AddTextLines(Layout.LabelLines, Layout.LabelLineWidths, Layout.LabelLineHeight, /*bLabel*/ true,
		static_cast<float>(Left), static_cast<float>(Top), Layout.BlockWidth, bAlignRight, Color, CharBudget);
	AddTextLines(Layout.BodyLines, Layout.BodyLineWidths, Layout.BodyLineHeight, /*bLabel*/ false,
		static_cast<float>(Left), static_cast<float>(Top + Layout.LabelLines.Num() * Layout.LabelLineHeight),
		Layout.BlockWidth, bAlignRight, Color, CharBudget);
}

// ---------------------------------------------------------------------------------------------
// Draw-list helpers
// ---------------------------------------------------------------------------------------------

void UVTTOverlayWidget::AddBox(const FBox2D& Rect, const FLinearColor& Color, float Thickness)
{
	const double T = Thickness;
	const double HalfT = T * 0.5;
	const FVector2D Min = Rect.Min - FVector2D(HalfT);
	const FVector2D Max = Rect.Max + FVector2D(HalfT);
	const FVector2D Size = Max - Min;

	RectDraws.Add(FRectDraw{ Min, FVector2D(Size.X, T), Color });                                 // top
	RectDraws.Add(FRectDraw{ FVector2D(Min.X, Max.Y - T), FVector2D(Size.X, T), Color });          // bottom
	RectDraws.Add(FRectDraw{ FVector2D(Min.X, Min.Y + T), FVector2D(T, Size.Y - 2.0 * T), Color }); // left
	RectDraws.Add(FRectDraw{ FVector2D(Max.X - T, Min.Y + T), FVector2D(T, Size.Y - 2.0 * T), Color }); // right
}

void UVTTOverlayWidget::AddTextLines(const TArray<FString>& Lines, const TArray<float>& Widths, float LineHeight, bool bLabel,
	float Left, float Top, float BlockWidth, bool bAlignRight, const FLinearColor& Color, int32& InOutCharBudget)
{
	for (int32 LineIndex = 0; LineIndex < Lines.Num() && InOutCharBudget > 0; ++LineIndex)
	{
		const FString& Line = Lines[LineIndex];
		const float X = bAlignRight ? Left + BlockWidth - Widths[LineIndex] : Left;
		const FVector2D Position(X, Top + LineIndex * LineHeight);

		// Typewriter: reveal from the left. Right-aligned lines keep their final position.
		const FString Visible = InOutCharBudget >= Line.Len() ? Line : Line.Left(InOutCharBudget);
		if (!Visible.IsEmpty())
		{
			TextDraws.Add(FTextDraw{ Position, Visible, bLabel, Color });
		}
		InOutCharBudget = InOutCharBudget == MAX_int32 ? MAX_int32 : InOutCharBudget - Line.Len();
	}
}

// ---------------------------------------------------------------------------------------------
// Paint
// ---------------------------------------------------------------------------------------------

int32 UVTTOverlayWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 Layer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

	++Layer;
	for (const FRectDraw& Rect : RectDraws)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(FVector2f(Rect.Size), FSlateLayoutTransform(FVector2f(Rect.Position))),
			&WhiteBrush, ESlateDrawEffect::None, Rect.Color);
	}

	for (const FLineDraw& Line : LineDraws)
	{
		TArray<FVector2f> Points;
		Points.Add(FVector2f(Line.Start));
		Points.Add(FVector2f(Line.End));
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(),
			MoveTemp(Points), ESlateDrawEffect::None, Line.Color, /*bAntialias*/ true, Line.Thickness);
	}

	++Layer;
	const UVTTSettings* Settings = UVTTSettings::Get();
	const FVector2f TextDrawSize(Settings->TextMaxWidth * 2.f, static_cast<float>(Settings->FontSize) * 3.f);
	for (const FTextDraw& Text : TextDraws)
	{
		FSlateDrawElement::MakeText(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(TextDrawSize, FSlateLayoutTransform(FVector2f(Text.Position))),
			Text.Text, Text.bLabel ? LabelFont : BodyFont, ESlateDrawEffect::None, Text.Color);
	}

	return Layer;
}
