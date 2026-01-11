#include "DXRTApp.h"

bool DXRTApp::init()
{
	if (false == initWindow())
	{
		return false;
	}

	//!
	renderer.prepareForRendering(mainWnd->getNativeWindowHandle());

	mainWnd->show();

	idleTimer = new QTimer(mainWnd);
	connect(idleTimer, &QTimer::timeout, this, &DXRTApp::onIdleTick);
	idleTimer->start(0);

	fpsTimer = new QTimer(mainWnd);
	connect(fpsTimer, &QTimer::timeout, this, &DXRTApp::updateRenderStats);
	fpsTimer->start(1'000);

	frameTimer.start();

	return true;
}

void DXRTApp::onQuit()
{
	idleTimer->stop();
	fpsTimer->stop();
	renderer.stopRendering();
}

void DXRTApp::rotateCamera(float yawDeg, float pitchDeg)
{
	auto& camera = renderer.getScene().getCamera();

	camera.rotate(yawDeg, pitchDeg);
}

float DXRTApp::getDeltaTime() const
{
	return deltaTime;
}

bool DXRTApp::initWindow()
{
	mainWnd = new DXRTMainWindow(this);
	mainWnd->resize(1280, 720);
	mainWnd->show();
	return true; // success
}

void DXRTApp::renderFrame()
{
	renderer.renderFrame();
	//mainWnd->updateViewport(renderer.getQImageForFrame());
	frameIdxAtLastFPSCalc++;
}

void DXRTApp::updateRenderStats()
{
	static int previousFrame = 0;
	int current = frameIdxAtLastFPSCalc;
	int fps = current - previousFrame;
	previousFrame = current;

	mainWnd->setFPS(fps);
}

void DXRTApp::updateCameraMovement(const QSet<int>& keys, float dt)
{
	auto& cam = renderer.getScene().getCamera();
	float speed = 10.0f;

	if (keys.contains(Qt::Key_W))
		cam.moveForward(-speed * dt);

	if (keys.contains(Qt::Key_S))
		cam.moveForward(speed * dt);

	if (keys.contains(Qt::Key_A))
		cam.moveRight(-speed * dt);

	if (keys.contains(Qt::Key_D))
		cam.moveRight(speed * dt);
}

void DXRTApp::onIdleTick()
{
	// Compute deltaTime in seconds
	deltaTime = frameTimer.elapsed() / 1000.f; // milliseconds -> seconds
	frameTimer.restart();

	const QSet<int>& keys = mainWnd->getViewport()->getPressedKeys();

	updateCameraMovement(keys, deltaTime);

	renderFrame();
}
