// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <cctype>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/core.h"
#include "core/frontend.h"
#include "core/hle/applets/swkbd.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"
#include "core/hle/service/gsp/gsp.h"
#include "core/hle/service/hid/hid.h"
#include "core/memory.h"
#include "video_core/video_core.h"

namespace HLE::Applets {

ValidationError ValidateFilters(const SoftwareKeyboardConfig& config, const std::string& input) {
    if ((config.filter_flags & SoftwareKeyboardFilter_Digits) == SoftwareKeyboardFilter_Digits) {
        int digits_count{};
        for (const char c : input)
            if (std::isdigit(static_cast<int>(c)))
                ++digits_count;
        if (digits_count > 0 && config.max_digits == 0)
            return ValidationError::DigitNotAllowed;
        if (digits_count > config.max_digits)
            return ValidationError::MaxLengthExceeded;
    }
    if ((config.filter_flags & SoftwareKeyboardFilter_At) == SoftwareKeyboardFilter_At)
        if (input.find('@') != std::string::npos)
            return ValidationError::AtSignNotAllowed;
    if ((config.filter_flags & SoftwareKeyboardFilter_Percent) == SoftwareKeyboardFilter_Percent)
        if (input.find('%') != std::string::npos)
            return ValidationError::PercentNotAllowed;
    if ((config.filter_flags & SoftwareKeyboardFilter_Backslash) ==
        SoftwareKeyboardFilter_Backslash)
        if (input.find('\\') != std::string::npos)
            return ValidationError::BackslashNotAllowed;
    if ((config.filter_flags & SoftwareKeyboardFilter_Profanity) ==
        SoftwareKeyboardFilter_Profanity)
        // TODO: check the profanity filter
        UNIMPLEMENTED();
    if ((config.filter_flags & SoftwareKeyboardFilter_Callback) == SoftwareKeyboardFilter_Callback)
        // TODO: check the callback
        UNIMPLEMENTED();
    return ValidationError::None;
}

ValidationError ValidateInput(const SoftwareKeyboardConfig& config, const std::string& input) {
    ValidationError error;
    if ((error = ValidateFilters(config, input)) != ValidationError::None)
        return error;
    if (input.size() > config.max_text_length)
        return ValidationError::MaxLengthExceeded;
    if (!config.multiline && (input.find('\n') != std::string::npos))
        return ValidationError::NewLineNotAllowed;
    bool is_blank{
        std::all_of(input.begin(), input.end(), [](unsigned char c) { return std::isspace(c); })};
    switch (config.valid_input) {
    case SoftwareKeyboardValidInput::FixedLen:
        if (input.size() != config.max_text_length)
            return ValidationError::FixedLengthRequired;
        break;
    case SoftwareKeyboardValidInput::NotEmptyNotBlank:
        if (is_blank)
            return ValidationError::BlankInputNotAllowed;
        if (input.empty())
            return ValidationError::EmptyInputNotAllowed;
        break;
    case SoftwareKeyboardValidInput::NotBlank:
        if (is_blank)
            return ValidationError::BlankInputNotAllowed;
        break;
    case SoftwareKeyboardValidInput::NotEmpty:
        if (input.empty())
            return ValidationError::EmptyInputNotAllowed;
        break;
    case SoftwareKeyboardValidInput::Anything:
        break;
    default:
        // TODO: What does hardware do in this case?
        UNREACHABLE_MSG("Program requested unknown validation method {}",
                        static_cast<u32>(config.valid_input));
    }
    switch (config.type) {
    case SoftwareKeyboardType::QWERTY:
    case SoftwareKeyboardType::Western:
    case SoftwareKeyboardType::Normal:
        return ValidationError::None;
    case SoftwareKeyboardType::Numpad:
        return std::all_of(input.begin(), input.end(), [](const char c) { return std::isdigit(c); })
                   ? ValidationError::None
                   : ValidationError::InputNotNumber;
    default:
        return ValidationError::None;
    }
}

ValidationError ValidateButton(const SoftwareKeyboardConfig& config, u8 button) {
    switch (config.num_buttons_m1) {
    case SoftwareKeyboardButtonConfig::NoButton:
        return ValidationError::None;
    case SoftwareKeyboardButtonConfig::SingleButton:
        if (button != 0)
            return ValidationError::ButtonOutOfRange;
        break;
    case SoftwareKeyboardButtonConfig::DualButton:
        if (button > 1)
            return ValidationError::ButtonOutOfRange;
        break;
    case SoftwareKeyboardButtonConfig::TripleButton:
        if (button > 2)
            return ValidationError::ButtonOutOfRange;
        break;
    default:
        UNREACHABLE();
    }
    return ValidationError::None;
}

ResultCode SoftwareKeyboard::ReceiveParameter(Service::APT::MessageParameter const& parameter) {
    if (parameter.signal != Service::APT::SignalType::Request) {
        LOG_ERROR(Applet_Swkbd, "unsupported signal {}", static_cast<u32>(parameter.signal));
        UNIMPLEMENTED();
        // TODO: Find the right error code
        return ResultCode(-1);
    }
    // The LibAppJustStarted message contains a buffer with the size of the framebuffer shared
    // memory.
    // Create the SharedMemory that will hold the framebuffer data
    Service::APT::CaptureBufferInfo capture_info;
    ASSERT(sizeof(capture_info) == parameter.buffer.size());
    std::memcpy(&capture_info, parameter.buffer.data(), sizeof(capture_info));
    using Kernel::MemoryPermission;
    // Create a SharedMemory that directly points to this heap block.
    framebuffer_memory = manager.System().Kernel().CreateSharedMemoryForApplet(
        0, capture_info.size, MemoryPermission::ReadWrite, MemoryPermission::ReadWrite,
        "Software Keyboard Shared Memory");
    // Send the response message with the newly created SharedMemory
    Service::APT::MessageParameter result;
    result.signal = Service::APT::SignalType::Response;
    result.buffer.clear();
    result.destination_id = AppletID::Program;
    result.sender_id = id;
    result.object = framebuffer_memory;
    SendParameter(result);
    return RESULT_SUCCESS;
}

ResultCode SoftwareKeyboard::StartImpl(const Service::APT::AppletStartupParameter& parameter) {
    ASSERT_MSG(parameter.buffer.size() == sizeof(config),
               "The size of the parameter (SoftwareKeyboardConfig) is wrong");
    std::memcpy(&config, parameter.buffer.data(), parameter.buffer.size());
    text_memory =
        boost::static_pointer_cast<Kernel::SharedMemory, Kernel::Object>(parameter.object);
    // TODO: Verify if this is the correct behavior
    std::memset(text_memory->GetPointer(), 0, text_memory->GetSize());
    is_running = true;
    return RESULT_SUCCESS;
}

void SoftwareKeyboard::Update() {
    switch (Settings::values.keyboard_mode) {
    case Settings::KeyboardMode::StdIn: {
        std::string input;
        std::cout << "Software Keyboard" << std::endl;
        // Display hint text
        std::u16string hint{reinterpret_cast<char16_t*>(config.hint_text.data())};
        if (!hint.empty())
            std::cout << "Hint text: " << Common::UTF16ToUTF8(hint) << std::endl;
        ValidationError error{ValidationError::ButtonOutOfRange};
        auto ValidateInputString{[&]() -> bool {
            ValidationError error{ValidateInput(config, input)};
            if (error != ValidationError::None) {
                switch (error) {
                case ValidationError::AtSignNotAllowed:
                    std::cout << "Input must not contain the @ symbol" << std::endl;
                    break;
                case ValidationError::BackslashNotAllowed:
                    std::cout << "Input must not contain the \\ symbol" << std::endl;
                    break;
                case ValidationError::BlankInputNotAllowed:
                    std::cout << "Input must not be blank." << std::endl;
                    break;
                case ValidationError::CallbackFailed:
                    std::cout << "Callbak failed." << std::endl;
                    break;
                case ValidationError::DigitNotAllowed:
                    std::cout << "Input must not contain any digits" << std::endl;
                    break;
                case ValidationError::EmptyInputNotAllowed:
                    std::cout << "Input must not be empty." << std::endl;
                    break;
                case ValidationError::FixedLengthRequired:
                    std::cout << fmt::format("Input must be exactly {} characters.",
                                             config.max_text_length)
                              << std::endl;
                    break;
                case ValidationError::InputNotNumber:
                    std::cout << "All characters must be digits." << std::endl;
                    break;
                case ValidationError::MaxLengthExceeded:
                    std::cout << fmt::format("Input is longer than the maximum length. Max: {}",
                                             config.max_text_length)
                              << std::endl;
                    break;
                case ValidationError::PercentNotAllowed:
                    std::cout << "Input must not contain the % symbol" << std::endl;
                    break;
                default:
                    UNREACHABLE();
                }
            }
            return error == ValidationError::None;
        }};
        do {
            std::cout << "Enter the text you will send to the program:" << std::endl;
            std::getline(std::cin, input);
        } while (!ValidateInputString());
        std::string option_text;
        // Convert all of the button texts into something we can output
        // num_buttons is in the range of 0-2 so use <= instead of <
        u32 num_buttons{static_cast<u32>(config.num_buttons_m1)};
        for (u32 i{}; i <= num_buttons; ++i) {
            std::string button_text;
            // Apps are allowed to set custom text to display on the button
            std::u16string custom_button_text{
                reinterpret_cast<char16_t*>(config.buttons_text[i].data())};
            if (custom_button_text.empty())
                // Use the system default text for that button
                button_text = default_button_text[num_buttons][i];
            else
                button_text = Common::UTF16ToUTF8(custom_button_text);
            option_text += "\t(" + std::to_string(i) + ") " + button_text + "\t";
        }
        std::string option;
        auto ValidateButtonString{[&]() -> bool {
            bool valid{};
            try {
                u32 num{static_cast<u32>(std::stoul(option))};
                valid = ValidateButton(config, static_cast<u8>(num)) == ValidationError::None;
                if (!valid)
                    std::cout << fmt::format("Please choose a number between 0 and {}",
                                             static_cast<u32>(config.num_buttons_m1))
                              << std::endl;
            } catch (const std::invalid_argument&) {
                std::cout << "Unable to parse input as a number." << std::endl;
            } catch (const std::out_of_range&) {
                std::cout << "Input number isn't valid." << std::endl;
            }
            return valid;
        }};
        do {
            std::cout << "\nPlease type the number of the button you will press: \n"
                      << option_text << std::endl;
            std::getline(std::cin, option);
        } while (!ValidateButtonString());
        s32 button{static_cast<s32>(std::stol(option))};
        switch (config.num_buttons_m1) {
        case SoftwareKeyboardButtonConfig::SingleButton:
            config.return_code = SoftwareKeyboardResult::D0Click;
            break;
        case SoftwareKeyboardButtonConfig::DualButton:
            if (button == 0)
                config.return_code = SoftwareKeyboardResult::D1Click0;
            else
                config.return_code = SoftwareKeyboardResult::D1Click1;
            break;
        case SoftwareKeyboardButtonConfig::TripleButton:
            if (button == 0)
                config.return_code = SoftwareKeyboardResult::D2Click0;
            else if (button == 1)
                config.return_code = SoftwareKeyboardResult::D2Click1;
            else
                config.return_code = SoftwareKeyboardResult::D2Click2;
            break;
        default:
            // TODO: what does the hardware do
            LOG_WARNING(Applet_Swkbd, "Unknown option for num_buttons_m1: {}",
                        static_cast<u32>(config.num_buttons_m1));
            config.return_code = SoftwareKeyboardResult::None;
            break;
        }
        auto utf16_input{Common::UTF8ToUTF16(input)};
        std::memcpy(text_memory->GetPointer(), utf16_input.c_str(),
                    utf16_input.length() * sizeof(char16_t));
        config.text_length = static_cast<u16>(utf16_input.size());
        config.text_offset = 0;
        Finalize();
        break;
    }
    case Settings::KeyboardMode::Qt: {
        std::u16string text;
        manager.System().GetFrontend().LaunchSoftwareKeyboard(config, text, is_running);
        std::mutex m;
        std::unique_lock lock{m};
        std::condition_variable cv;
        cv.wait(lock, [this]() -> bool { return !is_running; });
        std::memcpy(text_memory->GetPointer(), text.c_str(), text.length() * sizeof(char16_t));
        Finalize();
        break;
    }
    default:
        UNREACHABLE_MSG("Unknown button config {}", static_cast<u32>(config.num_buttons_m1));
    }
}

void SoftwareKeyboard::Finalize() {
    // Let the program know that we're closing
    Service::APT::MessageParameter message;
    message.buffer.resize(sizeof(SoftwareKeyboardConfig));
    std::memcpy(message.buffer.data(), &config, message.buffer.size());
    message.signal = Service::APT::SignalType::WakeupByExit;
    message.destination_id = AppletID::Program;
    message.sender_id = id;
    SendParameter(message);
    is_running = false;
    text_memory = nullptr;
}

} // namespace HLE::Applets
