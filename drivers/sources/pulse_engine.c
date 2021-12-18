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

	uint32_t sub_period;
	uint32_t sub_power;
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
	internal_pulse->period = period < 1 ? 1 : period;
	PULSE_SetPower(pulse, internal_pulse->power);
}

void PULSE_SetPower(HPULSE pulse, uint32_t power)
{
	PulseInternal* internal_pulse = (PulseInternal*)pulse;
	internal_pulse->power = power;
	if (power)
	{
		int divider = power;
		for (; divider > 0; --divider)
		{
			if ((0 == power % divider) && (0 == 100 % divider))
				break;
		}
		internal_pulse->sub_period = internal_pulse->period / divider;
		internal_pulse->sub_power = power / divider;
		// change power means restart loop
		internal_pulse->tick = 0;
	}
	else
	{
		internal_pulse->sub_period = 0;
	}
}

void PULSE_HandleTick(HPULSE pulse)
{
	PulseInternal* internal_pulse = (PulseInternal*)pulse;

	// do not overflaw pwm loop
	if (internal_pulse->tick == internal_pulse->sub_period)
	{
		internal_pulse->tick = 0;
	}
	if (internal_pulse->tick < internal_pulse->sub_power)
	{
		internal_pulse->on(internal_pulse->parameter);
	}
	else
	{
		internal_pulse->off(internal_pulse->parameter);
	}
	++internal_pulse->tick;
}

