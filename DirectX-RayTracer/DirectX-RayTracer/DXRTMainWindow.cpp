#include "DXRTMainWindow.h"
#include "DXRTApp.h"
#include <QMenuBar>
#include <QToolBar>

DXRTMainWindow::DXRTMainWindow(DXRTApp* app, QWidget* parent) : QMainWindow(parent), app(app)
{
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    // Use a vertical layout instead of a grid layout
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // --- Viewport ---
    viewport = new DXRTViewportWidget(central);
    viewport->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(viewport, /*stretch*/ 1);

    // --- Status bar container ---
    statusBar = new QWidget(central);
    statusBar->setFixedHeight(24); // small bar at bottom

    QHBoxLayout* statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(6, 0, 6, 0);
    statusLayout->setSpacing(10);

    statusFPS = new QLabel("FPS: 0", statusBar);
    statusLayout->addWidget(statusFPS);

    // Push FPS label to the left (optional)
    statusLayout->addStretch();

    mainLayout->addWidget(statusBar, /*stretch*/ 0);

    createMenusAndToolbars();
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
    // ----- Menu bar -----
    QMenu* fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction("Open");
    fileMenu->addAction("Save");
    fileMenu->addSeparator();
    fileMenu->addAction("Quit", this, SLOT(close()));

    QMenu* viewMenu = menuBar()->addMenu("View");
    viewMenu->addAction("Reset View");

    // ----- Toolbar -----
    QToolBar* toolbar = addToolBar("Main Toolbar");
    toolbar->addAction("Render");
    toolbar->addAction("Settings");
}
