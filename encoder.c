#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 定義符號結構來儲存字元資料
typedef struct {
    char symbol[16];       // UTF-8 符號字串（支援多字節符號）
    int count;             // 出現次數
    double probability;    // 出現機率
    char codeword[16];     // 編碼碼詞
} Symbol;

// 總共支援 ASCII 和 UTF-8 字元，最大值設為 1024 個符號
#define TOTAL_SYMBOLS 1024
Symbol symbols[TOTAL_SYMBOLS] = {0};
int symbol_count = 0;

// 函數宣告
void calculate_frequency(FILE *input);
void generate_codewords(Symbol symbols[], int symbol_count);
void write_codebook(const char *codebook_filename);
void encode_to_binary(const char *input_filename, const char *encoded_filename);
int compare(const void *a, const void *b);
int is_valid_utf8_char(const char *utf8_char, int len);
void convert_to_displayable_format(char *display, const char *utf8_char);

// 主函數，執行編碼
int main(int argc, char** argv) {
    if (argc != 4) {
        printf("Usage: %s <input.txt> <codebook.csv> <encoded.bin>\n", argv[0]);
        return 1;
    }

    char *input_filename = argv[1];      // 讀取的輸入檔名
    char *codebook_filename = argv[2];   // 生成的碼書檔名
    char *encoded_filename = argv[3];    // 輸出的編碼檔名

    // 讀取輸入檔案並計算字元頻率
    FILE *input_file = fopen(input_filename, "rb");
    if (!input_file) {
        printf("Error: Could not open input file %s.\n", input_filename);
        return -1;
    }

    // 計算每個符號的出現次數與頻率
    calculate_frequency(input_file);
    fclose(input_file);

    // 將符號按出現次數升序排列
    qsort(symbols, symbol_count, sizeof(Symbol), compare);

    // 生成簡單的二進位碼詞
    generate_codewords(symbols, symbol_count);

    // 將碼書輸出到 codebook.csv
    write_codebook(codebook_filename);

    // 將輸入文字轉換成二進位編碼，並儲存到 encoded.bin
    encode_to_binary(input_filename, encoded_filename);

    printf("Encoding completed successfully.\n");
    return 0;
}

void calculate_frequency(FILE *input) {
    int total_count = 0;
    char utf8_char[16];  // 暫存 UTF-8 符號
    int c;

    while ((c = fgetc(input)) != EOF) {
        int len = 0;

        // 處理單字節 ASCII 符號 (0x00-0x7F)
        if ((c & 0x80) == 0) {
            utf8_char[len++] = c;
        } else if ((c & 0xE0) == 0xC0) {  // 2 字節 UTF-8
            utf8_char[len++] = c;
            utf8_char[len++] = fgetc(input);
        } else if ((c & 0xF0) == 0xE0) {  // 3 字節 UTF-8
            utf8_char[len++] = c;
            utf8_char[len++] = fgetc(input);
            utf8_char[len++] = fgetc(input);
        } else if ((c & 0xF8) == 0xF0) {  // 4 字節 UTF-8
            utf8_char[len++] = c;
            utf8_char[len++] = fgetc(input);
            utf8_char[len++] = fgetc(input);
            utf8_char[len++] = fgetc(input);
        }
        utf8_char[len] = '\0';  // 字串結尾

        // 特殊字元轉換為易讀格式
        if (strcmp(utf8_char, "\n") == 0) strcpy(utf8_char, "\\n");
        else if (strcmp(utf8_char, "\t") == 0) strcpy(utf8_char, "\\t");
        else if (strcmp(utf8_char, "\r") == 0) strcpy(utf8_char, "\\r");

        // 查找是否已存在此符號
        int found = 0;
        for (int i = 0; i < symbol_count; i++) {
            if (strcmp(symbols[i].symbol, utf8_char) == 0) {
                symbols[i].count++;
                found = 1;
                break;
            }
        }

        // 若未找到，則新增此符號
        if (!found && symbol_count < TOTAL_SYMBOLS) {
            strcpy(symbols[symbol_count].symbol, utf8_char);
            symbols[symbol_count].count = 1;
            symbol_count++;
        }

        total_count++;
    }

    // 計算每個符號的機率
    for (int i = 0; i < symbol_count; i++) {
        symbols[i].probability = (double)symbols[i].count / total_count;
    }
}


// 比較函數，用於 qsort 排序 (根據符號出現次數進行升序排列)
// 比較函數，用於 qsort 排序 (根據符號出現次數升序排列，若次數相同則按 ASCII 值升序排列)
int compare(const void *a, const void *b) {
    Symbol *symbolA = (Symbol *)a;
    Symbol *symbolB = (Symbol *)b;
    
    // 首先按出現次數升序排列
    if (symbolA->count != symbolB->count) {
        return symbolA->count - symbolB->count;
    }
    
    // 如果次數相同，按符號字串的 ASCII 值進行升序排列
    return strcmp(symbolA->symbol, symbolB->symbol);
}


// 生成二進位碼詞，從 "0000000" 開始，依序分配
void generate_codewords(Symbol symbols[], int symbol_count) {
    for (int i = 0; i < symbol_count; i++) {
        int codeword = i; // 使用符號順序生成碼詞，從 0 開始
        int bit_length = 7; // 設定為 7 位元

        for (int j = 0; j < bit_length; j++) {
            symbols[i].codeword[bit_length - 1 - j] = (codeword & 1) ? '1' : '0';
            codeword >>= 1;
        }
        symbols[i].codeword[bit_length] = '\0'; // 碼詞字串結尾加上 '\0'
    }
}

// 將碼書輸出至 CSV 檔案
void write_codebook(const char *codebook_filename) {
    FILE *codebook = fopen(codebook_filename, "w");

    for (int i = 0; i < symbol_count; i++) {
        /*fprintf(codebook, "\"%s\",%d,%.7f,\"%s\"\n",
                symbols[i].symbol, symbols[i].count,
                symbols[i].probability, symbols[i].codeword);*/

        fprintf(codebook, "[%s],%d,%.7f,%s\n",symbols[i].symbol, symbols[i].count,symbols[i].probability, symbols[i].codeword);
    }
    fclose(codebook);
}


void encode_to_binary(const char *input_filename, const char *encoded_filename) {
    FILE *input = fopen(input_filename, "rb");
    FILE *encoded = fopen(encoded_filename, "wb");

    unsigned char buffer = 0;  // 用來暫存要寫入的 byte
    int buffer_len = 0;        // buffer 中目前的位元數量
    char utf8_char[16];        // 暫存讀入的 UTF-8 符號
    int c;

    // 逐個讀取 UTF-8 符號並進行編碼
    while ((c = fgetc(input)) != EOF) {
        int len = 0;
        if ((c & 0x80) == 0) {  // 單字節 (ASCII)
            utf8_char[len++] = c;
        } 
        else if ((c & 0xF0) == 0xE0) {  // 3 字節 UTF-8
            utf8_char[len++] = c;
            utf8_char[len++] = fgetc(input);
            utf8_char[len++] = fgetc(input);
        }
        utf8_char[len] = '\0';

        // 特殊字元轉換為易讀格式
        if (strcmp(utf8_char, "\n") == 0) strcpy(utf8_char, "\\n");
        else if (strcmp(utf8_char, "\t") == 0) strcpy(utf8_char, "\\t");
        else if (strcmp(utf8_char, "\r") == 0) strcpy(utf8_char, "\\r");

        // 查找對應的碼詞
        for (int i = 0; i < symbol_count; i++) {
            if (strcmp(symbols[i].symbol, utf8_char) == 0) {
                int bit_index = 0;  // 追蹤當前處理的 bit 索引

                while (symbols[i].codeword[bit_index] != '\0') {
                    buffer = (buffer << 1) | (symbols[i].codeword[bit_index] - '0');  // 將每個 bit 寫入 buffer
                    buffer_len++;  // 增加當前 buffer 中的位數
                    bit_index++;   // 移動到下個 bit

                    // 當 buffer 滿 8 個 bit 時，寫入一個 byte
                    if (buffer_len == 8) {
                        fwrite(&buffer, 1, 1, encoded);
                        buffer = 0;
                        buffer_len = 0;
                    }
                }
                break;
            }
        }
    }

    // 處理剩餘未滿 8 個 bit 的部分，補零後寫入
    if (buffer_len > 0) {
        buffer <<= (8 - buffer_len);  // 左移補零至 8 位元
        fwrite(&buffer, 1, 1, encoded);
    }

    fclose(input);
    fclose(encoded);
}
