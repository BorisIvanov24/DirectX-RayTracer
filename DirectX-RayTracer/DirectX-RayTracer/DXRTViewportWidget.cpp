#include "DXRTViewportWidget.h"

DXRTViewportWidget::DXRTViewportWidget(QWidget* parent)
	: QWidget(parent)
{
	setStyleSheet("background-color: black; border: 1px solid #555;");
}

void DXRTViewportWidget::updateImage(const QImage& image)
{
	this->image = image;

	// Resize the viewport to exactly match the texture
	resize(image.width(), image.height());

	update();
}

void DXRTViewportWidget::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	painter.drawImage(rect(), image);
}
