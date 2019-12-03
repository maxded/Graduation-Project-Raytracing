#pragma once

#include <chrono>

class HighResolutionClock
{
public:
	HighResolutionClock();

	// Tick the high resolution clock.
	// Tick the clock before reading the delta time for the first time.
	// Only tick the clock once per frame.
	// Use the Get* functions to return the elapsed time between ticks.
	void Tick();

	// Reset the clock.
	void Reset();

	double GetDeltaNanoseconds() const;
	double GetDeltaMicroseconds() const;
	double GetDeltaMilliseconds() const;
	double GetDeltaSeconds() const;

	double GetTotalNanoseconds() const;
	double GetTotalMicroseconds() const;
	double GetTotalMilliSeconds() const;
	double GetTotalSeconds() const;

private:
	// Initial time point.
	std::chrono::high_resolution_clock::time_point t0_;
	// Time since last tick.
	std::chrono::high_resolution_clock::duration delta_time_;
	std::chrono::high_resolution_clock::duration total_time_;
};
