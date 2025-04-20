#pragma once
#include <atomic>
#include <cstdint>

void ApplyFramelockPatch();
void ApplyAutolootPatch();
void ApplyIntroPatch();
void ApplyBorderlessPatch();
void ApplyFovPatch();
void ApplyResPatch();
void ApplyResScalingFix();
bool ApplyTimescalePatch();
void ApplyPlayerTimescalePatch();
void ApplyCamResetPatch();
void ApplyCamAutorotatePatch();
void ApplyFullscreenHZPatch();
void ApplyDeathPenaltiesPatch();
void ApplyDragonrotPatch();

inline std::atomic_bool INITIALIZED = false;
inline uint8_t GAME_LOADING;
