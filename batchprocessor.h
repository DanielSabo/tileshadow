#ifndef BATCHPROCESSOR_H
#define BATCHPROCESSOR_H

#include <QObject>

class BatchProcessor : public QObject
{
    Q_OBJECT
public:
    explicit BatchProcessor(QObject *parent = 0);

signals:

public slots:
    void execute(QString path);
};

#endif // BATCHPROCESSOR_H
