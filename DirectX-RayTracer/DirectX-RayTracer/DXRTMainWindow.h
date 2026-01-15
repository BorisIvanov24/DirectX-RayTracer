#pragma once
#include <QMainWindow>
#include <QGridLayout>
#include <QSlider>
#include <QLabel>
#include <QDockWidget>
#include "DXRTViewportWidget.h"
#include <QSpinBox>
#include <QComboBox>

class DXRTApp;

class DXRTMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    DXRTMainWindow(DXRTApp* app, QWidget* parent = nullptr);

    void setFPS(const int fps);
    void updateViewport(const QImage& image);
    HWND getNativeWindowHandle();
    DXRTViewportWidget* getViewport();
    void closeEvent(QCloseEvent* event) override;

private:
    void createMenusAndToolbars();
    void createCameraControlsDock(); // <-- new

private:
    QGridLayout* mainLayout = nullptr;
    DXRTViewportWidget* viewport = nullptr;
    QWidget* statusBar = nullptr;
    QLabel* statusFPS = nullptr;
    DXRTApp* app = nullptr;

    // Camera controls
    QSlider* moveSpeedSlider = nullptr;
    QSlider* mouseSensSlider = nullptr;
    QSlider* mouseScrollSpeedSlider = nullptr;
    QSpinBox* moveSpeedSpinBox = nullptr;
    QSpinBox* mouseSensSpinBox = nullptr;
    QSpinBox* mouseScrollSpeedSpinBox = nullptr;

    QComboBox* shadingModeComboBox = nullptr;

};
