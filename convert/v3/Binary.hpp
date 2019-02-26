//
//  Binary.hpp
//  convert
//
//  Created by 戴植锐 on 2019/2/10.
//  Copyright © 2019 Zhirui Dai. All rights reserved.
//

#ifndef Binary_hpp
#define Binary_hpp

#include <cstdio>
#include <cstdint>
#include <cstring>

#include <string>

#include "myDebug.h"
#include "Chip.h"
#include "../../elf_reader/elf.h"

class Binary {
public:
    enum SectionIndex {
        ignore      = -1,
        text        = 0,
        rodata      = 1,
        ehFrame     = 2,
        initArray   = 3,
        data        = 4,
        bss         = 5
    };
    
    const char* SectionName[6] = {".text", ".rodata", ".eh_frame", ".init_array", ".data", ".bss"};
    
    enum: uint32_t {
        DEFAULT_TARGET_ADDRESS      = 0x10000000,
        DEFAULT_DATA_ADDRESS        = 0x01000010,
        DEFAULT_GP                  = 0x01008000,
        DEFAULT_SP                  = 0x01001fd4,
        
        FLASH_ADDRESS_MIN           = 0x10000000,
        FLASH_ADDRESS_MAX           = 0x1001ffff,
        FLASH_VOLUME                = 0x1ffff,
        SRAM_ADDRESS_MIN            = 0x01000000,
        SRAM_ADDRESS_MAX            = 0x01002000,
        
        STACK_PAGE_SIZE_MIN         = 1,
        STACK_PAGE_SIZE_MAX         = 14,
        STACK_PAGE_VOLUME           = 512,
        
        MASTER_CORE_GPSP_SIZE       = 16,
        SLAVE_CORE_GPSP_SIZE        = 12,
        
        SLAVE_FUNC_EXCHANGE_SIZE    = 36,
    };
    
    const int LOADER_SIZE[2] = {7, 9};
    const int ADJUSTED_TARGET_ADDRESS_THRESHOLD[2] = {0x1000000c, 0x10000024};
    const int CORE_NUM[CHIP_NUM_MAX] = {1,2};
    const int JALOS_ADDRESS_OFFSET[CHIP_NUM_MAX][CORE_NUM_MAX] = {{0xa0, 0xa0}, {0xe0, 0x108}};            // relative to entry
    const int JISR_ADDRESS_OFFSET[CHIP_NUM_MAX][CORE_NUM_MAX] = {{0xc, 0xc}, {0xc, 0x1c}};                // relaative to entry
    const int JAL_INTERRUPT_ADDRESS_OFFSET[CHIP_NUM_MAX][CORE_NUM_MAX] = {{0xa8, 0xa8}, {0x28, 0x20}};    // relative to corresponding isr
    
    const int GP_LUI_OFFSET[CHIP_NUM_MAX][CORE_NUM_MAX] = {{0x1c, 0x1c}, {0x24, 0x44}};
    const int GP_ORI_OFFSET[CHIP_NUM_MAX][CORE_NUM_MAX] = {{0x20, 0x20}, {0x28, 0x48}};
    const int SP_LUI_OFFSET[CHIP_NUM_MAX][CORE_NUM_MAX] = {{0x24, 0x24}, {0x2c, 0x50}};
    const int SP_ORI_OFFSET[CHIP_NUM_MAX][CORE_NUM_MAX] = {{0x28, 0x28}, {0x30, 0x54}};
    const int DATA_LUI_OFFSET[CHIP_NUM_MAX][3] = {{0x2c, 0x34, 0x3c}, {0x5c, 0x64, 0x6c}};  // .data_start, .data_end, .data_addr
    const int DATA_ORI_OFFSET[CHIP_NUM_MAX][3] = {{0x30, 0x38, 0x40}, {0x60, 0x68, 0x70}};  // .data_start, .data_end, .data_addr
    const int BSS_LUI_OFFSET[CHIP_NUM_MAX][2] = {{0x4c, 0x54}, {0x7c, 0x84}};               // .bss_start, .bss_end
    const int BSS_ORI_OFFSET[CHIP_NUM_MAX][2] = {{0x50, 0x58}, {0x80, 0x88}};               // .bss_start, .bss_end
    const int INIT_LUI_OFFSSET[CHIP_NUM_MAX][2] = {{0x6c, 0x74}, {0xac, 0xb4}};    // .init_array_start .init_array_size
    const int INIT_ORI_OFFSET[CHIP_NUM_MAX][2] = {{0x70, 0x78}, {0xb0, 0xb8}};
    
    CHIP_T      chip;
    bool        convert;
    bool        rootMode;
    uint32_t    p1StackPageNum;
    
//    uint8_t*    sectionBuf[6];
    uint32_t    sectionSize[6];
    uint32_t    sectionOffset[6];
    uint32_t    sectionFlashAddr[6];

    uint32_t    bssSectionMemoryBeginAddr;
    uint32_t    bssSectionMemoryEndAddr;
    
    uint32_t    globalPtrAddr[CORE_NUM_MAX];
    uint32_t    stackPtrAddr[CORE_NUM_MAX];
    
    uint8_t*    flashBuf;
    uint32_t    flashBufSize;
    uint8_t*    memoryBuf;
    uint32_t    memoryBufSize;
    
    uint32_t    entryAddress;
    uint32_t    targetAddress;
    uint32_t    dataAddress;
    
    uint32_t    mainAddress[CORE_NUM_MAX];
    uint32_t    isrAddress[CORE_NUM_MAX];
    uint32_t    interruptAddress[CORE_NUM_MAX];
    
    Binary(ELF_T elf);
    ~Binary();
    
    void extractSections(std::string baseFileName);
    void updateCode();
    void analyseAddress();
    bool hasDataSection() { return this->sectionSize[data] != 0; }
private:
    void init();
    
    static uint32_t jTarget(uint32_t code, uint32_t address)
    {
        if ((code >> 26) != 0b000010) {
            fprintf(stderr, "instruction code: 0x%x is not [j target] instruction!\n", code);
            exit(EXIT_FAILURE);
        }
        return (address&0xf0000000)|((code&0x03ffffff)<<2);
    }
    
    static uint32_t jalTarget(uint32_t code, uint32_t address)
    {
        if ((code >> 26) != 0b000011) {
            fprintf(stderr, "instruction code: 0x%x is not [jal target] instruction!\n", code);
            exit(EXIT_FAILURE);
        }
        return (address&0xf0000000)|((code&0x03ffffff)<<2);
    }
    
    static void __update_code__(uint8_t *ptr, uint32_t offset, uint32_t value)
    {
        uint32_t opcode;
        opcode = *(uint32_t *)(ptr + offset);
        //    opcode = ntohl(opcode);
        opcode = (opcode & 0xffff0000) | (value & 0xffff);
        //    opcode = ntohl(opcode);
        *(uint32_t *)(ptr + offset) = opcode;
    }
    
    static void update_lui_code(uint8_t *ptr, uint32_t offset, uint32_t value)
    {
        __update_code__(ptr, offset, value >> 16);
    }
    
    static void update_ori_code(uint8_t *ptr, uint32_t offset, uint32_t value)
    {
        __update_code__(ptr, offset, value & 0xffff);
    }
    
    static void updateLuiOriCode(uint8_t* buf, const int* luiOffsets, const int* oriOffsets, uint32_t* values, int count,
                                 const char* msg, const char** valueNames);
};

#endif /* Binary_hpp */
