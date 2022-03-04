#include <algorithm>
#include <bitset>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include "gb_bus.h"
#include "gb_addresses.h"
namespace TKPEmu::Gameboy::Devices {
    using RamBank = std::array<uint8_t, 0x2000>;
	Bus::Bus(APU& apu, std::vector<DisInstr>& instrs) : apu_(apu), instructions_(instrs) {
		Channels[2].LengthInit = 256;
	}
	Bus::~Bus() {
		battery_save();
	}
	void Bus::handle_mbc(uint16_t address, uint8_t data) {
		auto type = cartridge_.GetCartridgeType();
		switch (type) {
			case CartridgeType::MBC1:
			case CartridgeType::MBC1_RAM:
			case CartridgeType::MBC1_RAM_BATTERY: {
				if (address <= 0x1FFF) {
					if ((data & 0b1111) == 0b1010) {
						ram_enabled_ = true;
					} else {
						ram_enabled_ = false;
					}
				}
				else if (address <= 0x3FFF) {
					// BANK register 1 (TODO: this doesnt happen on mbc0?)
					selected_rom_bank_ &= 0b1100000;
					selected_rom_bank_ |= data & 0b11111;
					selected_rom_bank_ %= rom_banks_size_;
				}
				else if (address <= 0x5FFF) {
					// BANK register 2
					selected_rom_bank_ &= 0b11111;
					selected_rom_bank_ |= ((data & 0b11) << 5);
					selected_rom_bank_ %= rom_banks_size_;
					selected_ram_bank_ = data & 0b11;
				}
				else {
					// MODE register
					banking_mode_ = data & 0b1;
				}
				break;
			}
			case CartridgeType::MBC2:
			case CartridgeType::MBC2_BATTERY: {
				if (address <= 0x3FFF) {
					// Different behavior when bit 8 of address is set
					bool ram_wr = (address >> 8) & 0b1;
					if (ram_wr) {
						selected_rom_bank_ = data;
					} else {
						if ((data & 0b1111) == 0b1010) {
							ram_enabled_ = true;
						}
						else {
							ram_enabled_ = false;
						}
					}
					
				}
				break;
			}
			case CartridgeType::MBC3:
			case CartridgeType::MBC3_RAM:
			case CartridgeType::MBC3_RAM_BATTERY: 
			case CartridgeType::MBC3_TIMER_RAM_BATTERY: {
				if (address <= 0x1FFF) {
					if ((data & 0b1111) == 0b1010) {
						ram_enabled_ = true;
						// TODO: enable writing to RTC mbc3 registers
					} else {
						ram_enabled_ = false;
					}
				}
				else if (address <= 0x3FFF) {
					selected_rom_bank_ = data & 0b0111'1111;
					if (selected_rom_bank_ == 0) {
						selected_rom_bank_ = 1;
					}
				}
				else if (address <= 0x5FFF) {
					if (data <= 0b11) {
						selected_ram_bank_ = data; 
					} else {
						// TODO: mbc3 rtc
					}
				}
				else {
					// MODE register
					banking_mode_ = data & 0b1;
				}
				break;
			}
			case CartridgeType::MBC5:
			case CartridgeType::MBC5_RAM: 
			case CartridgeType::MBC5_RAM_BATTERY:
			case CartridgeType::MBC5_RUMBLE:
			case CartridgeType::MBC5_RUMBLE_RAM:
			case CartridgeType::MBC5_RUMBLE_RAM_BATTERY: {
				if (address <= 0x1FFF) {
					if ((data & 0b1111) == 0b1010) {
						ram_enabled_ = true;
					} else {
						ram_enabled_ = false;
					}
				} else if (address <= 0x2FFF) {
					selected_rom_bank_ = data;
				} else if (address <= 0x3FFF) {
					selected_rom_bank_high_ = data & 0b1;
				} else if (address <= 0x5FFF) {
					if (data <= 0xF) {
						selected_ram_bank_ = data;
					}
				}
				break;
			}
			default: {
				return;
			}
		}
	}
	uint8_t& Bus::redirect_address(uint16_t address) {
		unused_mem_area_ = 0;
		WriteToVram = false;
		// Return address from ROM banks
		// TODO: create better exceptions
		switch (address & 0xF000) {
			case 0x0000: {
				if (BiosEnabled) {
					static constexpr uint16_t bios_verify_start = 0xA8;
					static constexpr uint16_t bios_verify_end = 0xD7;
					static constexpr uint16_t logo_cartridge_start = 0x104;
					static constexpr uint16_t logo_cartridge_end = 0x133;
					// The logo is hardcoded in the bios normally to check validity of cartridges, so
					// these two ifs allow us to circmvent the validity check and provide our own logo
					if (address >= bios_verify_start && address <= bios_verify_end) {
						return logo[address - bios_verify_start];
					}
					if (address >= logo_cartridge_start && address <= logo_cartridge_end) {
						return logo[address - logo_cartridge_start];
					}
					if (address < 0x100) {
						return bios[address];
					}
				}
				// If gameboy is not in bios mode, or if the address >= 0x100, we fallthrough
				// to the next case
				[[fallthrough]];  // This avoids a compiler warning. Fallthrough is intentional
			}
			case 0x1000:
			case 0x2000:
			case 0x3000:
			case 0x4000:
			case 0x5000:
			case 0x6000:
			case 0x7000: {
				auto ct = cartridge_.GetCartridgeType();
				switch (ct) {
					case CartridgeType::ROM_ONLY: {
						int index = address / 0x4000;
						return (rom_banks_[index])[address % 0x4000];
					}
					case CartridgeType::MBC1:
					case CartridgeType::MBC1_RAM:
					case CartridgeType::MBC1_RAM_BATTERY: {
						if (address <= 0x3FFF) {
							auto sel = (banking_mode_ ? selected_rom_bank_ & 0b1100000 : 0) % cartridge_.GetRomSize();
							return (rom_banks_[sel])[address % 0x4000];
						} else {
							auto sel = selected_rom_bank_ % cartridge_.GetRomSize();
							if ((sel & 0b11111) == 0) {
								// In 4000-7FFF, automatically maps to next addr if addr chosen is 00/20/40/60
								// TODO: fix multicart roms
								sel += 1;
							}
							return (rom_banks_[sel])[address % 0x4000];
						}
						break;
					}
					case CartridgeType::MBC2: 
					case CartridgeType::MBC2_BATTERY: {
						if (address <= 0x3FFF) {
							return (rom_banks_[0])[address % 0x4000];
						} else {
							if ((selected_rom_bank_ & 0b1111) == 0) {
								selected_rom_bank_ |= 0b1;
							}
							auto sel = selected_rom_bank_ % cartridge_.GetRomSize();
							return (rom_banks_[sel])[address % 0x4000];
						}
						break;
					}
					case CartridgeType::MBC3:
					case CartridgeType::MBC3_RAM:
					case CartridgeType::MBC3_RAM_BATTERY:
					case CartridgeType::MBC3_TIMER_RAM_BATTERY: {
						if (address <= 0x3FFF) {
							auto sel = (banking_mode_ ? selected_rom_bank_ & 0b1100000 : 0) % cartridge_.GetRomSize();
							return (rom_banks_[sel])[address % 0x4000];
						} else {
							auto sel = selected_rom_bank_ % cartridge_.GetRomSize();
							return (rom_banks_[sel])[address % 0x4000];
						}
						break;
					}
					case CartridgeType::MBC5:
					case CartridgeType::MBC5_RAM: 
					case CartridgeType::MBC5_RAM_BATTERY:
					case CartridgeType::MBC5_RUMBLE:
					case CartridgeType::MBC5_RUMBLE_RAM:
					case CartridgeType::MBC5_RUMBLE_RAM_BATTERY: {
						if (address <= 0x3FFF) {
							auto sel = (banking_mode_ ? selected_rom_bank_ & 0b1100000 : 0) % cartridge_.GetRomSize();
							return (rom_banks_[sel])[address % 0x4000];
						} else {
							auto sel = selected_rom_bank_ % cartridge_.GetRomSize();
							sel = sel | (selected_rom_bank_high_ << 8);
							return (rom_banks_[sel])[address % 0x4000];
						}
						break;
					}
					default: {
						// Unhandled cartridge type.
						// TODO: stop emulator instead
						return unused_mem_area_;
					}
				}
				break;
			}
			case 0x8000:
			case 0x9000: {
				WriteToVram = true;
				return vram_[address % 0x2000];
			}
			case 0xA000:
			case 0xB000: {
				auto ct = cartridge_.GetCartridgeType();
				switch(ct) {
					case CartridgeType::MBC2:
					case CartridgeType::MBC2_BATTERY: {
						if (ram_enabled_) {
							auto sel = (banking_mode_ ? selected_ram_bank_ : 0) % cartridge_.GetRamSize();
							(ram_banks_[sel])[address % 0x200] |= 0b1111'0000;
							return (ram_banks_[sel])[address % 0x200];
						} else {
							unused_mem_area_ = 0xFF;
							return unused_mem_area_;
						}
						break;
					}
					default: {
						if (ram_enabled_) {
							if (cartridge_.GetRamSize() == 0)
								return eram_default_[address % 0x2000];
							auto sel = (banking_mode_ ? selected_ram_bank_ : 0) % cartridge_.GetRamSize();
							return (ram_banks_[sel])[address % 0x2000];
						} else {
							unused_mem_area_ = 0xFF;
							return unused_mem_area_;
						}
						break;
					}
				}
			}
			case 0xC000:
			case 0xD000: {
				return wram_[address % 0x2000];
			}
			case 0xE000: {
				return redirect_address(address - 0x2000);
			}
			case 0xF000: {
				if (address <= 0xFDFF) {
					return redirect_address(address - 0x2000);
				}
				else if (address <= 0xFE9F) {
					// OAM
					if (dma_transfer_ && dma_index_ == 0 && dma_fresh_bug_) {
						return oam_[address & 0xFF];
					} else if (dma_transfer_ || !OAMAccessible) {
						unused_mem_area_ = 0xFF;
						return unused_mem_area_;
					}
					return oam_[address & 0xFF];
				}
				else if (address <= 0xFEFF) {
					return unused_mem_area_;
				}
				else {
					return hram_[address % 0xFF00];
				}
			}
		}
		return unused_mem_area_;
	}
	uint8_t Bus::Read(uint16_t address) {
		switch(address) {
			case addr_joy: {
				uint8_t res = ~(ActionKeys & DirectionKeys) & 0b00001111;
				if (!res) {
					// No key is currently pressed, return 0xCF
					return 0b1100'1111;
				}
				return action_key_mode_ ? ActionKeys : DirectionKeys;
			}
		}
		uint8_t read = redirect_address(address);
		return read;
	}
	uint16_t Bus::ReadL(uint16_t address) {
		return Read(address) + (Read(address + 1) << 8);
	}
	uint8_t& Bus::GetReference(uint16_t address) {
		return redirect_address(address);
	}
	std::vector<RamBank>& Bus::GetRamBanks() {
		return ram_banks_;
	}
	std::string Bus::GetVramDump() {
		std::stringstream s;
		for (const auto& m : vram_) {
			s << std::hex << std::setfill('0') << std::setw(2) << m;
		}
		for (int i = 0; i < oam_.size(); i += 4) {
			s << std::hex << std::setfill('0') << std::setw(2) << oam_[i + 3];
			s << std::hex << std::setfill('0') << std::setw(2) << oam_[i + 2];
			s << std::hex << std::setfill('0') << std::setw(2) << oam_[i + 1];
			s << std::hex << std::setfill('0') << std::setw(2) << oam_[i];
		}
		return std::move(s.str());
	}
	void Bus::Write(uint16_t address, uint8_t data) {	
		if (address <= 0x7FFF) {
			handle_mbc(address, data);
		}
		else {
			TIMAChanged = false;
			TMAChanged = false;
			if (!SoundEnabled) {
				if (address >= addr_NR10 && address <= addr_NR51) {
					// When sound is disabled, ignore writes
					return;
				} 
			}
			switch (address) {
				case addr_std: {
					// TODO: implement serial
					break;
				}
				case addr_bgp: {
					for (int i = 0; i < 4; i++) {
						BGPalette[i] = (data >> (i * 2)) & 0b11;
					}
					break;
				}
				case addr_ob0: {
					for (int i = 0; i < 4; i++) {
						OBJ0Palette[i] = (data >> (i * 2)) & 0b11;
					}
					break;
				}
				case addr_ob1: {
					for (int i = 0; i < 4; i++) {
						OBJ1Palette[i] = (data >> (i * 2)) & 0b11;
					}
					break;
				}
				case addr_dma: {
					if (!dma_transfer_) {
						dma_fresh_bug_ = true;
					} else {
						dma_fresh_bug_ = false;
					}
					dma_setup_ = true;
					dma_transfer_ = false;
					dma_new_offset_ = data << 8;
					break;
				}
				case addr_lcd: {
					bool enabled = data & LCDCFlag::LCD_ENABLE;
					if (!enabled) {
						OAMAccessible = true;
					}
					break;
				}
				case addr_bcps: {
					if (UseCGB) {
						if (data & 0b1000'0000) {
							palette_auto_increment_ = true;
						} else {
							palette_auto_increment_ = false;
						}
						palette_index_ = data & 0b111111;
					} else {
						data |= 0b1111'1111;
					}
					break;
				}
				case addr_bcpd: {
					if (UseCGB) {
						std::cout << "Write to pal" << std::endl;
						CGBPalette[palette_index_] = data;
						if (palette_auto_increment_) {
							palette_index_++;
						}
					} else {
						data |= 0b1111'1111;
					}
					break;
				}
				case addr_div: {
					DIVReset = true;
					break;
				}
				case addr_tac: {
					TACChanged = true;
					data |= 0b1111'1000;
					break;
				}
				case addr_tim: {
					TIMAChanged = true;
					break;
				}
				case addr_tma: {
					TMAChanged = true;
					break;
				}
				case addr_joy: {
					action_key_mode_ = (data == 0x10);
					return;
				}
				case addr_stc: {
					data |= 0b0111'1110;
					break;
				}
				case addr_ifl: {
					data |= 0b1110'0000;
					break;
				}
				case addr_sta: {
					data |= 0b1000'0000;
					break;
				}
				case addr_NR10: {
					data |= 0b1000'0000;
					Channels[0].SweepPeriod = (data >> 4) & 0b111;
					Channels[0].SweepIncrease = data & 0b1000;
					Channels[0].SweepShift = data & 0b111;
					break;
				}
				case addr_NR11: {
					handle_nrx1(1, data);
					data |= 0b0011'1111; 
					break;
				}
				case addr_NR12: {
					handle_nrx2(1, data);
					break;
				}
				case addr_NR13: {
					Channels[0].WaveFrequency &= 0b0111'0000'0000;
					Channels[0].WaveFrequency |= data;
					data |= 0b1111'1111;
					break;
				}
				case addr_NR14: {
					handle_nrx4(1, data);
					break;
				}
				case addr_NR20: {
					data |= 0b1111'1111;
					break;
				}
				case addr_NR21: {
					handle_nrx1(2, data);
					data |= 0b0011'1111; 
					break;
				}
				case addr_NR22: {
					handle_nrx2(2, data);
					break;
				}
				case addr_NR23: {
					Channels[1].WaveFrequency &= 0b0111'0000'0000;
					Channels[1].WaveFrequency |= data;
					data |= 0b1111'1111;
					break;
				}
				case addr_NR24: {
					handle_nrx4(2, data);
					break;
				}
				case addr_NR30: {
					if (!(data & 0b1000'0000)) {
						disable_dac(2);
					} else {
						Channels[2].DACEnabled = true;
					}
					data |= 0b0111'1111;
					break;
				}
				case addr_NR31: {
					handle_nrx1(3, data);
					data |= 0b1111'1111; 
					break;
				}
				case addr_NR32: {
					data |= 0b1001'1111;
					break;
				}
				case addr_NR33: {
					Channels[2].WaveFrequency &= 0b0111'0000'0000;
					Channels[2].WaveFrequency |= data;
					data |= 0b1111'1111;
					break;
				}
				case addr_NR34: {
					handle_nrx4(3, data);
					break;
				}
				case addr_NR40: {
					data |= 0b1111'1111;
					break;
				}
				case addr_NR41: {
					handle_nrx1(4, data);
					data |= 0b1111'1111; 
					break;
				}
				case addr_NR42: {
					handle_nrx2(4, data);
					break;
				}
				
				case addr_NR44: {
					handle_nrx4(4, data);
					break;
				}
				case addr_NR52: {
					data &= 0b1111'0000;
					bool enabled = data & 0b1000'0000;
					if (!enabled) {
						// When sound is disabled, clear all registers
						for (int i = addr_NR10; i <= addr_NR51; i++) {
							Write(i, 0);
						}
						data = 0;
					}
					SoundEnabled = enabled;
					data |= 0b0111'0000;
					break;
				}
				// Unused HWIO registers
				// Writing to these sets all the bits
				case 0xFF03: case 0xFF08: case 0xFF09: case 0xFF0A: case 0xFF0B:
				case 0xFF0C: case 0xFF0D: case 0xFF0E:
				case 0xFF27: case 0xFF28: case 0xFF29:
				case 0xFF2A: case 0xFF2B: case 0xFF2C: case 0xFF2D: case 0xFF2E: 
				case 0xFF2F: case 0xFF4C: case 0xFF4D:
				case 0xFF4E: case 0xFF4F: case 0xFF50: case 0xFF51: case 0xFF52:
				case 0xFF53: case 0xFF54: case 0xFF55: case 0xFF56: case 0xFF57:
				case 0xFF58: case 0xFF59: case 0xFF5A: case 0xFF5B: case 0xFF5C:
				case 0xFF5D: case 0xFF5E: case 0xFF5F: case 0xFF60: case 0xFF61:
				case 0xFF62: case 0xFF63: case 0xFF64: case 0xFF65: case 0xFF66:
				case 0xFF67: case 0xFF6A: case 0xFF6B:
				case 0xFF6C: case 0xFF6D: case 0xFF6E: case 0xFF6F: case 0xFF70:
				case 0xFF71: case 0xFF72: case 0xFF73: case 0xFF74: case 0xFF75:
				case 0xFF76: case 0xFF77: case 0xFF78: case 0xFF79: case 0xFF7A:
				case 0xFF7B: case 0xFF7C: case 0xFF7D: case 0xFF7E: case 0xFF7F: {
					data |= 0b1111'1111;
					break;
				}
			}
			redirect_address(address) = data;
		}
	}
	void Bus::WriteL(uint16_t address, uint16_t data) {
		Write(address, data & 0xFF);
		Write(address + 1, data >> 8);
	}
	void Bus::ClearNR52Bit(uint8_t bit) {
		redirect_address(addr_NR52) &= ~(1 << bit);
	}
	void Bus::Reset() {
		SoftReset();
		for (auto& rom : rom_banks_) {
			rom.fill(0xFF);
		}	
	}
	void Bus::SoftReset() {
		hram_.fill(0);
		oam_.fill(0);
		vram_.fill(0);
		DirectionKeys = 0b1110'1111;
        ActionKeys = 0b1101'1111;
		selected_rom_bank_ = 1;
		selected_ram_bank_ = 0;
		BiosEnabled = true;
	}
	Cartridge& Bus::GetCartridge() {
		return cartridge_;
	}
	void Bus::LoadCartridge(std::string filename) {
		Reset();
		cartridge_.Load(filename, rom_banks_, ram_banks_);
		rom_banks_size_ = cartridge_.GetRomSize();
		if (cartridge_.UsingBattery()) {
			auto path = static_cast<std::filesystem::path>(filename);
			std::string path_save = path.parent_path();
			path_save += "/";
			path_save += path.stem();
			path_save += ".sav";
			curr_save_file_ = path_save;
			if (std::filesystem::exists(path_save)) {
				std::ifstream is;
				is.open(path_save, std::ios::binary);
				if (is.is_open() && ram_banks_.size() > 0) {
					for (int i = 0; i < ram_banks_.size(); ++i) {
						is.read(reinterpret_cast<char*>(&ram_banks_[i]), sizeof(uint8_t) * 0x2000);
					}
				}
				is.close();
			}
		}
		UseCGB = cartridge_.UseCGB;
	}
	void Bus::TransferDMA(uint8_t clk) {
		if (dma_transfer_) {
			int times = clk / 4;
			for (int i = 0; i < times; ++i) {
				auto index = dma_index_ + i;
				if (index < oam_.size()) {
					uint16_t source = dma_offset_ | index;
					bool old = OAMAccessible;
					OAMAccessible = true;
					oam_[index] = Read(source);	
					OAMAccessible = old;
				} else {
					dma_transfer_ = false;
					dma_fresh_bug_ = false;
					return;
				}
			}
			dma_index_ += times;
		}
		if (dma_setup_) {
			dma_transfer_ = true;
			dma_setup_ = false;
			dma_index_ = 0;
			dma_offset_ = dma_new_offset_;
		}
	}
	void Bus::battery_save() {
		if (cartridge_.UsingBattery()) {
			std::ofstream of(curr_save_file_, std::ios::binary);
			if (cartridge_.GetRamSize() != 0) {
				for (int i = 0; i < cartridge_.GetRamSize(); ++i) {
					of.write(reinterpret_cast<char*>(&ram_banks_[i]), sizeof(uint8_t) * 0x2000);
				}
			} else {
				of.write(reinterpret_cast<char*>(&ram_banks_[0]), sizeof(uint8_t) * 0x2000);
			}
		}
	}
	void Bus::handle_nrx4(int channel_no, uint8_t& data) {
		// Take channel input with 1-based index to match the register names (eg. NR14)
		--channel_no;
		auto& chan = Channels[channel_no];
		bool old = chan.LengthDecOne;
		chan.LengthDecOne = data & 0b0100'0000;
		int div = redirect_address(addr_div);
		if (!old && chan.LengthDecOne) {
			// From 03-trigger blargg test:
			// Enabling in first half of length period should clock length
			// Because length clocks every 2 frame sequencer steps, first half is every time its even
			if ((chan.FrameSequencer % 2 == 0)) {
				chan.ClockLengthCtr();
				std::cout << "Secret clock! " << std::bitset<8>(div) << " " << chan.LengthTimer << " " << old << " " << chan.LengthDecOne << std::endl;
			}
			// If clock makes length zero, should disable chan
			if (chan.LengthTimer == 0) {
				ClearNR52Bit(channel_no);
			}
		}
		if ((data & 0b1000'0000) && !chan.LengthCtrEnabled) {
			chan.LengthCtrEnabled = true;
			if (chan.LengthTimer == 0) {
				chan.LengthTimer = chan.LengthInit;
				int div = redirect_address(addr_div);
				std::cout << "Set to max! " << chan.LengthTimer << std::endl;
				if (data & 0b0100'0000 && (chan.FrameSequencer % 2 == 0)) {
					// Trigger that un-freezes enabled length should clock it
					--chan.LengthTimer;
					std::cout << "Supersecret decrease " << chan.LengthTimer << std::endl;
				}
			}
			//if (chan.DACEnabled) {
				redirect_address(addr_NR52) |= 1 << channel_no;
			//}
		}
		chan.WaveFrequency &= 0b0000'1111'1111;
		chan.WaveFrequency |= (data & 0b111) << 8;
		if (data == 0x80) {
			std::cout << "data 0x80 " << chan.LengthTimer << std::endl;
		}
		if (data == 0xC0) {
			std::cout << "data 0xc0 " << chan.LengthTimer << std::endl;
		}
		data |= 0b1011'1111;
	}
    void Bus::handle_nrx2(int channel_no, uint8_t& data) {
		--channel_no;
		auto& chan = Channels[channel_no];
		chan.EnvelopeCurrentVolume = data >> 4;
		chan.EnvelopeIncrease = data & 0b0000'1000;
		int sweep = data & 0b0000'0111;
		chan.EnvelopePeriod = sweep;
		chan.VolEnvEnabled = sweep != 0;
		if ((data >> 3) == 0) {
			std::cout << "Disable dac " << chan.LengthTimer << std::endl;
			disable_dac(channel_no);
		} else {
			chan.DACEnabled = true;
			std::cout << "Enable dac " << chan.LengthTimer << std::endl;
		}
	}
	void Bus::handle_nrx1(int channel_no, uint8_t& data) {
		--channel_no;
		Channels[channel_no].LengthData = data & (Channels[channel_no].LengthInit - 1); // and with highest value
		Channels[channel_no].LengthTimer = Channels[channel_no].LengthInit - Channels[channel_no].LengthData;
		Channels[channel_no].LengthHalf = Channels[channel_no].LengthTimer / 2;
		//data |= 0b1111'1111;
	}
	void Bus::disable_dac(int channel_no) {
		auto& chan = Channels[channel_no];
		//chan.LengthCtrEnabled = false;
		//chan.LengthTimer = 0;
		ClearNR52Bit(channel_no);
		chan.DACEnabled = false;
		//chan.LengthTimer = 0;
	}
}
