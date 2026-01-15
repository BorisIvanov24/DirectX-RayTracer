#include "DXRTMainWindow.h"
#include "DXRTApp.h"
#include <QMenuBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDockWidget>
#include <QFormLayout>
#include <QSpinBox>

DXRTMainWindow::DXRTMainWindow(DXRTApp* app, QWidget* parent)
    : QMainWindow(parent), app(app)
{
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // --- Viewport ---
    viewport = new DXRTViewportWidget(app, central);
    viewport->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(viewport, 1);

    // --- Status bar container ---
    statusBar = new QWidget(central);
    statusBar->setFixedHeight(24);
    QHBoxLayout* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(6, 0, 6, 0);
    statusLayout->setSpacing(10);
    statusFPS = new QLabel("FPS: 0", statusBar);
    statusLayout->addWidget(statusFPS);
    statusLayout->addStretch();
    mainLayout->addWidget(statusBar, 0);

    createMenusAndToolbars();
    createCameraControlsDock(); // <-- create sliders dock
}

void DXRTMainWindow::createCameraControlsDock()
{
    QDockWidget* dock = new QDockWidget("Camera Controls", this);
    QWidget* dockWidget = new QWidget(dock);

    // Use a QFormLayout to align labels and sliders neatly
    QFormLayout* layout = new QFormLayout(dockWidget);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);

    auto addSliderWithMinMax = [&](const QString& labelText, QSlider*& slider, int min, int max, int value)
        {
            slider = new QSlider(Qt::Horizontal);
            slider->setRange(min, max);
            slider->setValue(value);

            // Create a horizontal container with min label, slider, max label
            QWidget* sliderContainer = new QWidget();
            QHBoxLayout* hLayout = new QHBoxLayout(sliderContainer);
            hLayout->setContentsMargins(0, 0, 0, 0);
            hLayout->setSpacing(4); // spacing between min, slider, max

            QLabel* minLabel = new QLabel(QString::number(min));
            QLabel* maxLabel = new QLabel(QString::number(max));

            hLayout->addWidget(minLabel);
            hLayout->addWidget(slider, 1); // stretch slider
            hLayout->addWidget(maxLabel);

            layout->addRow(labelText, sliderContainer);
        };

    // --- Move Speed Slider ---
    addSliderWithMinMax("Move Speed:", moveSpeedSlider, 1, 100, static_cast<int>(app->getCameraMoveSpeed()));
    connect(moveSpeedSlider, &QSlider::valueChanged, this, [this](int val) {
        app->setCameraMoveSpeed(static_cast<float>(val));
        });

    // --- Mouse Sensitivity Slider ---
    addSliderWithMinMax("Mouse Sensitivity:", mouseSensSlider, 1, 100,
        static_cast<int>(app->getCameraMouseSensitivity() * 100.0f));
    connect(mouseSensSlider, &QSlider::valueChanged, this, [this](int val) {
        app->setCameraMouseSensitivity(static_cast<float>(val) / 100.0f);
        });

    // --- Mouse Scroll Speed Slider ---
    addSliderWithMinMax("Scroll Speed:", mouseScrollSpeedSlider, 1, 100,
        static_cast<int>(app->getMouseScrollSpeed() * 100.0f));
    connect(mouseScrollSpeedSlider, &QSlider::valueChanged, this, [this](int val) {
        app->setMouseScrollSpeed(static_cast<float>(val) / 100.0f);
        });

    dockWidget->setLayout(layout);
    dock->setWidget(dockWidget);
    dock->setMinimumWidth(200);  // make dock a reasonable width
    addDockWidget(Qt::RightDockWidgetArea, dock);

    // --- Shading Mode Selector ---
    shadingModeComboBox = new QComboBox();
    shadingModeComboBox->addItem("Triangle Random Colors", 0);
    shadingModeComboBox->addItem("Object Spatial Shading", 1);
    shadingModeComboBox->addItem("Object Triangle Shades", 2);
    shadingModeComboBox->addItem("Barycentric Heatmap", 3);
    shadingModeComboBox->addItem("World-Space Height Gradient", 4);
    shadingModeComboBox->addItem("Distance to Camera Debug", 5);
    shadingModeComboBox->addItem("Checker Pattern", 6);

    shadingModeComboBox->setMaxVisibleItems(7);
    shadingModeComboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    shadingModeComboBox->setStyleSheet("QComboBox { combobox-popup: 0; }");

    layout->addRow("Shading Mode:", shadingModeComboBox);

    connect(shadingModeComboBox,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        [this](int index)
        {
            int mode = shadingModeComboBox->itemData(index).toInt();
            app->setShadingMode(mode);
        });
}




HWND DXRTMainWindow::getNativeWindowHandle()
{
    return viewport->getNativeWindowHandle();
}

DXRTViewportWidget* DXRTMainWindow::getViewport()
{
    return viewport;
}

void DXRTMainWindow::setFPS(const int fps)
{
    statusFPS->setText(QString("FPS: %1").arg(fps));
}

void DXRTMainWindow::updateViewport(const QImage& image)
{
    viewport->updateImage(image);
}

void DXRTMainWindow::closeEvent(QCloseEvent* event)
{
    app->onQuit();
    QMainWindow::closeEvent(event);
}

void DXRTMainWindow::createMenusAndToolbars()
{
    QMenu* fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction("Open");
    fileMenu->addAction("Save");
    fileMenu->addSeparator();
    fileMenu->addAction("Quit", this, SLOT(close()));

    QMenu* viewMenu = menuBar()->addMenu("View");
    viewMenu->addAction("Reset View");

    QToolBar* toolbar = addToolBar("Main Toolbar");
    toolbar->addAction("Render");
    toolbar->addAction("Settings");
}
