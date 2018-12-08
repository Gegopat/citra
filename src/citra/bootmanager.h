// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <QGLWidget>
#include <QImage>
#include <QThread>
#include "core/core.h"
#include "core/frontend.h"

class QKeyEvent;
class QScreen;

class GGLWidgetInternal;
class GMainWindow;
class Screens;
class QTouchEvent;

class EmuThread : public QThread {
    Q_OBJECT

public:
    explicit EmuThread(Core::System& system, Screens* screens);

    /**
     * Start emulation (on new thread)
     * @warning Only call when not running!
     */
    void run() override;

    /// Requests for the emulation thread to stop running
    void RequestStop() {
        stop_run = true;
        system.SetRunning(false);
    }

private:
    bool running{};
    std::atomic_bool stop_run{};
    Screens* screens;
    Core::System& system;

signals:
    void ErrorThrown(Core::System::ResultStatus, const std::string&);
};

class Screens : public QWidget, public Frontend {
    Q_OBJECT

public:
    explicit Screens(GMainWindow* parent, EmuThread* emu_thread, Core::System& system);
    ~Screens() override;

    void SwapBuffers() override;
    void MakeCurrent() override;
    void DoneCurrent() override;

    void BackupGeometry();
    void RestoreGeometry();
    void restoreGeometry(const QByteArray& geometry); // overridden
    QByteArray saveGeometry();                        // overridden

    qreal WindowPixelRatio() const;

    void closeEvent(QCloseEvent* event) override;

    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

    bool event(QEvent* event) override;

    void focusOutEvent(QFocusEvent* event) override;

    void InitRenderTarget();

    void CaptureScreenshot(const QString& screenshot_path);

    Q_INVOKABLE void LaunchSoftwareKeyboardImpl(HLE::Applets::SoftwareKeyboardConfig& config,
                                                std::u16string& text, bool& is_running);
    Q_INVOKABLE void LaunchErrEulaImpl(HLE::Applets::ErrEulaConfig& config, bool& is_running);
    Q_INVOKABLE void LaunchMiiSelectorImpl(const HLE::Applets::MiiConfig& config,
                                           HLE::Applets::MiiResult& result, bool& is_running);

    void LaunchSoftwareKeyboard(HLE::Applets::SoftwareKeyboardConfig& config, std::u16string& text,
                                bool& is_running) override {
        LaunchSoftwareKeyboardImpl(config, text, is_running);
    }

    void LaunchErrEula(HLE::Applets::ErrEulaConfig& config, bool& is_running) override {
        LaunchErrEulaImpl(config, is_running);
    }

    void LaunchMiiSelector(const HLE::Applets::MiiConfig& config, HLE::Applets::MiiResult& result,
                           bool& is_running) override {
        LaunchMiiSelectorImpl(config, result, is_running);
    }

    void Update3D() override;
    void UpdateNetwork() override;
    void UpdateFrameAdvancing() override;

public slots:
    void moveContext(); // Overridden

    void OnEmulationStarting(EmuThread* emu_thread);
    void OnEmulationStopping();
    void OnFramebufferSizeChanged();

signals:
    /// Emitted when the window is closed
    void Closed();

    void TouchScreenPositionChanged(unsigned, unsigned);

private:
    std::pair<unsigned, unsigned> ScaleTouch(const QPointF pos) const;
    void TouchBeginEvent(const QTouchEvent* event);
    void TouchUpdateEvent(const QTouchEvent* event);
    void TouchEndEvent();

    GGLWidgetInternal* child{};

    QByteArray geometry;

    EmuThread* emu_thread;

    /// Temporary storage of the screenshot taken
    QImage screenshot_image;

    Core::System& system;
    GMainWindow* window;

protected:
    void showEvent(QShowEvent* event) override;
};
