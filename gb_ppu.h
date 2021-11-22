#pragma once
#ifndef TKP_GB_PPU_H
#define TKP_GB_PPU_H
#include "gb_bus.h"
#include "../include/TKPImage.h"
#include <mutex>
#include <array>
namespace TKPEmu::Gameboy::Devices {
	constexpr int FRAME_CYCLES = 70224;
	class PPU {
	private:
		using IFInterrupt = Bus::IFInterrupt;
		using LCDCFlag = Bus::LCDCFlag;
		using STATFlag = Bus::STATFlag;
		using TKPImage = TKPEmu::Tools::TKPImage;
		using Pixel = TKPEmu::Gameboy::Devices::Bus::Pixel;
	public:
		PPU(Bus* bus, std::mutex* draw_mutex);
		void Update(uint8_t tTemp);
		void Reset();
		float* GetScreenData();
	private:
		Bus* bus_;
		std::array<float, 4 * 160 * 144> screen_color_data_{};

		// PPU register pointers
		uint8_t& LCDC, &STAT, &LYC, &LY, &IF, &SCY, &SCX, &WY, &WX;

		int clock_ = 0;
		int clock_target_ = 0;
		uint8_t& next_stat_mode;
		std::mutex* draw_mutex_ = nullptr;
		int set_mode(int mode);
		int get_mode();
		int update_lyc();
		void draw_scanline();
		inline void renderTiles();
		inline void renderSprites();
	};
}
#endif
