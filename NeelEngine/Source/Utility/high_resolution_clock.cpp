#include "neel_engine_pch.h"

#include "high_resolution_clock.h"

HighResolutionClock::HighResolutionClock()
	: delta_time_(0)
	  , total_time_(0)
{
	t0_ = std::chrono::high_resolution_clock::now();
}

void HighResolutionClock::Tick()
{
	auto t1 = std::chrono::high_resolution_clock::now();
	delta_time_ = t1 - t0_;
	total_time_ += delta_time_;
	t0_ = t1;
}

void HighResolutionClock::Reset()
{
	t0_ = std::chrono::high_resolution_clock::now();
	delta_time_ = std::chrono::high_resolution_clock::duration();
	total_time_ = std::chrono::high_resolution_clock::duration();
}

double HighResolutionClock::GetDeltaNanoseconds() const
{
	return delta_time_.count() * 1.0;
}

double HighResolutionClock::GetDeltaMicroseconds() const
{
	return delta_time_.count() * 1e-3;
}

double HighResolutionClock::GetDeltaMilliseconds() const
{
	return delta_time_.count() * 1e-6;
}

double HighResolutionClock::GetDeltaSeconds() const
{
	return delta_time_.count() * 1e-9;
}

double HighResolutionClock::GetTotalNanoseconds() const
{
	return total_time_.count() * 1.0;
}

double HighResolutionClock::GetTotalMicroseconds() const
{
	return total_time_.count() * 1e-3;
}

double HighResolutionClock::GetTotalMilliSeconds() const
{
	return total_time_.count() * 1e-6;
}

double HighResolutionClock::GetTotalSeconds() const
{
	return total_time_.count() * 1e-9;
}
