#pragma once

#include "CoreMinimal.h"

class UInputAction;
class UInputMappingContext;

/**
 * Code-built Enhanced Input fallback, so the project runs with zero input assets.
 *
 * AMegaCharacter uses these only for slots left empty on the character. When you later want
 * rebinding through Enhanced Input user settings, create real IA_/IMC_ assets, assign them on a
 * child Blueprint of AMegaCharacter (or in its C++ defaults), and these defaults stop being used.
 *
 * Default bindings (keyboard/mouse | gamepad):
 *   Move   WASD                 | Left stick
 *   Look   Mouse                | Right stick
 *   Jump   Space                | A / Cross
 *   Dash   Left Shift           | Left bumper
 *   Slide  Left ALT             | B / Circle     (moving: slide, still: crouch, air: slam)
 *   Grapple F                   | RB / R1
 *   Fire   Left mouse           | RT / R2
 *   Reload R                    | X / Square
 */
struct FMegaDefaultInput
{
	UInputMappingContext* MappingContext = nullptr;
	UInputAction* Move = nullptr;
	UInputAction* Look = nullptr;
	UInputAction* Jump = nullptr;
	UInputAction* Dash = nullptr;
	UInputAction* Slide = nullptr;
	UInputAction* Grapple = nullptr;
	UInputAction* Fire = nullptr;
	UInputAction* Reload = nullptr;

	/** Creates transient actions and a mapping context owned by Outer. */
	static FMegaDefaultInput Create(UObject* Outer);
};
