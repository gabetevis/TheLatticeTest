#include "Player/MegaDefaultInput.h"

#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace MegaDefaultInput
{
	static UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType ValueType)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, FName(Name), RF_Transient);
		Action->ValueType = ValueType;
		return Action;
	}

	static void AddSwizzle(UInputMappingContext* Context, FEnhancedActionKeyMapping& Mapping)
	{
		// Default order is YXZ: turns a 1D key press into the Y (forward) axis.
		Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Context));
	}

	static void AddNegate(UInputMappingContext* Context, FEnhancedActionKeyMapping& Mapping)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
	}

	static void AddDeadZone(UInputMappingContext* Context, FEnhancedActionKeyMapping& Mapping)
	{
		Mapping.Modifiers.Add(NewObject<UInputModifierDeadZone>(Context));
	}
}

FMegaDefaultInput FMegaDefaultInput::Create(UObject* Outer)
{
	using namespace MegaDefaultInput;

	FMegaDefaultInput Result;
	UInputMappingContext* Context = NewObject<UInputMappingContext>(Outer, TEXT("IMC_MegaDefault"), RF_Transient);
	Result.MappingContext = Context;

	Result.Move = MakeAction(Outer, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	Result.Look = MakeAction(Outer, TEXT("IA_Look"), EInputActionValueType::Axis2D);
	Result.Jump = MakeAction(Outer, TEXT("IA_Jump"), EInputActionValueType::Boolean);
	Result.Dash = MakeAction(Outer, TEXT("IA_Dash"), EInputActionValueType::Boolean);
	Result.Slide = MakeAction(Outer, TEXT("IA_Slide"), EInputActionValueType::Boolean);
	Result.Grapple = MakeAction(Outer, TEXT("IA_Grapple"), EInputActionValueType::Boolean);

	// Note: each MapKey may reallocate the mapping array, so each returned reference is used
	// immediately and never kept across MapKey calls.

	// Move: X = right, Y = forward.
	{ FEnhancedActionKeyMapping& M = Context->MapKey(Result.Move, EKeys::W); AddSwizzle(Context, M); }
	{ FEnhancedActionKeyMapping& M = Context->MapKey(Result.Move, EKeys::S); AddSwizzle(Context, M); AddNegate(Context, M); }
	{ FEnhancedActionKeyMapping& M = Context->MapKey(Result.Move, EKeys::A); AddNegate(Context, M); }
	Context->MapKey(Result.Move, EKeys::D);
	{ FEnhancedActionKeyMapping& M = Context->MapKey(Result.Move, EKeys::Gamepad_Left2D); AddDeadZone(Context, M); }

	// Look: raw values; AMegaCharacter converts them to degrees (positive Y = look up).
	Context->MapKey(Result.Look, EKeys::Mouse2D);
	{
		FEnhancedActionKeyMapping& M = Context->MapKey(Result.Look, EKeys::Gamepad_Right2D);
		AddDeadZone(Context, M);
		// Sticks report a deflection, not a delta: scale by frame time to get a turn rate.
		M.Modifiers.Add(NewObject<UInputModifierScaleByDeltaTime>(Context));
		UInputModifierScalar* Scalar = NewObject<UInputModifierScalar>(Context);
		Scalar->Scalar = FVector(70.f, 50.f, 1.f);
		M.Modifiers.Add(Scalar);
	}

	Context->MapKey(Result.Jump, EKeys::SpaceBar);
	Context->MapKey(Result.Jump, EKeys::Gamepad_FaceButton_Bottom);

	Context->MapKey(Result.Dash, EKeys::LeftShift);
	Context->MapKey(Result.Dash, EKeys::Gamepad_LeftShoulder);

	Context->MapKey(Result.Slide, EKeys::LeftControl);
	Context->MapKey(Result.Slide, EKeys::Gamepad_FaceButton_Right);

	Context->MapKey(Result.Grapple, EKeys::F);
	Context->MapKey(Result.Grapple, EKeys::Gamepad_RightShoulder);

	return Result;
}
