#include "deviceselectdialog.h"
#include "canvaswidget-opencl.h"
#include "opencldeviceinfo.h"
#include <QDebug>
#include <QSettings>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

static QPushButton *makeButton(OpenCLDeviceInfo &dev)
{
    QString name = QString("%1: %2").arg(dev.getPlatformName(), dev.getDeviceName()).simplified();
    cl_ulong memSize = dev.getDeviceInfo<cl_ulong>(CL_DEVICE_GLOBAL_MEM_SIZE) / 0x100000;
    name += QString("\n") + QString("%1MB").arg(memSize);

    QPushButton *button = new QPushButton(name);
    button->setAutoDefault(false);
    button->setStyleSheet("text-align: left");

    return button;
}

DeviceSelectDialog::DeviceSelectDialog(QWidget *parent) :
    QDialog(parent)
{
    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSpacing(3);
    setLayout(layout);

//    layout->addWidget(new QLabel("DeviceSelect", this));

    std::list<OpenCLDeviceInfo> devices = enumerateOpenCLDevices();

    auto cpuDev = devices.end();
    auto gpuDev = devices.end();

    for (auto iter = devices.begin(); iter != devices.end(); ++iter)
    {
        if (cpuDev == devices.end() && iter->getType() == CL_DEVICE_TYPE_CPU)
            cpuDev = iter;
        else if (gpuDev == devices.end() && iter->getType() == CL_DEVICE_TYPE_GPU)
            gpuDev = iter;
    }

#ifndef Q_OS_MAC // Disabled due to unresolved GPU freezing in shared mode
    layout->addWidget(new QLabel("Zero-copy (pick this one):"));

    {
        QPushButton *button = new QPushButton("OpenGL Device");
        button->setStyleSheet("text-align: left");
        button->setAutoDefault(false);
        connect(button, &QPushButton::clicked, [this]() {
           QSettings().setValue("OpenCL/Device", QVariant::fromValue<int>(0));
           this->accept();
        });
        layout->addWidget(button);
    }
#endif

    layout->addWidget(new QLabel("Indirect:"));

    if (cpuDev != devices.end())
    {
        QPushButton *button = makeButton(*cpuDev);
        connect(button, &QPushButton::clicked, [this]() {
           QSettings().setValue("OpenCL/Device", QVariant::fromValue<int>(CL_DEVICE_TYPE_CPU));
           this->accept();
        });
        layout->addWidget(button);
    }

    if (gpuDev != devices.end())
    {
        QPushButton *button = makeButton(*gpuDev);
        connect(button, &QPushButton::clicked, [this]() {
           QSettings().setValue("OpenCL/Device", QVariant::fromValue<int>(CL_DEVICE_TYPE_GPU));
           this->accept();
        });
        layout->addWidget(button);
    }
}
