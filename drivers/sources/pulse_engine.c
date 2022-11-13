#include "include/pulse_engine.h"
#include <stdlib.h>

// private members part

typedef struct PulseInternal_type
{
	uint32_t period;
	uint32_t power;
	uint32_t signal_type;

	uint32_t signal_tick;
	uint32_t tick;

} PulseInternal;

HPULSE PULSE_Configure(PULSE_SINGAL signal_type)
{
	PulseInternal* pulse = DeviceAlloc(sizeof(PulseInternal));

	pulse->period = 1;
	pulse->power = 0;
	pulse->signal_type = signal_type;

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

bool PULSE_HandleTick(HPULSE pulse)
{
	PulseInternal* internal_pulse = (PulseInternal*)pulse;

	// Calculate signal state
	//
	// n > 0
	// on = (n + 1) * P / T, signal if on == prev + 1
	//
	// this gave us constant density across whole signal

	++internal_pulse->tick;
	
	// HIGH signal produces 'on' signal as a 1st signal, if power permits
	uint32_t signal_tick = internal_pulse->signal_type * internal_pulse->power +
		(internal_pulse->tick - internal_pulse->signal_type) * ((float)internal_pulse->power / (float)internal_pulse->period);
	
	bool result = signal_tick > internal_pulse->signal_tick;
	if (result)
	{
		internal_pulse->signal_tick = signal_tick;
	}
	
	// Zeroing values is not necessary here
	// doing it to avoid any potential overflows.
	if (internal_pulse->tick == internal_pulse->period)
	{
		internal_pulse->tick = 0;
		internal_pulse->signal_tick = 0;
	}
	return result;
}

uint32_t PULSE_GetPower(HPULSE hpulse)
{
	PulseInternal* pulse = (PulseInternal*)hpulse;
	return pulse->power;
}

