#pragma once
#include <cstdint>

constexpr uintptr_t fps_offset = 0x11AB550;
constexpr uintptr_t speedfix_offset = 0x7DE8AD;
constexpr uintptr_t autoloot_offset = 0x91C606;
constexpr uintptr_t introskip_offset = 0xE1B51B;
constexpr uintptr_t fovsetting_offset = 0x73c688;
constexpr uintptr_t player_deaths_offset = 0x7b3c51;
constexpr uintptr_t total_kills_offset = 0x7c95e5;

constexpr uintptr_t resolution_pointer_offset = 0x1194d85;
constexpr uintptr_t resolution_default_3840_offset = 0x3b1d788;
constexpr uintptr_t resolution_default_2560_offset = 0x3b1d778;
constexpr uintptr_t resolution_default_1920_offset = 0x3b1d768;
constexpr uintptr_t resolution_default_1280_offset = 0x3b1d748;
constexpr uintptr_t resolution_default_840_offset = 0x3b1d730;
constexpr uintptr_t resolution_scaling_fix_offset = 0x12ac68;

constexpr uintptr_t camadjust_pitch_offset = 0x73e056;
constexpr uintptr_t camadjust_yaw_z_offset = 0x73e07c;
constexpr uintptr_t camadjust_pitch_xy_offset = 0x73e5df;
constexpr uintptr_t camadjust_yaw_xy_offset = 0x73e6cd;
constexpr uintptr_t camreset_lockon_offset = 0x73dece;
constexpr uintptr_t dragonrot_effect_offset = 0x11d383f;
constexpr uintptr_t deathpenalties1_offset = 0x11d36c7;
constexpr uintptr_t deathpenalties2_offset = 0x11d378b;
constexpr uintptr_t deaths_counter_offset = 0x69d73e;
constexpr uintptr_t emblem_upgrade_offset = 0xa9ab59;
constexpr uintptr_t timescale_offset = 0x1193f87; // also found at 0x1194f2b
constexpr uintptr_t timescale_player_offset = 0x6c1a57;

inline uintptr_t fps_addr;
inline uintptr_t speedfix_addr;
inline uintptr_t autoloot_addr;
inline uintptr_t introskip_addr;
inline uintptr_t player_deaths_addr;
inline uintptr_t total_kills_addr;
inline uintptr_t fovsetting_addr;
