// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QIcon>
#include "citra/aboutdialog.h"
#include "common/scm_rev.h"
#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog{parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint},
      ui{std::make_unique<Ui::AboutDialog>()} {
    ui->setupUi(this);
    ui->labelLogo->setPixmap(QIcon::fromTheme("citra").pixmap(200));
    ui->labelBuildInfo->setText(ui->labelBuildInfo->text().arg(
        Common::g_scm_branch, Common::g_scm_desc, QString(Common::g_build_date).left(10)));
}

AboutDialog::~AboutDialog() = default;
