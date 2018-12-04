// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QLabel>
#include <QMainWindow>
#include <QTimer>
#include "citra/hotkeys.h"
#include "common/announce_multiplayer_room.h"
#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "network/room.h"
#include "network/room_member.h"
#include "ui_main.h"

class AboutDialog;
class Config;
class ControlPanel;
class ClickableLabel;
class EmuThread;
class ProgramList;
enum class ProgramListOpenTarget;
class ProgramListPlaceholder;
class GImageInfo;
class Screens;
class MultiplayerState;
template <typename>
class QFutureWatcher;
class QProgressBar;

class GMainWindow : public QMainWindow {
    Q_OBJECT

    /// Max number of recently loaded files to keep track of
    static constexpr int MaxRecentFiles{10};

public:
    explicit GMainWindow(Config& config, Core::System& system);
    ~GMainWindow();

    void filterBarSetChecked(bool state);
    void UpdateUITheme();

    Q_INVOKABLE void Update3D();
    Q_INVOKABLE void UpdateNetwork();
    Q_INVOKABLE void UpdateFrameAdvancing();

signals:
    /**
     * Signal that is emitted when a new EmuThread has been created and an emulation session is
     * about to start. At this time, the core system emulation has been initialized, and all
     * emulation handles and memory should be valid.
     *
     * @param emu_thread Pointer to the newly created EmuThread (to be used by widgets that need to
     *      access/change emulation state).
     */
    void EmulationStarting(EmuThread* emu_thread);

    /**
     * Signal that is emitted when emulation is about to stop. At this time, the EmuThread and core
     * system emulation handles and memory are still valid, but are about become invalid.
     */
    void EmulationStopping();

    void UpdateProgress(std::size_t written, std::size_t total);
    void CIAInstallReport(Service::AM::InstallStatus status, const QString& filepath);
    void CIAInstallFinished();

    // Signal that tells widgets to update icons to use the current theme
    void UpdateThemedIcons();

private slots:
    void OnStartProgram();
    void OnPauseProgram();
    void OnStopProgram();
    void OnTouchChanged(unsigned, unsigned);

    /// Called when user selects an program in the program list widget.
    void OnProgramListLoadFile(const QString& path);

    void OnProgramListOpenFolder(u64 program_id, ProgramListOpenTarget target);
    void OnProgramListOpenDirectory(const QString& path);
    void OnProgramListAddDirectory();
    void OnProgramListShowList(bool show);
    void OnMenuLoadFile();
    void OnMenuInstallCIA();
    void OnMenuAddSeed();
    void OnUpdateProgress(std::size_t written, std::size_t total);
    void OnCIAInstallReport(Service::AM::InstallStatus status, const QString& filepath);
    void OnCIAInstallFinished();
    void OnMenuRecentFile();
    void OnOpenConfiguration();
    void OnSetPlayCoins();
    void OnCheats();
    void OnControlPanel();
    void OnOpenUserDirectory();
    void OnNANDDefault();
    void OnNANDCustom();
    void OnSDMCDefault();
    void OnSDMCCustom();
    void OnLoadAmiibo();
    void OnRemoveAmiibo();
    void OnToggleFilterBar();
    void ToggleFullscreen();
    void ChangeScreenLayout();
    void ToggleScreenLayout();
    void OnSwapScreens();
    void OnCustomLayout();
    void ToggleSleepMode();
    void ShowFullscreen();
    void HideFullscreen();
    void OnRecordMovie();
    void OnPlayMovie();
    void OnStopRecordingPlayback();
    void OnCaptureScreenshot();
    void OnDumpRAM();
    void OnCoreError(Core::System::ResultStatus, const std::string&);

    /// Called when user selects Help -> About Citra
    void OnMenuAboutCitra();

private:
    void InitializeDiscordRPC();
    void ShutdownDiscordRPC();
    void UpdateDiscordRPC(const Network::RoomInformation& info);

    void InitializeWidgets();
    void InitializeRecentFileMenuActions();
    void InitializeHotkeys();
    void SetDefaultUIGeometry();
    void SyncMenuUISettings();
    void RestoreUIState();
    void ConnectWidgetEvents();
    void ConnectMenuEvents();

    bool LoadProgram(const std::string& filename);
    void BootProgram(const std::string& filename);
    void ShutdownProgram();

    /**
     * Stores the filename in the recently loaded files list.
     * The new filename is stored at the beginning of the recently loaded files list.
     * After inserting the new entry, duplicates are removed meaning that if
     * this was inserted from OnMenuRecentFile(), the entry will be put on top
     * and remove from its previous position.
     *
     * Finally, this function calls \a UpdateRecentFiles() to update the UI.
     *
     * @param filename the filename to store
     */
    void StoreRecentFile(const QString& filename);

    /**
     * Updates the recent files menu.
     * Menu entries are rebuilt from the configuration file.
     * If there is no entry in the menu, the menu is greyed out.
     */
    void UpdateRecentFiles();

    /**
     * If the emulation is running,
     * asks the user if he really want to close the emulator
     *
     * @return true if the user confirmed
     */
    bool ConfirmClose();
    bool ConfirmChangeProgram();
    void closeEvent(QCloseEvent* event) override;

    bool ValidateMovie(const QString& path, u64 program_id = 0);
    void UpdatePerfStats();
    void UpdateTitle();

    Q_INVOKABLE void OnMoviePlaybackCompleted();

    Ui::MainWindow ui;

    Screens* screens;

    ProgramList* program_list;
    ProgramListPlaceholder* program_list_placeholder;

    // Status bar elements
    QProgressBar* progress_bar;
    QLabel* message_label;
    QLabel* perf_stats_label;
    QLabel* touch_label;

    s64 discord_rpc_start_time;

    QTimer perf_stats_update_timer;
    QTimer movie_play_timer;

    Network::RoomMember::CallbackHandle<Network::RoomInformation> callback_handle;

    Config& config;
    Core::System& system;

    MultiplayerState* multiplayer_state;

    std::unique_ptr<EmuThread> emu_thread;

    // The short title of the program currently running
    std::string short_title;

    // Movie
    bool movie_record_on_start{};
    QString movie_record_path;

    std::shared_ptr<ControlPanel> control_panel;

    QAction* actions_recent_files[MaxRecentFiles];

    // Stores default icon theme search paths for the platform
    QStringList default_theme_paths;

    HotkeyRegistry hotkey_registry;

protected:
    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
};

Q_DECLARE_METATYPE(std::size_t);
Q_DECLARE_METATYPE(Service::AM::InstallStatus);
