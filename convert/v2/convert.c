//
//  convert.c
//  convert
//
//  Created by Zhirui Dai on 2018/8/2.
//  Copyright © 2018 Zhirui Dai. All rights reserved.
//

#include "convert.h"

uint8_t convert_debug = 0;
uint32_t targetAddress = M2_DEFAULT_TARGET_ADDR;
uint32_t dataAddress = M2_DEFAULT_DATA_ADDR;
uint32_t mainAddress = 0;
uint32_t userInterruptAddress = 0;
uint32_t offset = 0;    // this value will be greater than 0 when optimization is enabled.

/*!
 * j target
 * | 31 - 26 |        25 - 0       |
 * |  000010 | (target >> 2)[25:0] |
 * Encode: code = 0x0800_0000 | (target >> 2) & 0x03ff_ffff
 * Decode: target = {PC[31:28], code & 0x03ff_ffff, 00}
 */

uint32_t __Mips_J_Code__(uint32_t target_address)
{
    uint32_t op_code = 0x08000000; // 000010 ...
    return (op_code | ((target_address >> 2) & 0x03ffffff));
}

uint32_t __Mips_J_Target__(uint32_t code, uint32_t address)
{
    return (address&0xf0000000)|((code&0x03ffffff)<<2);
}

/*!
 * jal target
 * | 31 - 26 |        25 - 0       |
 * |  000011 | (target >> 2)[25:0] |
 * Encode: code = 0x0c00_0000 | (target >> 2) & 0x03ff_ffff
 * Decode: target = {PC[31:28], code & 0x03ff_ffff, 00}
 */
uint32_t __Mips_Jal_Target__(uint32_t code, uint32_t address)
{
    return (address&0xf0000000)|((code&0x03ffffff)<<2);
}

M2_BIN_T M2_Convert_toBin(ELF_T elf)
{
    M2_BIN_T bin;
    bin.code_buf = NULL;
    bin.code_buf_size = 0;
    bin.code_section_addr = targetAddress;
    bin.rodata_buf = NULL;
    bin.rodata_buf_size = 0;
    bin.rodata_section_addr = targetAddress;
    bin.data_buf = NULL;
    bin.data_buf_size = 0;
    bin.data_memory_addr = dataAddress;
    bin.data_flash_begin_addr = targetAddress;
    bin.data_flash_end_addr = targetAddress;
    bin.bss_size = 0;
    bin.bss_begin_addr = dataAddress;
    bin.bss_end_addr = dataAddress;
    bin.elf_fileName = elf.fileName;

    for (uint32_t i = 0; i < elf.sectionHeaderNum; i++) {
        Elf32_Shdr  *section = elf.sectionsHeaders[i];
        char        *sectionName = (char*)elf.strTable + section->sh_name;
        if (!strcmp(sectionName, INSTR_CODE_SECTION)) {
            bin.code_buf_size = section->sh_size;
            bin.code_section_addr = section->sh_addr;

            if (elf.fileHeader->e_entry != bin.code_section_addr) {
                if (convert_debug) {
                    printf("Entry point address is adjusted from 0x%x to 0x%x!\n", bin.code_section_addr, elf.fileHeader->e_entry);
                }

                if (bin.code_section_addr >= FLASH_ADDR_THRESHOLD) {
                    offset = bin.code_section_addr - M2_DEFAULT_TARGET_ADDR;
                    bin.code_buf_size += offset;
                } else {
                    fprintf(stderr, "Target address is 0x%x, it should be greater than 0x%x when optimization is enabled.\n", bin.code_section_addr, FLASH_ADDR_THRESHOLD);
                    exit(EXIT_FAILURE);
                }

            } else {
                if (elf.fileHeader->e_entry != M2_DEFAULT_TARGET_ADDR) {
                    if (convert_debug) {
                        printf("Entry point address is not 0x%x! Now it is 0x%x\n", M2_DEFAULT_TARGET_ADDR, elf.fileHeader->e_entry);
                    }

                    if (bin.code_section_addr >= FLASH_ADDR_THRESHOLD) {
                        offset = bin.code_section_addr - M2_DEFAULT_TARGET_ADDR;
                        bin.code_buf_size += offset;
                    } else {
                        fprintf(stderr, "Target address is 0x%x, it should be greater than 0x%x when optimization is enabled.\n", bin.code_section_addr, FLASH_ADDR_THRESHOLD);
                        exit(EXIT_FAILURE);
                    }
                }
            }

            bin.code_buf = (uint8_t*)malloc(bin.code_buf_size * sizeof(uint8_t));
            memset(bin.code_buf, 0, bin.code_buf_size);

            if (offset) {
                /*
                nop                          # 0x0  -- 0: reset entry
                j pre_load                   # 0x4  -- 4
                nop                          # 0x8  -- 8: INT ecpt entry
                j interrupt_service_routine  #      -- 12
                nop                          # 0x10 -- 16: other ecpt entry
                j interrupt_service_routine  #      -- 20
                nop                          #      -- 24
                */
                /*
                uint32_t j_pre_load_addr = elf.fileHeader->e_entry + 4;
                uint32_t j_isr_addr = elf.fileHeader->e_entry + 8;
                USERBOOTLOADER_T userBootloader;
                userBootloader.code_0x0  = 0x00000000;                          // nop
                userBootloader.code_0x4  = __Mips_J_Code__(j_pre_load_addr);    // jump to the real pre_load address.
                userBootloader.code_0x8  = 0x00000000;                          // nop
                userBootloader.code_0xc  = __Mips_J_Code__(j_isr_addr);         // jump to the real isr address.
                userBootloader.code_0x10 = 0x00000000;                          // nop
                userBootloader.code_0x14 = __Mips_J_Code__(j_isr_addr);         // jump to the real isr address.
                userBootloader.code_0x18 = 0x00000000;                          // nop
                memcpy(bin.code_buf, &userBootloader, sizeof(USERBOOTLOADER_T));
                */
                // copy first 7 instructions from code section directly! These codes are the same as the userBootloader calculated aboved in the comment.
                memcpy(bin.code_buf, elf.buf + section->sh_offset + (elf.fileHeader->e_entry - bin.code_section_addr), 7 * 4);
                memcpy(bin.code_buf + offset, elf.buf + section->sh_offset, section->sh_size);
                // address of main function
                uint32_t address = elf.fileHeader->e_entry + M2_JUMP_MAINOS_OFFSET;
                uint32_t code = *(uint32_t*)(elf.buf + section->sh_offset + (elf.fileHeader->e_entry - bin.code_section_addr) + M2_JUMP_MAINOS_OFFSET);
                mainAddress = __Mips_Jal_Target__(code, address);
                // address of user_interrupt function
                address = M2_DEFAULT_TARGET_ADDR + 0xc;
                code = *(uint32_t*)(bin.code_buf + 0xc);
                address = __Mips_J_Target__(code, address);                     // get isr address
                address += M2_USER_INTERRUPT_OFFSET;                            // get address of 'jal user_interrupt'
                code = *(uint32_t*)(bin.code_buf + address - M2_DEFAULT_TARGET_ADDR);
                userInterruptAddress = __Mips_Jal_Target__(code, address);

                if (convert_debug) {
                    printf("Insert user bootloader! Code:\n");
                    for (size_t i = 0; i < 7*4; i+=4) {
                        uint32_t address = M2_DEFAULT_TARGET_ADDR + (uint32_t)i;
                        uint32_t code = *(uint32_t*)(bin.code_buf + i);
                        printf("\t0x%08x: %08x\t", address, code);
                        if(code == 0) {
                            printf("nop\n");
                        } else {
                            uint32_t j_target = __Mips_J_Target__(code, address);
                            printf("jump to 0x%08x\n", j_target);
                        }
                    }
                }
            } else {
                memcpy(bin.code_buf, elf.buf + section->sh_offset, bin.code_buf_size);
            }

            offset = elf.fileHeader->e_entry - M2_DEFAULT_TARGET_ADDR;
            if (convert_debug) {
                printf("Detect instruction code:\n");
                printf("    Address of entry:          0x%08x\n", bin.code_section_addr);
                printf("    Actual address of entry:   0x%08x\n", elf.fileHeader->e_entry);
                printf("    Jump to main at address:   0x%08x\n", elf.fileHeader->e_entry + M2_JUMP_MAINOS_OFFSET);
                printf("    Address of main:           0x%08x\n", mainAddress);
                printf("    Address of user_interrupt: 0x%08x\n", userInterruptAddress);
                printf("    Size of instruction code:  %d(0x%x)(bytes)\n", bin.code_buf_size, bin.code_buf_size);
            }
        } else if (!strcmp(sectionName, DATA_SECTION)) {
            bin.data_buf_size = section->sh_size;
            bin.data_memory_addr = section->sh_addr;
            bin.data_buf = (uint8_t*)malloc(bin.data_buf_size * sizeof(uint8_t));
            memcpy(bin.data_buf, elf.buf + section->sh_offset, bin.data_buf_size);
        } else if (!strcmp(sectionName, RODATA_SECTION)) {
            bin.rodata_buf_size = section->sh_size;
            bin.rodata_section_addr = section->sh_addr;
            bin.rodata_buf = (uint8_t*)malloc(bin.rodata_buf_size * sizeof(uint8_t));
            memcpy(bin.rodata_buf, elf.buf + section->sh_offset, bin.rodata_buf_size);
            if (convert_debug) {
                printf("Detect rodata section:\n");
                printf("    Address in flash:         0x%08x\n", bin.rodata_section_addr);
                printf("    Size:                     %d(0x%x)(bytes)\n", bin.rodata_buf_size, bin.rodata_buf_size);
            }
        } else if (!strcmp(sectionName, BSS_SECTION)) {
            bin.bss_begin_addr = section->sh_addr;
            bin.bss_size = section->sh_size;
            bin.bss_end_addr = bin.bss_begin_addr + bin.bss_size;
            if (convert_debug) {
                printf("Detect bss section:\n");
                printf("    Address in memory:        0x%08x\n", bin.bss_begin_addr);
                printf("    Size:                     %d(0x%x)(bytes)\n", bin.bss_size, bin.bss_size);
            }
        }
    }

    bin.data_flash_begin_addr = M2_DEFAULT_TARGET_ADDR + bin.code_buf_size + bin.rodata_buf_size;
    bin.data_flash_end_addr = bin.data_flash_begin_addr + bin.data_buf_size;

    if (bin.data_buf != NULL) {

        if (convert_debug) {
            printf("Detect data section:\n");
            printf("    Address in memory:        0x%08x\n", bin.data_memory_addr);
            printf("    Address in flash:         0x%08x\n", bin.data_flash_begin_addr);
            printf("    Size:                     %d(bytes)\n", bin.data_buf_size);
        }
    }

    bin.global_pointer_addr = elf.regInfo->ri_gp_value;
    bin.stack_pointer_addr = M2_STACK_POINTER_ADDR;

    return bin;
}

void M2_Convert_freeBin(M2_BIN_T bin)
{
    free(bin.code_buf);
    free(bin.rodata_buf);
    free(bin.data_buf);
}

void __update_code__(uint8_t *ptr, uint32_t offset, uint32_t value)
{
    uint32_t opcode;
    opcode = *(uint32_t *)(ptr + offset);
    //    opcode = ntohl(opcode);
    opcode = (opcode & 0xffff0000) | (value & 0xffff);
    //    opcode = ntohl(opcode);
    *(uint32_t *)(ptr + offset) = opcode;
}

#define __GLOBAL_PTR_CODE_POS__     28
#define __DATA_MEMADDR_CODE_POS__   52
#define __DATA_FLASHADDR_CODE_POS__ 36
#define __BSS_MEMADDR_CODE_POS__    104
#define __STACK_PTR_CODE_POS__      120

void M2_Convert_updateInstrCode(M2_BIN_T bin)
{
    // update global_pointer
    __update_code__(bin.code_buf,
                    __GLOBAL_PTR_CODE_POS__ + offset,
                    bin.global_pointer_addr >> 16);     // lui
    __update_code__(bin.code_buf,
                    __GLOBAL_PTR_CODE_POS__ + offset + 4,
                    bin.global_pointer_addr & 0xffff);  // ori
    // update data section address in memory
    __update_code__(bin.code_buf,
                    __DATA_MEMADDR_CODE_POS__ + offset,
                    bin.data_memory_addr >> 16);        // lui
    __update_code__(bin.code_buf,
                    __DATA_MEMADDR_CODE_POS__ + offset + 4,
                    bin.data_memory_addr & 0xffff);     // ori
    // update data section beginning address in flash
    __update_code__(bin.code_buf,
                    __DATA_FLASHADDR_CODE_POS__ + offset,
                    bin.data_flash_begin_addr >> 16);   // lui
    __update_code__(bin.code_buf,
                    __DATA_FLASHADDR_CODE_POS__ + offset + 4,
                    bin.data_flash_begin_addr & 0xffff); // ori
    // update data section ending address in flash
    __update_code__(bin.code_buf,
                    __DATA_FLASHADDR_CODE_POS__ + offset + 8,
                    bin.data_flash_end_addr >> 16);     // lui
    __update_code__(bin.code_buf,
                    __DATA_FLASHADDR_CODE_POS__ + offset + 12,
                    bin.data_flash_end_addr & 0xffff);  // ori
    // update bss section beginning address in memory
    __update_code__(bin.code_buf,
                    __BSS_MEMADDR_CODE_POS__ + offset,
                    bin.bss_begin_addr >> 16);          // lui
    __update_code__(bin.code_buf,
                    __BSS_MEMADDR_CODE_POS__ + offset + 4,
                    bin.bss_begin_addr & 0xffff);       // ori
    // update bss section ending address in memory
    __update_code__(bin.code_buf,
                    __BSS_MEMADDR_CODE_POS__ + offset + 8,
                    bin.bss_end_addr >> 16);            // lui
    __update_code__(bin.code_buf,
                    __BSS_MEMADDR_CODE_POS__ + offset + 12,
                    bin.bss_end_addr & 0xffff);         // ori
    // update stack pointer
    __update_code__(bin.code_buf,
                    __STACK_PTR_CODE_POS__ + offset,
                    bin.stack_pointer_addr >> 16);      // lui
    __update_code__(bin.code_buf,
                    __STACK_PTR_CODE_POS__ + offset + 4,
                    bin.stack_pointer_addr & 0xffff);   // ori
    if (convert_debug) {
        printf("Update code:\n");
        printf("    Global pointer -> 0x%08x\n", bin.global_pointer_addr);
        printf("    Stack pointer  -> 0x%08x\n", bin.stack_pointer_addr);
        printf("    Data section memory addr -> 0x%08x\n", bin.data_memory_addr);
        printf("    Data section flash addr  -> 0x%08x ~ 0x%08x\n", bin.data_flash_begin_addr, bin.data_flash_end_addr);
        printf("    BSS memory addr          -> 0x%08x ~ 0x%08x\n", bin.bss_begin_addr, bin.bss_end_addr);
    }
}

void M2_Convert_writeBinToDisk(M2_BIN_T bin)
{
    char*   fileName;
    size_t  fileNameLength = strlen(bin.elf_fileName) + 12;
    fileName = (char*)malloc(fileNameLength * sizeof(char));
    FILE    *file;

    // binary bin
    strcpy(fileName, bin.elf_fileName);
    strcat(fileName, ".bin");
    file = fopen(fileName, "wb");
    fwrite(bin.code_buf, bin.code_buf_size, 1, file);
    if (bin.rodata_buf != NULL) {
        fwrite(bin.rodata_buf, bin.rodata_buf_size, 1, file);
    }
    if (bin.data_buf != NULL) {
        fwrite(bin.data_buf, bin.data_buf_size, 1, file);
    }
    // make sure the file size is a multiple of 4.
    uint32_t remnant = (bin.code_buf_size + bin.rodata_buf_size + bin.data_buf_size) % 4;
    if (remnant) {
        remnant = 4 - remnant;
        for (uint32_t i = 0; i < remnant; i++) {
            fprintf(file, "");
        }
    }
    fclose(file);
    if (convert_debug) {
        printf("\n%s: %d(bytes)\n", fileName, bin.code_buf_size + bin.rodata_buf_size + bin.data_buf_size);
        if (remnant) {
            printf("%d '0' are appended to the binary file!\n", remnant);
        }
    }

    // binary txt
    strcpy(fileName, bin.elf_fileName);
    strcat(fileName, ".txt");
    file = fopen(fileName, "w");
    for (uint32_t i = 0; i <= bin.code_buf_size; i += 4) {
        fprintf(file, "%8.8x: %8.8x\n", M2_DEFAULT_TARGET_ADDR + i, *(uint32_t*)(bin.code_buf + i));
    }
    if (bin.rodata_buf != NULL) {
        for (uint32_t i = 0; i <= bin.rodata_buf_size; i += 4) {
            fprintf(file, "%8.8x: %8.8x\n", bin.rodata_section_addr + i, *(uint32_t*)(bin.rodata_buf + i));
        }
    }
    if (bin.data_buf != NULL) {
        for (uint32_t i = 0; i <= bin.data_buf_size; i += 4) {
            fprintf(file, "%8.8x: %8.8x\n", bin.data_flash_begin_addr + i, *(uint32_t*)(bin.data_buf + i));
        }
    }
    fclose(file);

    // rodata bin txt
    if (bin.rodata_buf != NULL) {
        strcpy(fileName, bin.elf_fileName);
        strcat(fileName, "_rodata.bin");
        file = fopen(fileName, "wb");
        fwrite(bin.rodata_buf, bin.rodata_buf_size, 1, file);
        fclose(file);

        strcpy(fileName, bin.elf_fileName);
        strcat(fileName, "_rodata.txt");
        file = fopen(fileName, "w");
        for (uint32_t i = 0; i <= bin.rodata_buf_size; i += 4) {
            fprintf(file, "%8.8x: %8.8x ===> ", bin.rodata_section_addr + i, *(uint32_t*)(bin.rodata_buf + i));
            fwrite(bin.rodata_buf + i, sizeof(uint32_t), 1, file);
            fprintf(file, "\n");
        }
        fclose(file);
    }

    // data bin txt
    if (bin.data_buf != NULL) {
        strcpy(fileName, bin.elf_fileName);
        strcat(fileName, "_data.bin");
        file = fopen(fileName, "wb");
        fwrite(bin.data_buf, bin.data_buf_size, 1, file);
        fclose(file);

        strcpy(fileName, bin.elf_fileName);
        strcat(fileName, "_data.txt");
        file = fopen(fileName, "w");
        for (uint32_t i = 0; i <= bin.data_buf_size; i += 4) {
            fprintf(file, "%8.8x: %8.8x\n", bin.data_flash_begin_addr + i, *(uint32_t*)(bin.data_buf + i));
        }
        fclose(file);
    }

    // coe file
    strcpy(fileName, bin.elf_fileName);
    strcat(fileName, ".coe");
    file = fopen(fileName, "w");
    fprintf(file, "memory_initialization_radix = 16;\n");
    fprintf(file, "memory_initialization_vector =\n");
    for(uint32_t i = 0; i <= bin.code_buf_size; i += 4) {
        fprintf(file, "%8.8x\n", *(uint32_t*)(bin.code_buf + i));
    }
    fclose(file);
    file = NULL;
    free(fileName);
}
