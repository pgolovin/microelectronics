#include "include/pulse_engine.h"
#include <stdlib.h>

// private members part

typedef struct PulseInternal_type
{
	pulse_callback* on;
	pulse_callback* off;
	void* parameter;

	uint32_t period;
	uint32_t power;

	uint32_t signal_tick;
	uint32_t tick;

} PulseInternal;

HPULSE PULSE_Configure(pulse_callback* on_callback, pulse_callback* off_callback, void* parameter)
{
	if (!on_callback || !off_callback)
	{
		return 0;
	}

	PulseInternal* pulse = DeviceAlloc(sizeof(PulseInternal));

	pulse->on = on_callback;
	pulse->off = off_callback;
	pulse->parameter = parameter;
	pulse->period = 1;
	pulse->power = 0;

	return (HPULSE)pulse;
}

void PULSE_Release(HPULSE pulse)
{
	DeviceFree(pulse);
}

void PULSE_SetPeriod(HPULSE pulse, uint32_t period)
{
	PulseInternal* internal_pulse = (PulseInternal*)pulse;
	internal_pulse->period = period > 0 ? period : 1;
	// Reset internal signal counters
	internal_pulse->signal_tick = 0;
	internal_pulse->tick = 0;
}

void PULSE_SetPower(HPULSE pulse, uint32_t power)
{
	PulseInternal* internal_pulse = (PulseInternal*)pulse;
	internal_pulse->power = power;
	// Reset internal signal counters
	internal_pulse->signal_tick = 0;
	internal_pulse->tick = 0;
}

void PULSE_HandleTick(HPULSE pulse)
{
	PulseInternal* internal_pulse = (PulseInternal*)pulse;

	// Calculate signal state
	//
	// n > 0
	// on = (n + 1) * P / T, signal if on == prev + 1
	//
	// this gave us constant density across whole signal

	++internal_pulse->tick;

	uint32_t signal_tick = (internal_pulse->tick * internal_pulse->power) / internal_pulse->period;
	if (signal_tick > internal_pulse->signal_tick)
	{
		++internal_pulse->signal_tick;
		internal_pulse->on(internal_pulse->parameter);
	}
	else
	{
		internal_pulse->off(internal_pulse->parameter);
	}

	// Zeroing valuese is not necessary here
	// doing it to avoid any potential overflows.
	if (internal_pulse->tick == internal_pulse->period)
	{
		internal_pulse->tick = 0;
		internal_pulse->signal_tick = 0;
	}
}

