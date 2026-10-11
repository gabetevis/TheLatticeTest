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
		BuildCallout(Callout, MyGeometry.GetLocalSize(), Controller, InDeltaTime);
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

void UVTTOverlayWidget::BuildCallout(const FVTTActiveCallout& Callout, const FVector2D& ViewSize, APlayerController* Controller, float DeltaTime)
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

	// --- Direction: glide around the box toward open screen space ---
	FCalloutLayout& Layout = GetOrBuildLayout(Callout);
	if (!Layout.bAngleInitialized)
	{
		Layout.AngleDeg = Settings->bFollowOpenSpace ? ChooseAngle(Rect, ViewSize, Layout) : Settings->LineAngleDegrees;
		Layout.bAngleInitialized = true;
	}
	else if (Settings->bFollowOpenSpace && !Callout.bDismissing)
	{
		const float TargetAngle = ChooseAngle(Rect, ViewSize, Layout);
		const float Delta = FMath::FindDeltaAngleDegrees(Layout.AngleDeg, TargetAngle);
		Layout.AngleDeg = static_cast<float>(FRotator::NormalizeAxis(Layout.AngleDeg + Delta * FMath::Min(1.f, DeltaTime * Settings->LineTurnSpeed)));
	}

	const FCalloutPlacement Placement = ComputePlacement(Rect, Layout.AngleDeg, Layout);

	// --- Line: from the box edge outward, extending during the line intro ---
	const FVector2D DrawnEnd = Callout.bDismissing
		? Placement.LineEnd
		: FMath::Lerp(Placement.LineStart, Placement.LineEnd, FMath::Clamp(LineT, 0.f, 1.f));
	LineDraws.Add(FLineDraw{ Placement.LineStart, DrawnEnd, Color, Settings->LineThickness });

	if (Age < TypeStart && !Callout.bDismissing)
	{
		return;
	}

	// --- Text: hangs off the line end, kept on screen ---
	constexpr double ScreenMargin = 8.0;
	const double Left = FMath::Clamp(Placement.TextRect.Min.X, ScreenMargin, FMath::Max(ScreenMargin, ViewSize.X - ScreenMargin - Layout.BlockWidth));
	const double Top = FMath::Clamp(Placement.TextRect.Min.Y, ScreenMargin, FMath::Max(ScreenMargin, ViewSize.Y - ScreenMargin - Layout.BlockHeight));

	int32 CharBudget = (Settings->bTypewriter && !Callout.bDismissing)
		? FMath::FloorToInt((Age - TypeStart) * Settings->CharactersPerSecond)
		: MAX_int32;

	// Line pointing right -> left-aligned text, left -> right-aligned, up/down -> centered; blended.
	const float Align = FMath::Clamp(0.5f - static_cast<float>(Placement.Direction.X), 0.f, 1.f);
	AddTextLines(Layout.LabelLines, Layout.LabelLineWidths, Layout.LabelLineHeight, /*bLabel*/ true,
		static_cast<float>(Left), static_cast<float>(Top), Layout.BlockWidth, Align, Color, CharBudget);
	AddTextLines(Layout.BodyLines, Layout.BodyLineWidths, Layout.BodyLineHeight, /*bLabel*/ false,
		static_cast<float>(Left), static_cast<float>(Top + Layout.LabelLines.Num() * Layout.LabelLineHeight),
		Layout.BlockWidth, Align, Color, CharBudget);
}

// ---------------------------------------------------------------------------------------------
// Placement
// ---------------------------------------------------------------------------------------------

UVTTOverlayWidget::FCalloutPlacement UVTTOverlayWidget::ComputePlacement(const FBox2D& Rect, float AngleDeg, const FCalloutLayout& Layout)
{
	const UVTTSettings* Settings = UVTTSettings::Get();
	const float AngleRad = FMath::DegreesToRadians(AngleDeg);

	FCalloutPlacement Placement;
	Placement.Direction = FVector2D(FMath::Cos(AngleRad), -FMath::Sin(AngleRad)); // screen Y points down

	// Distance from a box/block center to its edge along a direction.
	auto DistanceToEdge = [](const FVector2D& Dir, double HalfWidth, double HalfHeight)
	{
		const double TX = FMath::Abs(Dir.X) > UE_KINDA_SMALL_NUMBER ? HalfWidth / FMath::Abs(Dir.X) : UE_BIG_NUMBER;
		const double TY = FMath::Abs(Dir.Y) > UE_KINDA_SMALL_NUMBER ? HalfHeight / FMath::Abs(Dir.Y) : UE_BIG_NUMBER;
		return FMath::Min(TX, TY);
	};

	const FVector2D Center = Rect.GetCenter();
	const FVector2D HalfBox = Rect.GetExtent();
	Placement.LineStart = Center + Placement.Direction * DistanceToEdge(Placement.Direction, HalfBox.X, HalfBox.Y);
	Placement.LineEnd = Placement.LineStart + Placement.Direction * Settings->LineLength;

	// Text block placed so the side facing the line sits TextGap past the line end.
	const FVector2D HalfText(Layout.BlockWidth * 0.5, Layout.BlockHeight * 0.5);
	const FVector2D Anchor = Placement.LineEnd + Placement.Direction * Settings->TextGap;
	const FVector2D TextCenter = Anchor + Placement.Direction * DistanceToEdge(Placement.Direction, HalfText.X, HalfText.Y);
	Placement.TextRect = FBox2D(TextCenter - HalfText, TextCenter + HalfText);
	return Placement;
}

float UVTTOverlayWidget::ChooseAngle(const FBox2D& Rect, const FVector2D& ViewSize, const FCalloutLayout& Layout)
{
	const UVTTSettings* Settings = UVTTSettings::Get();
	constexpr double Margin = 8.0;

	// Preferred direction, pulled toward the screen center the further the box is from it.
	const FVector2D ScreenCenter = ViewSize * 0.5;
	const FVector2D ToCenter = ScreenCenter - Rect.GetCenter();
	const float TowardCenterDeg = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(-ToCenter.Y), static_cast<float>(ToCenter.X)));
	const double EdgeFactor = FMath::Clamp(ToCenter.Size() / (0.35 * FMath::Min(ViewSize.X, ViewSize.Y)), 0.0, 1.0);
	const float CenterWeight = static_cast<float>(EdgeFactor) * Settings->PullTowardScreenCenter;

	auto AngleDistance = [](float A, float B) { return FMath::Abs(FMath::FindDeltaAngleDegrees(A, B)) / 180.f; };

	float BestAngle = Layout.bAngleInitialized ? Layout.AngleDeg : Settings->LineAngleDegrees;
	float BestScore = TNumericLimits<float>::Max();

	constexpr int32 NumCandidates = 36; // every 10 degrees
	for (int32 i = 0; i <= NumCandidates; ++i)
	{
		// Last candidate is the current angle, so staying put is always an option.
		const float Angle = i < NumCandidates ? -180.f + i * (360.f / NumCandidates) : BestAngle;
		const FCalloutPlacement P = ComputePlacement(Rect, Angle, Layout);

		// How far the text and line end poke off screen (pixels).
		const double Overflow =
			FMath::Max(0.0, Margin - P.TextRect.Min.X) + FMath::Max(0.0, P.TextRect.Max.X - (ViewSize.X - Margin)) +
			FMath::Max(0.0, Margin - P.TextRect.Min.Y) + FMath::Max(0.0, P.TextRect.Max.Y - (ViewSize.Y - Margin)) +
			FMath::Max(0.0, -P.LineEnd.X) + FMath::Max(0.0, P.LineEnd.X - ViewSize.X) +
			FMath::Max(0.0, -P.LineEnd.Y) + FMath::Max(0.0, P.LineEnd.Y - ViewSize.Y);

		// Text covering the framed object is bad too.
		double Overlap = 0.0;
		if (P.TextRect.Intersect(Rect))
		{
			const FVector2D OverlapSize = FVector2D::Min(P.TextRect.Max, Rect.Max) - FVector2D::Max(P.TextRect.Min, Rect.Min);
			Overlap = FMath::Max(0.0, OverlapSize.X) * FMath::Max(0.0, OverlapSize.Y);
		}

		const float Score =
			static_cast<float>(Overflow * 0.05 + Overlap * 0.0005) +
			AngleDistance(Angle, Settings->LineAngleDegrees) * (1.f - CenterWeight) +
			AngleDistance(Angle, TowardCenterDeg) * CenterWeight +
			(Layout.bAngleInitialized ? AngleDistance(Angle, Layout.AngleDeg) * 0.6f : 0.f); // stickiness: no jitter

		if (Score < BestScore)
		{
			BestScore = Score;
			BestAngle = Angle;
		}
	}
	return BestAngle;
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
	float Left, float Top, float BlockWidth, float Align, const FLinearColor& Color, int32& InOutCharBudget)
{
	for (int32 LineIndex = 0; LineIndex < Lines.Num() && InOutCharBudget > 0; ++LineIndex)
	{
		const FString& Line = Lines[LineIndex];
		const float X = Left + (BlockWidth - Widths[LineIndex]) * Align;
		const FVector2D Position(X, Top + LineIndex * LineHeight);

		// Typewriter: reveal from the left. Lines keep their final position while typing.
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
