// bom.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <stdio.h>
#include <string.h>

#define D_DEBUG

#define EE      0.000001
#define MAX_LEN     255
#define MAX_LINES   255
#define COLUMNS     5
#define MAX_COMMENTS_LEN    25  
#define MAX_COMPONENT_LEN   255
#define MAX_FOOTPRINT_LEN   25
#define MAX_PREFIX_LEN  25
#define MAX_POSITION_NUM    50
#define DEFAULT_PRECISION   5
#define WIDTH_1             15
#define MARGIN_1            2
#define WIDTH_2             35
#define MARGIN_2            2
#define MAX_RECORDS         500
#define LINES_PER_PAGE      35
#define MAX_FILENAME_LEN    200

enum tType { CAPACITOR, DIODE, TRIODE, INDUCTOR, RESISTOR, OSCILLATOR, LED, POTENTIOMETER, OTHER };
typedef enum tType tType;
typedef struct {
    tType type;
    char comment[MAX_COMMENTS_LEN];    //contains: value, precision
    char prefix[MAX_PREFIX_LEN];
    char footprint[MAX_FOOTPRINT_LEN];
    double value;
    int quantity;
    int positionNum[MAX_POSITION_NUM];
    int precision;
} tSingleData;

tSingleData glbRecords[MAX_RECORDS];
int glbWriteIndex = 0;
int glbReadIndex = 0;
int glbWarning = 0;

/* functions */
static int str2dec(const char str[]);
int handleSingleLine(const char str[]);
tType getType(const char components[], const char comments[]);
double getValue(const char str[]);
int getPrecision(const char str[]);
void getPrefix(char prefix[], const char components[], int len);
int getPositionNum(const char str[], int pNumArray[], int len);
void sortRecords(void);
void swapRecords(int i, int j);
int compareRecords(int i, int j);
int getDecLen(int number);
void outputRecords(FILE *fp);
#ifdef D_DEBUG
void printRecords(void);
#endif

int main(int argc, char *argv[]) {
    FILE *fpSrc = NULL, *fpDst = NULL;
    char dataLine[MAX_LEN];
    int len = 0;
    int lines = 0;
    char filename[MAX_FILENAME_LEN];
    char *ptemp = NULL;
    glbWarning = 0;
    if (argc == 1) {
        do {
            printf("Please input the filename:");
            ptemp = fgets(filename, MAX_FILENAME_LEN, stdin);
            if (ptemp != NULL) {
                ptemp = strchr(filename, '\n');
                if (ptemp != NULL)
                    *ptemp = '\0';
                ptemp = strchr(filename, '\r');
                if (ptemp != NULL)
                    *ptemp = '\0';
            }
        }
        while (strlen(filename) == 0);
    }
    else if (argc == 2) {
        strncpy(filename, argv[1], MAX_FILENAME_LEN);
        filename[MAX_FILENAME_LEN-1] = '\0';
    }
    else if (argc > 3)
    {
        printf("Usage:\n");
        printf("    %s fromFile [toFile]\n", argv[0]);
        return 1;
    }
    if (filename[0] == '"' && filename[strlen(filename)-1] == '"') {
        char fn[MAX_FILENAME_LEN];
        int i;
        for(i=1; i<strlen(filename)-1; i++) {
            fn[i-1] = filename[i];
        }
        fn[i-1] = '\0';
        strcpy(filename, fn);
    }
    fpSrc = fopen(filename, "r");
    if (fpSrc == NULL) {
        printf("%s not found.\n", filename);
        if (strlen(filename) < MAX_FILENAME_LEN-5) {
            strcat(filename, ".csv");
            printf("try %s...", filename);
            fpSrc = fopen(filename, "r");
        }
        if (fpSrc == NULL) {
            printf("Press Enter to exit.\n");
            getchar();
            return 1;
        }
        else {
            printf("dealing with %s.\n", filename);
        }
    }
    while (fgets(dataLine, MAX_LEN, fpSrc)) {
        if (glbWriteIndex >= MAX_RECORDS) {
            glbWarning++;
            printf("Crazy! Too many records! I can deal with the first %d ones.\n \
                    Please contact tandake@gmail.com.\n", MAX_RECORDS);
            break;
        }
        len = strlen(dataLine);
        if (len < COLUMNS) continue; 
        handleSingleLine(dataLine);
#if 0
#ifdef D_DEBUG
        printf("%s", dataLine);
#endif
#endif
    }
    glbWriteIndex--;

#ifdef D_DEBUG
    //printf("The end.\n");
#endif
    fclose(fpSrc);

    sortRecords();
    printf("-----------------------------------------------------\n");
#ifdef D_DEBUG
    printRecords();
    printf("-----------------------------------------------------\n");
#endif
    /* write result to target file */
    if (argc == 2 || argc == 1) {
        ptemp = strrchr(filename, '.');
        if (ptemp == NULL) {
            if (strlen(filename) >= MAX_FILENAME_LEN - sizeof(".txt")) {
                printf("No file generated\n", argv[2]);
                filename[0] = '\0';
            }
            else {
                strcat(filename, ".txt");
            }
        }
        else {
            *ptemp = '\0';
            strcat(filename, ".txt");

        }
    }
    else if (argc == 3) {
        strncpy(filename, argv[2], MAX_FILENAME_LEN);
        filename[MAX_FILENAME_LEN-1] = '\0';
    }
    if (strlen(filename) != 0) {
        fpDst = fopen(filename, "w");
        if (fpDst == NULL) {
            printf("Fail! Can't write to \"%s\".\n", filename);
            return 1;
        }
        outputRecords(fpDst);
        fclose(fpDst);
        printf("file \"%s\" generated.\n", filename);
        printf("-----------------------------------------------------\n");
    }
    if (glbWarning) {
        printf("Warning! Please pull back the scroll bar to check error log.\n");
        printf("警告! 请把滚动条拉到上面查看错误报告.\n");
    }
    printf("Press Enter to exit.\n");
    getchar();
    return 0;
}

/*************************************************************
   analysis single line
   the format should be:
    "Comment","Pattern","Quantity","Components","aa"
*************************************************************/
int handleSingleLine(const char str[]) {
    char *pStart = NULL, *pEnd = NULL;
    char *p[5];
    int i;
    int num = 0;
    pStart = (char *)str;
    p[0] = pStart + 1;
    for (i=1; i<COLUMNS; i++) {
        p[i] = strstr(p[i-1], "\",\"");
        if (p[i] == NULL)
            return i;
        *p[i] = '\0';
        p[i] += 3;
    }
    pEnd = strrchr(p[4], '"');
    *pEnd = '\0';

    /* p[0] ----------- value (1k, 2M, 1u, 3pF ... etc) precision(1%, 5%)
       p[1] ----------- footprint (SOT-23, 0603, DIP ... etc)
       p[2] ----------- quantity
       p[3] ----------- components (R101, R102 ... etc)
       p[4] ----------- remarks (useless) */
    glbRecords[glbWriteIndex].value = getValue(p[0]);
    glbRecords[glbWriteIndex].type = getType(p[3], p[0]);
    glbRecords[glbWriteIndex].quantity = str2dec(p[2]);
    glbRecords[glbWriteIndex].precision = getPrecision(p[0]);
    strncpy(glbRecords[glbWriteIndex].comment, p[0], MAX_COMMENTS_LEN);    //contains: value, precision
    glbRecords[glbWriteIndex].comment[MAX_COMMENTS_LEN-1] = '\0';

    getPrefix(glbRecords[glbWriteIndex].prefix, p[3], MAX_PREFIX_LEN);

    strncpy(glbRecords[glbWriteIndex].footprint, p[1], MAX_FOOTPRINT_LEN);
    glbRecords[glbWriteIndex].footprint[MAX_FOOTPRINT_LEN-1] = '\0';
    num = getPositionNum(p[3], glbRecords[glbWriteIndex].positionNum, MAX_POSITION_NUM);
    if (num != glbRecords[glbWriteIndex].quantity) {
        glbWarning++;
        printf("Warning! \"%s-%s\" 的数量可能不正确，我猜应该是%d个.\n", 
                glbRecords[glbWriteIndex].footprint, glbRecords[glbWriteIndex].comment, num);
    }
    glbWriteIndex++;

#ifdef D_DEBUG
    //printf("%-25s%-25s::%Le%25s\n", p[3], p[0], getValue(p[0]), p[2]);
#endif
#ifdef D_DEBUG
//    printf("%d\n", getType(p[3]));
#endif
    return 0;
}

/* judge the type of the components by prefix
    Rxxx: Resistor;
    Cxxx: Capacitor;
    Dxxx: Diode;
    Lxxx: Inductor;
*/
tType getType(const char components[], const char comments[]) {
    char *p;
    char prefix[MAX_PREFIX_LEN];
    p = (char *)components;
    if (p[1] >= '0' && p[1] <= '9') {
        if (p[0] == 'c' || p[0] == 'C' ) return CAPACITOR;
        if (p[0] == 'v' || p[0] == 'V' ) {
            if (strstr(comments, "9012") || strstr(comments, "9013") 
                    || strstr(comments, "9014") || strstr(comments, "8050") )
                return TRIODE;
            return DIODE;
        }
        if (p[0] == 'r' || p[0] == 'R' ) return RESISTOR;
        if (p[0] == 'l' || p[0] == 'L' ) return INDUCTOR;
        if (p[0] == 'g' || p[0] == 'G' ) return OSCILLATOR;
    }
    getPrefix(prefix, components, MAX_PREFIX_LEN);
    if (strcmp(prefix, "hl") == 0 || strcmp(prefix, "HL") == 0) {
        return LED;
    }
    if (strcmp(prefix, "rp") == 0 || strcmp(prefix, "RP") == 0) {
        return POTENTIOMETER;
    }
    return OTHER;
}

double getValue(const char str[]) {
    double value = 0.0;
    int precision = 0;
    int countingFlag = 0;
    int doneFlag = 0;
    int point = 0;
    int reset = 0;
    int start = 1;      //value start mark
    int power = 0;
    double weight = 0.1;     //0.1 0.01 0.001 ...
    int num;
    char *p = (char *)str;
    while(1) {
        if (*p == '\0') {
            doneFlag = 1;
            break;
        }
        if (doneFlag == 1) {
            /* get value, and the byte followed is not a letter, 
             * so the value should be true */
            if (*p < 'A' || (*p > 'Z' && *p < 'a') || *p > 'z')
                break;
            else
                reset = 1;
        }
        if (start == 1) {       //word started, for value is always started with a number
            if (*p >= '0' && *p <= '9') {
                value = *p - '0';
                countingFlag = 1;
                start = 0;
                p++;
                continue;
            }
            start = 0;
            countingFlag = 0;
        }

        if (countingFlag == 1) {        //counting now
            /* a number ? */
            if (*p >= '0' && *p <= '9') {
                num = *p - '0';
                if (point) {
                    value += num * weight;
                    weight /= 10;
                }
                else {
                    value = value * 10 + num;
                }
            }
            /* not a number ? */
            else if (*p == '.') {           // a point?
                if (point == 1) { //point again? seems not a number, reset
                    reset = 1;
                }
                else {
                    point = 1;
                    p++;
                    continue;
                }
            }
            else if ((*p == 'g' || *p == 'G') && power == 0) {
                value *= 1e9;
                power = 1;
            }
            else if ((*p == 'm' || *p == 'M') && power == 0) {
                value *= 1e6;
                power = 1;
            }
            else if ((*p == 'k' || *p == 'K') && power == 0) {
                value *= 1e3;
                power = 1;
            }
            else if ((*p == 'm' || *p == 'M') && power == 0) {
                value *= 1e-3;
                power = 1;
            }
            else if ((*p == 'u' || *p == 'U' || *p == 'μ') && power == 0) {
                value *= 1e-6;
                power = 1;
            }
            else if ((*p == 'n' || *p == 'N') && power == 0) {
                value *= 1e-9;
                power = 1;
            }
            else if ((*p == 'p' || *p == 'P') && power == 0) {
                value *= 1e-12;
                power = 1;
            }
            else if ((*p == 'h' || *p == 'H') && doneFlag == 0) {
                doneFlag = 1;
            }
            else if ((*p == 'f' || *p == 'F') && doneFlag == 0) {
                doneFlag = 1;
            }
            else if ((*p == 'h' || *p == 'H') && doneFlag == 0) {
                doneFlag = 1;
            }
            else {
                reset = 1;
            }
        }
        else {   //not counting, just find the start of the value
            if (*p == ' ' || *p == ',' || *p == '.')
                start = 1;
        }

        if (reset) {
            value = 0.0;
            countingFlag = 0;
            doneFlag = 0;
            point = 0;
            reset = 0;
            weight = 0.1;
            power = 0;
        }
        p++;
    }
    return value;
}

int getPrecision(const char str[]) {
    int precision = 0;
    int weight = 1;
    char *pEnd = NULL;
    char *p = NULL;
    p = (char *)strchr(str, '%');
    if (p != NULL) {
        while (p > str && (*p >= '0' && *p <= '9')) {
            precision += (*p - '0') * weight;
            weight *= 10;
        }
    }
    return precision >= 100 ? 0: precision;
}

void getPrefix(char prefix[], const char components[], int len) {
    char *p = (char *)components;
    int i;
    i = 0;
    while (1) {
        if (*p == '\0' || (*p >= '0' && *p <= '9'))
            break;
        prefix[i++] = *p++;
        if (i == len - 1)
            break;
    }
    prefix[i] = '\0';
}

int getPositionNum(const char str[], int pNumArray[], int len) {
    char *p = NULL;
    char delimiters[MAX_PREFIX_LEN+2];
    int i;
    char components[MAX_COMPONENT_LEN];
    int warnFlag = 0;
    strcpy(components, str);
    getPrefix(delimiters, components, MAX_PREFIX_LEN);
    strcat(delimiters, ", ");
    i = 0;
    p = strtok(components, delimiters);
    while (p != NULL) {
        if ((pNumArray[i++] = str2dec(p)) == 0) {
            warnFlag = 1;
        }

        p = strtok(NULL, delimiters);
        if (i >= len - 1) break;
    }
    pNumArray[i] = -1;      //set "-1" as the end
    if (warnFlag == 1) {
        glbWarning++;
        printf("Warning! \"%s\"不会处理，请手动修改.\n", str);
    }
    return i==0?1:i;        //return the number of positionNum
}

static int str2dec(const char str[]) {
    int ret = 0;
    char *p = (char *)str;
    while(*p >= '0' && *p <= '9') {
        ret = ret*10 + *p - '0';
        p++;
    }
    return ret;
}

void sortRecords(void) {
    int i,j; 
    int change = 1; 
    for(i=0; i<glbWriteIndex && change; i++) {
        change = 0;
        for(j = 0; j<glbWriteIndex-i; j++) {
            if (compareRecords(j, j+1) > 0) {
                swapRecords(j, j+1);
                change = 1;
            }
        }
    }
}

int compareRecords(int i, int j) {
    int ret = 0;
    ret = strcmp(glbRecords[i].prefix, glbRecords[j].prefix);
    if (ret == 0) {
        ret = strcmp(glbRecords[i].footprint, glbRecords[j].footprint);
    }
    if (ret == 0) {
        if (glbRecords[i].value > glbRecords[j].value) {
            ret = 1;
        }
        else if (glbRecords[i].value < glbRecords[j].value) {
            ret = -1;
        }
    }
    return ret;
}
void swapRecords(int i, int j) {
    tSingleData temp;
    temp = glbRecords[i];
    glbRecords[i] = glbRecords[j];
    glbRecords[j] = temp;
}

#ifdef D_DEBUG
void printRecords(void) {
    int i, j;
    int doneFlag = 1;
    int len_1 = 0;
    int len_2 = 0;
    int detailFlag = 1;         //indicate if only positionNum should be printed
    int firstFlag = 1;          //indicate the first positionNum in each line
    char ftPrint[MAX_FOOTPRINT_LEN];
    int line = 0;
    char lastPrefix[MAX_PREFIX_LEN] = "";
    glbReadIndex = 0;
    while (glbReadIndex <= glbWriteIndex) {
        len_1 = 0;
        len_2 = 0;
        if (line == LINES_PER_PAGE) {
            printf("-----------------------------------------------------\n");
            //strcpy(lastPrefix, glbRecords[glbReadIndex].prefix);
            line = 0;
            continue;
        }
        line++;
        if (strcmp(lastPrefix, glbRecords[glbReadIndex].prefix)) {
            strcpy(lastPrefix, glbRecords[glbReadIndex].prefix);
            if (line != 1) {
                printf("\n");
                continue;
            }
        }
        /* first column : components */
        len_1 += printf("%s", glbRecords[glbReadIndex].prefix);

        firstFlag = 1;
        if (doneFlag == 1) {    //last record done, start a new one
            i = 0;
        }
        for (; i<glbRecords[glbReadIndex].quantity; i++) {
            if (glbRecords[glbReadIndex].positionNum[i] >= 0) {
                if (len_1 + 1 + getDecLen(glbRecords[glbReadIndex].positionNum[i]) > WIDTH_1) {
                    doneFlag = 0;
                    break;
                }
                if (firstFlag == 0) {
                    len_1 += printf("、");
                }
                len_1 += printf("%d", glbRecords[glbReadIndex].positionNum[i]);
                firstFlag = 0;
            }
        }
        if (i == glbRecords[glbReadIndex].quantity)
            doneFlag = 1;
        for (j=len_1; j<WIDTH_1; j++) {
            putchar(' ');
        }
        for (j=0; j<MARGIN_1; j++) {
            putchar(' ');
        }

        if (detailFlag == 1) {
            detailFlag = 0;
            /* second column : comment */
            ftPrint[0] = '\0';
            if (strstr(glbRecords[glbReadIndex].footprint, "0402")) {
                strcpy(ftPrint, "0402");
                len_2 += printf("%s", "贴片");
            }
            else if (strstr(glbRecords[glbReadIndex].footprint, "0603")) {
                strcpy(ftPrint, "0603");
                len_2 += printf("%s", "贴片");
            }
            else if (strstr(glbRecords[glbReadIndex].footprint, "0805")) {
                strcpy(ftPrint, "0805");
                len_2 += printf("%s", "贴片");
            }
            else if (strstr(glbRecords[glbReadIndex].footprint, "1206")) {
                strcpy(ftPrint, "1206");
                len_2 += printf("%s", "贴片");
            }

            switch(glbRecords[glbReadIndex].type) {
                case CAPACITOR:
                    len_2 += printf("%s", "电容");
                    break;
                case DIODE:
                    len_2 += printf("%s", "二极管");
                    break;
                case TRIODE:
                    len_2 += printf("%s", "三极管");
                    break;
                case INDUCTOR:
                    len_2 += printf("%s", "电感");
                    break;
                case RESISTOR:
                    len_2 += printf("%s", "电阻");
                    break;
                case OSCILLATOR:
                    len_2 += printf("%s", "晶体");
                    break;
                case LED:
                    len_2 += printf("%s", "发光二极管");
                    break;
                case POTENTIOMETER:
                    len_2 += printf("%s", "电位器");
                    break;
                case OTHER:
                    break;
            }
            if (strlen(ftPrint) != 0) {
                len_2 += printf("%s-", ftPrint);
            }
			else {
			    len_2 += printf("%s-", glbRecords[glbReadIndex].footprint);
			}
            if (glbRecords[glbReadIndex].type == CAPACITOR) {
                if (glbRecords[glbReadIndex].value == 4.7e-6) {
                    len_2 += printf("%s-", "25V");
                }
                else {
                    len_2 += printf("%s-", "50V");
                }
            }
            len_2 += printf("%s", glbRecords[glbReadIndex].comment);

            for (j=len_2; j<WIDTH_2; j++) {
                putchar(' ');
            }
            for (j=0; j<MARGIN_2; j++) {
                putchar(' ');
            }

            /* third column : quantity */
            printf("%d", glbRecords[glbReadIndex].quantity);
        }

        printf("\n");
        if (doneFlag == 1) {
            glbReadIndex++;
            detailFlag = 1;
        }
    }
}
#endif


void outputRecords(FILE *fp) {
    int i, j;
    int doneFlag = 1;
    int len_1 = 0;
    int len_2 = 0;
    int detailFlag = 1;         //indicate if only positionNum should be printed
    int firstFlag = 1;          //indicate the first positionNum in each line
    char ftPrint[MAX_FOOTPRINT_LEN];
    int line = 0;
    char lastPrefix[MAX_PREFIX_LEN] = "";
    glbReadIndex = 0;
    while (glbReadIndex <= glbWriteIndex) {
        len_1 = 0;
        len_2 = 0;
        if (line == LINES_PER_PAGE) {
            fprintf(fp, "-----------------------------------------------------\n");
            //strcpy(lastPrefix, glbRecords[glbReadIndex].prefix);
            line = 0;
            continue;
        }
        line++;
        if (strcmp(lastPrefix, glbRecords[glbReadIndex].prefix)) {
            strcpy(lastPrefix, glbRecords[glbReadIndex].prefix);
            if (line != 1) {
                fprintf(fp, "\n");
                continue;
            }
        }
        /* first column : components */
        len_1 += fprintf(fp, "%s", glbRecords[glbReadIndex].prefix);

        firstFlag = 1;
        if (doneFlag == 1) {    //last record done, start a new one
            i = 0;
        }
        for (; i<glbRecords[glbReadIndex].quantity; i++) {
            if (glbRecords[glbReadIndex].positionNum[i] >= 0) {
                if (len_1 + 1 + getDecLen(glbRecords[glbReadIndex].positionNum[i]) > WIDTH_1) {
                    doneFlag = 0;
                    break;
                }
                if (firstFlag == 0) {
                    len_1 += fprintf(fp, "、");
                }
                len_1 += fprintf(fp, "%d", glbRecords[glbReadIndex].positionNum[i]);
                firstFlag = 0;
            }
        }
        if (i == glbRecords[glbReadIndex].quantity)
            doneFlag = 1;
        for (j=len_1; j<WIDTH_1; j++) {
            fputc(' ', fp);
        }
        for (j=0; j<MARGIN_1; j++) {
            fputc(' ', fp);
        }

        if (detailFlag == 1) {
            detailFlag = 0;
            /* second column : comment */
            ftPrint[0] = '\0';
            if (strstr(glbRecords[glbReadIndex].footprint, "0402")) {
                strcpy(ftPrint, "0402");
                len_2 += fprintf(fp, "%s", "贴片");
            }
            else if (strstr(glbRecords[glbReadIndex].footprint, "0603")) {
                strcpy(ftPrint, "0603");
                len_2 += fprintf(fp, "%s", "贴片");
            }
            else if (strstr(glbRecords[glbReadIndex].footprint, "0805")) {
                strcpy(ftPrint, "0805");
                len_2 += fprintf(fp, "%s", "贴片");
            }
            else if (strstr(glbRecords[glbReadIndex].footprint, "1206")) {
                strcpy(ftPrint, "1206");
                len_2 += fprintf(fp, "%s", "贴片");
            }

            switch(glbRecords[glbReadIndex].type) {
                case CAPACITOR:
                    len_2 += fprintf(fp, "%s", "电容");
                    break;
                case DIODE:
                    len_2 += fprintf(fp, "%s", "二极管");
                    break;
                case TRIODE:
                    len_2 += fprintf(fp, "%s", "三极管");
                    break;
                case INDUCTOR:
                    len_2 += fprintf(fp, "%s", "电感");
                    break;
                case RESISTOR:
                    len_2 += fprintf(fp, "%s", "电阻");
                    break;
                case OSCILLATOR:
                    len_2 += fprintf(fp, "%s", "晶体");
                    break;
                case LED:
                    len_2 += fprintf(fp, "%s", "发光二极管");
                    break;
                case POTENTIOMETER:
                    len_2 += fprintf(fp, "%s", "电位器");
                    break;
                case OTHER:
                    break;
            }
            if (strlen(ftPrint) != 0) {
                len_2 += fprintf(fp, "%s-", ftPrint);
            }
			else {
			    len_2 += fprintf(fp, "%s-", glbRecords[glbReadIndex].footprint);
			}
            if (glbRecords[glbReadIndex].type == CAPACITOR) {
                if (glbRecords[glbReadIndex].value == 4.7e-6) {
                    len_2 += fprintf(fp, "%s-", "25V");
                }
                else {
                    len_2 += fprintf(fp, "%s-", "50V");
                }
            }
            len_2 += fprintf(fp, "%s", glbRecords[glbReadIndex].comment);

            for (j=len_2; j<WIDTH_2; j++) {
                fputc(' ', fp);
            }
            for (j=0; j<MARGIN_2; j++) {
                fputc(' ', fp);
            }

            /* third column : quantity */
            fprintf(fp, "%d", glbRecords[glbReadIndex].quantity);
        }

        fprintf(fp, "\n");
        if (doneFlag == 1) {
            glbReadIndex++;
            detailFlag = 1;
        }
    }
}

int getDecLen(int number) {
    int ret = 0;
    int result = number;
    while (result > 0) {
        result /= 10;
        ret++;
    }
    return ret;
}
