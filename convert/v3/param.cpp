//
//  param.cpp
//  convert
//
//  Created by 戴植锐 on 2019/2/6.
//  Copyright © 2019 Zhirui Dai. All rights reserved.
//

#include "param.h"
#include "myDebug.h"
#include "Binary.hpp"

std::ostream& operator<< (std::ostream& os, param_t value) {
    
    os  << "Argument parsing result:" << std::endl
    << "                   chip = ";
    
    switch (value.chip_t) {
        case CHIP_M2:
            os << "M2" << std::endl;
            break;
        case CHIP_C2D:
            os << "C2D" << std::endl;
            break;
        default:
            break;
    }
    
    os  << "         input elf file = " << value.input_fileName << std::endl
    << "  output file base name = " << value.output_fileName << std:: endl
    << "          targetAddress = 0x" << std::hex << std::setw(8) << std::setfill('0') << value.targetAddress << std::endl
    << "            dataAddress = 0x" << std::hex << std::setw(8) << std::setfill('0') << value.dataAddress << std::endl;
    os.unsetf(std::ios::hex);
    os << "          p1_stack_size = " << value.p1_stack_size << " page(s)" << std::endl
    << "                convert = " << (value.convert ? "true" : "false") << (value.extractSections ? "(ignored)" : "") << std::endl
    << "       extract sections = " << (value.extractSections ? "true" : "false") << std::endl
    << "              root mode = " << (value.rootMode ? "true" : "false") << std::endl
    << "                verbose = " << (value.verbose ? "true" : "false" ) << std::endl
    << "      silent ELF reader = " << (value.silentElfReader ? "true" : "false" ) << std::endl;

    return os;
}

void printUsage() {
    std::cout << "\nconvert for products designed by Camel Microelectronics, Inc." << std::endl
    << "Version 3.0 by Zhirui Dai" << std::endl
    << "usage: convert --chip <chip_model> [other_commands] <elf_file>" << std::endl
    << "  --chip <value>            The value can be m2, c2d" << std::endl
    << "  --output -o <name>        Specify the name of the binary file. Effective with -m" << std::endl
    << "  --target-address <addr>   Specify the address of the entry. This command is optional," << std::endl
    << "                            just for monitoring potential address mistake" << std::endl
    << "  --data-address <addr>     Specify the address of the data section. This command is optioanl," << std::endl
    << "                            just for monitoring potential address mistake" << std::endl
    << "  --p1-stack-size <value>   Specify the number of stack page, a single page is 512-byte" << std::endl
    << "                            1 page by default" << std::endl
    << "  --make -m                 Convert the elf_file to binary with updating $gp and $sp" << std::endl
    << "                            without this option, convert will not generate a runable binary" << std::endl
    << "  --root-mode -r            Convert the ELF to a binary without target address check" << std::endl
    << "  --extract-sections -n     Extract the .text, .data and .rodata section and save them as files" << std::endl
    << "                            without changing $gp and $sp, --make will be ignored" << std::endl
    << "  --verbose -v              Print debugging information" << std::endl
    << "  --silent-elfreader -s     No printing ELF analysis result" << std::endl
    << "  --help -h                 Print this usage" << std::endl
    << "example:" << std::endl
    << "  convert --chip m2 -m elf_file    Convert the elf file to binary file for m2" << std::endl
    << "  convert --chip c2d -v elf_file   Just print the debugging information about converting" << std::endl
    << "                                   the elf file to binary for c2d" << std::endl;
}

static struct option long_options[] = {
    {"chip", required_argument, nullptr, 'c'},
    {"output", required_argument, nullptr, 'o'},
    {"target-address", required_argument, nullptr, 't'},
    {"data-address", required_argument, nullptr, 'd'},
    {"p1-stack-size", required_argument, nullptr, 'p'},
    {"make", no_argument, nullptr, 'm'},
    {"root-mode", no_argument, nullptr, 'r'},
    {"extract-sections", no_argument, nullptr, 'n'},
    {"verbose", no_argument, nullptr, 'v'},
    {"silent-elfreader", no_argument, nullptr, 's'},
    {"help", no_argument, nullptr, 'h'}
};

param_t processArgs(int argc, char* argv[]) {
    param_t param;
    param.chip_t = CHIP_M2;
    param.input_fileName = "";
    param.output_fileName = std::string("");
    param.targetAddress = Binary::DEFAULT_TARGET_ADDRESS;
    param.shouldCheckTargetAddress = false;
    param.dataAddress = Binary::DEFAULT_DATA_ADDRESS;
    param.shouldCheckDataAddress = false;
    param.p1_stack_size = 1;
    param.convert = false;
    param.extractSections = false;
    param.rootMode = false;
    param.verbose = false;
    param.silentElfReader = false;
    
    bool chip_defined = false;
    
    int option;
    int option_index;
    while ((option = getopt_long(argc, argv, ":c:o:t:d:p:mrnsvh", long_options, &option_index)) != -1) {
        switch (option) {
            case 'c': {
                std::string chip_str = optarg;
                if (chip_str == "m2") {
                    param.chip_t = CHIP_M2;
                } else if (chip_str == "c2d") {
                    param.chip_t = CHIP_C2D;
                } else {
                    fprintf(stderr, "Wrong value for command --chip! Available values: m2, c2d\n");
                    exit(EXIT_FAILURE);
                }
                chip_defined = true;
                break;
            }
                
            case 'o': {
                param.output_fileName = optarg;
                break;
            }
            case 't': {
                param.targetAddress = (uint32_t)strtol(optarg, nullptr, 16);
                param.shouldCheckTargetAddress = true;
                if (param.targetAddress < Binary::FLASH_ADDRESS_MIN || param.targetAddress > Binary::FLASH_ADDRESS_MAX) {
                    fprintf(stderr, "the targetAddress %s is wrong!\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            }
            case 'd': {
                param.dataAddress = (uint32_t)strtol(optarg, nullptr, 16);
                param.shouldCheckDataAddress = true;
                if (param.dataAddress < Binary::SRAM_ADDRESS_MIN || param.dataAddress > Binary::SRAM_ADDRESS_MAX) {
                    fprintf(stderr, "the dataAddress %s is wrong!\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            }
            case 'p': {
                char* end;
                param.p1_stack_size = (uint32_t)std::strtol(optarg, &end, 10);
                if (param.p1_stack_size < Binary::STACK_PAGE_SIZE_MIN || param.p1_stack_size > Binary::STACK_PAGE_SIZE_MAX) {
                    fprintf(stderr, "The value for %s is too large. 1<= p1_stack_size <=14\n",
                            long_options[option_index].name);
                    exit(EXIT_FAILURE);
                }
                break;
            }
            case 'm': {
                param.convert = true;
                break;
            }
            case 'n': {
                param.extractSections = true;
                break;
            }
            case 'r': {
                param.rootMode = true;
                break;
            }
            case 'v': {
                param.verbose = true;
                __verbose__ = true;
                break;
            }
            case 's': {
                param.silentElfReader = true;
                break;
            }
            case 'h': {
                printUsage();
                if (argc > 2) {
                    fprintf(stderr, "Nothing will be done with '%s'.\n", long_options[option_index].name);
                    exit(EXIT_FAILURE);
                }
                exit(EXIT_SUCCESS);
            }
            case '?': {
                fprintf(stderr, "Unsupported option '%s'\n", long_options[option_index].name);
                printUsage();
                exit(EXIT_FAILURE);
            }
            case ':':
                fprintf(stderr, "option '%s' requires a parameter\n", long_options[option_index].name);
                printUsage();
                exit(EXIT_FAILURE);
            default: {
                fprintf(stderr, "Unknown error in processArgs\n");
                printUsage();
                exit(EXIT_FAILURE);
            }
        }
    } // end of while
    
    if (!chip_defined) {
        fprintf(stderr, "--chip <value> is required!\n");
        printUsage();
        exit(EXIT_FAILURE);
    }
    
    if (argc - optind == 1) {
        param.input_fileName = argv[optind];
    } else if (argc - optind > 1) {
        fprintf(stderr, "More than one file is provided!\n");
        exit(EXIT_FAILURE);
    } else if (argc - optind <= 0) {
        fprintf(stderr, "<elf_file> is required!\n");
        exit(EXIT_FAILURE);
    }
    
    return param;
}
