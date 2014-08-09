#include "canvaseventthread.h"
#include "canvastile.h"
#include "canvascontext.h"

using namespace std;

CanvasEventThread::CanvasEventThread(QObject *parent)
    : QThread(parent),
      ctx(0),
      workPending(0),
      threadIsSynced(true),
      needResultTiles(false)
{
}

bool CanvasEventThread::checkSync()
{
    if (threadIsSynced)
        return true;

    queueMutex.lock();
    if (workPending == 0)
        threadIsSynced = true;
    queueMutex.unlock();

    return threadIsSynced;
}

void CanvasEventThread::sync()
{
    if (threadIsSynced)
        return;

    queueMutex.lock();
    if (workPending != 0)
    {
        queueFinished.wait(&queueMutex);
    }
    threadIsSynced = true;
    queueMutex.unlock();

    return;
}

void CanvasEventThread::enqueueCommand(function<void(CanvasContext *)> msg)
{
    threadIsSynced = false;

    queueMutex.lock();
    workPending++;
    inQueue.push_back(msg);
    queueNotEmpty.wakeOne();
    queueMutex.unlock();
}

void CanvasEventThread::run()
{
    QList< function<void(CanvasContext *)> > messages;
    int batchSize = 0;

    while (1)
    {
        queueMutex.lock();
        workPending -= batchSize;
        Q_ASSERT(workPending >= 0);

        if (inQueue.empty())
        {
            queueFinished.wakeOne();
            queueNotEmpty.wait(&queueMutex);
        }

        while (!inQueue.empty())
        {
            messages.append(inQueue.takeFirst());
        }

        batchSize = messages.size();
        Q_ASSERT(workPending == batchSize);
        queueMutex.unlock();


        while (!messages.empty())
        {
            messages.takeFirst()(ctx);

            if ((needResultTiles || messages.empty()) && !ctx->dirtyTiles.empty())
            {
                TileMap newTiles;
                for (QPoint const &iter : ctx->dirtyTiles)
                {
                    int x = iter.x();
                    int y = iter.y();

                    newTiles[QPoint(x, y)] = ctx->layers.getTileMaybe(x, y, false);
                }
                ctx->dirtyTiles.clear();

                resultTilesMutex.lock();
                for (auto &iter: newTiles)
                {
                    CanvasTile *& tile = resultTiles[iter.first];
                    if (tile)
                        delete tile;
                    tile = iter.second;
                }
                newTiles.clear();
                needResultTiles = false;
                resultTilesMutex.unlock();

                emit hasResultTiles();
            }
        }
    }
}