// Read-only access to DK64's game state for the Android second screen (health, current Kong,
// collectibles). Values are copied out of the emulated N64 RDRAM; nothing is ever written.
//
// Addresses and layouts come from this repository: DK64Syms/data_dump.toml for symbol
// addresses (US 1.0) and patches/patches_ui.c for the PlayerProgress / CharacterProgress
// structures.

#include <android/log.h>
#include <jni.h>

#include <atomic>
#include <string>

#include "librecomp/mods.hpp"
#include "gpu_driver.h"
#include <cstdint>

namespace {
    std::atomic<uint8_t*> g_rdram{nullptr};
    std::atomic<bool> g_touch_kong_swap{false};
    std::atomic<uint32_t> g_input_set{0xFFFFFFFFu};  // patches/android_hooks.c
    std::atomic<uint32_t> g_frames{0};
    std::atomic<bool> g_show_fps{false};
    constexpr const char* kTagAnywhereModId = "dk64_tag_anywhere";

    // N64 virtual addresses (KSEG0) for the US 1.0 release.
    constexpr uint32_t kCurrentCharacterIndex = 0x8074E77C; // u8[]: 0 DK, 1 Diddy, 2 Lanky, 3 Tiny, 4 Chunky
    constexpr uint32_t kPlayerProgress = 0x807FC950;        // PlayerProgress, player 1

    // Permanent flags (saveFile.c: getFlagBlockAddress / isFlagSet). They live in the EEPROM
    // copy, in the slot that the file mapping assigns to the current file.
    constexpr uint32_t kEepromCopy = 0x807ECEA8;            // u8[]
    constexpr uint32_t kFileBitsBase = 0x807ECEA0;          // s16, used to size a save file
    constexpr uint32_t kFileMapping = 0x807EDEA8;           // u8[4]: file index per slot
    constexpr uint32_t kCurrentFile = 0x807467C8;           // u8
    constexpr uint32_t kCurrentMap = 0x8076A0A8;            // s32 (Maps)
    constexpr uint32_t kLevelIndexByMap = 0x807445E0;       // u8[map] (getLevelIndex)
    constexpr int kLevelSlots = 9;                          // 0-6 levels, 7 Isles, 8 Helm
    constexpr uint32_t kPlayerPointer = 0x807FBB4C; // Actor* gPlayerPointer
    constexpr uint32_t kGlobalProperties = 0x807FBB60;      // u32 global_properties_bitfield (2: paused / tag barrel)
    constexpr uint32_t kCutsceneActive = 0x807444EC;        // u8 is_cutscene_active
    constexpr uint32_t kPlayerGunOut = 0x20C;               // u8 in the player actor: gun held
    constexpr uint32_t kPlayerControlState = 0x154;         // u8 in the player actor
    // Control states 2-5: first-person view (looking, and aiming in first person).
    constexpr uint32_t kCharacterChange = 0x807FC924;       // CharacterChange* character_change_array
    constexpr uint32_t kCcSize = 0x2C0;                     // u8: 0 small, 1 normal, 2 big
    constexpr uint32_t kExtraPlayerInfo = 0x807FD580;       // PlayerAdditionalActorData* extra_player_info_pointer
    constexpr uint32_t kPaadFlags = 0x1F0;                  // s32 bitfield
    constexpr uint32_t kGameMode = 0x80755318;              // u8 game_mode (6: adventure, i.e. a file is loaded)
    constexpr uint8_t kGameModeAdventure = 6;

    // Per-control-state input handlers (code_E4090.c: D_global_asm_80751004[control_state]):
    // 17 function pointers per state, called for each button. A button that does nothing in
    // a state points at a shared do-nothing function.
    constexpr uint32_t kInputHandlers = 0x80751004;
    constexpr uint32_t kInputHandlerSize = 0x44;
    constexpr int kControlStates = 107;                     // up to action_initiation_function_list
    constexpr uint32_t kHandlerZ = 0x10;
    constexpr uint32_t kHandlerCUp = 0x30;

    // Reported as bits: "Kong freed" for DK, Diddy, Lanky, Tiny, Chunky, then orange
    // throwing (training barrel) and the fairy camera (with Shockwave).
    constexpr int16_t kProgressFlags[7] = { 0x181, 0x006, 0x046, 0x042, 0x075, 0x184, 0x179 };

    // PlayerProgress layout.
    constexpr uint32_t kCharacterProgressSize = 0x5E;       // 6 entries at offset 0
    constexpr uint32_t kStandardAmmo = 0x2F0;               // u16
    constexpr uint32_t kHomingAmmo = 0x2F2;                 // u16
    constexpr uint32_t kOranges = 0x2F4;                    // u16
    constexpr uint32_t kCrystals = 0x2F6;                   // u16
    constexpr uint32_t kFilm = 0x2F8;                       // u16
    constexpr uint32_t kHealth = 0x2FB;                     // s8, in quarter melons
    constexpr uint32_t kMelons = 0x2FC;                     // u8

    // CharacterProgress layout.
    constexpr uint32_t kCpMoves = 0x00;                     // u8: special moves bought (Cranky)
    constexpr uint32_t kCpWeapon = 0x02;                    // u8
    constexpr uint32_t kCpInstrument = 0x04;                // u8
    constexpr uint32_t kCpCoins = 0x06;                     // u16
    constexpr uint32_t kCpInstrumentAmmo = 0x08;            // u16
    constexpr uint32_t kCpColouredBananas = 0x0A;           // u16[14]
    constexpr uint32_t kCpBananasFedToTns = 0x26;           // u16[14]: spent on Troff 'n' Scoff (still collected)
    constexpr uint32_t kCpGoldenBananas = 0x42;             // u16[14]
    constexpr int kLevels = 14;

    // The recompiler stores RDRAM with 32-bit words byte-swapped (see MEM_BU / MEM_HU in
    // N64Recomp's recomp.h).
    uint8_t read_u8(const uint8_t* rdram, uint32_t address) {
        return rdram[(address ^ 3) - 0x80000000u];
    }
    uint16_t read_u16(const uint8_t* rdram, uint32_t address) {
        return *reinterpret_cast<const uint16_t*>(rdram + ((address ^ 2) - 0x80000000u));
    }

    uint32_t read_u32(const uint8_t* rdram, uint32_t address) {
        return *reinterpret_cast<const uint32_t*>(rdram + (address - 0x80000000u));
    }

    // Address of the current file's permanent flag block (saveFile.c: getFlagBlockAddress).
    uint32_t permanent_flag_block(const uint8_t* rdram) {
        const int32_t bits = static_cast<int16_t>(read_u16(rdram, kFileBitsBase));
        const int32_t file_size_bits = (((bits + 0x6B7) & -64) + 0x27) & -8;
        const uint8_t file = read_u8(rdram, kCurrentFile);
        uint32_t slot = 0;
        for (uint32_t i = 0; i < 4; i++) {
            if (read_u8(rdram, kFileMapping + i) == file) {
                slot = i;
                break;
            }
        }
        if (file_size_bits <= 0 || file_size_bits > 0x2000 * 8) {
            return 0;
        }
        return kEepromCopy + slot * static_cast<uint32_t>(file_size_bits / 8);
    }

    bool permanent_flag(const uint8_t* rdram, uint32_t block, int flag) {
        return block != 0 && flag >= 0 && ((read_u8(rdram, block + (flag >> 3)) >> (flag & 7)) & 1);
    }

    // Bit i set when kProgressFlags[i] is set in the current file's permanent flags.
    int read_progress_flags(const uint8_t* rdram) {
        const uint32_t block = permanent_flag_block(rdram);
        if (block == 0) {
            return 0;
        }
        int set = 0;
        for (int i = 0; i < 7; i++) {
            if (permanent_flag(rdram, block, kProgressFlags[i])) {
                set |= 1 << i;
            }
        }
        return set;
    }
}

// Called once from the renderer setup (RecompFrontend Android patch) with the RDRAM base.
extern "C" void dk64_android_set_rdram(uint8_t* rdram) {
    g_rdram.store(rdram);
}

// Output layout, mirrored in GameState.java:
//   [0] current Kong index, [1] health (quarters), [2] melons,
//   [3] standard ammo, [4] homing ammo, [5] oranges, [6] crystals, [7] film,
//   then for each of the 5 Kongs (8 + k * 6):
//   weapon, instrument, coins, instrument ammo, golden bananas (total), coloured bananas (total)
//   then [38] progress bits: Kongs freed (bit 0 DK ... bit 4 Chunky), 5 oranges, 6 camera
//   then [39] level index of the current map (0-6 levels, 7 Isles, 8 Helm; -1 elsewhere)
//   then [40 + (k * 9 + level) * 2] golden bananas, coloured bananas for Kong k in that level
//   then [130] 1 while the Kong is holding their gun out
namespace {
    uint32_t handler(const uint8_t* rdram, uint8_t state, uint32_t button) {
        return read_u32(rdram, kInputHandlers + state * kInputHandlerSize + button);
    }

    // Input handlers that mark a mode (see move_availability).
    constexpr uint32_t kZCombos[] = { 0x806E4E88u, 0x806E5044u };          // Z + C-Left / C-Right
    constexpr uint32_t kFirstPersonEntries[] = { 0x806EA958u, 0x806EAA44u, 0x806EA04Cu };
    constexpr uint32_t kSwimmingHandlers[] = { 0x806E6B98u, 0x806E65BCu }; // swimming steering
    constexpr uint32_t kFirstPersonUnderwater = 0x806EA04Cu;
    constexpr uint8_t kSetCrouching = 0x3C;
    constexpr uint8_t kSetWalking = 0x0D;

    bool in(uint32_t value, const uint32_t* list, size_t n) {
        for (size_t i = 0; i < n; i++) {
            if (list[i] == value) {
                return true;
            }
        }
        return false;
    }

    // Bits for the input-handler set the player is using (reported each frame by
    // patches/android_hooks.c): 4 Z + C moves work (standing, walking, crouching), 8 C-Up
    // enters first person, 32 swimming.
    jint move_availability(const uint8_t* rdram, uint32_t set) {
        if (set >= static_cast<uint32_t>(kControlStates)) {
            return 4 | 8; // not known yet: don't hide anything
        }
        const uint8_t s = static_cast<uint8_t>(set);
        const uint32_t z = handler(rdram, s, kHandlerZ);
        const uint32_t c_up = handler(rdram, s, kHandlerCUp);
        // handlers[1] is the patch's reporting wrapper; the swimming steering is the last one.
        const uint32_t last = handler(rdram, s, 0x40);
        jint bits = 0;
        if (s == kSetCrouching || s == kSetWalking || in(z, kZCombos, 2)) {
            bits |= 4;
        }
        if (in(c_up, kFirstPersonEntries, 3)) {
            bits |= 8;
        }
        if (in(last, kSwimmingHandlers, 2) || c_up == kFirstPersonUnderwater) {
            bits |= 32;
        }

        // Log each set once, to check these rules on devices.
        static std::atomic<uint64_t> seen[2]{};
        const uint64_t bit = 1ull << (s & 63);
        if ((seen[s >> 6].fetch_or(bit) & bit) == 0) {
            __android_log_print(ANDROID_LOG_INFO, "DK64Recomp", "Input set 0x%02X: Z %08X, C-Up %08X, last %08X -> Z+C %s, first person %s, swimming %s",
                s, z, c_up, last, (bits & 4) ? "yes" : "no", (bits & 8) ? "yes" : "no", (bits & 32) ? "yes" : "no");
        }
        return bits;
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_bownz_dk64recomp_GameState_nativeRead(JNIEnv* env, jclass, jintArray out) {
    const uint8_t* rdram = g_rdram.load();
    constexpr jsize kCount = 8 + 5 * 6 + 2 + 5 * kLevelSlots * 2 + 1;
    if (rdram == nullptr || out == nullptr || env->GetArrayLength(out) < kCount) {
        return JNI_FALSE;
    }

    jint values[kCount];
    values[0] = read_u8(rdram, kCurrentCharacterIndex);
    values[1] = static_cast<int8_t>(read_u8(rdram, kPlayerProgress + kHealth));
    values[2] = read_u8(rdram, kPlayerProgress + kMelons);
    values[3] = read_u16(rdram, kPlayerProgress + kStandardAmmo);
    values[4] = read_u16(rdram, kPlayerProgress + kHomingAmmo);
    values[5] = read_u16(rdram, kPlayerProgress + kOranges);
    values[6] = read_u16(rdram, kPlayerProgress + kCrystals);
    values[7] = read_u16(rdram, kPlayerProgress + kFilm);

    for (int k = 0; k < 5; k++) {
        const uint32_t cp = kPlayerProgress + k * kCharacterProgressSize;
        jint golden = 0, coloured = 0;
        for (int level = 0; level < kLevels; level++) {
            golden += read_u16(rdram, cp + kCpGoldenBananas + level * 2);
            coloured += read_u16(rdram, cp + kCpColouredBananas + level * 2) + read_u16(rdram, cp + kCpBananasFedToTns + level * 2);
        }
        jint* dst = values + 8 + k * 6;
        dst[0] = read_u8(rdram, cp + kCpWeapon) | (read_u8(rdram, cp + kCpMoves) << 8); // weapon | special moves << 8
        dst[1] = read_u8(rdram, cp + kCpInstrument);
        dst[2] = read_u16(rdram, cp + kCpCoins);
        dst[3] = read_u16(rdram, cp + kCpInstrumentAmmo);
        dst[4] = golden;
        dst[5] = coloured;
    }

    values[38] = read_progress_flags(rdram);
    {
        const uint32_t map = read_u32(rdram, kCurrentMap);
        const int level = map < 0xD8 ? read_u8(rdram, kLevelIndexByMap + map) : -1;
        values[39] = level < kLevelSlots ? level : -1;
    }
    for (int k = 0; k < 5; k++) {
        const uint32_t cp = kPlayerProgress + k * kCharacterProgressSize;
        for (int level = 0; level < kLevelSlots; level++) {
            jint* dst = values + 40 + (k * kLevelSlots + level) * 2;
            dst[0] = read_u16(rdram, cp + kCpGoldenBananas + level * 2);
            dst[1] = read_u16(rdram, cp + kCpColouredBananas + level * 2) + read_u16(rdram, cp + kCpBananasFedToTns + level * 2);
        }
    }

    {
        const uint32_t player = read_u32(rdram, kPlayerPointer);
        // Bits: 1 gun held, 2 first-person view, 4 Z + C moves possible now, 8 C-Up (first
        // person) possible now, 16 a save file is being played, 32 swimming, 64 paused, 128 a barrel ability is on; bits 8-15: the
        // player's control state; bits 16-23: the input-handler set (0xFF: not known).
        jint bits = 0;
        if (read_u8(rdram, kGameMode) == kGameModeAdventure) {
            bits |= 16;
        }
        if (read_u32(rdram, kGlobalProperties) & 2) {
            bits |= 64; // paused (code_450.c sets this once the pause menu opens)
        }
        // A Kong barrel ability is on (code_CEAE0.c): not normal size (Mini Monkey, Hunky
        // Chunky), or PaaD->unk1F0 & 0x30 (Strong Kong and the like), or the Rocketbarrel
        // (control state 0x63). Z + C-Left ends them.
        {
            const uint32_t cc = read_u32(rdram, kCharacterChange);
            const uint32_t paad = read_u32(rdram, kExtraPlayerInfo);
            bool transformed = false;
            if (cc >= 0x80000000u && cc < 0x80800000u - 0x300) {
                transformed |= read_u8(rdram, cc + kCcSize) != 1;
            }
            if (paad >= 0x80000000u && paad < 0x80800000u - 0x200) {
                transformed |= (read_u32(rdram, paad + kPaadFlags) & 0x30) != 0;
            }
            if (player >= 0x80000000u && player < 0x80800000u - 0x300) {
                transformed |= read_u8(rdram, player + kPlayerControlState) == 0x63;
            }
            if (transformed) {
                bits |= 128;
            }
        }
        if (player >= 0x80000000u && player < 0x80800000u - 0x300) {
            if (read_u8(rdram, player + kPlayerGunOut) != 0) {
                bits |= 1;
            }
            const uint8_t control_state = read_u8(rdram, player + kPlayerControlState);
            if (control_state >= 2 && control_state <= 5) {
                bits |= 2;
            }
            const uint32_t input_set = g_input_set.load();
            bits |= move_availability(rdram, input_set);
            bits |= static_cast<jint>(control_state) << 8;
            bits |= static_cast<jint>(input_set < 0xFF ? input_set : 0xFF) << 16;
        }
        values[kCount - 1] = bits;
    }

    env->SetIntArrayRegion(out, 0, kCount, values);
    return JNI_TRUE;
}

// Permanent flags, read-only: out[i] = 1 if flag ids[i] is set in the current file.
extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_bownz_dk64recomp_GameState_nativeReadFlags(JNIEnv* env, jclass, jintArray ids, jintArray out) {
    const uint8_t* rdram = g_rdram.load();
    if (rdram == nullptr || ids == nullptr || out == nullptr) {
        return JNI_FALSE;
    }
    const jsize count = env->GetArrayLength(ids);
    if (env->GetArrayLength(out) < count) {
        return JNI_FALSE;
    }
    const uint32_t block = permanent_flag_block(rdram);
    if (block == 0) {
        return JNI_FALSE;
    }
    jint* flags = env->GetIntArrayElements(ids, nullptr);
    jint* result = env->GetIntArrayElements(out, nullptr);
    for (jsize i = 0; i < count; i++) {
        result[i] = permanent_flag(rdram, block, flags[i]) ? 1 : 0;
    }
    env->ReleaseIntArrayElements(ids, flags, JNI_ABORT);
    env->ReleaseIntArrayElements(out, result, 0);
    return JNI_TRUE;
}

void dk64::android::set_touch_kong_swap(bool enabled) {
    g_touch_kong_swap.store(enabled);
}

// Touch Kong swap (bottom screen + the Tag Anywhere mod), as bits:
//   1: the "Touch Kong Swap" setting is on
//   2: the Tag Anywhere mod is enabled
//   4: the game is in normal play (a player exists, not paused or in a cutscene), so a
//      D-pad press goes to the mod rather than a menu.
extern "C" JNIEXPORT jint JNICALL
Java_io_github_bownz_dk64recomp_GameState_nativeKongSwap(JNIEnv*, jclass) {
    jint bits = 0;
    if (g_touch_kong_swap.load()) {
        bits |= 1;
    }
    if (recomp::mods::is_mod_enabled(kTagAnywhereModId)) {
        bits |= 2;
    }
    const uint8_t* rdram = g_rdram.load();
    if (rdram != nullptr) {
        const uint32_t player = read_u32(rdram, kPlayerPointer);
        const bool has_player = player >= 0x80000000u && player < 0x80800000u;
        const bool paused = (read_u32(rdram, kGlobalProperties) & 2) != 0;
        const bool cutscene = read_u8(rdram, kCutsceneActive) != 0;
        if (has_player && !paused && !cutscene) {
            bits |= 4;
        }
    }
    return bits;
}

extern "C" void dk64_android_report_input_handler(uint32_t index) {
    g_input_set.store(index, std::memory_order_relaxed);
}

extern "C" void dk64_android_report_frame() {
    g_frames.fetch_add(1, std::memory_order_relaxed);
}

void dk64::android::set_show_fps(bool enabled) {
    g_show_fps.store(enabled);
}

// Game frames so far, or -1 while the "Show FPS" setting is off.
extern "C" JNIEXPORT jint JNICALL
Java_io_github_bownz_dk64recomp_GameState_nativeFrameCount(JNIEnv*, jclass) {
    return g_show_fps.load() ? static_cast<jint>(g_frames.load(std::memory_order_relaxed) & 0x7FFFFFFF) : -1;
}
