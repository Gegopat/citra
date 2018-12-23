// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>
#include <QMessageBox>
#include "citra/applets/mii_selector.h"
#include "common/file_util.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/settings.h"
#include "ui_mii_selector.h"

MiiSelectorDialog::MiiSelectorDialog(QWidget* parent, const HLE::Applets::MiiConfig& config,
                                     HLE::Applets::MiiResult& result)
    : QDialog{parent, Qt::WindowTitleHint | Qt::WindowSystemMenuHint},
      ui{std::make_unique<Ui::MiiSelectorDialog>()} {
    ui->setupUi(this);
    ui->cancel->setEnabled(config.enable_cancel_button == 1);
    std::u16string title{reinterpret_cast<const char16_t*>(config.title.data())};
    setWindowTitle(title.empty() ? "Mii Selector" : QString::fromStdU16String(title));
    auto nand_directory{
        FileUtil::GetUserPath(FileUtil::UserPath::NANDDir, Settings::values.nand_dir)};
    FileSys::ArchiveFactory_ExtSaveData extdata_archive_factory{nand_directory, true};
    auto archive_result{extdata_archive_factory.Open(Service::PTM::ptm_shared_extdata_id)};
    if (!archive_result.Succeeded()) {
        ShowNoSelectableMiiCharacters(result);
        return;
    }
    auto archive{std::move(archive_result).Unwrap()};
    FileSys::Path file_path{"/CFL_DB.dat"};
    FileSys::Mode mode{};
    mode.read_flag.Assign(1);
    auto file_result{archive->_OpenFile(file_path, mode)};
    if (!file_result.Succeeded()) {
        ShowNoSelectableMiiCharacters(result);
        return;
    }
    auto file{std::move(file_result).Unwrap()};
    u32 offset{0x8};
    HLE::Applets::MiiData mii;
    for (int i{}; i < 100; ++i) {
        file->Read(offset, sizeof(mii), reinterpret_cast<u8*>(&mii));
        if (mii.mii_id != 0 && config.user_mii_whitelist[i] != 0) {
            miis.emplace(ui->mii->count(), mii);
            ui->mii->addItem(QString::fromStdU16String(
                std::u16string(reinterpret_cast<char16_t*>(mii.mii_name.data()))));
        }
        offset += sizeof(mii);
    }
    if (miis.empty()) {
        ShowNoSelectableMiiCharacters(result);
        return;
    }
    if (ui->mii->count() > static_cast<int>(config.initially_selected_mii_index))
        ui->mii->setCurrentIndex(static_cast<int>(config.initially_selected_mii_index));
    connect(ui->cancel, &QPushButton::released, this, [&] {
        result.return_code = 1;
        close();
    });
    connect(ui->confirm, &QPushButton::released, this, [&] {
        auto mii{miis.at(ui->mii->currentIndex())};
        result.selected_mii_data = std::move(mii);
        result.selected_guest_mii_index = 0xFFFFFFFF;
        close();
    });
}

MiiSelectorDialog::~MiiSelectorDialog() = default;

void MiiSelectorDialog::ShowNoSelectableMiiCharacters(HLE::Applets::MiiResult& result) {
    QMessageBox message_box;
    message_box.setWindowTitle("Mii Selector");
    message_box.setText("There are no selectable<br>Mii characters.");
    message_box.addButton("Back", QMessageBox::ButtonRole::AcceptRole);
    message_box.exec();
    result.return_code = 0xFFFFFFFF;
    QMetaObject::invokeMethod(this, "close", Qt::QueuedConnection);
}
