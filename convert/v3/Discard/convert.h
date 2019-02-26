//
//  convert.h
//  elf_reader
//
//  Created by 戴植锐 on 2019/1/29.
//  Copyright © 2019 Zhirui Dai. All rights reserved.
//

#ifndef convert_h
#define convert_h

#include <cstdint>
#include <string>
#include "../elf_reader/elf.h"
#include "Chip.h"

class SectionIndex {
public:
    static const int text        = 0;
    static const int rodata      = 1;
    static const int eh_frame    = 2;
    static const int init_array  = 3;
    static const int data        = 4;
};

typedef struct {
    
    uint8_t**   section_buf[5];
    
    uint8_t*    code_buf;
    uint32_t    code_buf_size;
    uint32_t    code_section_addr;
    
    uint8_t*    rodata_buf;
    uint32_t    rodata_buf_size;
    uint32_t    rodata_section_addr;
    
    uint8_t*    eh_frame_buf;
    uint32_t    eh_frame_buf_size;
    uint32_t    eh_frame_section_addr;
    
    uint8_t*    init_array_buf;
    uint32_t    init_array_buf_size;
    uint32_t    init_array_section_addr;
    
    uint8_t*    data_buf;
    uint32_t    data_buf_size;
    uint32_t    data_memory_addr;
    uint32_t    data_flash_begin_addr;
    uint32_t    data_flash_end_addr;
    
    uint32_t    bss_begin_addr;
    uint32_t    bss_end_addr;
    uint32_t    bss_size;
    
    uint32_t    global_pointer_addr[CORE_NUM_MAX];
    uint32_t    stack_pointer_addr[CORE_NUM_MAX];
} binary_t;

class CamelConvert {

    const char* INSTR_CODE_SECTION_NAME = ".text";
    const char* RODATA_SECTION_NAME     = ".rodata";
    const char* DATA_SECTION_NAME       = ".data";
    const char* BSS_SECTION_NAME        = ".bss";
    const char* EH_FRAME_SECTION_NAME   = ".eh_frame";
    const char* INIT_ARRAY_SECTION_NAME = ".init_array";
    
public:
    
    enum: uint32_t {
        DEFAULT_TARGET_ADDRESS      = 0x10000000,
        DEFAULT_DATA_ADDRESS        = 0x01000010,
        DEFAULT_GP                  = 0x01008000,
        DEFAULT_SP                  = 0x01001fd4,
        
        FLASH_ADDRESS_MIN           = 0x10000000,
        FLASH_ADDRESS_MAX           = 0x1001ffff,
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
    const int JALOS_ADDRESS_OFFSET[CHIP_NUM_MAX][CORE_NUM_MAX] = {{0xac, 0xac}, {0xac, 0xd4}};            // relative to entry
    const int JISR_ADDRESS_OFFSET[CHIP_NUM_MAX][CORE_NUM_MAX] = {{0xc, 0xc}, {0xc, 0x1c}};                // relaative to entry
    const int JAL_INTERRUPT_ADDRESS_OFFSET[CHIP_NUM_MAX][CORE_NUM_MAX] = {{0xa8, 0xa8}, {0x28, 0x20}};    // relative to corresponding isr
    
    const int GP_LUI_OFFSET[CHIP_NUM_MAX][CORE_NUM_MAX] = {{0x24, 0x24}, {0x24, 0x44}};
    const int GP_ORI_OFFSET[CHIP_NUM_MAX][CORE_NUM_MAX] = {{0x28, 0x28}, {0x28, 0x48}};
    const int SP_LUI_OFFSET[CHIP_NUM_MAX][CORE_NUM_MAX] = {{0x2c, 0x2c}, {0x2c, 0x50}};
    const int SP_ORI_OFFSET[CHIP_NUM_MAX][CORE_NUM_MAX] = {{0x30, 0x30}, {0x30, 0x54}};
    const int DATA_LUI_OFFSET[CHIP_NUM_MAX][3] = {{0x34, 0x3c, 0x44}, {0x5c, 0x64, 0x6c}};  // .data_start, .data_end, .data_addr
    const int DATA_ORI_OFFSET[CHIP_NUM_MAX][3] = {{0x38, 0x40, 0x48}, {0x60, 0x68, 0x70}};  // .data_start, .data_end, .data_addr
    const int BSS_LUI_OFFSET[CHIP_NUM_MAX][2] = {{0x54, 0x5c}, {0x7c, 0x84}};               // .bss_start, .bss_end
    const int BSS_ORI_OFFSET[CHIP_NUM_MAX][2] = {{0x58, 0x60}, {0x80, 0x88}};               // .bss_start, .bss_end
    
    ELF_T       elf;
    CHIP_T      chip;
    binary_t    binary;
    uint32_t    entryAddress;
    uint32_t    targetAddress;
    uint32_t    shouldCheckTargetAddress;
    uint32_t    entryAddress_offset;
    uint32_t    dataAddress;
    uint32_t    shouldCheckDataAddress;
    uint32_t    p1_stack_size;
    uint32_t    main_address[CORE_NUM_MAX];
    uint32_t    isr_address[CORE_NUM_MAX];
    uint32_t    interrupt_address[CORE_NUM_MAX];
    bool        extractSections;
    bool        rootMode;
    std::string outputFileName;
    
    CamelConvert(ELF_T elf, CHIP_T chip,
                 uint32_t targetAddress, bool shouldCheckTargetAddress,
                 uint32_t dataAddress, bool shouldCheckDataAddress,
                 uint32_t p1_stack_size = 1);
    ~CamelConvert();
    void updateCode();
    void updateTargetAddressOffset(uint32_t newEntryAddress) {
        this->entryAddress = newEntryAddress;
        this->entryAddress_offset = this->entryAddress - CamelConvert::DEFAULT_TARGET_ADDRESS;
        this->binary.code_buf_size += this->entryAddress_offset;
    }
    void convertToBin();
    void saveBin();
    
private:
    static uint32_t jalTarget(uint32_t code, uint32_t address)
    {
        if ((code >> 26) != 0b000011) {
            fprintf(stderr, "instruction code: 0x%x is not [jal target] instruction!\n", code);
            exit(EXIT_FAILURE);
        }
        return (address&0xf0000000)|((code&0x03ffffff)<<2);
    }
    
    static uint32_t jTarget(uint32_t code, uint32_t address)
    {
        if ((code >> 26) != 0b000010) {
            fprintf(stderr, "instruction code: 0x%x is not [j target] instruction!\n", code);
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
        CamelConvert::__update_code__(ptr, offset, value >> 16);
    }
    
    static void update_ori_code(uint8_t *ptr, uint32_t offset, uint32_t value)
    {
        CamelConvert::__update_code__(ptr, offset, value & 0xffff);
    }
};

#endif /* convert_h */
