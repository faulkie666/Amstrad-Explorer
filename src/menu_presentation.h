#pragma once

namespace cpcexplorer::menu_presentation {

inline constexpr char Instruction[] = "Pick a game";
inline constexpr char Signature[] = "Faulkie";
inline constexpr int SignatureScale = 1;
inline constexpr int SignatureWidth = 41;
inline constexpr int SignatureHeight = 7;
inline constexpr int SignatureX = 275;
inline constexpr int SignatureY = 232;

static_assert(SignatureX + SignatureWidth <= 320 - 4);
static_assert(SignatureY > 224 + 7);
static_assert(SignatureY + SignatureHeight <= 240 - 1);

}  // namespace cpcexplorer::menu_presentation
