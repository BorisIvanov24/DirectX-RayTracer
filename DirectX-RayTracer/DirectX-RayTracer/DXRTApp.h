#pragma once
#include "DXRTRenderer.h"
#include <QObject>
#include "DXRTMainWindow.h"
#include <QTimer>
#include <QElapsedTimer>

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

public:

	void updateCameraMovement(const QSet<int>& keys, float deltaTime);
	void rotateCamera(float yawDeg, float pitchDeg);

	float getDeltaTime() const;
private:
	// Create the main window for the editor
	bool initWindow();

	// Initiate frame rendering and consume the result
	void renderFrame();

	// Update the rendering stats based on the FPS timer
	void updateRenderStats();

private:
	DXRTRenderer renderer; // The actual GPU DX 12 renderer
	DXRTMainWindow* mainWnd = nullptr; // The main window for the editor
	QTimer* idleTimer = nullptr; // The timer for implementing the rendering loop 
	QTimer* fpsTimer = nullptr; // The timer to track the FPS value
	int frameIdxAtLastFPSCalc = 0;

	QElapsedTimer frameTimer; // tracks time between frames
	float deltaTime = 0.f;
};

