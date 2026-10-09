
#include "ui_MainWindow.h"
#include "MainWindow.h"
#include "Devices/Eld7ModbusDriver.h"
#include "Modbus/ModbusChannel.h"


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_ui(std::make_unique<Ui::MainWindow>())
{
    m_ui->setupUi(this);

    const uint32_t modbusAddr = 1;

    auto channel = std::make_shared<Modbus::ModbusChannel>();
    connect(channel.get(), &Modbus::ModbusChannel::signalOpened, this, [this, channel]()
    {
        m_ui->chConnLbl->setText(channel->isChannelOpened() ? "+" : "-");
    });
    connect(channel.get(), &Modbus::ModbusChannel::signalClosed, this, [this, channel]()
    {
        m_ui->chConnLbl->setText(channel->isChannelOpened() ? "+" : "-");
    });
    connect(channel.get(), &Modbus::ModbusChannel::signalConnectionStateChanged, this, [this, modbusAddr, channel]()
    {
        m_ui->dev1ConnLbl->setText(channel->isConnected(modbusAddr) ? "+" : "-");
    });

    DeviceControl::Eld7ModbusDriver::Parameters params;
    params.boardIdent = "TEST_ELD_BOARD";
    params.modbusAddr = modbusAddr;
    params.modbusChannel = channel;
    params.servoOnInput = DeviceControl::Eld7ModbusDriver::EnDiscreteInputNum::DI_5;
    params.cModeInput = DeviceControl::Eld7ModbusDriver::EnDiscreteInputNum::DI_3;

    m_drive = std::make_unique<DeviceControl::Eld7ModbusDriver>(params);

    initSpeedPanel();
    initPositioningPanel();
    initTorquePanel();
    connectSignals();

    Modbus::ConnectionSettings connSett;
    connSett.connectionString = "tcp://192.168.1.1:1241";
    channel->setConnectionSettings(connSett);
    channel->start();
}

MainWindow::~MainWindow()
{
}

void MainWindow::initSpeedPanel()
{
    connect(m_ui->speedMinusBtn, &QPushButton::pressed, this, [this]()
        {
            const int16_t speed = m_ui->speedBox->value();
            m_drive->startSpeedRegulation(-speed);
        });

    connect(m_ui->speedMinusBtn, &QPushButton::released, this, [this]()
        {
            m_drive->stopMoving();
        });

    connect(m_ui->speedPlusBtn, &QPushButton::pressed, this, [this]()
        {
            const int16_t speed = m_ui->speedBox->value();
            m_drive->startSpeedRegulation(speed);
        });

    connect(m_ui->speedPlusBtn, &QPushButton::released, this, [this]()
        {
            m_drive->stopMoving();
        });
}

void MainWindow::initPositioningPanel()
{
    const int16_t speed = m_ui->positioningSpeedBox->value();
    const int32_t targetPos = m_ui->targetPosBox->value();
    m_drive->startPositioning(targetPos, speed);
}

void MainWindow::initTorquePanel()
{
    const int32_t targetAmperage = m_ui->amperageBox->value();
    std::optional<int16_t> speed;
    if (m_ui->torqueSpeedLimitCheckBox->isChecked())
        speed = m_ui->torqueSpeedLimitBox->value();

    m_drive->startAmperageRegulation(targetAmperage, speed);
}

void MainWindow::connectSignals()
{
    connect(m_drive.get(), &DeviceControl::Eld7ModbusDriver::signalSpeedChanged, this, [this](int speed)
        {
            m_ui->currSpeedBox->setValue(speed);
        });
    connect(m_drive.get(), &DeviceControl::Eld7ModbusDriver::signalPositionChanged, this, [this](int pos)
        {
            m_ui->currPosBox->setValue(pos);
        });
    connect(m_drive.get(), &DeviceControl::Eld7ModbusDriver::signalAmperageChanged, this, [this](float amperage)
        {
            m_ui->currAmperageBox->setValue(amperage);
        });

    connect(m_drive.get(), &DeviceControl::Eld7ModbusDriver::signalDriveStatusChanged, this, [this](auto status)
        {
            auto toStr = [](const DeviceControl::EnDriveStatus& st)
                {
                    switch (st)
                    {
                        case DeviceControl::EnDriveStatus::enConnectionLost: return "ConnectionLost";
                        case DeviceControl::EnDriveStatus::enInitialization: return "Initialization";
                        case DeviceControl::EnDriveStatus::enInitializationError: return "InitializationError";
                        case DeviceControl::EnDriveStatus::enInitializationCompleted: return "InitializationCompleted";
                        case DeviceControl::EnDriveStatus::enStatusOk: return "StatusOk";
                        case DeviceControl::EnDriveStatus::enStatusError: return "StatusError";
                        default: return "";
                    }
                };

            m_ui->driveStatusLbl->setText(toStr(status));
        });

    connect(m_drive.get(), &DeviceControl::Eld7ModbusDriver::signalRunningStatusChanged, this, [this](bool isRun)
        {
            m_ui->runStatusLbl->setText(isRun ? "+" : "-");
        });

    connect(m_ui->enableBtn, &QPushButton::clicked, this, [this](bool checked)
        {
            bool res = false;
            if (checked)
                res = m_drive->servoOn();
            else
                res = m_drive->servoOff();

            if (!res)
                m_ui->enableBtn->setChecked(!checked);

            m_ui->enableBtn->setText(m_ui->enableBtn->isChecked() ? "Servo OFF" : "Servo ON");
        });
    connect(m_ui->stopBtn, &QPushButton::clicked, this, [this]()
        {
            m_drive->stopMoving();
        });
    connect(m_ui->hardStopBtn, &QPushButton::clicked, this, [this]()
        {
            m_drive->hardStop();
        });
    connect(m_ui->clearErrBtn, &QPushButton::clicked, this, [this]()
        {
            m_drive->resetError();
        });
}
