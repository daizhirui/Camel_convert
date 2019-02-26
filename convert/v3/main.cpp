//
// Created by 戴植锐 on 2019-01-28.
//

#include <iostream>

#include "../../elf_reader/elf.h"
#include "param.h"
//#include "convert.h"
#include "myDebug.h"
#include "Binary.hpp"

int main(int argc, char* argv[]) {
    param_t param = processArgs(argc, argv);
    if (param.output_fileName.length() < 1) {
        param.output_fileName = param.input_fileName;
    }
    
    verbose(GREEN_TEXT, ">>>>Analyze posted options ...\n");
    if (__verbose__) {
        std::cout << param << std::endl;
    }
    
    verbose(GREEN_TEXT, ">>>>Load and analyse the input file: %s ...\n", param.input_fileName.c_str());
    ELF_T elf = Elf_Reader_loadElf(param.input_fileName.c_str());
    if (__verbose__ && !param.silentElfReader) {
        Elf_Reader_printFileSize(elf);
        Elf_Reader_printFileHeader(elf.fileHeader);
        Elf_Reader_printAllSectionHeaders(elf);
        Elf_Reader_printAllProgramHeaders(elf);
        if (elf.regInfo != nullptr) {
            Elf_Reader_printMIPSRegInfo(elf.regInfo);
        }
    }
    
    Binary binary = Binary(elf);
    binary.chip = param.chip_t;
    binary.convert = param.convert && !param.extractSections;
    binary.rootMode = param.rootMode;
    binary.p1StackPageNum = param.p1_stack_size;
    
    if (param.shouldCheckTargetAddress) {
        assert(binary.targetAddress == param.targetAddress,
               "The target address of %s is not 0x%8.8x but 0x%8.8x!\n",
               param.input_fileName.c_str(), param.targetAddress, binary.targetAddress);
    }
    if (param.shouldCheckDataAddress && binary.hasDataSection()) {
        assert(binary.dataAddress == param.dataAddress,
               "The data address of %s is not 0x%8.8x but 0x%8.8x!\n",
               param.input_fileName.c_str(), param.dataAddress, binary.dataAddress);
    }
    
    if (param.verbose) {
        binary.analyseAddress();
    }
    
    binary.extractSections(param.output_fileName);

    return 0;
}
