#include "canvaseventthread.h"
#include "canvastile.h"
#include "canvascontext.h"

using namespace std;

CanvasEventThread::CanvasEventThread(QObject *parent)
    : QThread(parent),
      workPending(0),
      threadIsSynced(true),
      synchronous(false),
      needResultTiles(false),
      needExit(false),
      ctx(nullptr)
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

void CanvasEventThread::stop()
{
    queueMutex.lock();
    needExit = true;
    queueNotEmpty.wakeOne();
    queueMutex.unlock();

    wait();
}

bool CanvasEventThread::getSynchronous()
{
    return synchronous;
}

void CanvasEventThread::setSynchronous(bool synced)
{
    synchronous = synced;

    if (synchronous)
        sync();
}

void CanvasEventThread::enqueueCommand(function<void(CanvasContext *)> msg)
{
    if (synchronous)
    {
        msg(ctx);
        return;
    }

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

        if (inQueue.empty() && !needExit)
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

        if (needExit)
            return;

        while (!messages.empty())
        {
            messages.takeFirst()(ctx);

            if ((needResultTiles || messages.empty()) && !ctx->dirtyTiles.empty())
            {
                TileMap newTiles;
                ctx->renderDirty(&newTiles);

                resultTilesMutex.lock();
                for (auto &iter: newTiles)
                {
                    resultTiles[iter.first] = std::move(iter.second);
                }
                newTiles.clear();
                needResultTiles = false;
                resultTilesMutex.unlock();

                emit hasResultTiles();
            }
        }
    }
}
