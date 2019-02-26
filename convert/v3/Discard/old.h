//
//  old.h
//  elf_reader
//
//  Created by 戴植锐 on 2019/2/15.
//  Copyright © 2019 Zhirui Dai. All rights reserved.
//

#ifndef old_h
#define old_h

//                this->sectionBuf[secIndex] = (uint8_t*)malloc(section->sh_size * sizeof(uint8_t));
//                assert(this->sectionBuf[secIndex] != nullptr, "Fail to malloc!\n");
//                memcpy(this->sectionBuf[secIndex], elf.buf + section->sh_offset, section->sh_size);

//                this->sectionBuf[secIndex] = (uint8_t*)malloc(section->sh_size * sizeof(uint8_t));
//                memcpy(this->sectionBuf[secIndex], elf.buf + section->sh_offset, section->sh_size);

// extract .text .rodata .eh_frame .init_array .data
//    for (uint64_t i = text; i < data; ++i) {
//        if (this->sectionBuf[i] != nullptr) {
//            std::string fileName = baseFileName + SectionName[i] + ".bin";
//            verbose("Extract %s section, save it as %s ...\n", SectionName[i], fileName.c_str());
//            FILE* file = fopen(fileName.c_str(), "wb");
//            assert(file != nullptr, "Fail to open file %s\n", fileName.c_str());
//            fwrite(this->sectionBuf[i], sizeof(uint8_t), this->sectionSize[i], file);
//            fclose(file);
//
//            fileName = baseFileName + SectionName[i] + ".txt";
//            verbose("Generate file: %s ...\n", fileName.c_str());
//            file = fopen(fileName.c_str(), "w");
//            assert(file != nullptr, "Fail to open file %s\n", fileName.c_str());
//            for (int j = 0; j < this->sectionSize[i]; j += 4) {
//                fprintf(file, "%8.8x\n", *(uint32_t*)(this->sectionBuf[i] + j));
//            }
//            fclose(file);
//            file = nullptr;
//        }
//    }

//
//    CamelConvert    convert = CamelConvert(elf,
//                                           param.chip_t,
//                                           param.targetAddress,
//                                           param.shouldCheckTargetAddress,
//                                           param.dataAddress,
//                                           param.shouldCheckDataAddress);
//    convert.extractSections = param.extractSections;
//    convert.rootMode = param.rootMode;
//    convert.outputFileName = param.output_fileName;
//
//    if (param.verbose || param.convert || param.extractSections) {
//        verbose("\n[50%%]Convert ELF to Binary ...\n");
//        convert.convertToBin();
//    }
//
//    if (param.convert && !param.extractSections) {
//        verbose("\n[80%%]Update the instruction code ...\n");
//        convert.updateCode();
//        verbose("\n[90%%]Generate binary with basename '%s'\n", param.output_fileName.c_str());
//        convert.saveBin();
//    }
//
//    verbose("\n[100%%]Completed!\n\n");


#endif /* old_h */
