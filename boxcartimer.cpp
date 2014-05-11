#include "boxcartimer.h"

BoxcarTimer::BoxcarTimer(int numCars, int carLength) :
    started(false), currentCar(0),
    numCars(numCars), carLength(carLength)
{
    cars = new int[numCars];

    for (int i = 0; i < numCars; ++i)
        cars[i] = 0;
}

BoxcarTimer::~BoxcarTimer()
{
    delete[] cars;
}

void BoxcarTimer::addEvents(int count)
{
    if (!started)
    {
        timer.start();
        started = true;
        carStart = 0;
    }

    qint64 currentTime = timer.elapsed();

    while (carStart + carLength <= currentTime)
    {
        carStart += carLength;
        currentCar += 1;

        if (currentCar >= numCars)
            currentCar = 0;

        cars[currentCar] = 0;
    }

    cars[currentCar] += count;
}

double BoxcarTimer::getRate()
{
    if (!started)
        return 0.0;

    double total = 0;
    qint64 currentTime = timer.elapsed();

    while (carStart + carLength <= currentTime)
    {
        carStart += carLength;
        currentCar += 1;

        if (currentCar >= numCars)
            currentCar = 0;

        cars[currentCar] = 0;
    }

    for (int i = 0; i < numCars; ++i)
    {
        total += cars[i];
    }

    if (currentTime < numCars * carLength)
        total /= currentTime / 1000.0;
    else
    {
        double dt = (numCars - 1) * carLength + currentTime % carLength;
        total /= dt / 1000.0;
    }

    return total;
}
