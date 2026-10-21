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
extern "C" JNIEXPORT jboolean JNICALL
Java_io_github_bownz_dk64recomp_GameState_nativeRead(JNIEnv* env, jclass, jintArray out) {
    const uint8_t* rdram = g_rdram.load();
    constexpr jsize kCount = 8 + 5 * 6;
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

    env->SetIntArrayRegion(out, 0, kCount, values);
    return JNI_TRUE;
}
