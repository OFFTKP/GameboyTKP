#pragma once
#ifndef TKP_GB_BUS_H
#define TKP_GB_BUS_H
#include <string>
#include <cstdint>
#include <array>
#include <vector>
#include <fstream>
#include <iterator>
#include <memory>
#include <deque>
#include <optional>
#include "gb_cartridge.h"
#include "gb_addresses.h"
#include "gb_apu.h"
#include "gb_apu_ch.h"

namespace TKPEmu::Gameboy::Devices {
    struct Change {
		int type = 0;
		std::optional<std::array<uint16_t, 4>> new_bg_pal = std::nullopt;
        int new_bg_pal_index = 0;
        std::optional<bool> new_bg_en = std::nullopt;
	};
    using PaletteColors = std::array<uint16_t, 4>;
    class PPU;
    class Bus {
    private:
        using RamBank = std::array<uint8_t, 0x2000>;
        using RomBank = std::array<uint8_t, 0x4000>;
        using CartridgeType = TKPEmu::Gameboy::Devices::CartridgeType;
    public:
        bool BiosEnabled = true;
        uint8_t logo[0x30] = {
            // Every 2 bytes is an 8x8 tile. If you convert the hex to binary, each bit is a 2x2 pixel
            // And within the byte each 4 bits is a new line
            0x22, 0x72, 0x22, 0xA3, 0x55, 0x99, 0xE1, 0x11, 0x74, 0x47, 0xC0, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x08, 0x05, 0x55, 0x00, 0x12, 0x00, 0x4A,
            0x22, 0x23, 0x32, 0x2A, 0x19, 0x55, 0xE0, 0x00, 0x44, 0x47, 0x01, 0x1D, 0xA5, 0x11, 0x44, 0x43, 0x88, 0x84, 0x08, 0x70, 0x29, 0x00, 0x24, 0x80,
        };
        uint8_t bios[0x100] = {
            0x31, 0xFE, 0xFF, 0xAF, 0x21, 0xFF, 0x9F, 0x32, 0xCB, 0x7C, 0x20, 0xFB, 0x21, 0x26, 0xFF, 0x0E,
            0x11, 0x3E, 0x80, 0x32, 0xE2, 0x0C, 0x3E, 0xF3, 0xE2, 0x32, 0x3E, 0x77, 0x77, 0x3E, 0xFC, 0xE0,
            0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1A, 0xCD, 0x95, 0x00, 0xCD, 0x96, 0x00, 0x13, 0x7B,
            0xFE, 0x34, 0x20, 0xF3, 0x11, 0xD8, 0x00, 0x06, 0x08, 0x1A, 0x13, 0x22, 0x23, 0x05, 0x20, 0xF9,
            0x3E, 0x19, 0xEA, 0x10, 0x99, 0x21, 0x2F, 0x99, 0x0E, 0x0C, 0x3D, 0x28, 0x08, 0x32, 0x0D, 0x20,
            0xF9, 0x2E, 0x0F, 0x18, 0xF3, 0x67, 0x3E, 0x64, 0x57, 0xE0, 0x42, 0x3E, 0x91, 0xE0, 0x40, 0x04,
            0x1E, 0x02, 0x0E, 0x0C, 0xF0, 0x44, 0xFE, 0x90, 0x20, 0xFA, 0x0D, 0x20, 0xF7, 0x1D, 0x20, 0xF2,
            0x0E, 0x13, 0x24, 0x7C, 0x1E, 0x83, 0xFE, 0x62, 0x28, 0x06, 0x1E, 0xC1, 0xFE, 0x64, 0x20, 0x06,
            0x7B, 0xE2, 0x0C, 0x3E, 0x87, 0xF2, 0xF0, 0x42, 0x90, 0xE0, 0x42, 0x15, 0x20, 0xD2, 0x05, 0x20,
            0x4F, 0x16, 0x20, 0x18, 0xCB, 0x4F, 0x06, 0x04, 0xC5, 0xCB, 0x11, 0x17, 0xC1, 0xCB, 0x11, 0x17,
            0x05, 0x20, 0xF5, 0x22, 0x23, 0x22, 0x23, 0xC9,
            // After the logo is scrolled down in the gameboy, these bytes are checked for validity.
            // We set these to 0 and read from the logo array above instead, so we can customize our logo.
            // Range: [0xA8,0xD8)
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            // These bytes are normally the little copyright symbol
            0x7E, 0x81, 0xA5, 0x81, 0xA5, 0xBD, 0x81, 0x7E,
            // Back to normal instructions
            0x21, 0x04, 0x01, 0x11, 0xA8, 0x00, 0x1A, 0x13, 0xBE, 0x20, 0xFE, 0x23, 0x7D, 0xFE, 0x34, 0x20,
            0xF5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xFB, 0x86, 0x20, 0xFE, 0x3E, 0x01, 0xE0, 0x50,
        };
        Bus(ChannelArrayPtr channel_array_ptr, std::vector<DisInstr>& instrs);
        ~Bus();
        uint8_t Read(uint16_t address);
        uint16_t ReadL(uint16_t address);
        uint8_t& GetReference(uint16_t address);
        void ClearNR52Bit(uint8_t bit);
        void Write(uint16_t address, uint8_t data);
        void WriteL(uint16_t address, uint16_t data);
        void TransferDMA(uint8_t clk);
        void Reset();
        void SoftReset();
        std::vector<RamBank>& GetRamBanks();
        Cartridge& GetCartridge();
        bool LoadCartridge(std::string filename);
        std::array<std::array<float, 3>, 4> Palette;
        std::unordered_map<uint8_t, Change> ScanlineChanges;
        std::array<PaletteColors, 8> BGPalettes{};
        std::array<PaletteColors, 8> OBJPalettes{};
        bool SoundEnabled = false;
        bool DIVReset = false;
        bool TMAChanged = false;
        bool TIMAChanged = false;
        bool WriteToVram = false;
        bool OAMAccessible = true;
        bool UseCGB = false;
        uint8_t DirectionKeys = 0b1110'1111;
        uint8_t ActionKeys = 0b1101'1111;
        uint8_t CurScanlineX = 0;
        uint8_t selected_ram_bank_ = 0;
        uint8_t selected_rom_bank_ = 1;
        uint8_t selected_rom_bank_high_ = 0;
    private:
        bool ram_enabled_ = false;
        bool rtc_enabled_ = false;
        bool banking_mode_ = false;
        bool action_key_mode_ = false;
        bool dma_transfer_ = false;
        bool dma_setup_ = false;
        bool dma_fresh_bug_ = false;
        uint16_t hdma_source_ = 0;
        uint16_t hdma_dest_ = 0;
        uint16_t hdma_size_ = 0;
        bool use_gdma_ = false;
        bool bg_palette_auto_increment_ = false;
        uint8_t bg_palette_index_ = 0;
        bool obj_palette_auto_increment_ = false;
        uint8_t obj_palette_index_ = 0;
        bool vram_sel_bank_ = 0;
        uint8_t wram_sel_bank_ = 1;
        uint8_t rom_banks_size_ = 2;
        std::string curr_save_file_;
        size_t dma_index_ = 0;
        uint16_t dma_offset_ = 0;
        uint16_t dma_new_offset_ = 0;
        uint8_t unused_mem_area_ = 0;
        std::vector<RamBank> ram_banks_;
        std::vector<RomBank> rom_banks_;
        Cartridge cartridge_;
        std::array<uint8_t, 0x100> hram_{};
        std::array<uint8_t, 0x2000> eram_default_{};
        std::array<std::array<uint8_t, 0x1000>, 8> wram_banks_{};
        std::array<std::array<uint8_t, 0x2000>, 2> vram_banks_{};
        std::array<uint8_t, 0xA0> oam_{};
        std::array<uint8_t, 0x40> bg_cram_{};
        std::array<uint8_t, 0x40> obj_cram_{};
        std::array<uint8_t*, 0x10000> fast_map_{};
        ChannelArrayPtr channel_array_ptr_;
        std::vector<DisInstr>& instructions_;
        uint8_t& redirect_address(uint16_t address);
        uint8_t& fast_redirect_address(uint16_t address);
        void fill_fast_map();
        void handle_mbc(uint16_t address, uint8_t data);
        void battery_save();
	    // Take channel input with 1-based index to match the register names (eg. NR14)
        void handle_nrx4(int channel_no, uint8_t& data);
        void handle_nrx2(int channel_no, uint8_t& data);
        void handle_nrx1(int channel_no, uint8_t& data);
        void disable_dac(int channel_no);

        friend class PPU;
    };
}
#endif
