#include "DXRTViewportWidget.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include "DXRTApp.h"
#include <iostream>

DXRTViewportWidget::DXRTViewportWidget(DXRTApp* app, QWidget* parent)
	: QWidget(parent), app(app)
{
	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);
}

void DXRTViewportWidget::keyPressEvent(QKeyEvent* event)
{
	keysPressed.insert(event->key());

	if (event->key() == Qt::Key_Escape && mouseCaptured)
	{
		mouseCaptured = false;

		releaseMouse();
		releaseKeyboard();
		unsetCursor();
	}
}

void DXRTViewportWidget::keyReleaseEvent(QKeyEvent* event)
{
	keysPressed.remove(event->key());
}

void DXRTViewportWidget::mousePressEvent(QMouseEvent* event)
{
	if (!mouseCaptured)
	{
		mouseCaptured = true;

		setCursor(Qt::BlankCursor);
		grabMouse();
		grabKeyboard();

		QPoint center(width() / 2, height() / 2);
		ignoreNextMouseMove = true;
		QCursor::setPos(mapToGlobal(center));
	}
}


void DXRTViewportWidget::mouseMoveEvent(QMouseEvent* event)
{
	if (!mouseCaptured)
		return;

	QPoint center(width() / 2, height() / 2);
	QPoint delta = event->pos() - center;

	const float sensitivity = 0.1f;

	float yawDegrees = delta.x() * sensitivity;
	float pitchDegrees = -delta.y() * sensitivity;

	if (delta.x() != 0 || delta.y() != 0)
	{
		if (ignoreNextMouseMove)
		{
			ignoreNextMouseMove = false;
			return;
		}
		app->rotateCamera(-yawDegrees, -pitchDegrees);
		ignoreNextMouseMove = true;
		QCursor::setPos(mapToGlobal(center));
	}
}

void DXRTViewportWidget::wheelEvent(QWheelEvent* event)
{
	const float zoomSpeed = 0.8f; // adjust as needed
	float delta = event->angleDelta().y() / 120.0f; // one notch = 120
	app->zoomCamera(-delta * zoomSpeed);
}

void DXRTViewportWidget::updateImage(const QImage& image)
{
	this->image = image;

	// Resize the viewport to exactly match the texture
	resize(image.width(), image.height());

	update();
}

HWND DXRTViewportWidget::getNativeWindowHandle()
{
	return reinterpret_cast<HWND>(winId());
}

const QSet<int>& DXRTViewportWidget::getPressedKeys() const
{
	return keysPressed;
}

void DXRTViewportWidget::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	painter.drawImage(rect(), image);
}
