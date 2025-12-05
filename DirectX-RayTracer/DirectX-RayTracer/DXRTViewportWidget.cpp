#include "DXRTViewportWidget.h"

DXRTViewportWidget::DXRTViewportWidget(QWidget* parent)
	: QWidget(parent)
{
	resize(800, 800);
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

void DXRTViewportWidget::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	painter.drawImage(rect(), image);
}
