#pragma once
#ifndef TKP_GB_GAMEBOY_H
#define TKP_GB_GAMEBOY_H
#include <array>
#include <include/emulator.h>
#include <GameboyTKP/gb_breakpoint.h>
#include <GameboyTKP/gb_addresses.h>
#include <GameboyTKP/gb_cpu.h>
#include <GameboyTKP/gb_ppu.h>
#include <GameboyTKP/gb_bus.h>
#include <GameboyTKP/gb_timer.h>
#include <GameboyTKP/gb_apu.h>
#include <GameboyTKP/gb_apu_ch.h>

namespace TKPEmu {
	namespace Applications {
		class GameboyRomData;
	}
	namespace Gameboy::QA {
		struct TestMooneye;
	}
}
namespace TKPEmu::Gameboy {
	class Gameboy : public Emulator {
		TKP_EMULATOR(Gameboy);
	private:
		using GameboyPalettes = std::array<std::array<float, 3>,4>;
		using GameboyKeys = std::array<SDL_Keycode, 4>;
		using CPU = TKPEmu::Gameboy::Devices::CPU;
		using PPU = TKPEmu::Gameboy::Devices::PPU;
		using APU = TKPEmu::Gameboy::Devices::APU;
		using ChannelArrayPtr = TKPEmu::Gameboy::Devices::ChannelArrayPtr;
		using ChannelArray = TKPEmu::Gameboy::Devices::ChannelArray;
		using Bus = TKPEmu::Gameboy::Devices::Bus;
		using Timer = TKPEmu::Gameboy::Devices::Timer;
		using Cartridge = TKPEmu::Gameboy::Devices::Cartridge;
		using GameboyBreakpoint = TKPEmu::Gameboy::Utils::GameboyBreakpoint;
	public:
		void SetLogTypes(std::unique_ptr<std::vector<LogType>> types_ptr);
		std::string GetScreenshotHash() override;
		std::vector<std::string> Disassemble(std::string instr) override;
		bool* DebugSpriteTint();
		bool* GetDrawSprites();
		bool* GetDrawWindow();
		bool* GetDrawBackground();
		void RemoveBreakpoint(int index);
		void SetKeysLate(GameboyKeys dirkeys, GameboyKeys actionkeys);
		const auto& GetOpcodeDescription(uint8_t opc);
		GameboyPalettes& GetPalette();
		Cartridge& GetCartridge();
		CPU& GetCPU() { return cpu_; }
		PPU& GetPPU() { return ppu_; }
		// Used by automated tests
		void Update() { update(); }
	private:
		ChannelArrayPtr channel_array_ptr_;
		Bus bus_;
		APU apu_;
		PPU ppu_;
		Timer timer_;
		CPU cpu_;
		GameboyKeys direction_keys_;
		GameboyKeys action_keys_;
		uint8_t& joypad_, &interrupt_flag_;
		std::chrono::system_clock::time_point frame_start = std::chrono::system_clock::now();
		int frames = 0;
		int frame_counter = 0;
		std::unique_ptr<std::vector<LogType>> log_types_ptr_;
		void v_log_state() override;
		void save_state(std::ofstream& ofstream) override;
		void load_state(std::ifstream& ifstream) override;
		void start_normal() override;
		void reset_normal() override;
		void update();
		// this is the old update function that was replaced by update_audio_sync
		// keeping it anyway
		__always_inline void update_spinloop();
		__always_inline void update_audio_sync();
		void init_image();
		std::string print() const override;
		friend class TKPEmu::Applications::GameboyRomData;
		friend class TKPEmu::Gameboy::QA::TestMooneye;
	};
}
#endif