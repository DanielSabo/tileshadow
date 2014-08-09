#ifndef CANVASEVENTTHREAD_H
#define CANVASEVENTTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QList>
#include <functional>
#include "tileset.h"

class CanvasContext;
class CanvasEventThread : public QThread
{
    Q_OBJECT
public:
    explicit CanvasEventThread(QObject *parent = 0);

    QMutex queueMutex;
    QWaitCondition queueNotEmpty;
    QWaitCondition queueFinished;
    QList< std::function<void(CanvasContext *)> > inQueue;
    bool threadIsSynced;
    int workPending;

    QMutex resultTilesMutex;
    TileMap resultTiles;

    CanvasContext *ctx;

    void enqueueCommand(std::function<void(CanvasContext *)> msg);
    bool checkSync();
    void sync();

signals:
    void hasResultTiles();

protected:
    void run();
};

#endif // CANVASEVENTTHREAD_H
