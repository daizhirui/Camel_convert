//
//  myDebug.h
//  elf_reader
//
//  Created by 戴植锐 on 2019/2/1.
//  Copyright © 2019 Zhirui Dai. All rights reserved.
//

#ifndef myDebug_hpp
#define myDebug_hpp

#include <unistd.h>

extern bool __verbose__;
extern char myDebugBuf[100];

#define WHITE_TEXT  "\x1b[0m"
#define GREEN_TEXT  "\x1b[32m"
#define RED_TEXT    "\x1b[31m"

#define verbose(color, A, ...)      if (__verbose__) { \
                                        sprintf(myDebugBuf, A, ##__VA_ARGS__);\
                                        printf("%s%s\x1b[0m", color, myDebugBuf);\
                                    }
#undef assert
#define assert(A, B, ...)   if(!(A)) { \
                                sprintf(myDebugBuf, B, ##__VA_ARGS__); \
                                fprintf(stderr, "File %s, Line %d:\n\x1b[31m%s\x1b[0m", __FILE__, __LINE__, myDebugBuf); \
                                exit(EXIT_FAILURE); \
                            }

#define warn(A, B, ...)     if(!(A)) { \
                                sprintf(myDebugBuf, B, ##__VA_ARGS__); \
                                printf("\x1b[31m%s\x1b[0m", myDebugBuf); \
                            }


#endif /* myDebug_h */
