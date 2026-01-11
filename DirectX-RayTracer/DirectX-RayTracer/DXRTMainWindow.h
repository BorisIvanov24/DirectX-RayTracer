#pragma once
#include <QMainWindow>
#include <QGridLayout>
#include "DXRTViewportWidget.h"
#include <QLabel>

class DXRTApp;

class DXRTMainWindow : public QMainWindow
{
	Q_OBJECT

public:
	DXRTMainWindow(DXRTApp* app, QWidget* parent = nullptr);

	// Update the status UI element for FPS with the given value
	void setFPS(const int fps);

	// Change the viewport image with the given one
	void updateViewport(const QImage& image);

	// Close the editor properly, wait for the current GPU tasks
	void closeEvent(QCloseEvent* event) override;

	HWND getNativeWindowHandle();

	DXRTViewportWidget* getViewport();
private:
	// Create UI elements outside the viewport widget
	void createMenusAndToolbars();

private:
	QGridLayout* mainLayout = nullptr; // The layout, which holds the viewport and status bar
	DXRTViewportWidget* viewport = nullptr; // Widget for presenting the rendered result from GPU
	QWidget* statusBar = nullptr; // Widget, which holds the rendering status UI elements
	QLabel* statusFPS = nullptr; // Label for the "FPS: " text
	DXRTApp* app = nullptr; // Pointer to the application module, which links the editor and renderer
};

