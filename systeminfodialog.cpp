#include "systeminfodialog.h"
#include "ui_systeminfodialog.h"

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

SystemInfoDialog::SystemInfoDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SystemInfoDialog)
{
    ui->setupUi(this);

    outputLabel = findChild<QLabel*>("queryOutput");
}

void SystemInfoDialog::showEvent(QShowEvent *event)
{
    (void)event;

    if (!outputLabel)
        return;

    if (queryResultString.isNull())
    {
        queryResultString.reset(new QString ());
        unsigned int num_platforms;
        clGetPlatformIDs (0, NULL, &num_platforms);

        queryResultString->append(QString("Platforms: %1\n").arg(num_platforms));

        unsigned int platform_idx;

        cl_platform_id *platforms = new cl_platform_id[num_platforms];
        clGetPlatformIDs (num_platforms, platforms, NULL);

        for (platform_idx = 0; platform_idx < num_platforms; ++platform_idx)
        {
            char platform_name[1024];
            clGetPlatformInfo (platforms[platform_idx], CL_PLATFORM_NAME, sizeof(platform_name), platform_name, NULL);

            QString formatted_platform_id;
            formatted_platform_id.sprintf("%p", platforms[platform_idx]);

            QString platform_desc = QString("%2 %3\n").arg(formatted_platform_id).arg(platform_name);

            queryResultString->append(platform_desc);
        }

        delete[] platforms;
    }

    outputLabel->setText(*queryResultString);
}

SystemInfoDialog::~SystemInfoDialog()
{
    delete ui;
}
