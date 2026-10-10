#include "VTT/VTTOverlayWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
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

	static void WrapText(const FString& Source, const FSlateFontInfo& Font, float MaxWidth,
		const TSharedRef<FSlateFontMeasure>& Measure, TArray<FString>& OutLines)
	{
		const FString Clean = Source.Replace(TEXT("\r"), TEXT(""));
		if (Clean.IsEmpty())
		{
			return;
		}

		TArray<FString> Paragraphs;
		Clean.ParseIntoArray(Paragraphs, TEXT("\n"), /*bCullEmpty*/ false);

		for (const FString& Paragraph : Paragraphs)
		{
			TArray<FString> Words;
			Paragraph.ParseIntoArrayWS(Words);
			if (Words.Num() == 0)
			{
				OutLines.Add(FString());
				continue;
			}

			FString Line;
			for (const FString& Word : Words)
			{
				const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
				if (!Line.IsEmpty() && Measure->Measure(Candidate, Font).X > MaxWidth)
				{
					OutLines.Add(Line);
					Line = Word;
				}
				else
				{
					Line = Candidate;
				}
			}
			OutLines.Add(Line);
		}
	}

	static FString Scramble(const FString& Source)
	{
		static const TCHAR Glyphs[] = TEXT("#%&*+=<>/\\[]{}?!0123456789ABCDEFXZ");
		constexpr int32 NumGlyphs = UE_ARRAY_COUNT(Glyphs) - 1;

		FString Result = Source;
		for (int32 i = 0; i < Result.Len(); ++i)
		{
			if (!FChar::IsWhitespace(Result[i]))
			{
				Result[i] = Glyphs[FMath::RandRange(0, NumGlyphs - 1)];
			}
		}
		return Result;
	}
}

UVTTOverlayWidget::UVTTOverlayWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, WhiteBrush(FLinearColor::White)
{
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
		LabelFont = FSlateFontInfo(Font, static_cast<float>(Settings->LabelFontSize));
		BodyFont = FSlateFontInfo(Font, static_cast<float>(Settings->BodyFontSize));
	}
	else
	{
		LabelFont = FCoreStyle::GetDefaultFontStyle("Bold", Settings->LabelFontSize);
		BodyFont = FCoreStyle::GetDefaultFontStyle("Regular", Settings->BodyFontSize);
	}

	LabelFont.LetterSpacing = Settings->LetterSpacing;
	BodyFont.LetterSpacing = Settings->LetterSpacing;
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
	for (auto It = TextLayouts.CreateIterator(); It; ++It)
	{
		const int32 Id = It.Key();
		if (!Callouts.ContainsByPredicate([Id](const FVTTActiveCallout& Callout) { return Callout.Id == Id; }))
		{
			It.RemoveCurrent();
		}
	}

	for (int32 Index = 0; Index < Callouts.Num(); ++Index)
	{
		BuildCallout(Callouts[Index], Index, MyGeometry, Controller);
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

UVTTOverlayWidget::FCalloutTextLayout& UVTTOverlayWidget::GetOrBuildTextLayout(const FVTTActiveCallout& Callout)
{
	if (FCalloutTextLayout* Existing = TextLayouts.Find(Callout.Id))
	{
		return *Existing;
	}

	const UVTTSettings* Settings = UVTTSettings::Get();
	const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

	FCalloutTextLayout& Layout = TextLayouts.Add(Callout.Id);
	VTTDraw::WrapText(Callout.Label.ToString(), LabelFont, Settings->TextMaxWidth, Measure, Layout.LabelLines);
	VTTDraw::WrapText(Callout.Body.ToString(), BodyFont, Settings->TextMaxWidth, Measure, Layout.BodyLines);
	Layout.LabelLineHeight = Measure->GetMaxCharacterHeight(LabelFont);
	Layout.BodyLineHeight = Measure->GetMaxCharacterHeight(BodyFont);

	for (const FString& Line : Layout.LabelLines)
	{
		Layout.BlockWidth = FMath::Max(Layout.BlockWidth, static_cast<float>(Measure->Measure(Line, LabelFont).X));
	}
	for (const FString& Line : Layout.BodyLines)
	{
		Layout.BlockWidth = FMath::Max(Layout.BlockWidth, static_cast<float>(Measure->Measure(Line, BodyFont).X));
	}
	return Layout;
}

void UVTTOverlayWidget::BuildCallout(const FVTTActiveCallout& Callout, int32 StackIndex, const FGeometry& Geometry, APlayerController* Controller)
{
	using namespace VTTDraw;

	const UVTTSettings* Settings = UVTTSettings::Get();
	const FVector2D ViewSize = Geometry.GetLocalSize();

	FBox2D TargetRect;
	if (!ProjectBounds(Callout.LastWorldBounds, Controller, TargetRect))
	{
		return; // target behind the camera (e.g. during outro): nothing to anchor to
	}

	// Pad, enforce a minimum size, keep on screen.
	FVector2D Center = TargetRect.GetCenter();
	FVector2D HalfSize = TargetRect.GetExtent() + FVector2D(Settings->BoxPadding);
	HalfSize.X = FMath::Max(HalfSize.X, Settings->MinBoxSize * 0.5f);
	HalfSize.Y = FMath::Max(HalfSize.Y, Settings->MinBoxSize * 0.5f);
	FBox2D Rect(Center - HalfSize, Center + HalfSize);
	Rect.Min = FVector2D::Max(Rect.Min, FVector2D::ZeroVector);
	Rect.Max = FVector2D::Min(Rect.Max, ViewSize);
	Center = Rect.GetCenter();
	HalfSize = Rect.GetExtent();

	// --- Animation state ---
	const float Age = Callout.Age;
	const float BoxIn = Settings->BoxIntroDuration;
	const float LineIn = Settings->LineIntroDuration;
	const float TypeStart = BoxIn + LineIn;

	const float BoxT = BoxIn > 0.f ? FMath::Clamp(Age / BoxIn, 0.f, 1.f) : 1.f;
	const float LineT = LineIn > 0.f ? FMath::Clamp((Age - BoxIn) / LineIn, 0.f, 1.f) : (Age >= BoxIn ? 1.f : 0.f);

	float Alpha = 1.f;
	bool bGlitching = BoxT < 1.f;
	if (Callout.bDismissing)
	{
		Alpha = 1.f - FMath::Clamp((Age - Callout.DismissStartAge) / Settings->OutroDuration, 0.f, 1.f);
		bGlitching = true;
	}

	FVector2D Jitter = FVector2D::ZeroVector;
	if (Settings->bGlitchEffects && bGlitching)
	{
		if (FMath::FRand() < 0.3f)
		{
			Alpha *= 0.3f;
		}
		Jitter = FVector2D(FMath::FRandRange(-2.f, 2.f), FMath::FRandRange(-1.f, 1.f));
	}

	// --- Frame: grows out from the center ---
	const float BoxEase = 1.f - FMath::Pow(1.f - BoxT, 3.f);
	AddFrame(FBox2D(Center - HalfSize * BoxEase + Jitter, Center + HalfSize * BoxEase + Jitter), Alpha);

	if (Age < BoxIn && !Callout.bDismissing)
	{
		return;
	}

	// --- Text placement ---
	FCalloutTextLayout& Layout = GetOrBuildTextLayout(Callout);
	if (Layout.SideX == 0)
	{
		Layout.SideX = Center.X < ViewSize.X * 0.5f ? 1 : -1;  // text toward the emptier half
		Layout.SideY = Center.Y > ViewSize.Y * 0.4f ? -1 : 1;
	}
	const float SideX = static_cast<float>(Layout.SideX);
	const float SideY = static_cast<float>(Layout.SideY);

	const FVector2D Anchor(
		(Layout.SideX > 0 ? Rect.Max.X : Rect.Min.X) + Jitter.X,
		(Layout.SideY < 0 ? Rect.Min.Y : Rect.Max.Y) + Jitter.Y);

	const float Diagonal = Settings->LeaderLength * UE_INV_SQRT_2;
	FVector2D Elbow = Anchor + FVector2D(SideX * Diagonal, SideY * (Diagonal + StackIndex * 22.f));

	const float LabelHeight = Layout.LabelLines.Num() * Layout.LabelLineHeight;
	const float BodyHeight = Layout.BodyLines.Num() * Layout.BodyLineHeight;
	const float Pad = Settings->TextBackdropPadding;

	float TextX = Layout.SideX > 0 ? Elbow.X + Settings->ShelfLength : Elbow.X - Settings->ShelfLength - Layout.BlockWidth;

	// Keep the text block on screen by sliding elbow + text together (the anchor stays put).
	{
		const float MinX = TextX - Pad;
		const float MaxX = TextX + Layout.BlockWidth + Pad;
		const float MinY = Elbow.Y - Settings->TextGap - LabelHeight - Pad;
		const float MaxY = Elbow.Y + Settings->TextGap + BodyHeight + Pad;
		constexpr float ScreenMargin = 8.f;

		FVector2D Shift = FVector2D::ZeroVector;
		if (MinX < ScreenMargin) { Shift.X = ScreenMargin - MinX; }
		else if (MaxX > ViewSize.X - ScreenMargin) { Shift.X = ViewSize.X - ScreenMargin - MaxX; }
		if (MinY < ScreenMargin) { Shift.Y = ScreenMargin - MinY; }
		else if (MaxY > ViewSize.Y - ScreenMargin) { Shift.Y = ViewSize.Y - ScreenMargin - MaxY; }

		Elbow += Shift;
		TextX += Shift.X;
	}

	const float LabelTop = Elbow.Y - Settings->TextGap - LabelHeight;
	const float BodyTop = Elbow.Y + Settings->TextGap;
	// The shelf continues under the label, so the line doubles as its underline.
	const FVector2D ShelfEnd(Layout.SideX > 0 ? TextX + Layout.BlockWidth : TextX, Elbow.Y);

	// --- Leader line: drawn progressively during the line intro ---
	{
		const float FirstLength = FVector2D::Distance(Anchor, Elbow);
		const float SecondLength = FVector2D::Distance(Elbow, ShelfEnd);
		float Remaining = (FirstLength + SecondLength) * LineT;

		TArray<FVector2D> Points;
		Points.Add(Anchor);
		if (Remaining <= FirstLength)
		{
			Points.Add(FMath::Lerp(Anchor, Elbow, FirstLength > 0.f ? Remaining / FirstLength : 1.f));
		}
		else
		{
			Remaining -= FirstLength;
			Points.Add(Elbow);
			Points.Add(FMath::Lerp(Elbow, ShelfEnd, SecondLength > 0.f ? Remaining / SecondLength : 1.f));
		}
		AddPolyline(MoveTemp(Points), WithAlpha(Settings->FrameColor, Alpha), Settings->LineThickness);

		// Small square marking where the line leaves the frame.
		RectDraws.Add({ Anchor - FVector2D(2.f), FVector2D(4.f), WithAlpha(Settings->FrameColor, Alpha) });
	}

	if (Age < TypeStart && !Callout.bDismissing)
	{
		return;
	}

	// --- Backdrop + typed text ---
	if (Settings->TextBackdropColor.A > 0.f)
	{
		const FVector2D BackdropPos(TextX - Pad, LabelTop - Pad);
		const FVector2D BackdropSize(Layout.BlockWidth + 2.f * Pad, (BodyTop + BodyHeight) - LabelTop + 2.f * Pad);
		RectDraws.Insert({ BackdropPos, BackdropSize, WithAlpha(Settings->TextBackdropColor, Alpha) }, 0);
	}

	const bool bFullyRevealed = Callout.bDismissing;
	int32 CharBudget = FMath::FloorToInt((Age - TypeStart) * Settings->CharactersPerSecond);
	AddTextLines(Layout.LabelLines, FVector2D(TextX, LabelTop), Layout.LabelLineHeight, true, Settings->LabelColor, Alpha, CharBudget, bFullyRevealed);
	AddTextLines(Layout.BodyLines, FVector2D(TextX, BodyTop), Layout.BodyLineHeight, false, Settings->BodyColor, Alpha, CharBudget, bFullyRevealed);
}

// ---------------------------------------------------------------------------------------------
// Draw-list helpers
// ---------------------------------------------------------------------------------------------

void UVTTOverlayWidget::AddPolyline(TArray<FVector2D> Points, const FLinearColor& Color, float Thickness)
{
	if (Points.Num() < 2)
	{
		return;
	}

	FLineDraw& Line = LineDraws.AddDefaulted_GetRef();
	Line.Points.Reserve(Points.Num());
	for (const FVector2D& Point : Points)
	{
		Line.Points.Add(FVector2f(Point));
	}
	Line.Color = Color;
	Line.Thickness = Thickness;
}

void UVTTOverlayWidget::AddFrame(const FBox2D& Rect, float Alpha)
{
	using namespace VTTDraw;

	const UVTTSettings* Settings = UVTTSettings::Get();
	const FVector2D Min = Rect.Min;
	const FVector2D Max = Rect.Max;

	// Faint full outline.
	if (Settings->FrameOutlineOpacity > 0.f)
	{
		AddPolyline({ Min, FVector2D(Max.X, Min.Y), Max, FVector2D(Min.X, Max.Y), Min },
			WithAlpha(Settings->FrameColor, Alpha * Settings->FrameOutlineOpacity), 1.f);
	}

	// Solid corner brackets.
	const FVector2D Size = Rect.GetSize();
	const float Corner = FMath::Min3(Settings->CornerLength, Size.X * 0.5f, Size.Y * 0.5f);
	const FLinearColor Color = WithAlpha(Settings->FrameColor, Alpha);
	const float Thickness = Settings->FrameThickness;

	AddPolyline({ FVector2D(Min.X, Min.Y + Corner), Min, FVector2D(Min.X + Corner, Min.Y) }, Color, Thickness);
	AddPolyline({ FVector2D(Max.X - Corner, Min.Y), FVector2D(Max.X, Min.Y), FVector2D(Max.X, Min.Y + Corner) }, Color, Thickness);
	AddPolyline({ FVector2D(Max.X, Max.Y - Corner), Max, FVector2D(Max.X - Corner, Max.Y) }, Color, Thickness);
	AddPolyline({ FVector2D(Min.X + Corner, Max.Y), FVector2D(Min.X, Max.Y), FVector2D(Min.X, Max.Y - Corner) }, Color, Thickness);
}

void UVTTOverlayWidget::AddTextLines(const TArray<FString>& Lines, const FVector2D& TopLeft, float LineHeight, bool bLabelFont,
	const FLinearColor& Color, float Alpha, int32& InOutCharBudget, bool bFullyRevealed)
{
	using namespace VTTDraw;

	const UVTTSettings* Settings = UVTTSettings::Get();
	const FSlateFontInfo& Font = bLabelFont ? LabelFont : BodyFont;
	const FVector2D DrawSize(Settings->TextMaxWidth * 1.5f, LineHeight);

	for (int32 LineIndex = 0; LineIndex < Lines.Num(); ++LineIndex)
	{
		if (!bFullyRevealed && InOutCharBudget < 0)
		{
			return; // typing cursor is above this line
		}

		const FString& Line = Lines[LineIndex];
		const FVector2D Position(TopLeft.X, TopLeft.Y + LineIndex * LineHeight);

		if (bFullyRevealed || InOutCharBudget >= Line.Len())
		{
			TextDraws.Add({ Position, DrawSize, Line, bLabelFont, WithAlpha(Color, Alpha) });
			InOutCharBudget -= Line.Len();
			continue;
		}

		// Partially typed: revealed text, then a run of scrambled glyphs ahead of the cursor.
		const FString Revealed = Line.Left(InOutCharBudget);
		const int32 NumScrambled = FMath::Min(Settings->ScrambleLeadCharacters, Line.Len() - InOutCharBudget);

		if (!Revealed.IsEmpty())
		{
			TextDraws.Add({ Position, DrawSize, Revealed, bLabelFont, WithAlpha(Color, Alpha) });
		}
		if (NumScrambled > 0)
		{
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			const float Offset = Revealed.IsEmpty() ? 0.f : static_cast<float>(Measure->Measure(Revealed, Font).X);
			TextDraws.Add({ Position + FVector2D(Offset, 0.f), DrawSize, Scramble(Line.Mid(InOutCharBudget, NumScrambled)),
				bLabelFont, WithAlpha(Settings->ScrambleColor, Alpha) });
		}

		InOutCharBudget = -1; // everything after the cursor stays hidden
		return;
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

	++Layer;
	for (const FLineDraw& Line : LineDraws)
	{
		FSlateDrawElement::MakeLines(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(),
			Line.Points, ESlateDrawEffect::None, Line.Color, /*bAntialias*/ true, Line.Thickness);
	}

	++Layer;
	for (const FTextDraw& Text : TextDraws)
	{
		FSlateDrawElement::MakeText(OutDrawElements, Layer,
			AllottedGeometry.ToPaintGeometry(FVector2f(Text.Size), FSlateLayoutTransform(FVector2f(Text.Position))),
			Text.Text, Text.bLabelFont ? LabelFont : BodyFont, ESlateDrawEffect::None, Text.Color);
	}

	return Layer;
}
