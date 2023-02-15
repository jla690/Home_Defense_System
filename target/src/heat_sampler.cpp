#include "heat_sampler.h"

#include <iostream>
#include <exception>
#include <string>

#include "utils.h"

HeatSampler::HeatSampler(ShutdownManager* shutdownManager, int sampleRateHz, int windowSize, bool printUpdates)
{
    if (sampleRateHz <= 0) {
        throw std::invalid_argument(
            "sampleRateHz = " + std::to_string(sampleRateHz) + ", but sample rate must be greater than 0."
        );
    }
    if (windowSize <= 0) {
        throw std::invalid_argument(
            "windowSize = " + std::to_string(sampleRateHz) + ", but window size must be greater than 0."
        );
    }
    if (shutdownManager == nullptr) {
        std::invalid_argument("shutdownManager = nullptr");
    }

    this->shutdownManager = shutdownManager;
    this->sampleRateHz = sampleRateHz;
    this->windowSize = windowSize;
    this->printUpdates = printUpdates;

    samplePeriodMs = 1000.0 / sampleRateHz;
    sum = 0;
    thread = std::thread([this] {run();});
}

void HeatSampler::run()
{
    // Fill up the window.
    for (int i = 0; !shutdownManager->isShutdownRequested() && i < windowSize; i++) {
        double sample = thermometer.read();

        lock.lock();
        {
            sum += sample;
            samples.push(sample);
            if (printUpdates) {
                _printUpdate(sample);
            }
        }
        lock.unlock();

        sleepForMs(samplePeriodMs);
    }

    // Discard old samples and add new ones to the window.
    while (!shutdownManager->isShutdownRequested()) {
        double newSample = thermometer.read();

        lock.lock();
        {
            // Discarded oldest sample.
            double oldestSample = samples.front();
            samples.pop();

            samples.push(newSample);
            sum = sum + newSample - oldestSample;
            if (printUpdates) {
                _printUpdate(newSample);
            }
        }
        lock.unlock();

        sleepForMs(samplePeriodMs);
    }
}

void HeatSampler::waitForShutdown()
{
    thread.join();
}

double HeatSampler::getMeanTemperature()
{
    double meanTemperature = 0;

    lock.lock();
    {
            meanTemperature = _getMeanTemperature();
    }
    lock.unlock();

    return meanTemperature;
}

std::queue<double> HeatSampler::getSamples()
{
    std::queue<double> windowSamples;

    lock.lock();
    {
        windowSamples = samples;
    }
    lock.unlock();

    return windowSamples;
}

double HeatSampler::_getMeanTemperature()
{
    return sum / samples.size();
}

void HeatSampler::_printUpdate(double newSample)
{
    double avgTemp = _getMeanTemperature();
    int currentWindowSize = samples.size();
    double windowPeriodMs = samplePeriodMs * currentWindowSize;
    std::cout << "**** HeatSampler ****\n"
                << "Current temperature = " << newSample << " degrees Celsius\n"
                << "Average temperature in last " << windowPeriodMs << " ms = " << avgTemp << " degrees Celsius\n"
                << "\n";
}