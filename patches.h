#pragma once

#include <windows.h>

void ApplyFramelockPatch();
void ApplyAutolootPatch();
void ApplyIntroPatch();
void ApplyBorderlessPatch();
void ApplyFovPatch();
void ApplyResPatch();
void ApplyResScalingFix();

inline bool initialized = false;

inline BOOL FULLSCREEN_STATE;
inline int FPS_LIMIT;
inline int FOV_VALUE;
inline int CUSTOM_RES_ENABLED;
inline int CUSTOM_RES_WIDTH;
inline int CUSTOM_RES_HEIGHT;
inline int FPSUNLOCK_ENABLED;
inline int AUTOLOOT_ENABLED;
inline int INTROSKIP_ENABLED;
inline int BORDERLESS_ENABLED;
inline int FOV_ENABLED;
inline int SHOW_PLAYER_DEATHSKILLS_ENABLED;
inline int PLAYER_DEATHSKILLS_X;
inline int PLAYER_DEATHSKILLS_Y;
inline int PLAYER_DEATHSKILLS_FZ;
