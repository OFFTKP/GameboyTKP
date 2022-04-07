#pragma once
#ifndef TKP_GB_TIMER_H
#define TKP_GB_TIMER_H
#include "gb_bus.h"
#include "gb_apu_ch.h"
#include "gb_addresses.h"

namespace TKPEmu::Gameboy::Devices {
    class Timer {
    public:
        Timer(ChannelArrayPtr channel_array_ptr, Bus& bus);
        void Reset();
        bool Update(uint8_t cycles, uint8_t old_if);
    private:
        ChannelArrayPtr channel_array_ptr_;
        Bus& bus_;
        RegisterType &DIV, &TIMA, &TAC, &TMA, &IF;
        int oscillator_, timer_counter_;
        bool tima_overflow_, just_overflown_;
        const std::array<const unsigned, 4> interr_times_ { 1024, 16, 64, 256 };
    };
}
#endif