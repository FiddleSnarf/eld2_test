#pragma once

#include <QMainWindow>

namespace Ui
{
    class MainWindow;
}

namespace DeviceControl
{
    class Eld7ModbusDriver;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    void initSpeedPanel();
    void initPositioningPanel();
    void initTorquePanel();
    void connectSignals();

private:
    std::unique_ptr<Ui::MainWindow> m_ui;
    std::unique_ptr<DeviceControl::Eld7ModbusDriver> m_drive;
};
