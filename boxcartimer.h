#ifndef BOXCARTIMER_H
#define BOXCARTIMER_H

#include <QElapsedTimer>

class BoxcarTimer
{
public:
    BoxcarTimer(int numCars, int carLength = 100);
    ~BoxcarTimer();

    void addEvents(int count);
    double getRate();

    bool started;
    int currentCar;
    int numCars;
    int carLength;
    int *cars;
    qint64 carStart;

    QElapsedTimer timer;
};

#endif // BOXCARTIMER_H
