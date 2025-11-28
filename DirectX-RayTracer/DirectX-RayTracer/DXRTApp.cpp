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

	return true;
}

void DXRTApp::onQuit()
{
	idleTimer->stop();
	fpsTimer->stop();
	renderer.stopRendering();
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
	renderer.renderFrameWithSwapChain();
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

void DXRTApp::onIdleTick()
{
	renderFrame();
}
