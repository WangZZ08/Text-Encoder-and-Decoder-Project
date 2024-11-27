#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char symbol[32];        // 增加符號字串的大小以支援更多字符
    char code[32];          // 對應編碼
} CodebookEntry;

#define MAX_ENTRIES 1024    // 最多支援 1024 個符號
#define MAX_BITSTREAM_SIZE 262144  // 擴大 bitstream 大小至 262144 位元（32 KB）

CodebookEntry codebook[MAX_ENTRIES];  // 存放 codebook 條目
int codebook_size = 0;                // codebook 條目數量

// 函數宣告
void load_codebook(const char *filename);
void decode_binary(const char *encoded_filename, const char *output_filename);
void print_codebook();  // 用於除錯：顯示所有 codebook 條目
void extract_symbol(char *symbol, char default_symbol);  // 提取符號中的特殊字符

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <output.txt> <codebook.csv> <encoded.bin>\n", argv[0]);
        return 1;
    }

    const char *output_filename = argv[1];
    const char *codebook_filename = argv[2];
    const char *encoded_filename = argv[3];

    // 讀取 codebook
    printf("Loading codebook from %s...\n", codebook_filename);
    load_codebook(codebook_filename);
    //print_codebook();  // 顯示 codebook 內容，確認是否正確讀取

    // 進行解碼並輸出至 output_filename
    printf("Decoding binary file %s...\n", encoded_filename);
    decode_binary(encoded_filename, output_filename);

    printf("Decoding completed. Output written to %s\n", output_filename);
    return 0;
}

// 提取csv檔符號中的特殊字符並進行轉換
void extract_symbol(char *symbol, char default_symbol) {
    char *start = strchr(symbol, '[');  // 查找 '['
    char *end = strchr(symbol, ']');    // 查找 ']'

    if (start && end && start < end) {
        start++;  // 移動到 '[' 後的第一個字符
        size_t length = end - start;  // 計算 `[]` 中的字符長度
        if (length > 0) {
            // 如果 `[]` 中有字符，則進行提取
            strncpy(symbol, start, length);
            symbol[length] = '\0';  // 添加結束符號
        } else {
            // 如果 `[]` 中沒有字符或只有空白，統一設為 default_symbol
            symbol[0] = default_symbol;
            symbol[1] = '\0';
        }
    } else {
        // 如果沒有提取到有效的符號，統一設為 default_symbol
        symbol[0] = default_symbol;
        symbol[1] = '\0';
    }
}

// 顯示所有載入的 codebook 條目
void print_codebook() {
    printf("Codebook loaded with %d entries:\n", codebook_size);
    for (int i = 0; i < codebook_size; i++) {
        printf("Symbol: %s, Code: %s\n", codebook[i].symbol, codebook[i].code);
    }
}

// 解析 CSV 並載入 codebook
void load_codebook(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Failed to open codebook file: %s\n", filename);
        exit(1);
    }

    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';  // 去除行末換行符
        char *fields[10];
        int num_fields = 0;
        char *token = strtok(line, ",");
        while (token) {
            fields[num_fields++] = token;
            token = strtok(NULL, ",");
        }

        if (num_fields >= 4) {
            // 根據欄位數量決定預設符號
            char default_symbol = (num_fields == 4) ? ']' : ',';  // 四欄設定為 `]`，五欄設定為 `,`
            extract_symbol(fields[0], default_symbol);  // 提取並轉換符號
            strncpy(codebook[codebook_size].symbol, fields[0], sizeof(codebook[codebook_size].symbol) - 1);
            strncpy(codebook[codebook_size].code, fields[num_fields - 1], sizeof(codebook[codebook_size].code) - 1);
            codebook[codebook_size].symbol[sizeof(codebook[codebook_size].symbol) - 1] = '\0';
            codebook[codebook_size].code[sizeof(codebook[codebook_size].code) - 1] = '\0';
            codebook_size++;
        }
    }
    fclose(file);
}

// 解碼二進位檔案並輸出結果
void decode_binary(const char *encoded_filename, const char *output_filename) {
    FILE *encoded_file = fopen(encoded_filename, "rb");
    FILE *output_file = fopen(output_filename, "w");
    if (encoded_file == NULL || output_file == NULL) {
        printf("Failed to open files\n");
        return;
    }

    unsigned char byte;
    char bitstream[MAX_BITSTREAM_SIZE] = {0};
    int bit_index = 0;

    // 開始逐位元組讀取並轉換為二進制格式
    while (fread(&byte, 1, 1, encoded_file) == 1) {
        for (int i = 7; i >= 0; i--) {
            bitstream[bit_index++] = (byte & (1 << i)) ? '1' : '0';
        }
    }
    fclose(encoded_file);

    // 直接抓取每 7 個位元來比對
    for (int i = 0; i + 7 <= bit_index; i += 7) {
        int value = 0;
        for (int k = 0; k < 7; k++) {
            value = (value << 1) | (bitstream[i + k] - '0');
        }

        // 忽略最後的全部為 0 的片段
        if (value == 0 && i + 7 == bit_index) {
            break;  // 跳過最後的全部 0 的片段
        }

        // 比對整數值與 codebook 中的二進制值是否相符
        for (int j = 0; j < codebook_size; j++) {
            int code_value = 0;
            for (int m = 0; m < 7; m++) {
                code_value = (code_value << 1) | (codebook[j].code[m] - '0');
            }

            if (value == code_value) {
                
                // 判斷是否為特殊符號並執行相應操作
                if (strcmp(codebook[j].symbol, "\\r") == 0) {
                    // 遇到回車符號 '\r'，直接跳過後續的 '\n'，不執行換行操作
                    // 假設 bitstream 是已經被分段的符號流，跳過後續的 '\n'
                    if (bitstream[i + 1] == '\n') {
                        // 如果下一個符號是 '\n'，跳過這一位
                        i++;  // 跳過 '\n'
                    }
                    // 不輸出任何東西，直接跳過回車處理
                } else if (strcmp(codebook[j].symbol, "\\n") == 0) {
                    // 如果是單獨的換行符號，則輸出換行
                    fprintf(output_file, "\n");
                }


                else if (strcmp(codebook[j].symbol, "\\t") == 0) {
                    fprintf(output_file, "\t");  // 執行 Tab 操作
                } 
                
                else {
                    fprintf(output_file, "%s", codebook[j].symbol);  // 輸出一般符號
                }
                break;
            }
        }
    }

    fclose(output_file);
}