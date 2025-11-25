#pragma once
#include "CDXCRenderer.h"
#include <QObject>
#include "DXRTMainWindow.h"
#include <QTimer>

class DXRTApp : public QObject
{
	Q_OBJECT

public:
	// Prepare the application for rendering
	bool init();

public slots:
	// Initiate frame rendering
	void onIdleTick();

	// Close the editor properly, wait for the current GPU tasks
	void onQuit();

private:
	// Create the main window for the editor
	bool initWindow();

	// Initiate frame rendering and consume the result
	void renderFrame();

	// Update the rendering stats based on the FPS timer
	void updateRenderStats();

private:
	CDXCRenderer renderer; // The actual GPU DX 12 renderer
	DXRTMainWindow* mainWnd; // The main window for the editor
	QTimer* idleTimer; // The timer for implementing the rendering loop 
	QTimer* fpsTimer; // The timer to track the FPS value
	int frameIdxAtLastFPSCalc = 0;
};

