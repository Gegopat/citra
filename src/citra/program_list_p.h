// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QObject>
#include <QPainter>
#include <QRunnable>
#include <QStandardItem>
#include <QString>
#include <QWidget>
#include "citra/issues_map.h"
#include "citra/ui_settings.h"
#include "citra/util/util.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/loader/smdh.h"
#include "core/settings.h"

enum class ProgramListItemType {
    Program = QStandardItem::UserType + 1,
    CustomDir = QStandardItem::UserType + 2,
    InstalledDir = QStandardItem::UserType + 3,
    SystemDir = QStandardItem::UserType + 4,
    AddDir = QStandardItem::UserType + 5
};

/**
 * Gets the icon from SMDH data.
 * @param smdh SMDH data
 * @param large If true, returns large icon (48x48), otherwise returns small icon (24x24)
 * @return QPixmap icon
 */
static QPixmap GetQPixmapFromSMDH(const Loader::SMDH& smdh, bool large) {
    std::vector<u16> icon_data{smdh.GetIcon(large)};
    const uchar* data{reinterpret_cast<const uchar*>(icon_data.data())};
    int size{large ? 48 : 24};
    QImage icon{data, size, size, QImage::Format::Format_RGB16};
    return QPixmap::fromImage(icon);
}

/**
 * Gets the default icon (for program without valid SMDH)
 * @param large If true, returns large icon (48x48), otherwise returns small icon (24x24)
 * @return QPixmap default icon
 */
static QPixmap GetDefaultIcon(bool large) {
    int size{large ? 48 : 24};
    QPixmap icon{size, size};
    icon.fill(Qt::transparent);
    return icon;
}

/**
 * Gets the short title from SMDH data.
 * @param smdh SMDH data
 * @param language title language
 * @return QString short title
 */
static QString GetQStringShortTitleFromSMDH(const Loader::SMDH& smdh,
                                            Loader::SMDH::TitleLanguage language) {
    return QString::fromUtf16(smdh.GetShortTitle(language).data());
}

/**
 * Gets the long title from SMDH data.
 * @param smdh SMDH data
 * @param language title language
 * @return QString sholongrt title
 */
static QString GetQStringLongTitleFromSMDH(const Loader::SMDH& smdh,
                                           Loader::SMDH::TitleLanguage language) {
    return QString::fromUtf16(smdh.GetLongTitle(language).data());
}

/**
 * Gets the publisher from SMDH data.
 * @param smdh SMDH data
 * @param language title language
 * @return QString publisher
 */
static QString GetQStringPublisherFromSMDH(const Loader::SMDH& smdh,
                                           Loader::SMDH::TitleLanguage language) {
    return QString::fromUtf16(smdh.GetPublisher(language).data());
}

/**
 * Gets the region from SMDH data.
 * @param smdh SMDH data
 * @return QString region
 */
static QString GetRegionFromSMDH(const Loader::SMDH& smdh) {
    const Loader::SMDH::Region region{smdh.GetRegion()};
    switch (region) {
    case Loader::SMDH::Region::Invalid:
        return "Invalid region";
    case Loader::SMDH::Region::Japan:
        return "Japan";
    case Loader::SMDH::Region::NorthAmerica:
        return "North America";
    case Loader::SMDH::Region::Europe:
        return "Europe";
    case Loader::SMDH::Region::Australia:
        return "Australia";
    case Loader::SMDH::Region::China:
        return "China";
    case Loader::SMDH::Region::Korea:
        return "Korea";
    case Loader::SMDH::Region::Taiwan:
        return "Taiwan";
    case Loader::SMDH::Region::RegionFree:
        return "Region free";
    default:
        return "Invalid Region";
    }
}

class ProgramListItem : public QStandardItem {
public:
    ProgramListItem() : QStandardItem{} {}
    ProgramListItem(const QString& string) : QStandardItem{string} {}

    virtual ~ProgramListItem() override {}
};

/// Program list icon sizes (in px)
static const std::unordered_map<UISettings::ProgramListIconSize, int> IconSizes{
    {UISettings::ProgramListIconSize::NoIcon, 0},
    {UISettings::ProgramListIconSize::SmallIcon, 24},
    {UISettings::ProgramListIconSize::LargeIcon, 48},
};

/**
 * A specialization of ProgramListItem for path values.
 * This class ensures that for every full path value it holds, a correct string
 * representation of just the filename (with no extension) will be displayed to the user. If
 * this class receives valid SMDH data, it will also display icons and titles.
 */
class ProgramListItemPath : public ProgramListItem {
public:
    ProgramListItemPath() : ProgramListItem{} {}

    ProgramListItemPath(const QString& program_path, const std::vector<u8>& smdh_data,
                        u64 program_id, u64 extdata_id)
        : ProgramListItem{} {
        setData(program_path, FullPathRole);
        setData(qulonglong(program_id), ProgramIdRole);
        setData(qulonglong(extdata_id), ExtdataIdRole);
        if (UISettings::values.program_list_icon_size == UISettings::ProgramListIconSize::NoIcon)
            // Don't display icons
            setData(QPixmap{}, Qt::DecorationRole);
        bool large{UISettings::values.program_list_icon_size ==
                   UISettings::ProgramListIconSize::LargeIcon};
        if (!Loader::IsValidSMDH(smdh_data)) {
            // SMDH isn't valid, set a default icon
            if (UISettings::values.program_list_icon_size !=
                UISettings::ProgramListIconSize::NoIcon)
                setData(GetDefaultIcon(large), Qt::DecorationRole);
            return;
        }
        Loader::SMDH smdh;
        std::memcpy(&smdh, smdh_data.data(), sizeof(Loader::SMDH));
        // Get icon from SMDH
        if (UISettings::values.program_list_icon_size != UISettings::ProgramListIconSize::NoIcon)
            setData(GetQPixmapFromSMDH(smdh, large), Qt::DecorationRole);
        // Get short title from SMDH
        setData(GetQStringShortTitleFromSMDH(smdh, Loader::SMDH::TitleLanguage::English),
                TitleRole);
        // Get publisher from SMDH
        setData(GetQStringPublisherFromSMDH(smdh, Loader::SMDH::TitleLanguage::English),
                PublisherRole);
    }

    int type() const override {
        return static_cast<int>(ProgramListItemType::Program);
    }

    QVariant data(int role) const override {
        if (role == Qt::DisplayRole) {
            std::string path, filename, extension;
            auto sdmc_dir{
                FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir, Settings::values.sdmc_dir)};
            Common::SplitPath(data(FullPathRole).toString().toStdString(), &path, &filename,
                              &extension);
            const std::unordered_map<UISettings::ProgramListText, QString> display_texts{
                {UISettings::ProgramListText::FileName,
                 QString::fromStdString(filename + extension)},
                {UISettings::ProgramListText::FullPath, data(FullPathRole).toString()},
                {UISettings::ProgramListText::ProgramName, data(TitleRole).toString()},
                {UISettings::ProgramListText::ProgramID,
                 QString::fromStdString(fmt::format("{:016X}", data(ProgramIdRole).toULongLong()))},
                {UISettings::ProgramListText::Publisher, data(PublisherRole).toString()},
            };
            const auto& row1{display_texts.at(UISettings::values.program_list_row_1)};
            QString row2;
            auto row_2_id{UISettings::values.program_list_row_2};
            if (row_2_id != UISettings::ProgramListText::NoText)
                row2 = (row1.isEmpty() ? "" : "\n     ") + display_texts.at(row_2_id);
            return row1 + row2;
        } else {
            return ProgramListItem::data(role);
        }
    }

    static const int FullPathRole{Qt::UserRole + 1};
    static const int TitleRole{Qt::UserRole + 2};
    static const int ProgramIdRole{Qt::UserRole + 3};
    static const int ExtdataIdRole{Qt::UserRole + 4};
    static const int PublisherRole{Qt::UserRole + 5};
};

class ProgramListItemIssues : public ProgramListItem {
public:
    ProgramListItemIssues() : ProgramListItem{} {}

    explicit ProgramListItemIssues(u64 program_id) {
        auto it{issues_map.find(program_id)};
        if (it == issues_map.end())
            setText("0");
        else
            setText(QString::number(it->second.size()));
    }

    int type() const override {
        return static_cast<int>(ProgramListItemType::Program);
    }
};

class ProgramListItemRegion : public ProgramListItem {
public:
    ProgramListItemRegion() : ProgramListItem{} {}

    explicit ProgramListItemRegion(const std::vector<u8>& smdh_data) {
        if (!Loader::IsValidSMDH(smdh_data))
            setText("Invalid region");
        else {
            Loader::SMDH smdh;
            std::memcpy(&smdh, smdh_data.data(), sizeof(Loader::SMDH));
            setText(GetRegionFromSMDH(smdh));
        }
    }
};

/**
 * A specialization of ProgramListItem for size values.
 * This class ensures that for every numerical size value it holds (in bytes), a correct
 * human-readable string representation will be displayed to the user.
 */
class ProgramListItemSize : public ProgramListItem {
public:
    ProgramListItemSize() : ProgramListItem{} {}

    ProgramListItemSize(const qulonglong size_bytes) : ProgramListItem{} {
        setData(size_bytes, SizeRole);
    }

    void setData(const QVariant& value, int role) override {
        // By specializing setData for SizeRole, we can ensure that the numerical and string
        // representations of the data are always accurate and in the correct format.
        if (role == SizeRole) {
            qulonglong size_bytes{value.toULongLong()};
            ProgramListItem::setData(ReadableByteSize(size_bytes), Qt::DisplayRole);
            ProgramListItem::setData(value, SizeRole);
        } else
            ProgramListItem::setData(value, role);
    }

    int type() const override {
        return static_cast<int>(ProgramListItemType::Program);
    }

    /**
     * This operator is, in practice, only used by the TreeView sorting systems.
     * Override it so that it will correctly sort by numerical value instead of by string
     * representation.
     */
    bool operator<(const QStandardItem& other) const override {
        return data(SizeRole).toULongLong() < other.data(SizeRole).toULongLong();
    }

private:
    static const int SizeRole{Qt::UserRole + 1};
};

class ProgramListDir : public ProgramListItem {
public:
    ProgramListDir() : ProgramListItem{} {}

    explicit ProgramListDir(UISettings::AppDir& directory,
                            ProgramListItemType type = ProgramListItemType::CustomDir)
        : dir_type{type} {
        auto program_dir{&directory};
        setData(QVariant::fromValue(program_dir), AppDirRole);
        int icon_size{IconSizes.at(UISettings::values.program_list_icon_size)};
        switch (dir_type) {
        case ProgramListItemType::InstalledDir:
            setData(QIcon::fromTheme("sd_card").pixmap(icon_size), Qt::DecorationRole);
            setData("Installed", Qt::DisplayRole);
            break;
        case ProgramListItemType::SystemDir:
            setData(QIcon::fromTheme("chip").pixmap(icon_size), Qt::DecorationRole);
            setData("System", Qt::DisplayRole);
            break;
        case ProgramListItemType::CustomDir:
            QString icon_name{QFileInfo::exists(program_dir->path) ? "folder" : "bad_folder"};
            setData(QIcon::fromTheme(icon_name).pixmap(icon_size), Qt::DecorationRole);
            setData(program_dir->path, Qt::DisplayRole);
            break;
        }
    }

    int type() const override {
        return static_cast<int>(dir_type);
    }

    static const int AppDirRole{Qt::UserRole + 1};

private:
    ProgramListItemType dir_type;
};

class ProgramListAddDir : public ProgramListItem {
public:
    explicit ProgramListAddDir() : ProgramListItem{} {
        setData(type(), TypeRole);
        int icon_size{IconSizes.at(UISettings::values.program_list_icon_size)};
        setData(QIcon::fromTheme("plus").pixmap(icon_size), Qt::DecorationRole);
        setData("Add New Program Directory", Qt::DisplayRole);
    }

    int type() const override {
        return static_cast<int>(ProgramListItemType::AddDir);
    }

private:
    static const int TypeRole{Qt::UserRole + 1};
};

class ProgramList;
class QHBoxLayout;
class QTreeView;
class QLabel;
class QLineEdit;
class QToolButton;

class ProgramListSearchField : public QWidget {
    Q_OBJECT

public:
    explicit ProgramListSearchField(ProgramList* parent = nullptr);

    void setFilterResult(int visible, int total);

    void clear();
    void setFocus();

    int visible;
    int total;

private:
    class KeyReleaseEater : public QObject {
    public:
        explicit KeyReleaseEater(ProgramList* programlist);

    private:
        ProgramList* programlist;
        QString edit_filter_text_old;

    protected:
        // EventFilter in order to process systemkeys while editing the searchfield
        bool eventFilter(QObject* obj, QEvent* event) override;
    };

    QHBoxLayout* layout_filter;
    QTreeView* tree_view;
    QLabel* label_filter;
    QLineEdit* edit_filter;
    QLabel* label_filter_result;
    QToolButton* button_filter_close;
};
