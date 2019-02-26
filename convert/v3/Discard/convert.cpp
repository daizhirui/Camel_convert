//
//  convert.cpp
//  convert
//
//  Created by 戴植锐 on 2019/1/29.
//  Copyright © 2019 Zhirui Dai. All rights reserved.
//
#include <iostream>
#include <iomanip>
#include <cstdio>
#include "convert.h"
#include "myDebug.h"

CamelConvert::CamelConvert(ELF_T elf,
                           CHIP_T chip,
                           uint32_t targetAddress,
                           bool shouldCheckTargetAddress,
                           uint32_t dataAddress,
                           bool shouldCheckDataAddress,
                           uint32_t p1_stack_size)
{
    this->elf = elf;
    this->chip = chip;
    this->binary = binary_t();
    
    this->entryAddress = targetAddress;
    this->targetAddress = targetAddress;
    this->shouldCheckTargetAddress = shouldCheckTargetAddress;
    this->entryAddress_offset = this->entryAddress - CamelConvert::DEFAULT_TARGET_ADDRESS;
    this->dataAddress = dataAddress;
    this->shouldCheckDataAddress = shouldCheckDataAddress;
    
    this->p1_stack_size = p1_stack_size;
    
    for (int i = 0; i < CORE_NUM_MAX; i++) {
        this->main_address[i] = 0;
        this->isr_address[i] = 0;
        this->interrupt_address[i] = 0;
    }
    
    this->binary.code_buf = nullptr;
    this->binary.code_buf_size = 0;
    this->binary.code_section_addr = CamelConvert::DEFAULT_TARGET_ADDRESS;

    this->binary.rodata_buf = nullptr;
    this->binary.rodata_buf_size = 0;
    this->binary.rodata_section_addr = CamelConvert::DEFAULT_TARGET_ADDRESS;
    
    this->binary.eh_frame_buf = nullptr;
    this->binary.eh_frame_buf_size = 0;
    this->binary.eh_frame_section_addr = CamelConvert::DEFAULT_TARGET_ADDRESS;
    
    this->binary.init_array_buf = nullptr;
    this->binary.init_array_buf_size = 0;
    this->binary.init_array_section_addr = CamelConvert::DEFAULT_TARGET_ADDRESS;

    this->binary.data_buf = nullptr;
    this->binary.data_buf_size = 0;
    this->binary.data_memory_addr = CamelConvert::DEFAULT_DATA_ADDRESS;
    this->binary.data_flash_begin_addr = CamelConvert::DEFAULT_TARGET_ADDRESS;
    this->binary.data_flash_end_addr = CamelConvert::DEFAULT_TARGET_ADDRESS;

    this->binary.bss_size = 0;
    this->binary.bss_begin_addr = CamelConvert::DEFAULT_DATA_ADDRESS;
    this->binary.bss_end_addr = CamelConvert::DEFAULT_DATA_ADDRESS;

    for (int i = 0; i < CORE_NUM_MAX; ++i) {
        this->binary.global_pointer_addr[i] = CamelConvert::DEFAULT_GP;
        this->binary.stack_pointer_addr[i] = CamelConvert::DEFAULT_SP;
    }
}

CamelConvert::~CamelConvert()
{
    free(this->binary.code_buf);
    free(this->binary.rodata_buf);
    free(this->binary.data_buf);
}

void CamelConvert::updateCode()
{
    verbose("\tUpdate $gp ...\n");
    const int* gp_lui_offset = CamelConvert::GP_LUI_OFFSET[this->chip];
    const int* gp_ori_offset = CamelConvert::GP_ORI_OFFSET[this->chip];
    for (int i = 0; i < CamelConvert::CORE_NUM[this->chip]; i++) {
        CamelConvert::update_lui_code(this->binary.code_buf, gp_lui_offset[i], this->binary.global_pointer_addr[i]);
        CamelConvert::update_ori_code(this->binary.code_buf, gp_ori_offset[i], this->binary.global_pointer_addr[i]);
        verbose("\t\t$gp of p%d = 0x%8.8x\n", i, this->binary.global_pointer_addr[i]);
    }
    verbose("\tUpdate $sp ...\n");
    const int* sp_lui_offset = CamelConvert::SP_LUI_OFFSET[this->chip];
    const int* sp_ori_offset = CamelConvert::SP_ORI_OFFSET[this->chip];
    for (int i = 0; i < CamelConvert::CORE_NUM[this->chip]; i++) {
        CamelConvert::update_lui_code(this->binary.code_buf, sp_lui_offset[i], this->binary.stack_pointer_addr[i]);
        CamelConvert::update_ori_code(this->binary.code_buf, sp_ori_offset[i], this->binary.stack_pointer_addr[i]);
        verbose("\t\t$sp of p%d = 0x%8.8x\n", i, this->binary.stack_pointer_addr[i]);
    }
    verbose("\tUpdate data initialization address ...\n");
    const int* data_lui_offset = CamelConvert::DATA_LUI_OFFSET[this->chip];
    const int* data_ori_offset = CamelConvert::DATA_ORI_OFFSET[this->chip];
    uint32_t data_init_addr[3] = {
        this->binary.data_flash_begin_addr,
        this->binary.data_flash_end_addr,
        this->binary.data_memory_addr
    };
    const char* data_addr_name[3] = {".data_flash_begin_addr", ".data_flash_end_addr", ".data_memory_addr"};
    for (int i = 0; i < sizeof(data_init_addr) / sizeof(uint32_t); i++) {
        CamelConvert::update_lui_code(this->binary.code_buf, data_lui_offset[i], data_init_addr[i]);
        CamelConvert::update_ori_code(this->binary.code_buf, data_ori_offset[i], data_init_addr[i]);
        verbose("\t\t%s = 0x%8.8x\n", data_addr_name[i], data_init_addr[i]);
    }
    verbose("\tUpdate bss data initialization address ...\n");
    const int* bss_lui_offset = CamelConvert::BSS_LUI_OFFSET[this->chip];
    const int* bss_ori_offset = CamelConvert::BSS_ORI_OFFSET[this->chip];
    uint32_t bss_init_addr[2] = {
        this->binary.bss_begin_addr,
        this->binary.bss_end_addr
    };
    const char* bss_addr_name[2] = {".bss_begin_addr", ".bss_end_addr"};
    for (int i = 0; i < sizeof(bss_init_addr) / sizeof(uint32_t); i++) {
        CamelConvert::update_lui_code(this->binary.code_buf, bss_lui_offset[i], bss_init_addr[i]);
        CamelConvert::update_ori_code(this->binary.code_buf, bss_ori_offset[i], bss_init_addr[i]);
        verbose("\t\t%s = 0x%8.8x\n", bss_addr_name[i], bss_init_addr[i]);
    }
}

void CamelConvert::convertToBin()
{
    for (uint32_t i = 0; i < this->elf.sectionHeaderNum; i++) {
        
        Elf32_Shdr  *section = this->elf.sectionsHeaders[i];
        char        *sectionName = (char*)this->elf.strTable + section->sh_name;
        
        if (!strcmp(sectionName, CamelConvert::INSTR_CODE_SECTION_NAME)) {  // check ".text" section
            
            this->binary.code_buf_size = section->sh_size;
            this->binary.code_section_addr = section->sh_addr;              // targetAddress from the elf
            uint8_t* elfCodeBufPtr = this->elf.buf + section->sh_offset;
            this->entryAddress = this->elf.fileHeader->e_entry;             // entryAddress ==> actual entrance address in Linux
            
            if (this->extractSections) {
                std::string fileName = this->outputFileName + ".bin";
                verbose("Extract .text section, save it as %s ...\n", fileName.c_str());
                FILE* file = fopen(fileName.c_str(), "w");
                assert(file != nullptr);
                fwrite(elfCodeBufPtr, sizeof(uint8_t), section->sh_size, file);
                fclose(file);
                file = nullptr;
                
                std::string textFileName = this->outputFileName + ".txt";
                verbose("Generate file: %s ...\n", textFileName.c_str());
                FILE*   textFilePtr = fopen(textFileName.c_str(), "w");
                assert(textFilePtr != nullptr);
                for (uint32_t i = 0; i <= this->binary.code_buf_size; i += 4) {
                    fprintf(textFilePtr, "%8.8x\n", *(uint32_t*)(elfCodeBufPtr + i));
                }
                fclose(textFilePtr);
                textFilePtr = nullptr;
                
                continue;   // don't generate a runable binary by updating $gp and $sp!
            }
            
            // this->binary.code_section_addr ==> targetAddress
            // compare targetAddress in the elf with targetAddress from the command line
            if (this->shouldCheckTargetAddress && this->binary.code_section_addr != this->targetAddress) {
                fprintf(stderr, "\x1b[31mThe targetAddress is not 0x%x as expectation!\x1b[0m\n", this->targetAddress);
                exit(EXIT_FAILURE);
            } else {
                this->targetAddress = this->binary.code_section_addr;
            }
            // compare entry point address with targetAddress; normally, entry point address == targetAddress
            bool compareWithThreshold = false;
            if (this->entryAddress != this->targetAddress) {
                verbose("\x1b[31mEntry point address is adjusted from 0x%x to 0x%x!\x1b[0m\n",
                        this->targetAddress, this->entryAddress);
                compareWithThreshold = true;
            } else if (this->entryAddress != CamelConvert::DEFAULT_TARGET_ADDRESS) {
                verbose("\x1b[31mEntry point address is not the default value 0x%x! Now it is 0x%x\x1b[0m\n",
                        CamelConvert::DEFAULT_TARGET_ADDRESS, this->entryAddress);
                compareWithThreshold = true;
            }
            if (compareWithThreshold && this->rootMode) {
                if (this->targetAddress >= CamelConvert::ADJUSTED_TARGET_ADDRESS_THRESHOLD[this->chip]) {
                    this->updateTargetAddressOffset(this->targetAddress);   // update targetAddress_offset and code_buf_size
                } else {
                    fprintf(stderr, "\x1b[31mTarget address is 0x%x, it should be greater than 0x%x when optimization is enabled.\x1b[0m\n",
                            this->targetAddress, CamelConvert::ADJUSTED_TARGET_ADDRESS_THRESHOLD[this->chip]);
                    exit(EXIT_FAILURE);
                }
            }
            
            // prepare the memory for code_buf
            this->binary.code_buf = (uint8_t*)malloc(this->binary.code_buf_size * sizeof(uint8_t));
            memset(this->binary.code_buf, 0, this->binary.code_buf_size);
            
            // store the code section
            if (this->entryAddress_offset) {
                // copy the loader from code section directly!
                // offset of the loader is "entryAddress - this->targetAddress"
                memcpy(this->binary.code_buf,
                       elfCodeBufPtr + (this->entryAddress - this->targetAddress),    // ptr to the loader
                       CamelConvert::LOADER_SIZE[this->chip] * sizeof(uint32_t));
                
                if (__verbose__) {
                    printf("Insert user bootloader! Code:\n");
                    for (uint32_t i = 0; i < CamelConvert::LOADER_SIZE[this->chip] * sizeof(uint32_t); i += sizeof(uint32_t)) {
                        uint32_t address = CamelConvert::DEFAULT_TARGET_ADDRESS + i;
                        uint32_t code = *(uint32_t*)(elfCodeBufPtr + i);
                        printf("\t0x%08x: %08x\t", address, code);
                        if(code == 0) {
                            printf("nop\n");
                        } else {
                            printf("jump to 0x%08x\n", CamelConvert::jTarget(code, address));
                        }
                    }
                }
            }
            memcpy(this->binary.code_buf + this->entryAddress_offset, elfCodeBufPtr, section->sh_size);
            
            // calculate a series of addresses
            uint32_t address = 0;
            uint32_t code = 0;
            // calculate a set of addresses
            for (int i = 0; i < CamelConvert::CORE_NUM[this->chip]; i++) {
                address = entryAddress + CamelConvert::JALOS_ADDRESS_OFFSET[this->chip][i];
                code = *(uint32_t*)(elfCodeBufPtr + address - this->targetAddress);
                this->main_address[i] = CamelConvert::jalTarget(code, address);
                
                address = entryAddress + CamelConvert::JISR_ADDRESS_OFFSET[this->chip][i];
                code = *(uint32_t*)(elfCodeBufPtr + address - this->targetAddress);
                this->isr_address[i] = CamelConvert::jTarget(code, address);
                
                address = this->isr_address[i] + CamelConvert::JAL_INTERRUPT_ADDRESS_OFFSET[this->chip][i];
                code = *(uint32_t*)(elfCodeBufPtr + address - this->targetAddress);
                this->interrupt_address[i] = CamelConvert::jalTarget(code, address);
            }
            if (__verbose__) {
                printf("Important Address:\n");
                printf("\tAddress of entry point:  0x%08x\n", entryAddress);
                printf("\t        Target Address:  0x%08x\n", this->targetAddress);
                for (int i = 0; i < CamelConvert::CORE_NUM[this->chip]; i++) {
                    printf("\t--------------------------------------\n");
                    printf("\t       p%d main Address:  0x%08x\n", i, this->main_address[i]);
                    printf("\t        p%d isr Address:  0x%08x\n", i, this->isr_address[i]);
                    printf("\t  p%d interrupt Address:  0x%08x\n", i, this->interrupt_address[i]);
                }
                printf("\nSize of instruction code:  %d(0x%08x)(bytes)\n", this->binary.code_buf_size, this->binary.code_buf_size);
            }
            
        } else if (!strcmp(sectionName, CamelConvert::DATA_SECTION_NAME)) {
            this->binary.data_buf_size = section->sh_size;
            this->binary.data_memory_addr = section->sh_addr;
            this->binary.data_buf = (uint8_t*)malloc(this->binary.data_buf_size * sizeof(uint8_t));
            memcpy(this->binary.data_buf, elf.buf + section->sh_offset, this->binary.data_buf_size);
            
            if (this->extractSections) {
                std::string fileName = this->outputFileName + "_data.bin";
                verbose("Extract .data section, save it as %s ...\n", fileName.c_str());
                FILE* file = fopen(fileName.c_str(), "w");
                assert(file != nullptr);
                fwrite(this->binary.data_buf, sizeof(uint8_t), section->sh_size, file);
                fclose(file);
                file = nullptr;
                
                std::string dataTextFileName = this->outputFileName + "_data.txt";
                verbose("Generate file: %s ...\n", dataTextFileName.c_str());
                FILE*   dataTextFilePtr = fopen(dataTextFileName.c_str(), "w");
                assert(dataTextFilePtr != nullptr);
                for (uint32_t i = 0; i <= this->binary.data_buf_size; i += 4) {
                    fprintf(dataTextFilePtr, "%8.8x\n", *(uint32_t*)(this->binary.data_buf + i));
                }
                fclose(dataTextFilePtr);
                dataTextFilePtr = nullptr;
                
                continue;
            }
            
        } else if (!strcmp(sectionName, CamelConvert::RODATA_SECTION_NAME)) {
            
            this->binary.rodata_buf_size = section->sh_size;
            this->binary.rodata_section_addr = section->sh_addr;
            this->binary.rodata_buf = (uint8_t*)malloc(this->binary.rodata_buf_size * sizeof(uint8_t));
            memcpy(this->binary.rodata_buf, this->elf.buf + section->sh_offset, this->binary.rodata_buf_size);
            if (__verbose__) {
                printf("\nDetect rodata section:\n");
                printf("\t Address in flash:   0x%08x\n", this->binary.rodata_section_addr);
                printf("\t             Size:   %d(0x%x)(bytes)\n", this->binary.rodata_buf_size, this->binary.rodata_buf_size);
            }
            
            if (this->extractSections) {
                std::string fileName = this->outputFileName + "_rodata.bin";
                verbose("Extract .rodata section, save it as %s ...\n", fileName.c_str());
                FILE* file = fopen(fileName.c_str(), "w");
                assert(file != nullptr);
                fwrite(this->binary.rodata_buf, sizeof(uint8_t), this->binary.rodata_buf_size, file);
                fclose(file);
                file = nullptr;
                
                std::string rodataTextFileName = this->outputFileName + "_rodata.txt";
                verbose("Generate file: %s ...\n", rodataTextFileName.c_str());
                FILE*   rodataTextFilePtr = fopen(rodataTextFileName.c_str(), "w");
                assert(rodataTextFilePtr != nullptr);
                for (uint32_t i = 0; i <= this->binary.rodata_buf_size; i += 4) {
                    fprintf(rodataTextFilePtr, "%8.8x\n", *(uint32_t*)(this->binary.rodata_buf + i));
                }
                fclose(rodataTextFilePtr);
                rodataTextFilePtr = nullptr;
                
                continue;
            }
            
        } else if (!strcmp(sectionName, CamelConvert::EH_FRAME_SECTION_NAME)){
            
            this->binary.eh_frame_buf_size = section->sh_size;
            this->binary.eh_frame_section_addr = section->sh_addr;
            this->binary.eh_frame_buf = (uint8_t*)malloc(this->binary.eh_frame_buf_size * sizeof(uint8_t));
            memcpy(this->binary.eh_frame_buf, this->elf.buf + section->sh_offset, this->binary.eh_frame_buf_size);
            if (__verbose__) {
                printf("\nDetect eh_frame section:\n");
                printf("\t Address in flash:   0x%08x\n", this->binary.eh_frame_section_addr);
                printf("\t             Size:   %d(0x%x)(bytes)\n", this->binary.eh_frame_buf_size, this->binary.eh_frame_buf_size);
            }
            
            if (this->extractSections) {
                std::string fileName = this->outputFileName + "_eh_frame.bin";
                verbose("Extract .eh_frame section, save it as %s ...\n", fileName.c_str());
                FILE* file = fopen(fileName.c_str(), "w");
                assert(file != nullptr);
                fwrite(this->binary.eh_frame_buf, sizeof(uint8_t), this->binary.eh_frame_buf_size, file);
                fclose(file);
                file = nullptr;
                continue;
            }
        } else if (!strcmp(sectionName, CamelConvert::INIT_ARRAY_SECTION_NAME)) {
            
            this->binary.init_array_buf_size = section->sh_size;
            this->binary.init_array_section_addr = section->sh_addr;
            this->binary.init_array_buf = (uint8_t*)malloc(this->binary.init_array_buf_size * sizeof(uint8_t));
            memcpy(this->binary.init_array_buf, this->elf.buf + section->sh_offset, this->binary.init_array_buf_size);
            if (__verbose__) {
                printf("\nDetect init_array section:\n");
                printf("\t Address in flash:   0x%08x\n", this->binary.init_array_section_addr);
                printf("\t             Size:   %d(0x%x)(bytes)\n", this->binary.init_array_buf_size, this->binary.init_array_buf_size);
            }
            
            if (this->extractSections) {
                std::string fileName = this->outputFileName + "_init_array.bin";
                verbose("Extract .init_array section, save it as %s ...\n", fileName.c_str());
                FILE* file = fopen(fileName.c_str(), "w");
                assert(file != nullptr);
                fwrite(this->binary.init_array_buf, sizeof(uint8_t), this->binary.init_array_buf_size, file);
                fclose(file);
                file = nullptr;
                continue;
            }
            
        } else if (!strcmp(sectionName, CamelConvert::BSS_SECTION_NAME)) {
            
            this->binary.bss_begin_addr = section->sh_addr;
            this->binary.bss_size = section->sh_size;
            this->binary.bss_end_addr = this->binary.bss_begin_addr + this->binary.bss_size;
            if (__verbose__) {
                printf("\nDetect bss section:\n");
                printf("\t Address in memory:  0x%08x\n", this->binary.bss_begin_addr);
                printf("\t              Size:  %d(0x%x)(bytes)\n", this->binary.bss_size, this->binary.bss_size);
            }
        }
    }
    
    if (this->extractSections) {
        return; // just extract sections
    }
    
    //// calculate .data section addrs
    this->binary.data_flash_begin_addr = CamelConvert::DEFAULT_TARGET_ADDRESS + this->binary.code_buf_size + this->binary.rodata_buf_size;
    this->binary.data_flash_end_addr = this->binary.data_flash_begin_addr + this->binary.data_buf_size;
    
    if (__verbose__ && this->binary.data_buf != nullptr) {
        printf("\nDetect data section:\n");
        printf("\t Address in memory:   0x%08x\n", this->binary.data_memory_addr);
        printf("\t  Address in flash:   0x%08x\n", this->binary.data_flash_begin_addr);
        printf("\t              Size:   %d(bytes)\n", this->binary.data_buf_size);
    }
    
    //// extract GP
    for (int i = 0; i < CamelConvert::CORE_NUM[this->chip]; i++) {
        this->binary.global_pointer_addr[i] = this->elf.regInfo->ri_gp_value;   // all cores share the same global area!
    }
    
    //// calculate SP
    // p0 sp
    uint32_t offset = MASTER_CORE_GPSP_SIZE + (SLAVE_CORE_GPSP_SIZE + SLAVE_FUNC_EXCHANGE_SIZE) * (CORE_NUM[this->chip] - 1);
    this->binary.stack_pointer_addr[0] = CamelConvert::SRAM_ADDRESS_MAX - offset;
    uint32_t globalAreaSize = this->binary.data_buf_size + this->binary.bss_size;
    // p1 ~ px
    for (int i = 1; i < CORE_NUM[this->chip]; i++) {
        this->binary.stack_pointer_addr[i] = SRAM_ADDRESS_MIN + globalAreaSize + i * this->p1_stack_size * STACK_PAGE_VOLUME;
    }
    
    if (__verbose__) {
        printf("\nGlobal Pointer of %d core(s):  0x%08x\n", CORE_NUM[this->chip], this->binary.global_pointer_addr[0]);
        for (int i = 0; i < CORE_NUM[this->chip]; i++) {
            printf("     Stack Pointer of Core%d:  0x%08x\n", i, this->binary.stack_pointer_addr[i]);
        }
    }
}

void CamelConvert::saveBin()
{
    std::string binaryFileName = this->outputFileName + ".bin";
    verbose("Generate file: %s ...\n", binaryFileName.c_str())
    FILE*       binaryFilePtr = fopen(binaryFileName.c_str(), "wb");
    assert(binaryFilePtr != nullptr);
    // .text
    fwrite(this->binary.code_buf, this->binary.code_buf_size, 1, binaryFilePtr);
    // .rodata
    if (this->binary.rodata_buf != nullptr) {
        fwrite(this->binary.rodata_buf, this->binary.rodata_buf_size, 1, binaryFilePtr);
    }
    // .data
    if (this->binary.data_buf != nullptr) {
        fwrite(this->binary.code_buf, this->binary.data_buf_size, 1, binaryFilePtr);
    }
    // make sure the file size is a multiple of 4.
    uint32_t remnant = (this->binary.code_buf_size + this->binary.rodata_buf_size + this->binary.data_buf_size) % 4;
    if (remnant) {
        remnant = 4 - remnant;
        size_t  count = remnant * sizeof(uint8_t);
        uint8_t* zeros = (uint8_t*)malloc(count);
        memset(zeros, 0, count);
        fwrite(zeros, count, 1, binaryFilePtr);
        free(zeros);
    }
    fclose(binaryFilePtr);
    binaryFilePtr = nullptr;
    if (__verbose__) {
        printf("\t%s: %d(bytes)\n", binaryFileName.c_str(),
               this->binary.code_buf_size + this->binary.rodata_buf_size + this->binary.data_buf_size);
        if (remnant) {
            printf("\t%d '0' are appended to the binary file!\n", remnant);
        }
    }
    
    // binary txt
    std::string bintextFileName = this->outputFileName + ".txt";
    verbose("Generate file: %s ...\n", bintextFileName.c_str());
    FILE*   bintextFilePtr = fopen(bintextFileName.c_str(), "w");
    assert(bintextFilePtr != nullptr);
    for (int i = 0; i < this->binary.code_buf_size; i += 4) {
        fprintf(bintextFilePtr, "%8.8x: %8.8x\n",
                CamelConvert::DEFAULT_TARGET_ADDRESS + i,
                *(uint32_t*)(this->binary.code_buf + i));
    }
    if (this->binary.rodata_buf != nullptr) {
        for (int i = 0; i <= this->binary.rodata_buf_size; i += 4) {
            fprintf(bintextFilePtr, "%8.8x: %8.8x\n",
                    this->binary.rodata_section_addr + i,
                    *(uint32_t*)(this->binary.rodata_buf + i));
        }
    }
    if (this->binary.data_buf != nullptr) {
        for (uint32_t i = 0; i <= this->binary.data_buf_size; i += 4) {
            fprintf(bintextFilePtr, "%8.8x: %8.8x\n",
                    this->binary.data_flash_begin_addr + i,
                    *(uint32_t*)(this->binary.data_buf + i));
        }
    }
    fclose(bintextFilePtr);
    bintextFilePtr = nullptr;
    
    // rodata bin txt
    if (this->binary.rodata_buf != nullptr) {
        std::string rodataBinFileName = this->outputFileName + "_rodata.bin";
        verbose("Generate file: %s ...\n", rodataBinFileName.c_str());
        FILE*   rodataFilePtr  = fopen(rodataBinFileName.c_str(), "wb");
        assert(rodataFilePtr != nullptr);
        fwrite(this->binary.rodata_buf, this->binary.rodata_buf_size, 1, rodataFilePtr);
        fclose(rodataFilePtr);
        rodataFilePtr = nullptr;
        
        std::string rodataTextFileName = this->outputFileName + "_rodata.txt";
        verbose("Generate file: %s ...\n", rodataTextFileName.c_str());
        FILE*   rodataTextFilePtr = fopen(rodataTextFileName.c_str(), "w");
        assert(rodataTextFilePtr != nullptr);
        for (uint32_t i = 0; i <= this->binary.rodata_buf_size; i += 4) {
            fprintf(rodataTextFilePtr, "%8.8x: %8.8x ===> ",
                    this->binary.rodata_section_addr + i,
                    *(uint32_t*)(this->binary.rodata_buf + i));
            fwrite(this->binary.rodata_buf + i, sizeof(uint32_t), 1, rodataTextFilePtr);
            fprintf(rodataTextFilePtr, "\n");
        }
        fclose(rodataTextFilePtr);
        rodataTextFilePtr = nullptr;
    }
    
    // data bin txt
    if (this->binary.data_buf != nullptr) {
        std::string dataBinFileName = this->outputFileName + "_data.bin";
        verbose("Generate file: %s ...\n", dataBinFileName.c_str());
        FILE*   dataFilePtr = fopen(dataBinFileName.c_str(), "wb");
        assert(dataFilePtr != nullptr);
        fwrite(this->binary.data_buf, this->binary.data_buf_size, 1, dataFilePtr);
        fclose(dataFilePtr);
        dataFilePtr = nullptr;
        
        std::string dataTextFileName = this->outputFileName + "_data.txt";
        verbose("Generate file: %s ...\n", dataTextFileName.c_str());
        FILE*   dataTextFilePtr = fopen(dataTextFileName.c_str(), "w");
        for (uint32_t i = 0; i <= this->binary.data_buf_size; i += 4) {
            fprintf(dataTextFilePtr, "%8.8x: %8.8x\n",
                    this->binary.data_flash_begin_addr + i,
                    *(uint32_t*)(this->binary.data_buf + i));
        }
        fclose(dataTextFilePtr);
        dataTextFilePtr = nullptr;
    }
    
    // coe file
    std::string coeFileName = this->outputFileName + ".coe";
    verbose("Generate file: %s ...\n", coeFileName.c_str());
    FILE*   coeFilePtr = fopen(coeFileName.c_str(), "w");
    fprintf(coeFilePtr, "memory_initialization_radix = 16;\n");
    fprintf(coeFilePtr, "memory_initialization_vector =\n");
    for(uint32_t i = 0; i <= this->binary.code_buf_size; i += 4) {
        fprintf(coeFilePtr, "%8.8x\n", *(uint32_t*)(this->binary.code_buf + i));
    }
    fclose(coeFilePtr);
    coeFilePtr = nullptr;
}



