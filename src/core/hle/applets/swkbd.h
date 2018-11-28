// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/hle/applets/applet.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"
#include "core/hle/service/ns/ns.h"

namespace HLE::Applets {

/// Maximum number of buttons that can be in the keyboard.
#define SWKBD_MAX_BUTTON 3

/// Maximum button text length, in UTF-16 code units.
#define SWKBD_MAX_BUTTON_TEXT_LEN 16

/// Maximum hint text length, in UTF-16 code units.
#define SWKBD_MAX_HINT_TEXT_LEN 64

/// Maximum filter callback error message length, in UTF-16 code units.
#define SWKBD_MAX_CALLBACK_MSG_LEN 256

/// Keyboard types
enum class SoftwareKeyboardType : u32 {
    Normal,  ///< Normal keyboard with several pages (QWERTY/accents/symbol/mobile)
    QWERTY,  ///< QWERTY keyboard only.
    Numpad,  ///< Number pad.
    Western, ///< On JPN systems, a text keyboard without Japanese input capabilities,
             /// otherwise same as SWKBD_TYPE_NORMAL.
};

/// Keyboard dialog buttons.
enum class SoftwareKeyboardButtonConfig : u32 {
    SingleButton, ///< Ok button
    DualButton,   ///< Cancel | Ok buttons
    TripleButton, ///< Cancel | I Forgot | Ok buttons
    NoButton,     ///< No button (returned by swkbdInputText in special cases)
};

/// Accepted input types.
enum class SoftwareKeyboardValidInput : u32 {
    Anything,         ///< All inputs are accepted.
    NotEmpty,         ///< Empty inputs aren't accepted.
    NotEmptyNotBlank, ///< Empty or blank inputs (consisting solely of whitespace) aren't
                      /// accepted.
    NotBlank,         ///< Blank inputs (consisting solely of whitespace) aren't accepted, but empty
                      /// inputs are.
    FixedLen,         ///< The input must have a fixed length (specified by maxTextLength in
                      /// swkbdInit).
};

/// Keyboard password modes.
enum class SoftwareKeyboardPasswordMode : u32 {
    None = 0,  ///< Characters aren't concealed.
    Hide,      ///< Characters are concealed immediately.
    HideDelay, ///< Characters are concealed a second after they've been typed.
};

/// Keyboard input filtering flags.
enum SoftwareKeyboardFilter {
    SoftwareKeyboardFilter_Digits =
        1, ///< Disallow the use of more than a certain number of digits (0 or more)
    SoftwareKeyboardFilter_At = 1 << 1,        ///< Disallow the use of the @ sign.
    SoftwareKeyboardFilter_Percent = 1 << 2,   ///< Disallow the use of the % sign.
    SoftwareKeyboardFilter_Backslash = 1 << 3, ///< Disallow the use of the \ sign.
    SoftwareKeyboardFilter_Profanity =
        1 << 4, ///< Disallow profanity using Nintendo's profanity filter.
    SoftwareKeyboardFilter_Callback = 1 << 5, ///< Use a callback in order to check the input.
};

/// Keyboard features.
enum class SoftwareKeyboardFeatures {
    Parental = 1,             ///< Parental PIN mode.
    DarkenTopScreen = 1 << 1, ///< Darken the top screen when the keyboard is shown.
    PredictiveInput =
        1 << 2,             ///< Enable predictive input (necessary for Kanji input in JPN systems).
    MultiLine = 1 << 3,     ///< Enable multiline input.
    FixedWidth = 1 << 4,    ///< Enable fixed-width mode.
    AllowHome = 1 << 5,     ///< Allow the usage of the HOME button.
    AllowReset = 1 << 6,    ///< Allow the usage of a software-reset combination.
    AllowPower = 1 << 7,    ///< Allow the usage of the POWER button.
    DefaultQWERTY = 1 << 9, ///< Default to the QWERTY page when the keyboard is shown.
};

/// Keyboard filter callback return values.
enum class SoftwareKeyboardCallbackResult : u32 {
    Ok,       ///< Specifies that the input is valid.
    Close,    ///< Displays an error message, then closes the keyboard.
    Continue, ///< Displays an error message and continues displaying the keyboard.
};

/// Keyboard return values.
enum class SoftwareKeyboardResult : s32 {
    None = -1,         ///< Dummy/unused.
    InvalidInput = -2, ///< Invalid parameters to swkbd.
    OutOfMem = -3,     ///< Out of memory.

    D0Click = 0, ///< The button was clicked in 1-button dialogs.
    D1Click0,    ///< The left button was clicked in 2-button dialogs.
    D1Click1,    ///< The right button was clicked in 2-button dialogs.
    D2Click0,    ///< The left button was clicked in 3-button dialogs.
    D2Click1,    ///< The middle button was clicked in 3-button dialogs.
    D2Click2,    ///< The right button was clicked in 3-button dialogs.

    HomePressed = 10, ///< The HOME button was pressed.
    ResetPressed,     ///< The soft-reset key combination was pressed.
    PowerPressed,     ///< The POWER button was pressed.

    ParentalOk = 20, ///< The parental PIN was verified successfully.
    ParentalFail,    ///< The parental PIN was incorrect.

    BannedInput = 30, ///< The filter callback returned SWKBD_CALLBACK_CLOSE.
};

enum class ValidationError {
    None,

    // Button Selection
    ButtonOutOfRange,

    // Configured Filters
    DigitNotAllowed,
    AtSignNotAllowed,
    PercentNotAllowed,
    BackslashNotAllowed,
    ProfanityNotAllowed,
    CallbackFailed,

    // Allowed Input Type
    FixedLengthRequired,
    MaxLengthExceeded,
    BlankInputNotAllowed,
    EmptyInputNotAllowed,
    NewLineNotAllowed,
    InputNotNumber,
};

struct SoftwareKeyboardConfig {
    enum_le<SoftwareKeyboardType> type;
    enum_le<SoftwareKeyboardButtonConfig> num_buttons_m1;
    enum_le<SoftwareKeyboardValidInput> valid_input;
    enum_le<SoftwareKeyboardPasswordMode> password_mode;
    s32_le is_parental_screen;
    s32_le darken_top_screen;
    u32_le filter_flags;
    u32_le save_state_flags;
    u16_le max_text_length;
    u16_le dict_word_count;
    u16_le max_digits;
    std::array<std::array<u16_le, SWKBD_MAX_BUTTON_TEXT_LEN + 1>, SWKBD_MAX_BUTTON> buttons_text;
    std::array<u16_le, 2> numpad_keys;
    std::array<u16_le, SWKBD_MAX_HINT_TEXT_LEN + 1>
        hint_text; ///< Text to display when asking the user for input
    bool predictive_input;
    bool multiline;
    bool fixed_width;
    bool allow_home;
    bool allow_reset;
    bool allow_power;
    bool unknown;
    bool default_qwerty;
    std::array<bool, 4> button_submits_text;
    u16_le language;

    u32_le initial_text_offset; ///< Offset of the default text in the output SharedMemory
    u32_le dict_offset;
    u32_le initial_status_offset;
    u32_le initial_learning_offset;
    u32_le shared_memory_size; ///< Size of the SharedMemory
    u32_le version;

    enum_le<SoftwareKeyboardResult> return_code;

    u32_le status_offset;
    u32_le learning_offset;

    u32_le text_offset; ///< Offset in the SharedMemory where the output text starts
    u16_le text_length; ///< Length in characters of the output text

    enum_le<SoftwareKeyboardCallbackResult> callback_result;
    std::array<u16_le, SWKBD_MAX_CALLBACK_MSG_LEN + 1> callback_msg;
    bool skip_at_check;
    INSERT_PADDING_BYTES(0xAB);
};

/**
 * The size of this structure (0x400) has been verified via reverse engineering of multiple games
 * that use the software keyboard.
 */
static_assert(sizeof(SoftwareKeyboardConfig) == 0x400, "SoftwareKeyboardConfig size is wrong");

const std::string default_button_text[][3]{
    {
        "OK",
    },
    {
        "Cancel",
        "OK",
    },
    {
        "Cancel",
        "I Forgot",
        "OK",
    },
};

const SoftwareKeyboardResult results[][3]{
    {
        SoftwareKeyboardResult::D0Click,
    },
    {
        SoftwareKeyboardResult::D1Click0,
        SoftwareKeyboardResult::D1Click1,
    },
    {
        SoftwareKeyboardResult::D2Click0,
        SoftwareKeyboardResult::D2Click1,
        SoftwareKeyboardResult::D2Click2,
    },
};

ValidationError ValidateFilters(const SoftwareKeyboardConfig& config, const std::string& input);
ValidationError ValidateInput(const SoftwareKeyboardConfig& config, const std::string& input);
ValidationError ValidateButton(const SoftwareKeyboardConfig& config, u8 button);

class SoftwareKeyboard final : public Applet {
public:
    explicit SoftwareKeyboard(AppletId id, Service::APT::AppletManager& manager)
        : Applet{id, manager} {}

    ResultCode ReceiveParameter(const Service::APT::MessageParameter& parameter) override;
    ResultCode StartImpl(const Service::APT::AppletStartupParameter& parameter) override;
    void Update() override;

    /**
     * Sends the LibAppletClosing signal to the program,
     * along with the relevant data buffers.
     */
    void Finalize();

private:
    /// This SharedMemory will be created when we receive the LibAppJustStarted message.
    /// It holds the framebuffer info retrieved by the program with
    /// gsp::Gpu:ImportDisplayCaptureInfo
    Kernel::SharedPtr<Kernel::SharedMemory> framebuffer_memory;

    /// SharedMemory where the output text will be stored
    Kernel::SharedPtr<Kernel::SharedMemory> text_memory;

    /// Configuration of this instance of the SoftwareKeyboard, as received from the program
    SoftwareKeyboardConfig config;
};

} // namespace HLE::Applets
