// @recomp (Android bottom screen): which control-state input handler the player uses.
//
// The game handles the player's buttons with a table of handlers per input mode
// (D_global_asm_80751004, indexed by the number each control state passes to
// handleInputsForControlState, which isn't always the control state itself). The Android
// bottom screen greys out moves that the current mode can't do, so it needs the mode that's
// actually in use. At startup each mode's "always called first" handler is wrapped to report
// its index (recomp_report_input_handler) and then run the original handler.
//
// recomp_report_frame() is called once per game frame for the on-screen FPS counter.

#include "patch_helpers.h"
#include "input.h"

typedef void (*InputHandlerFunc)(void);

typedef struct {
    InputHandlerFunc handlers[17];
} InputHandlerSet;

extern InputHandlerSet D_global_asm_80751004[];

#define INPUT_HANDLER_SETS 107

static InputHandlerFunc original_first_handlers[INPUT_HANDLER_SETS];
static int input_handlers_wrapped = 0;

static void input_handler_report_0(void) { recomp_report_input_handler(0); original_first_handlers[0](); }
static void input_handler_report_1(void) { recomp_report_input_handler(1); original_first_handlers[1](); }
static void input_handler_report_2(void) { recomp_report_input_handler(2); original_first_handlers[2](); }
static void input_handler_report_3(void) { recomp_report_input_handler(3); original_first_handlers[3](); }
static void input_handler_report_4(void) { recomp_report_input_handler(4); original_first_handlers[4](); }
static void input_handler_report_5(void) { recomp_report_input_handler(5); original_first_handlers[5](); }
static void input_handler_report_6(void) { recomp_report_input_handler(6); original_first_handlers[6](); }
static void input_handler_report_7(void) { recomp_report_input_handler(7); original_first_handlers[7](); }
static void input_handler_report_8(void) { recomp_report_input_handler(8); original_first_handlers[8](); }
static void input_handler_report_9(void) { recomp_report_input_handler(9); original_first_handlers[9](); }
static void input_handler_report_10(void) { recomp_report_input_handler(10); original_first_handlers[10](); }
static void input_handler_report_11(void) { recomp_report_input_handler(11); original_first_handlers[11](); }
static void input_handler_report_12(void) { recomp_report_input_handler(12); original_first_handlers[12](); }
static void input_handler_report_13(void) { recomp_report_input_handler(13); original_first_handlers[13](); }
static void input_handler_report_14(void) { recomp_report_input_handler(14); original_first_handlers[14](); }
static void input_handler_report_15(void) { recomp_report_input_handler(15); original_first_handlers[15](); }
static void input_handler_report_16(void) { recomp_report_input_handler(16); original_first_handlers[16](); }
static void input_handler_report_17(void) { recomp_report_input_handler(17); original_first_handlers[17](); }
static void input_handler_report_18(void) { recomp_report_input_handler(18); original_first_handlers[18](); }
static void input_handler_report_19(void) { recomp_report_input_handler(19); original_first_handlers[19](); }
static void input_handler_report_20(void) { recomp_report_input_handler(20); original_first_handlers[20](); }
static void input_handler_report_21(void) { recomp_report_input_handler(21); original_first_handlers[21](); }
static void input_handler_report_22(void) { recomp_report_input_handler(22); original_first_handlers[22](); }
static void input_handler_report_23(void) { recomp_report_input_handler(23); original_first_handlers[23](); }
static void input_handler_report_24(void) { recomp_report_input_handler(24); original_first_handlers[24](); }
static void input_handler_report_25(void) { recomp_report_input_handler(25); original_first_handlers[25](); }
static void input_handler_report_26(void) { recomp_report_input_handler(26); original_first_handlers[26](); }
static void input_handler_report_27(void) { recomp_report_input_handler(27); original_first_handlers[27](); }
static void input_handler_report_28(void) { recomp_report_input_handler(28); original_first_handlers[28](); }
static void input_handler_report_29(void) { recomp_report_input_handler(29); original_first_handlers[29](); }
static void input_handler_report_30(void) { recomp_report_input_handler(30); original_first_handlers[30](); }
static void input_handler_report_31(void) { recomp_report_input_handler(31); original_first_handlers[31](); }
static void input_handler_report_32(void) { recomp_report_input_handler(32); original_first_handlers[32](); }
static void input_handler_report_33(void) { recomp_report_input_handler(33); original_first_handlers[33](); }
static void input_handler_report_34(void) { recomp_report_input_handler(34); original_first_handlers[34](); }
static void input_handler_report_35(void) { recomp_report_input_handler(35); original_first_handlers[35](); }
static void input_handler_report_36(void) { recomp_report_input_handler(36); original_first_handlers[36](); }
static void input_handler_report_37(void) { recomp_report_input_handler(37); original_first_handlers[37](); }
static void input_handler_report_38(void) { recomp_report_input_handler(38); original_first_handlers[38](); }
static void input_handler_report_39(void) { recomp_report_input_handler(39); original_first_handlers[39](); }
static void input_handler_report_40(void) { recomp_report_input_handler(40); original_first_handlers[40](); }
static void input_handler_report_41(void) { recomp_report_input_handler(41); original_first_handlers[41](); }
static void input_handler_report_42(void) { recomp_report_input_handler(42); original_first_handlers[42](); }
static void input_handler_report_43(void) { recomp_report_input_handler(43); original_first_handlers[43](); }
static void input_handler_report_44(void) { recomp_report_input_handler(44); original_first_handlers[44](); }
static void input_handler_report_45(void) { recomp_report_input_handler(45); original_first_handlers[45](); }
static void input_handler_report_46(void) { recomp_report_input_handler(46); original_first_handlers[46](); }
static void input_handler_report_47(void) { recomp_report_input_handler(47); original_first_handlers[47](); }
static void input_handler_report_48(void) { recomp_report_input_handler(48); original_first_handlers[48](); }
static void input_handler_report_49(void) { recomp_report_input_handler(49); original_first_handlers[49](); }
static void input_handler_report_50(void) { recomp_report_input_handler(50); original_first_handlers[50](); }
static void input_handler_report_51(void) { recomp_report_input_handler(51); original_first_handlers[51](); }
static void input_handler_report_52(void) { recomp_report_input_handler(52); original_first_handlers[52](); }
static void input_handler_report_53(void) { recomp_report_input_handler(53); original_first_handlers[53](); }
static void input_handler_report_54(void) { recomp_report_input_handler(54); original_first_handlers[54](); }
static void input_handler_report_55(void) { recomp_report_input_handler(55); original_first_handlers[55](); }
static void input_handler_report_56(void) { recomp_report_input_handler(56); original_first_handlers[56](); }
static void input_handler_report_57(void) { recomp_report_input_handler(57); original_first_handlers[57](); }
static void input_handler_report_58(void) { recomp_report_input_handler(58); original_first_handlers[58](); }
static void input_handler_report_59(void) { recomp_report_input_handler(59); original_first_handlers[59](); }
static void input_handler_report_60(void) { recomp_report_input_handler(60); original_first_handlers[60](); }
static void input_handler_report_61(void) { recomp_report_input_handler(61); original_first_handlers[61](); }
static void input_handler_report_62(void) { recomp_report_input_handler(62); original_first_handlers[62](); }
static void input_handler_report_63(void) { recomp_report_input_handler(63); original_first_handlers[63](); }
static void input_handler_report_64(void) { recomp_report_input_handler(64); original_first_handlers[64](); }
static void input_handler_report_65(void) { recomp_report_input_handler(65); original_first_handlers[65](); }
static void input_handler_report_66(void) { recomp_report_input_handler(66); original_first_handlers[66](); }
static void input_handler_report_67(void) { recomp_report_input_handler(67); original_first_handlers[67](); }
static void input_handler_report_68(void) { recomp_report_input_handler(68); original_first_handlers[68](); }
static void input_handler_report_69(void) { recomp_report_input_handler(69); original_first_handlers[69](); }
static void input_handler_report_70(void) { recomp_report_input_handler(70); original_first_handlers[70](); }
static void input_handler_report_71(void) { recomp_report_input_handler(71); original_first_handlers[71](); }
static void input_handler_report_72(void) { recomp_report_input_handler(72); original_first_handlers[72](); }
static void input_handler_report_73(void) { recomp_report_input_handler(73); original_first_handlers[73](); }
static void input_handler_report_74(void) { recomp_report_input_handler(74); original_first_handlers[74](); }
static void input_handler_report_75(void) { recomp_report_input_handler(75); original_first_handlers[75](); }
static void input_handler_report_76(void) { recomp_report_input_handler(76); original_first_handlers[76](); }
static void input_handler_report_77(void) { recomp_report_input_handler(77); original_first_handlers[77](); }
static void input_handler_report_78(void) { recomp_report_input_handler(78); original_first_handlers[78](); }
static void input_handler_report_79(void) { recomp_report_input_handler(79); original_first_handlers[79](); }
static void input_handler_report_80(void) { recomp_report_input_handler(80); original_first_handlers[80](); }
static void input_handler_report_81(void) { recomp_report_input_handler(81); original_first_handlers[81](); }
static void input_handler_report_82(void) { recomp_report_input_handler(82); original_first_handlers[82](); }
static void input_handler_report_83(void) { recomp_report_input_handler(83); original_first_handlers[83](); }
static void input_handler_report_84(void) { recomp_report_input_handler(84); original_first_handlers[84](); }
static void input_handler_report_85(void) { recomp_report_input_handler(85); original_first_handlers[85](); }
static void input_handler_report_86(void) { recomp_report_input_handler(86); original_first_handlers[86](); }
static void input_handler_report_87(void) { recomp_report_input_handler(87); original_first_handlers[87](); }
static void input_handler_report_88(void) { recomp_report_input_handler(88); original_first_handlers[88](); }
static void input_handler_report_89(void) { recomp_report_input_handler(89); original_first_handlers[89](); }
static void input_handler_report_90(void) { recomp_report_input_handler(90); original_first_handlers[90](); }
static void input_handler_report_91(void) { recomp_report_input_handler(91); original_first_handlers[91](); }
static void input_handler_report_92(void) { recomp_report_input_handler(92); original_first_handlers[92](); }
static void input_handler_report_93(void) { recomp_report_input_handler(93); original_first_handlers[93](); }
static void input_handler_report_94(void) { recomp_report_input_handler(94); original_first_handlers[94](); }
static void input_handler_report_95(void) { recomp_report_input_handler(95); original_first_handlers[95](); }
static void input_handler_report_96(void) { recomp_report_input_handler(96); original_first_handlers[96](); }
static void input_handler_report_97(void) { recomp_report_input_handler(97); original_first_handlers[97](); }
static void input_handler_report_98(void) { recomp_report_input_handler(98); original_first_handlers[98](); }
static void input_handler_report_99(void) { recomp_report_input_handler(99); original_first_handlers[99](); }
static void input_handler_report_100(void) { recomp_report_input_handler(100); original_first_handlers[100](); }
static void input_handler_report_101(void) { recomp_report_input_handler(101); original_first_handlers[101](); }
static void input_handler_report_102(void) { recomp_report_input_handler(102); original_first_handlers[102](); }
static void input_handler_report_103(void) { recomp_report_input_handler(103); original_first_handlers[103](); }
static void input_handler_report_104(void) { recomp_report_input_handler(104); original_first_handlers[104](); }
static void input_handler_report_105(void) { recomp_report_input_handler(105); original_first_handlers[105](); }
static void input_handler_report_106(void) { recomp_report_input_handler(106); original_first_handlers[106](); }

static const InputHandlerFunc input_handler_reporters[INPUT_HANDLER_SETS] = {
    input_handler_report_0,
    input_handler_report_1,
    input_handler_report_2,
    input_handler_report_3,
    input_handler_report_4,
    input_handler_report_5,
    input_handler_report_6,
    input_handler_report_7,
    input_handler_report_8,
    input_handler_report_9,
    input_handler_report_10,
    input_handler_report_11,
    input_handler_report_12,
    input_handler_report_13,
    input_handler_report_14,
    input_handler_report_15,
    input_handler_report_16,
    input_handler_report_17,
    input_handler_report_18,
    input_handler_report_19,
    input_handler_report_20,
    input_handler_report_21,
    input_handler_report_22,
    input_handler_report_23,
    input_handler_report_24,
    input_handler_report_25,
    input_handler_report_26,
    input_handler_report_27,
    input_handler_report_28,
    input_handler_report_29,
    input_handler_report_30,
    input_handler_report_31,
    input_handler_report_32,
    input_handler_report_33,
    input_handler_report_34,
    input_handler_report_35,
    input_handler_report_36,
    input_handler_report_37,
    input_handler_report_38,
    input_handler_report_39,
    input_handler_report_40,
    input_handler_report_41,
    input_handler_report_42,
    input_handler_report_43,
    input_handler_report_44,
    input_handler_report_45,
    input_handler_report_46,
    input_handler_report_47,
    input_handler_report_48,
    input_handler_report_49,
    input_handler_report_50,
    input_handler_report_51,
    input_handler_report_52,
    input_handler_report_53,
    input_handler_report_54,
    input_handler_report_55,
    input_handler_report_56,
    input_handler_report_57,
    input_handler_report_58,
    input_handler_report_59,
    input_handler_report_60,
    input_handler_report_61,
    input_handler_report_62,
    input_handler_report_63,
    input_handler_report_64,
    input_handler_report_65,
    input_handler_report_66,
    input_handler_report_67,
    input_handler_report_68,
    input_handler_report_69,
    input_handler_report_70,
    input_handler_report_71,
    input_handler_report_72,
    input_handler_report_73,
    input_handler_report_74,
    input_handler_report_75,
    input_handler_report_76,
    input_handler_report_77,
    input_handler_report_78,
    input_handler_report_79,
    input_handler_report_80,
    input_handler_report_81,
    input_handler_report_82,
    input_handler_report_83,
    input_handler_report_84,
    input_handler_report_85,
    input_handler_report_86,
    input_handler_report_87,
    input_handler_report_88,
    input_handler_report_89,
    input_handler_report_90,
    input_handler_report_91,
    input_handler_report_92,
    input_handler_report_93,
    input_handler_report_94,
    input_handler_report_95,
    input_handler_report_96,
    input_handler_report_97,
    input_handler_report_98,
    input_handler_report_99,
    input_handler_report_100,
    input_handler_report_101,
    input_handler_report_102,
    input_handler_report_103,
    input_handler_report_104,
    input_handler_report_105,
    input_handler_report_106,
};

// Called every frame from the main loop (boot_logos_patches.c).
void dk64_android_hooks_every_frame(void) {
    s32 i;
    if (!input_handlers_wrapped) {
        input_handlers_wrapped = 1;
        for (i = 0; i < INPUT_HANDLER_SETS; i++) {
            original_first_handlers[i] = D_global_asm_80751004[i].handlers[1];
            D_global_asm_80751004[i].handlers[1] = input_handler_reporters[i];
        }
    }
    recomp_report_frame();
}
