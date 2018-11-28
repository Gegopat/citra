// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QModelIndex>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QThreadPool>
#include <QToolButton>
#include <QTreeView>
#include "citra/main.h"
#include "citra/program_list.h"
#include "citra/program_list_p.h"
#include "citra/program_list_worker.h"
#include "citra/ui_settings.h"
#include "common/common_paths.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/archive_source_sd_savedata.h"
#include "core/hle/service/fs/archive.h"
#include "core/settings.h"

ProgramListSearchField::KeyReleaseEater::KeyReleaseEater(ProgramList* programlist)
    : programlist{programlist} {}

// EventFilter in order to process systemkeys while editing the searchfield
bool ProgramListSearchField::KeyReleaseEater::eventFilter(QObject* obj, QEvent* event) {
    // If it isn't a KeyRelease event then continue with standard event processing
    if (event->type() != QEvent::KeyRelease)
        return QObject::eventFilter(obj, event);
    auto keyEvent{static_cast<QKeyEvent*>(event)};
    auto edit_filter_text{programlist->search_field->edit_filter->text().toLower()};
    // If the searchfield's text hasn't changed special function keys get checked
    // If no function key changes the searchfield's text the filter doesn't need to get reloaded
    if (edit_filter_text == edit_filter_text_old) {
        switch (keyEvent->key()) {
        // Escape: Resets the searchfield
        case Qt::Key_Escape: {
            if (edit_filter_text_old.isEmpty())
                return QObject::eventFilter(obj, event);
            else {
                programlist->search_field->edit_filter->clear();
                edit_filter_text.clear();
            }
            break;
        }
        // Return and Enter
        // If the enter key gets pressed first checks how many and which entry is visible
        // If there is only one result launch this program
        case Qt::Key_Return:
        case Qt::Key_Enter: {
            if (programlist->search_field->visible == 1) {
                auto file_path{programlist->getLastFilterResultItem()};
                // To avoid loading error dialog loops while confirming them using enter
                // Also users usually want to run a different program after closing one
                programlist->search_field->edit_filter->clear();
                edit_filter_text.clear();
                emit programlist->ProgramChosen(file_path);
            } else
                return QObject::eventFilter(obj, event);
            break;
        }
        default:
            return QObject::eventFilter(obj, event);
        }
    }
    edit_filter_text_old = edit_filter_text;
    return QObject::eventFilter(obj, event);
}

void ProgramListSearchField::setFilterResult(int visible, int total) {
    this->visible = visible;
    this->total = total;
    label_filter_result->setText(
        QString("%1 of %3 %4").arg(visible).arg(total).arg(total == 1 ? "result" : "results"));
}

QString ProgramList::getLastFilterResultItem() {
    QStandardItem* folder;
    QStandardItem* child;
    QString file_path;
    int folderCount{item_model->rowCount()};
    for (int i{}; i < folderCount; ++i) {
        folder = item_model->item(i, 0);
        auto folder_index{folder->index()};
        int childrenCount{folder->rowCount()};
        for (int j{}; j < childrenCount; ++j)
            if (!tree_view->isRowHidden(j, folder_index)) {
                child = folder->child(j, 0);
                file_path = child->data(ProgramListItemPath::FullPathRole).toString();
            }
    }
    return file_path;
}

void ProgramListSearchField::clear() {
    edit_filter->clear();
}

void ProgramListSearchField::setFocus() {
    if (edit_filter->isVisible())
        edit_filter->setFocus();
}

ProgramListSearchField::ProgramListSearchField(ProgramList* parent) : QWidget{parent} {
    auto keyReleaseEater{new KeyReleaseEater(parent)};
    layout_filter = new QHBoxLayout;
    layout_filter->setMargin(8);
    label_filter = new QLabel;
    label_filter->setText("Filter:");
    edit_filter = new QLineEdit;
    edit_filter->clear();
    edit_filter->setPlaceholderText("Enter pattern to filter");
    edit_filter->installEventFilter(keyReleaseEater);
    edit_filter->setClearButtonEnabled(true);
    connect(edit_filter, &QLineEdit::textChanged, parent, &ProgramList::onTextChanged);
    label_filter_result = new QLabel;
    button_filter_close = new QToolButton(this);
    button_filter_close->setText("X");
    button_filter_close->setCursor(Qt::ArrowCursor);
    button_filter_close->setStyleSheet("QToolButton{ border: none; padding: 0px; color: "
                                       "#000000; font-weight: bold; background: #F0F0F0; }"
                                       "QToolButton:hover{ border: none; padding: 0px; color: "
                                       "#EEEEEE; font-weight: bold; background: #E81123}");
    connect(button_filter_close, &QToolButton::clicked, parent, &ProgramList::onFilterCloseClicked);
    layout_filter->setSpacing(10);
    layout_filter->addWidget(label_filter);
    layout_filter->addWidget(edit_filter);
    layout_filter->addWidget(label_filter_result);
    layout_filter->addWidget(button_filter_close);
    setLayout(layout_filter);
}

/**
 * Checks if all words separated by spaces are contained in another string
 * This offers a word order insensitive search function
 * @param String that gets checked if it contains all words of the userinput string
 * @param String containing all words getting checked
 * @return true if the haystack contains all words of userinput
 */

static bool ContainsAllWords(const QString& haystack, const QString& userinput) {
    const auto userinput_split{userinput.split(' ', QString::SplitBehavior::SkipEmptyParts)};
    return std::all_of(userinput_split.begin(), userinput_split.end(),
                       [&haystack](const QString& s) { return haystack.contains(s); });
}

// Syncs the expanded state of Program Directories with settings to persist across sessions
void ProgramList::OnItemExpanded(const QModelIndex& item) {
    // The click should still register in the ProgramListItemPath item no matter which column was
    // clicked
    int row{item_model->itemFromIndex(item)->row()};
    QStandardItem* child{item_model->invisibleRootItem()->child(row, COLUMN_NAME)};
    ProgramListItemType type{static_cast<ProgramListItemType>(child->type())};
    if (type == ProgramListItemType::CustomDir || type == ProgramListItemType::InstalledDir ||
        type == ProgramListItemType::SystemDir)
        child->data(ProgramListDir::AppDirRole).value<UISettings::AppDir*>()->expanded =
            tree_view->isExpanded(child->index());
}

// Event in order to filter the ProgramList after editing the searchfield
void ProgramList::onTextChanged(const QString& newText) {
    int folderCount{tree_view->model()->rowCount()};
    auto edit_filter_text{newText.toLower()};
    QStandardItem* folder;
    int childrenTotal{};
    // If the searchfield is empty every item is visible
    // Otherwise the filter gets applied
    if (edit_filter_text.isEmpty()) {
        for (int i{}; i < folderCount; ++i) {
            folder = item_model->item(i, 0);
            auto folder_index{folder->index()};
            int childrenCount{folder->rowCount()};
            for (int j{}; j < childrenCount; ++j) {
                ++childrenTotal;
                tree_view->setRowHidden(j, folder_index, false);
            }
        }
        search_field->setFilterResult(childrenTotal, childrenTotal);
    } else {
        int result_count{};
        for (int i{}; i < folderCount; ++i) {
            folder = item_model->item(i, 0);
            auto folder_index{folder->index()};
            int childrenCount{folder->rowCount()};
            for (int j{}; j < childrenCount; ++j) {
                ++childrenTotal;
                const auto child{folder->child(j, 0)};
                const auto file_path{
                    child->data(ProgramListItemPath::FullPathRole).toString().toLower()};
                auto file_name{file_path.mid(file_path.lastIndexOf("/") + 1)};
                auto file_title{child->data(ProgramListItemPath::TitleRole).toString().toLower()};
                auto file_programid{
                    child->data(ProgramListItemPath::ProgramIdRole).toString().toLower()};
                // Only items which filename in combination with its title contains all words
                // that are in the searchfield will be visible in the ProgramList
                // The search is case insensitive because of toLower()
                // I decided not to use Qt::CaseInsensitive in ContainsAllWords to prevent
                // multiple conversions of edit_filter_text for each program in the program
                // list
                if (ContainsAllWords(file_name.append(' ').append(file_title), edit_filter_text) ||
                    (file_programid.count() == 16 && edit_filter_text.contains(file_programid))) {
                    tree_view->setRowHidden(j, folder_index, false);
                    ++result_count;
                } else
                    tree_view->setRowHidden(j, folder_index, true);
                search_field->setFilterResult(result_count, childrenTotal);
            }
        }
    }
}

void ProgramList::OnUpdateThemedIcons() {
    for (int i{}; i < item_model->invisibleRootItem()->rowCount(); i++) {
        QStandardItem* child{item_model->invisibleRootItem()->child(i)};
        switch (static_cast<ProgramListItemType>(child->type())) {
        case ProgramListItemType::InstalledDir:
            child->setData(QIcon::fromTheme("sd_card").pixmap(48), Qt::DecorationRole);
            break;
        case ProgramListItemType::SystemDir:
            child->setData(QIcon::fromTheme("chip").pixmap(48), Qt::DecorationRole);
            break;
        case ProgramListItemType::CustomDir: {
            const UISettings::AppDir* program_dir{
                child->data(ProgramListDir::AppDirRole).value<UISettings::AppDir*>()};
            QString icon_name{QFileInfo::exists(program_dir->path) ? "folder" : "bad_folder"};
            child->setData(QIcon::fromTheme(icon_name).pixmap(48), Qt::DecorationRole);
            break;
        }
        case ProgramListItemType::AddDir:
            child->setData(QIcon::fromTheme("plus").pixmap(48), Qt::DecorationRole);
            break;
        }
    }
}

void ProgramList::onFilterCloseClicked() {
    main_window->filterBarSetChecked(false);
}

ProgramList::ProgramList(Core::System& system, GMainWindow* parent)
    : QWidget{parent}, system{system} {
    watcher = new QFileSystemWatcher(this);
    connect(watcher, &QFileSystemWatcher::directoryChanged, this, &ProgramList::Refresh,
            Qt::UniqueConnection);
    this->main_window = parent;
    layout = new QVBoxLayout;
    tree_view = new QTreeView;
    search_field = new ProgramListSearchField(this);
    item_model = new QStandardItemModel(tree_view);
    tree_view->setModel(item_model);
    tree_view->setAlternatingRowColors(true);
    tree_view->setSelectionMode(QHeaderView::SingleSelection);
    tree_view->setSelectionBehavior(QHeaderView::SelectRows);
    tree_view->setVerticalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setHorizontalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setSortingEnabled(true);
    tree_view->setEditTriggers(QHeaderView::NoEditTriggers);
    tree_view->setUniformRowHeights(true);
    tree_view->setContextMenuPolicy(Qt::CustomContextMenu);
    item_model->insertColumns(0, COLUMN_COUNT);
    item_model->setHeaderData(COLUMN_NAME, Qt::Horizontal, "Name");
    item_model->setHeaderData(COLUMN_ISSUES, Qt::Horizontal, "Issues");
    item_model->setHeaderData(COLUMN_REGION, Qt::Horizontal, "Region");
    item_model->setHeaderData(COLUMN_FILE_TYPE, Qt::Horizontal, "File type");
    item_model->setHeaderData(COLUMN_SIZE, Qt::Horizontal, "Size");
    tree_view->setColumnWidth(COLUMN_NAME, 500);
    item_model->setSortRole(ProgramListItemPath::TitleRole);
    connect(main_window, &GMainWindow::UpdateThemedIcons, this, &ProgramList::OnUpdateThemedIcons);
    connect(tree_view, &QTreeView::activated, this, &ProgramList::ValidateEntry);
    connect(tree_view, &QTreeView::customContextMenuRequested, this,
            &ProgramList::PopupContextMenu);
    connect(tree_view, &QTreeView::expanded, this, &ProgramList::OnItemExpanded);
    connect(tree_view, &QTreeView::collapsed, this, &ProgramList::OnItemExpanded);
    // We must register all custom types with the Qt Automoc system so that we are able to use
    // it with signals/slots. In this case, QList falls under the umbrells of custom types.
    qRegisterMetaType<QList<QStandardItem*>>("QList<QStandardItem*>");
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tree_view);
    layout->addWidget(search_field);
    setLayout(layout);
}

ProgramList::~ProgramList() {
    emit ShouldCancelWorker();
}

void ProgramList::setFilterFocus() {
    if (tree_view->model()->rowCount() > 0)
        search_field->setFocus();
}

void ProgramList::setFilterVisible(bool visibility) {
    search_field->setVisible(visibility);
}

void ProgramList::setDirectoryWatcherEnabled(bool enabled) {
    if (enabled)
        connect(watcher, &QFileSystemWatcher::directoryChanged, this, &ProgramList::Refresh,
                Qt::UniqueConnection);
    else
        disconnect(watcher, &QFileSystemWatcher::directoryChanged, this, &ProgramList::Refresh);
}

void ProgramList::clearFilter() {
    search_field->clear();
}

void ProgramList::AddDirEntry(ProgramListDir* entry_items) {
    item_model->invisibleRootItem()->appendRow(entry_items);
    tree_view->setExpanded(
        entry_items->index(),
        entry_items->data(ProgramListDir::AppDirRole).value<UISettings::AppDir*>()->expanded);
}

void ProgramList::AddEntry(const QList<QStandardItem*>& entry_items, ProgramListDir* parent) {
    parent->appendRow(entry_items);
}

void ProgramList::ValidateEntry(const QModelIndex& item) {
    // The click should still register in the first column no matter which column was
    // clicked
    int row{item_model->itemFromIndex(item)->row()};
    const auto* parent{item_model->itemFromIndex(item.parent())};
    if (!parent)
        parent = item_model->invisibleRootItem();
    const auto child{parent->child(row, COLUMN_NAME)};
    switch (static_cast<ProgramListItemType>(child->type())) {
    case ProgramListItemType::Program: {
        auto file_path{child->data(ProgramListItemPath::FullPathRole).toString()};
        if (file_path.isEmpty())
            return;
        QFileInfo file_info{file_path};
        if (!file_info.exists() || file_info.isDir())
            return;
        // Users usually want to run a different program after closing one
        search_field->clear();
        emit ProgramChosen(file_path);
        break;
    }
    case ProgramListItemType::AddDir:
        emit AddDirectory();
        break;
    }
}

bool ProgramList::isEmpty() {
    for (int i{}; i < item_model->invisibleRootItem()->rowCount(); i++) {
        const auto child{item_model->invisibleRootItem()->child(i)};
        auto type{static_cast<ProgramListItemType>(child->type())};
        if (!child->hasChildren() &&
            (type == ProgramListItemType::InstalledDir || type == ProgramListItemType::SystemDir)) {
            item_model->invisibleRootItem()->removeRow(child->row());
            i--;
        };
    }
    return !item_model->invisibleRootItem()->hasChildren();
}

void ProgramList::DonePopulating(QStringList watch_list) {
    if (isEmpty())
        emit ShowList(false);
    else {
        item_model->sort(COLUMN_NAME);
        item_model->invisibleRootItem()->appendRow(new ProgramListAddDir());
        emit ShowList(true);
    }
    // Clear out the old directories to watch for changes and add the new ones
    auto watch_dirs{watcher->directories()};
    if (!watch_dirs.isEmpty())
        watcher->removePaths(watch_dirs);
    // Workaround: Add the watch paths in chunks to allow the gui to refresh
    // This prevents the UI from stalling when a large number of watch paths are added
    // Also artificially caps the watcher to a certain number of directories
    constexpr int LIMIT_WATCH_DIRECTORIES{5000};
    constexpr int SLICE_SIZE{25};
    int len{std::min(watch_list.length(), LIMIT_WATCH_DIRECTORIES)};
    for (int i{}; i < len; i += SLICE_SIZE) {
        watcher->addPaths(watch_list.mid(i, i + SLICE_SIZE));
        QCoreApplication::processEvents();
    }
    tree_view->setEnabled(true);
    int folderCount{tree_view->model()->rowCount()};
    int childrenTotal{};
    for (int i{}; i < folderCount; ++i) {
        int childrenCount{item_model->item(i, 0)->rowCount()};
        for (int j{}; j < childrenCount; ++j)
            ++childrenTotal;
    }
    search_field->setFilterResult(childrenTotal, childrenTotal);
    if (childrenTotal > 0)
        search_field->setFocus();
}

void ProgramList::PopupContextMenu(const QPoint& menu_location) {
    auto item{tree_view->indexAt(menu_location)};
    if (!item.isValid())
        return;
    // The click should still register in the first column no matter which column was
    // clicked
    int row{item_model->itemFromIndex(item)->row()};
    auto parent{item_model->itemFromIndex(item.parent())};
    // invisibleRootItem() will not appear as an item's parent
    if (!parent)
        parent = item_model->invisibleRootItem();
    auto child{parent->child(row, COLUMN_NAME)};
    QMenu context_menu;
    switch (static_cast<ProgramListItemType>(child->type())) {
    case ProgramListItemType::Program:
        AddAppPopup(context_menu, child);
        break;
    case ProgramListItemType::CustomDir:
        AddPermDirPopup(context_menu, child);
        AddCustomDirPopup(context_menu, child);
        break;
    case ProgramListItemType::InstalledDir:
    case ProgramListItemType::SystemDir:
        AddPermDirPopup(context_menu, child);
        break;
    }
    context_menu.exec(tree_view->viewport()->mapToGlobal(menu_location));
}

void ProgramList::AddAppPopup(QMenu& context_menu, QStandardItem* child) {
    u64 program_id{child->data(ProgramListItemPath::ProgramIdRole).toULongLong()};
    u64 extdata_id{child->data(ProgramListItemPath::ExtdataIdRole).toULongLong()};
    auto path{child->data(ProgramListItemPath::FullPathRole).toString()};
    auto open_save_location{context_menu.addAction("Open Save Data Location")};
    auto open_extdata_location{context_menu.addAction("Open Extra Data Location")};
    auto open_program_location{context_menu.addAction("Open Program Location")};
    auto open_update_location{context_menu.addAction("Open Update Data Location")};
    auto copy_program_id{context_menu.addAction("Copy Program ID")};
    auto uninstall{context_menu.addAction("Uninstall")};
    open_save_location->setVisible(0x0004000000000000 <= program_id &&
                                   program_id <= 0x00040000FFFFFFFF);
    uninstall->setVisible(child->parent()->text() == "Installed");
    auto sdmc_dir{FileUtil::GetUserPath(FileUtil::UserPath::SDMCDir, Settings::values.sdmc_dir)};
    open_save_location->setVisible(FileUtil::Exists(
        FileSys::ArchiveSource_SDSaveData::GetSaveDataPathFor(sdmc_dir, program_id)));
    if (extdata_id)
        open_extdata_location->setVisible(
            0x0004000000000000 <= program_id && program_id <= 0x00040000FFFFFFFF &&
            FileUtil::Exists(FileSys::GetExtDataPathFromId(sdmc_dir, extdata_id)));
    else
        open_extdata_location->setVisible(false);
    auto media_type{Service::AM::GetProgramMediaType(program_id)};
    open_program_location->setVisible(path.toStdString() ==
                                      Service::AM::GetProgramContentPath(media_type, program_id));
    open_update_location->setVisible(
        0x0004000000000000 <= program_id && program_id <= 0x00040000FFFFFFFF &&
        FileUtil::Exists(
            Service::AM::GetProgramPath(Service::FS::MediaType::SDMC, program_id + 0xe00000000) +
            "content/"));
    connect(open_save_location, &QAction::triggered, [&, program_id]() {
        emit OpenFolderRequested(program_id, ProgramListOpenTarget::SaveData);
    });
    connect(open_extdata_location, &QAction::triggered, [this, extdata_id] {
        emit OpenFolderRequested(extdata_id, ProgramListOpenTarget::ExtData);
    });
    connect(open_program_location, &QAction::triggered, [&, program_id]() {
        emit OpenFolderRequested(program_id, ProgramListOpenTarget::Program);
    });
    connect(open_update_location, &QAction::triggered, [&, program_id]() {
        emit OpenFolderRequested(program_id, ProgramListOpenTarget::UpdateData);
    });
    connect(copy_program_id, &QAction::triggered, [program_id]() {
        QApplication::clipboard()->setText(
            QString::fromStdString(fmt::format("{:016X}", program_id)));
    });
    connect(uninstall, &QAction::triggered, [this, program_id]() {
        FileUtil::DeleteDirRecursively(
            Service::AM::GetProgramPath(Service::FS::MediaType::SDMC, program_id));
    });
    auto it{issues_map.find(program_id)};
    if (it != issues_map.end() && it->second.size() != 0) {
        auto issues{context_menu.addAction("Issues")};
        connect(issues, &QAction::triggered, [&, list = it->second]() {
            QStringList qsl;
            for (const auto& issue : list) {
                switch (issue.type) {
                case Issue::Type::Normal:
                    qsl.append(QString::fromStdString(issue.data));
                    break;
                case Issue::Type::GitHub:
                    qsl.append(QString("<a href=\"https://github.com/%1/issues/%2\"><span "
                                       "style=\"text-decoration: "
                                       "underline; color:#039be5;\">%1#%2</span></a>")
                                   .arg(QString::fromStdString(issue.data),
                                        QString::number(issue.number)));
                    break;
                }
            }
            QMessageBox::information(this, "Issues", qsl.join("<br>"));
        });
    }
};

void ProgramList::AddCustomDirPopup(QMenu& context_menu, QStandardItem* child) {
    auto& program_dir{*child->data(ProgramListDir::AppDirRole).value<UISettings::AppDir*>()};
    auto deep_scan{context_menu.addAction("Scan Subfolders")};
    auto delete_dir{context_menu.addAction("Remove Program Directory")};
    deep_scan->setCheckable(true);
    deep_scan->setChecked(program_dir.deep_scan);
    connect(deep_scan, &QAction::triggered, [&] {
        program_dir.deep_scan = !program_dir.deep_scan;
        PopulateAsync(UISettings::values.program_dirs);
    });
    connect(delete_dir, &QAction::triggered, [&, child] {
        UISettings::values.program_dirs.removeOne(program_dir);
        item_model->invisibleRootItem()->removeRow(child->row());
    });
}

void ProgramList::AddPermDirPopup(QMenu& context_menu, QStandardItem* child) {
    auto& program_dir{*child->data(ProgramListDir::AppDirRole).value<UISettings::AppDir*>()};
    auto move_up{context_menu.addAction("\U000025b2 Move Up")};
    auto move_down{context_menu.addAction("\U000025bc Move Down")};
    auto open_directory_location{context_menu.addAction("Open Directory Location")};
    int row{child->row()};
    move_up->setEnabled(row > 0);
    move_down->setEnabled(row < item_model->invisibleRootItem()->rowCount() - 2);
    connect(move_up, &QAction::triggered, [&, child, row] {
        // Find the indices of the items in settings and swap them
        UISettings::values.program_dirs.swap(
            UISettings::values.program_dirs.indexOf(program_dir),
            UISettings::values.program_dirs.indexOf(*item_model->invisibleRootItem()
                                                         ->child(row - 1, COLUMN_NAME)
                                                         ->data(ProgramListDir::AppDirRole)
                                                         .value<UISettings::AppDir*>()));
        // Move the treeview items
        auto item{item_model->takeRow(row)};
        item_model->invisibleRootItem()->insertRow(row - 1, item);
        tree_view->setExpanded(child->index(), program_dir.expanded);
    });
    connect(move_down, &QAction::triggered, [&, child, row] {
        // Find the indices of the items in settings and swap them
        UISettings::values.program_dirs.swap(
            UISettings::values.program_dirs.indexOf(
                *child->data(ProgramListDir::AppDirRole).value<UISettings::AppDir*>()),
            UISettings::values.program_dirs.indexOf(*item_model->invisibleRootItem()
                                                         ->child(row + 1, COLUMN_NAME)
                                                         ->data(ProgramListDir::AppDirRole)
                                                         .value<UISettings::AppDir*>()));
        // Move the treeview items
        auto item{item_model->takeRow(row)};
        item_model->invisibleRootItem()->insertRow(row + 1, item);
        tree_view->setExpanded(child->index(), program_dir.expanded);
    });
    connect(open_directory_location, &QAction::triggered,
            [&] { emit OpenDirectory(program_dir.path); });
}

void ProgramList::PopulateAsync(QList<UISettings::AppDir>& program_dirs) {
    tree_view->setEnabled(false);
    // Delete any rows that might already exist if we're repopulating
    item_model->removeRows(0, item_model->rowCount());
    search_field->clear();
    emit ShouldCancelWorker();
    auto worker{new ProgramListWorker(system, program_dirs)};
    connect(worker, &ProgramListWorker::EntryReady, this, &ProgramList::AddEntry,
            Qt::QueuedConnection);
    connect(worker, &ProgramListWorker::DirEntryReady, this, &ProgramList::AddDirEntry,
            Qt::QueuedConnection);
    connect(worker, &ProgramListWorker::Finished, this, &ProgramList::DonePopulating,
            Qt::QueuedConnection);
    // Use DirectConnection here because worker->Cancel() is thread-safe and we want it to
    // cancel without delay.
    connect(this, &ProgramList::ShouldCancelWorker, worker, &ProgramListWorker::Cancel,
            Qt::DirectConnection);
    QThreadPool::globalInstance()->start(worker);
    current_worker = std::move(worker);
}

void ProgramList::SaveInterfaceLayout() {
    UISettings::values.programlist_header_state = tree_view->header()->saveState();
}

void ProgramList::LoadInterfaceLayout() {
    auto header{tree_view->header()};
    if (!header->restoreState(UISettings::values.programlist_header_state))
        // We're using the name column to display icons and titles
        // so make it as large as possible as default.
        header->resizeSection(COLUMN_NAME, header->width());
    item_model->sort(header->sortIndicatorSection(), header->sortIndicatorOrder());
}

const QStringList ProgramList::supported_file_extensions{
    {"3ds", "3dsx", "elf", "axf", "cci", "cxi", "app"}};

void ProgramList::Refresh() {
    if (!UISettings::values.program_dirs.isEmpty() && current_worker) {
        LOG_INFO(Frontend, "Change detected in the program directories. Reloading program list.");
        PopulateAsync(UISettings::values.program_dirs);
    }
}

QString ProgramList::FindProgramByProgramID(u64 program_id) {
    return FindProgramByProgramID(item_model->invisibleRootItem(), program_id);
}

QString ProgramList::FindProgramByProgramID(QStandardItem* current_item, u64 program_id) {
    if (current_item->type() == static_cast<int>(ProgramListItemType::Program) &&
        current_item->data(ProgramListItemPath::ProgramIdRole).toULongLong() == program_id)
        return current_item->data(ProgramListItemPath::FullPathRole).toString();
    else if (current_item->hasChildren()) {
        for (int child_id{}; child_id < current_item->rowCount(); child_id++) {
            QString path{FindProgramByProgramID(current_item->child(child_id, 0), program_id)};
            if (!path.isEmpty())
                return path;
        }
    }
    return "";
}

ProgramListPlaceholder::ProgramListPlaceholder(GMainWindow* parent)
    : QWidget{parent}, main_window{parent} {
    main_window = parent;
    connect(main_window, &GMainWindow::UpdateThemedIcons, this,
            &ProgramListPlaceholder::OnUpdateThemedIcons);
    layout = new QVBoxLayout;
    image = new QLabel;
    text = new QLabel;
    layout->setAlignment(Qt::AlignCenter);
    image->setPixmap(QIcon::fromTheme("plus_folder").pixmap(200));
    text->setText("Double-click to add a new folder to the program list ");
    QFont font = text->font();
    font.setPointSize(20);
    text->setFont(font);
    text->setAlignment(Qt::AlignHCenter);
    image->setAlignment(Qt::AlignHCenter);
    layout->addWidget(image);
    layout->addWidget(text);
    setLayout(layout);
}

ProgramListPlaceholder::~ProgramListPlaceholder() = default;

void ProgramListPlaceholder::OnUpdateThemedIcons() {
    image->setPixmap(QIcon::fromTheme("plus_folder").pixmap(200));
}

void ProgramListPlaceholder::mouseDoubleClickEvent(QMouseEvent* event) {
    emit ProgramListPlaceholder::AddDirectory();
}
