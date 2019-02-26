//
//  Binary.cpp
//  convert
//
//  Created by 戴植锐 on 2019/2/10.
//  Copyright © 2019 Zhirui Dai. All rights reserved.
//

#include "Binary.hpp"

Binary::Binary(ELF_T elf)
{
    verbose(GREEN_TEXT, ">>>>Pick necessary section information ...\n");
    this->init();
    // seek for .text .rodata .eh_frame .init_array .data
    for (int i = 0; i < elf.sectionHeaderNum; ++i) {
        Elf32_Shdr* section = elf.sectionsHeaders[i];
        char*       secName = (char*)elf.strTable + section->sh_name;
        // secName ==> secIndex
        SectionIndex    secIndex = ignore;
        if (!strcmp(secName, SectionName[text])) {
            secIndex = text;        // secIndex  = 0
            this->targetAddress = section->sh_addr;
        } else if (!strcmp(secName, SectionName[rodata])) {
            secIndex = rodata;      // secIndex  = 1
        } else if (!strcmp(secName, SectionName[ehFrame])) {
            secIndex = ehFrame;     // secIndex  = 2
        } else if (!strcmp(secName, SectionName[initArray])) {
            secIndex = initArray;   // secIndex  = 3
        } else if (!strcmp(secName, SectionName[data])) {
            secIndex = data;        // secIndex  = 4
            this->dataAddress = section->sh_addr;
        } else if (!strcmp(secName, SectionName[bss])) {
            secIndex = bss;         // secIndex  = 5
            this->bssSectionMemoryBeginAddr = section->sh_addr;
            this->bssSectionMemoryEndAddr = section->sh_addr + section->sh_size;
        } else {
            printf("Ignore section %s\n", secName);
            continue;
        }
        // verbose
        if (__verbose__) {
            printf("\nDetect %s section:\n", secName);
            printf("\tAddress: 0x%08x\n", section->sh_addr);
            printf("\t   Size: %d(0x%x)(bytes)\n", section->sh_size, section->sh_size);
        }
        
        this->sectionOffset[secIndex] = section->sh_offset;
        this->sectionSize[secIndex] = section->sh_size;
        this->sectionFlashAddr[secIndex] = section->sh_addr;
    }
    // entry address may be different from target address
    this->entryAddress = elf.fileHeader->e_entry;
    // GP
    this->globalPtrAddr[0] = elf.regInfo->ri_gp_value;
    // seek for PT_LOAD(flash) and PT_LOAD(memory)
    for (int i = 0; i < elf.programHeaderNum; ++i) {
        Elf32_Phdr* program = elf.programHeaders[i];
        if ((PROGRAM_TYPE_T)program->p_type == PT_LOAD && program->p_paddr == this->dataAddress) {
            this->memoryBufSize = program->p_memsz - this->sectionSize[bss];
            if (this->memoryBufSize == 0) {
                continue;   // no .data section
            }
            this->memoryBuf = (uint8_t*)malloc(this->memoryBufSize * sizeof(uint8_t));
            memset(this->memoryBuf, 0, this->memoryBufSize);
            memcpy(this->memoryBuf, elf.buf + program->p_offset, this->memoryBufSize);
            
        } else if ((PROGRAM_TYPE_T)program->p_type == PT_LOAD && program->p_paddr == this->targetAddress) {
            this->flashBufSize = program->p_memsz;
            assert(this->flashBufSize != 0, "Size of PT_LOAD for FLASH is 0!\n");
            this->flashBuf = (uint8_t*)malloc(this->flashBufSize * sizeof(uint8_t));
            memset(this->flashBuf, 0, this->flashBufSize);
            memcpy(this->flashBuf, elf.buf + program->p_offset, this->flashBufSize);
        }
    }
    
    assert(this->flashBuf != nullptr, "PT_LOAD Program Header for FLASH doesn't exist!\n");
}

void Binary::init()
{
    this->chip = CHIP_M2;
    this->convert = false;
    this->rootMode = false;
    
    for (int i = 0; i < 6; ++i) {
        this->sectionOffset[i] = 0;
        this->sectionSize[i] = 0;
        this->sectionFlashAddr[i] = FLASH_ADDRESS_MIN;
    }
    
    this->bssSectionMemoryBeginAddr = this->bssSectionMemoryEndAddr = SRAM_ADDRESS_MIN;
    
    this->flashBuf = this->memoryBuf = nullptr;
    this->flashBufSize = this->memoryBufSize = 0;
    
    this->entryAddress = this->targetAddress = FLASH_ADDRESS_MIN;
    this->dataAddress = SRAM_ADDRESS_MIN;
    this->p1StackPageNum = 1;
    
    for (int i = 0; i < CORE_NUM_MAX; ++i) {
        this->globalPtrAddr[i] = SRAM_ADDRESS_MIN;
        this->stackPtrAddr[i] = SRAM_ADDRESS_MIN;
        this->mainAddress[i] = FLASH_ADDRESS_MIN;
        this->isrAddress[i] = FLASH_ADDRESS_MIN;
        this->interruptAddress[i] = FLASH_ADDRESS_MIN;
    }
}

Binary::~Binary()
{
    if (this->flashBuf != nullptr) {
        free(this->flashBuf);
    }
    if (this->memoryBuf != nullptr) {
        free(this->memoryBuf);
    }
}

void Binary::extractSections(std::string baseFileName)
{
    verbose(GREEN_TEXT, ">>>>Extract and save sections ...\n");
    // original file
    verbose(GREEN_TEXT, "  >>Save PT_LOAD sections ...\n");
    std::string fileName = baseFileName + ".PT_LOAD_flash.txt";
    verbose(WHITE_TEXT, "Extract PT_LOAD(flash) program section, save it as %s ...\n", fileName.c_str());
    FILE* file = fopen(fileName.c_str(), "w");
    assert(file != nullptr, "Fail to open file %s\n", fileName.c_str());
    for (int j = 0; j < this->flashBufSize; j += 4) {
        fprintf(file, "%8.8x\n", *(uint32_t*)(this->flashBuf + j));
    }
    fclose(file);
    
    fileName = baseFileName + ".PT_LOAD_mem.txt";
    verbose(WHITE_TEXT, "Extract PT_LOAD(memory) program section, save it as %s ...\n", fileName.c_str());
    file = fopen(fileName.c_str(), "w");
    assert(file != nullptr, "Fail to open file %s\n", fileName.c_str());
    for (int j = 0; j < this->memoryBufSize; j += 4) {
        fprintf(file, "%8.8x\n", *(uint32_t*)(this->memoryBuf + j));
    }
    fclose(file);
    file = nullptr;
    
    if (convert) {
        verbose(GREEN_TEXT, "  >>Update some instruction code ...\n");
        this->updateCode();
    }
    
    // calculate binary size
    verbose(GREEN_TEXT, "  >>Determine binary size ...\n");
    size_t binSize = this->flashBufSize;
    verbose(WHITE_TEXT, "\tCurrent binary size = %ld(0x%lx)byte(s) ==> %.2f%% Flash Volume\n", binSize, binSize, 100.0 * binSize / FLASH_VOLUME);
    if (this->convert && this->memoryBufSize > 0) {
        binSize += this->memoryBufSize;
        verbose(GREEN_TEXT, "\tIncrease binary size to %ld(0x%lx)byte(s) ==> %.2f%% Flash Volume\n", binSize, binSize, 100.0 * binSize / FLASH_VOLUME);
    }
    if (int remnant = binSize % 4) {
        binSize += (4 - remnant);
        warn(remnant == 0, "\tAppend %d '0' to the end of the binary!\n", 4 - remnant);
    }
    // init binary buf
    uint8_t*    binBuf = (uint8_t*)malloc(binSize * sizeof(uint8_t));
    assert(binBuf != nullptr, "Fail to malloc!\n");
    memset(binBuf, 0, binSize);
    memcpy(binBuf, this->flashBuf, this->flashBufSize);
    if (convert && this->memoryBufSize > 0) {
        memcpy(binBuf + this->sectionFlashAddr[data] - this->targetAddress, this->memoryBuf, this->memoryBufSize);
    }
    
    verbose(GREEN_TEXT, ">>>>Generate binary ...\n");
    std::string binFileName = baseFileName + ".bin";
    verbose(WHITE_TEXT, "Generate file: %s ...\n", binFileName.c_str());
    FILE*   binFile = fopen(binFileName.c_str(), "wb");
    assert(binFile != nullptr, "Fail to open file %s\n", binFileName.c_str());
    fwrite(binBuf, sizeof(uint8_t), binSize, binFile);
    fclose(binFile);
    
    binFileName = baseFileName + ".txt";
    verbose(WHITE_TEXT, "Generate file: %s ...\n", binFileName.c_str());
    binFile = fopen(binFileName.c_str(), "w");
    assert(binFile != nullptr, "Fail to open file %s\n", binFileName.c_str());
    for (int j = 0; j < binSize; j += 4) {
        fprintf(binFile, "%8.8x\n", *(uint32_t*)(binBuf + j));
    }
    fclose(binFile);
    binFile = nullptr;
    
    std::string coeFileName = baseFileName + ".coe";
    verbose(WHITE_TEXT, "Generate file: %s ...\n", coeFileName.c_str());
    FILE*   coeFilePtr = fopen(coeFileName.c_str(), "w");
    fprintf(coeFilePtr, "memory_initialization_radix = 16;\n");
    fprintf(coeFilePtr, "memory_initialization_vector =\n");
    for(uint32_t i = 0; i < binSize; i += 4) {
        fprintf(coeFilePtr, "%8.8x\n", *(uint32_t*)(binBuf + i));
    }
    fclose(coeFilePtr);
    coeFilePtr = nullptr;
}

void Binary::updateLuiOriCode(uint8_t *buf, const int *luiOffsets, const int *oriOffsets, uint32_t *values, int count,
                              const char* msg, const char **valueNames)
{
    assert(buf != nullptr, "buf should not be nullptr!\n");
    assert(luiOffsets != nullptr, "luiOffsets should not be nullptr!\n");
    assert(oriOffsets != nullptr, "oriOffsets should not be nullptr!\n");
    assert(values != nullptr, "values should not be nullptr!\n");
    assert(valueNames != nullptr, "valueNames should not be nullptr!\n");
    for (int i = 0; i < count; ++i) {
        update_lui_code(buf, luiOffsets[i], values[i]);
        update_ori_code(buf, oriOffsets[i], values[i]);
        verbose(WHITE_TEXT, msg, valueNames[i], values[i]);
    }
}

// | target address ... | entry address ... | ... |
// In general, targetAddress == entryAddress!
void Binary::updateCode() {
    uint8_t* textBuf = this->flashBuf;
    assert(textBuf != nullptr, ".text doesn't exist!\n");
    int entryOffset = this->entryAddress - this->targetAddress;
    // GP
    for (int i = 0; i < CORE_NUM[this->chip]; ++i) {
        this->globalPtrAddr[i] = this->globalPtrAddr[0];   // all cores share the same global area!
    }
    verbose(GREEN_TEXT, "\tUpdate $gp ...\n");
    const int* gp_lui_offset = GP_LUI_OFFSET[this->chip];
    const int* gp_ori_offset = GP_ORI_OFFSET[this->chip];
    for (int i = 0; i < CORE_NUM[this->chip]; i++) {
        update_lui_code(textBuf + entryOffset, gp_lui_offset[i], this->globalPtrAddr[i]);
        update_ori_code(textBuf + entryOffset, gp_ori_offset[i], this->globalPtrAddr[i]);
        verbose(WHITE_TEXT, "\t\t$gp of p%d = 0x%8.8x\n", i, this->globalPtrAddr[i]);
    }
    // SP
    // p0 sp
    uint32_t offset = MASTER_CORE_GPSP_SIZE + (SLAVE_CORE_GPSP_SIZE + SLAVE_FUNC_EXCHANGE_SIZE) * (CORE_NUM[chip] - 1);
    this->stackPtrAddr[0] = SRAM_ADDRESS_MAX - offset;
    uint32_t globalAreaSize = this->sectionSize[data] + this->sectionSize[bss];
    // p1 ~ px
    for (int i = 1; i < CORE_NUM[this->chip]; i++) {
        this->stackPtrAddr[i] = SRAM_ADDRESS_MIN + globalAreaSize + i * this->p1StackPageNum * STACK_PAGE_VOLUME;
    }
    verbose(GREEN_TEXT, "\tUpdate $sp ...\n");
    const int* sp_lui_offset = SP_LUI_OFFSET[this->chip];
    const int* sp_ori_offset = SP_ORI_OFFSET[this->chip];
    for (int i = 0; i < CORE_NUM[this->chip]; i++) {
        update_lui_code(textBuf + entryOffset, sp_lui_offset[i], this->stackPtrAddr[i]);
        update_ori_code(textBuf + entryOffset, sp_ori_offset[i], this->stackPtrAddr[i]);
        verbose(WHITE_TEXT, "\t\t$sp of p%d = 0x%8.8x\n", i, this->stackPtrAddr[i]);
    }
    // .data
    verbose(GREEN_TEXT, "\tUpdate data initialization address ...\n");
    this->sectionFlashAddr[data] = this->flashBufSize + this->targetAddress;
    const int* data_lui_offset = DATA_LUI_OFFSET[this->chip];
    const int* data_ori_offset = DATA_ORI_OFFSET[this->chip];
    uint32_t data_init_addr[3] = {
        this->sectionFlashAddr[data],
        this->sectionFlashAddr[data] + this->memoryBufSize,
        this->dataAddress
    };
    const char* data_addr_name[3] = {".data_flash_begin_addr", ".data_flash_end_addr", ".data_memory_addr"};
    updateLuiOriCode(textBuf + entryOffset, data_lui_offset, data_ori_offset, data_init_addr, 3, "\t\t%s = 0x%8.8x\n", data_addr_name);
    // .bss
    verbose(GREEN_TEXT, "\tUpdate .bss initialization address ...\n");
    const int* bss_lui_offset = BSS_LUI_OFFSET[this->chip];
    const int* bss_ori_offset = BSS_ORI_OFFSET[this->chip];
    uint32_t bss_init_addr[2] = {
        this->bssSectionMemoryBeginAddr,
        this->bssSectionMemoryEndAddr
    };
    const char* bss_addr_name[2] = {".bss_begin_addr", ".bss_end_addr"};
    updateLuiOriCode(textBuf + entryOffset, bss_lui_offset, bss_ori_offset, bss_init_addr, 2, "\t\t%s = 0x%8.8x\n", bss_addr_name);
    // .init_array
    verbose(GREEN_TEXT, "\tUpdate .init_array initialization address ...\n");
    if (this->sectionSize[initArray] != 0) {
        const int* init_lui_offset = INIT_LUI_OFFSSET[this->chip];
        const int* init_ori_offset = INIT_ORI_OFFSET[this->chip];
        uint32_t init_array_value[2] = {
            this->sectionFlashAddr[initArray],
            this->sectionSize[initArray]
        };
        const char* init_array_name[2] = {".init_array_addr", ".init_array_size"};
        updateLuiOriCode(textBuf + entryOffset, init_lui_offset, init_ori_offset, init_array_value, 2, "\t\t%s = 0x%8.8x\n", init_array_name);
    }
    // check targetAddress and entryAddress
    bool compareWithThreshold = false;
    warn(entryOffset == 0, "Entry point address is adjusted from 0x%x to 0x%x!\n",
         this->targetAddress, this->entryAddress);
    warn(this->entryAddress == DEFAULT_TARGET_ADDRESS, "Entry point address is not the default value 0x%x! Now it is 0x%x\n",
         DEFAULT_TARGET_ADDRESS, this->entryAddress);
    if (entryOffset != 0 || this->entryAddress != DEFAULT_TARGET_ADDRESS) {
        compareWithThreshold = true;
    }
    if (compareWithThreshold && !this->rootMode) {
        assert(this->targetAddress >= ADJUSTED_TARGET_ADDRESS_THRESHOLD[this->chip],
               "Target address is 0x%x, it should be 0x%x or greater than 0x%x.\n",
               this->targetAddress, DEFAULT_TARGET_ADDRESS, ADJUSTED_TARGET_ADDRESS_THRESHOLD[chip]);
        // insert loader
        int offset = this->targetAddress - DEFAULT_TARGET_ADDRESS;
        uint8_t* newBuf = (uint8_t*)malloc((this->flashBufSize + offset) * sizeof(uint8_t));
        assert(newBuf != nullptr, "Fail to malloc!\n");
        memset(newBuf, 0, this->flashBufSize * sizeof(uint8_t));   // init
        memcpy(newBuf, textBuf + entryOffset, LOADER_SIZE[chip] * sizeof(uint32_t));  // copy loader
        if (__verbose__) {
            printf("Insert user bootloader! Code:\n");
            for (uint32_t i = 0; i < LOADER_SIZE[chip] * sizeof(uint32_t); i += sizeof(uint32_t)) {
                uint32_t address = DEFAULT_TARGET_ADDRESS + i;
                uint32_t code = *(uint32_t*)(textBuf + i);
                printf("\t0x%08x: %08x\t", address, code);
                if(code == 0) {
                    printf("nop\n");
                } else {
                    printf("jump to 0x%08x\n", jTarget(code, address));
                }
            }
        }
        memcpy(newBuf + offset, textBuf, this->flashBufSize);  // copy original part
        free(textBuf);
        textBuf = this->flashBuf = newBuf;
        this->targetAddress = DEFAULT_TARGET_ADDRESS;
        this->sectionSize[text] += offset;
        this->flashBufSize += offset;
    }
}

void Binary::analyseAddress()
{
    uint32_t address = 0;
    uint32_t code = 0;
    uint8_t* textBuf = this->flashBuf;
    // calculate a set of addresses
    for (int i = 0; i < CORE_NUM[chip]; i++) {
        address = entryAddress + JALOS_ADDRESS_OFFSET[chip][i];
        code = *(uint32_t*)(textBuf + address - this->targetAddress);
        this->mainAddress[i] = jalTarget(code, address);
        
        address = entryAddress + JISR_ADDRESS_OFFSET[this->chip][i];
        code = *(uint32_t*)(textBuf + address - this->targetAddress);
        this->isrAddress[i] = jTarget(code, address);
        
        address = this->isrAddress[i] + JAL_INTERRUPT_ADDRESS_OFFSET[this->chip][i];
        code = *(uint32_t*)(textBuf + address - this->targetAddress);
        this->interruptAddress[i] = jalTarget(code, address);
    }
    verbose(GREEN_TEXT, ">>>>Important Address:\n");
    verbose(WHITE_TEXT, "\tAddress of entry point:  0x%08x\n", entryAddress);
    verbose(WHITE_TEXT, "\t        Target Address:  0x%08x\n", this->targetAddress);
    for (int i = 0; i < CORE_NUM[this->chip]; i++) {
        verbose(WHITE_TEXT, "\t--------------------------------------\n");
        verbose(WHITE_TEXT, "\t       p%d main Address:  0x%08x\n", i, this->mainAddress[i]);
        verbose(WHITE_TEXT, "\t        p%d isr Address:  0x%08x\n", i, this->isrAddress[i]);
        verbose(WHITE_TEXT, "\t  p%d interrupt Address:  0x%08x\n", i, this->interruptAddress[i]);
    }
}
