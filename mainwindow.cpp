#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <stdio.h>
#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    outputLabel = findChild<QLabel*>("queryOutput");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_queryButton_clicked()
{
//    QLabel *outputLabel = this->findChild<QLabel*>("queryOutput");

    if (!outputLabel)
        return;

//    QString text = outputLabel->text();
//    text.append("Clicked\n");
    QString text;

    unsigned int num_platforms;
    clGetPlatformIDs (0, NULL, &num_platforms);

    text = QString("Platforms: %1\n").arg(num_platforms);

    unsigned int platform_idx;

    cl_platform_id *platforms = new cl_platform_id[num_platforms];
    clGetPlatformIDs (num_platforms, platforms, NULL);

    for (platform_idx = 0; platform_idx < num_platforms; ++platform_idx)
    {
        char platform_name[1024];
        clGetPlatformInfo (platforms[platform_idx], CL_PLATFORM_NAME, sizeof(platform_name), platform_name, NULL);

        QString formatted_platform_id;
        formatted_platform_id.sprintf("%p", platforms[platform_idx]);

        text = QString("%1 %2 %3\n")
                .arg(text)
                .arg(formatted_platform_id)
                .arg(platform_name);
//        append_value.sprintf("%x: %s\n", , platform_name);
//        text.append(append_value);
    }

    outputLabel->setText(text);

    delete[] platforms;
}
