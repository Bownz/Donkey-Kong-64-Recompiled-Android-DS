// Read-only access to DK64's game state for the Android second screen (health, current Kong,
// collectibles). Values are copied out of the emulated N64 RDRAM; nothing is ever written.
//
// Addresses and layouts come from this repository: DK64Syms/data_dump.toml for symbol
// addresses (US 1.0) and patches/patches_ui.c for the PlayerProgress / CharacterProgress
// structures.

#include <jni.h>

#include <atomic>
#include <cstdint>

namespace {
    std::atomic<uint8_t*> g_rdram{nullptr};

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
    constexpr uint32_t kCpWeapon = 0x02;                    // u8
    constexpr uint32_t kCpInstrument = 0x04;                // u8
    constexpr uint32_t kCpCoins = 0x06;                     // u16
    constexpr uint32_t kCpInstrumentAmmo = 0x08;            // u16
    constexpr uint32_t kCpColouredBananas = 0x0A;           // u16[14]
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
extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_bownz_dk64recomp_GameState_nativeRead(JNIEnv* env, jclass, jintArray out) {
    const uint8_t* rdram = g_rdram.load();
    constexpr jsize kCount = 8 + 5 * 6 + 2 + 5 * kLevelSlots * 2;
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
            coloured += read_u16(rdram, cp + kCpColouredBananas + level * 2);
        }
        jint* dst = values + 8 + k * 6;
        dst[0] = read_u8(rdram, cp + kCpWeapon);
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
            dst[1] = read_u16(rdram, cp + kCpColouredBananas + level * 2);
        }
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
