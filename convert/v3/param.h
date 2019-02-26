//
//  param.hpp
//  convert
//
//  Created by 戴植锐 on 2019/2/6.
//  Copyright © 2019 Zhirui Dai. All rights reserved.
//

#ifndef param_h
#define param_h

#include <iostream>
#include <iomanip>
#include <getopt.h>
#include <unistd.h>
#include <cstdlib>
#include <string>

#include "Chip.h"

typedef struct {
    CHIP_T      chip_t;
    std::string input_fileName;
    std::string output_fileName;
    uint32_t    targetAddress;
    bool        shouldCheckTargetAddress;
    uint32_t    dataAddress;
    bool        shouldCheckDataAddress;
    uint32_t    p1_stack_size;
    bool        convert;
    bool        extractSections;
    bool        rootMode;
    bool        verbose;
    bool        silentElfReader;
} param_t;

std::ostream& operator<< (std::ostream& os, param_t value);
void printUsage();
param_t processArgs(int argc, char* argv[]);

#endif /* param_h */
