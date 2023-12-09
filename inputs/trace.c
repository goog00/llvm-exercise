#include <stdint.h>
#include <stdio.h>

extern FILE *_MemoryTraceFP;

/**
 * 一个用于内存追踪的函数。当有内存读取（load）或写入（write）操作时，
 * 该函数会记录下相关的信息，包括地址和值，并将这些信息输出到一个文件
 * 指针 _MemoryTraceFP 所指向的文件中
 * void *Addr: 要追踪的内存地址
 * uint64_t Value:无符号64位整型数据类型,表示加载或存储的值。
 * int ISload: 表示这是一个加载操作还是存储操作
*/
void traceMemory(void *Addr, uint64_t Value, int IsLoad) {
  if (IsLoad) {
    fprintf(_MemoryTraceFP, "[Read] Read value 0x%lx from address %p\n", Value,
            Addr);

  } else {
    fprintf(_MemoryTraceFP, "[Write] Wrote value 0x%lx to address %p\n", Value,
            Addr);
  }
}