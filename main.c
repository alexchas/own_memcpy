#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>

#define SIZE (32*1024*100)                        // будем копировать между буферами 100Мб данных
#define SIZE2 (SIZE + 50)                           // перекрывающие данные

void myMemcpy(uint8_t *pd, const uint8_t *ps, size_t len) {
    uint32_t i;
    for(i = 0; i < len; i++) {                      // копирование данных вручную
        *pd++ = *ps++;                              // копирование данных из источника в целевой буфер
    }
}

void *myAdaptiveMemcpy(void *d, const void *s, size_t len) {
        uint8_t *pd = d;
  const uint8_t *ps = s;
        uint32_t srcCnt, destCnt, newLen, endLen, longLen, *pLongSrc, *pLongDest, longWord1, longWord2, methodSelect;

  if( len <= 32 ) {
    myMemcpy(pd, ps, len);                          // при небольших длинах данных укладывающихся в ширину шины - побайтное копирование в целевой буфер
    return pd;
  }
  // копирование словами данных так как обмен с памятью происходит в словах а не в байтах
  srcCnt = 4 - ( (intptr_t)ps & 0x03 );             // определяем количество байтов в первом слове src и dest
  destCnt = 4 - ( (intptr_t)pd & 0x03 );

  myMemcpy(pd, ps, destCnt);                        // копирование начальных байтов в целевой буфер
  newLen = len - destCnt;                           // количество оставшихся байтов
  longLen = newLen / 16;                            // количество полных слов для копирования
  endLen = newLen & (16-1);                         // количество нескопированных байт в конце
  pLongDest = (uint32_t*) ( pd + destCnt );         // изначальная длина слов в целевом буфере

  if(srcCnt <= destCnt) {
    pLongSrc = (uint32_t*) (ps + srcCnt);           // Переход к началу следующего полного слова в pSrc
  }
  else {                                            // В первом слове все еще остаются исходные байты
    pLongSrc = (uint32_t*) (ps + srcCnt - 4);       // Установим pSrc в начало первого полного слова
  }

  methodSelect = (srcCnt - destCnt) & 0x03;         //  Есть 4 различных метода копирования слов

  if(methodSelect == 0) {
      do{
          *pLongDest++ = *pLongSrc++;
          *pLongDest++ = *pLongSrc++;
          *pLongDest++ = *pLongSrc++;
          *pLongDest++ = *pLongSrc++;
      }
      while ( longLen -= 4 > 0 );
  } else {
      int left = 0, right = 0;
      switch (methodSelect) {
      case 1: left=8;  right=24; break;
      case 2: left=16; right=16; break;
      case 3: left=24; right=8;  break;
      default:
          exit(1);                                  // всё пошло не так
      }

      longWord1 = *pLongSrc++;                      // получить первое слово

      // Копирование слов, созданных объединением 2 смежных слов
      do {
          longWord2 = *pLongSrc++;
          *pLongDest++ = (longWord1 >> right) | (longWord2 << left);
          longWord1 = longWord2;

          longWord2 = *pLongSrc++;
          *pLongDest++ = (longWord1 >> right) | (longWord2 << left);
          longWord1 = longWord2;

          longWord2 = *pLongSrc++;
          *pLongDest++ = ( longWord1 >> right ) | ( longWord2 << left );
          longWord1 = longWord2;

          longWord2 = *pLongSrc++;
          *pLongDest++ = ( longWord1 >> right ) | ( longWord2 << left );
          longWord1 = longWord2;
      }
      while(longLen -= 4 < 0);
  }
  // копирование всех оставшихся байтов
  if(endLen != 0) {
      pd = (uint8_t*) pLongDest;             // конечные байты будут скопированы следующими
      ps += len - endLen;                     // определение где расположены конечные байты источника
      myMemcpy(pd, ps, endLen);          // копирование оставшихся байтов

  }
  return pd;
}

void fill(char *p, size_t s, char f) {          // заполнение области данных символом
    while (s--) {
        *p-- = f;
    }
}


int main(int argc, char *argv[]) {
    struct {
       char *fname;
       void *(*func)(void*, const void*, size_t);
    } tests[] = {
#define TEST(f) #f, f
    { TEST(memcpy) },
    { TEST(myAdaptiveMemcpy) },
    };

    int fromOffset = (argc > 1) ? atoi(argv[1]) : 0;
    int toOffset = (argc > 2) ? atoi(argv[1]) : 0;
    char *fromBlock = malloc(SIZE2);
    char *toBlock = malloc(SIZE2);
    char *dummy_block = malloc(SIZE2);
    char *ref_block = NULL;

    printf("оффсеты %d и %d\n", fromOffset, toOffset);
    for(size_t i=0; i < sizeof(tests)/sizeof(tests[0]); i++) {
        fill(fromBlock, SIZE2, 'D');
        fill(toBlock, SIZE2, '$');
        fill(fromBlock, SIZE2, 'W');
        struct timeval start, end;
        gettimeofday(&start, NULL);
        tests[i].func(toBlock + toOffset, fromBlock + fromOffset, SIZE);
        gettimeofday(&end, NULL);
        if(ref_block == 0) {
            ref_block = malloc(SIZE2);
            memcpy(ref_block, toBlock, SIZE2);
        }
        int diff = memcmp(toBlock, ref_block, SIZE2);
        unsigned long elapsed = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        printf("Копирование %d байтов через %s заняло %lu микросекунд, %d (0 - блоки совпадают)\n", SIZE, tests[i].fname, elapsed, diff);
    }
    free(fromBlock);
    free(toBlock);
    free(dummy_block);
    free(ref_block);
    return 0;
}

// параметры компиляции и линковки
// # gcc -std=c11 -Wall -Wextra -g -O2 main.c
// # ./a.out ; ./a.out 1 2 ; ./a.out 2 1 ; ./a.out 2 2














