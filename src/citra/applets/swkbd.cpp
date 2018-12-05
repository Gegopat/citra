// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra/applets/swkbd.h"
#include "ui_swkbd.h"

SoftwareKeyboardDialog::SoftwareKeyboardDialog(QWidget* parent,
                                               HLE::Applets::SoftwareKeyboardConfig& config,
                                               std::u16string& text)
    : QDialog{parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint},
      ui{std::make_unique<Ui::SoftwareKeyboardDialog>()} {
    ui->setupUi(this);
    setWindowTitle("Software Keyboard");
    switch (config.num_buttons_m1) {
    case HLE::Applets::SoftwareKeyboardButtonConfig::SingleButton:
        ui->button0->setEnabled(
            HLE::Applets::ValidateInput(config, ui->text->toPlainText().toStdString()) ==
            HLE::Applets::ValidationError::None);
        break;
    case HLE::Applets::SoftwareKeyboardButtonConfig::DualButton:
        ui->button1->setEnabled(
            HLE::Applets::ValidateInput(config, ui->text->toPlainText().toStdString()) ==
            HLE::Applets::ValidationError::None);
        break;
    case HLE::Applets::SoftwareKeyboardButtonConfig::TripleButton:
        ui->button2->setEnabled(
            HLE::Applets::ValidateInput(config, ui->text->toPlainText().toStdString()) ==
            HLE::Applets::ValidationError::None);
        break;
    default:
        UNREACHABLE();
        break;
    }
    std::u16string button0_text{reinterpret_cast<char16_t*>(config.buttons_text[0].data())};
    std::u16string button1_text{reinterpret_cast<char16_t*>(config.buttons_text[1].data())};
    std::u16string button2_text{reinterpret_cast<char16_t*>(config.buttons_text[2].data())};
    std::u16string hint_text{reinterpret_cast<char16_t*>(config.hint_text.data())};
    ui->text->setPlaceholderText(QString::fromStdU16String(hint_text));
    ui->button1->setVisible(config.num_buttons_m1 >=
                            HLE::Applets::SoftwareKeyboardButtonConfig::DualButton);
    ui->button2->setVisible(config.num_buttons_m1 ==
                            HLE::Applets::SoftwareKeyboardButtonConfig::TripleButton);
    ui->button0->setText(
        button0_text.empty()
            ? QString::fromStdString(
                  HLE::Applets::default_button_text[static_cast<u32>(config.num_buttons_m1)][0])
            : QString::fromStdU16String(button0_text));
    ui->button1->setText(
        button1_text.empty()
            ? QString::fromStdString(
                  HLE::Applets::default_button_text[static_cast<u32>(config.num_buttons_m1)][1])
            : QString::fromStdU16String(button1_text));
    ui->button2->setText(
        button2_text.empty()
            ? QString::fromStdString(
                  HLE::Applets::default_button_text[static_cast<u32>(config.num_buttons_m1)][2])
            : QString::fromStdU16String(button2_text));
    connect(ui->text, &QTextEdit::textChanged, [&] {
        switch (config.num_buttons_m1) {
        case HLE::Applets::SoftwareKeyboardButtonConfig::SingleButton:
            ui->button0->setEnabled(
                HLE::Applets::ValidateInput(config, ui->text->toPlainText().toStdString()) ==
                HLE::Applets::ValidationError::None);
            break;
        case HLE::Applets::SoftwareKeyboardButtonConfig::DualButton:
            ui->button1->setEnabled(
                HLE::Applets::ValidateInput(config, ui->text->toPlainText().toStdString()) ==
                HLE::Applets::ValidationError::None);
            break;
        case HLE::Applets::SoftwareKeyboardButtonConfig::TripleButton:
            ui->button2->setEnabled(
                HLE::Applets::ValidateInput(config, ui->text->toPlainText().toStdString()) ==
                HLE::Applets::ValidationError::None);
            break;
        default:
            UNREACHABLE();
            break;
        }
    });
    connect(ui->button0, &QPushButton::clicked, [&](bool) {
        text = ui->text->toPlainText().toStdU16String();
        config.text_length = static_cast<u16>(text.length());
        config.return_code = HLE::Applets::results[static_cast<u32>(config.num_buttons_m1)][0];
        config.text_offset = 0;
        close();
    });
    connect(ui->button1, &QPushButton::clicked, [&](bool) {
        text = ui->text->toPlainText().toStdU16String();
        config.return_code = HLE::Applets::results[static_cast<u32>(config.num_buttons_m1)][1];
        config.text_length = static_cast<u16>(text.length());
        config.text_offset = 0;
        close();
    });
    connect(ui->button2, &QPushButton::clicked, [&](bool) {
        text = ui->text->toPlainText().toStdU16String();
        config.return_code = HLE::Applets::results[static_cast<u32>(config.num_buttons_m1)][2];
        config.text_length = static_cast<u16>(text.length());
        config.text_offset = 0;
        close();
    });
}

SoftwareKeyboardDialog::~SoftwareKeyboardDialog() {}
