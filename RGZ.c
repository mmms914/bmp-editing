#include <stdio.h>
#include <stdint.h>
#include <malloc.h>
#include <math.h>

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} RGB_pixel;

typedef struct {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    int32_t offset_bits;
} BMP_file_header;

typedef struct {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bit_count;
    uint32_t compression;
    uint32_t size_image;
    uint32_t x_per_meter;
    uint32_t y_per_meter;
    uint32_t clr_used;
    uint32_t clr_important;

} BMP_file_info;


typedef struct {
    BMP_file_header header;
    BMP_file_info info;
    RGB_pixel** data;
} BMP_file;


void read_bmp_header(FILE* file, BMP_file_header* header, BMP_file_info* info) {
    fread(&header->type, 2, 1, file);
    fread(&header->size, 4, 1, file);
    fread(&header->reserved1, 2, 1, file);
    fread(&header->reserved2, 2, 1, file);
    fread(&header->offset_bits, 4, 1, file);

    fread(&info->size, 4, 1, file);
    fread(&info->width, 4, 1, file);
    fread(&info->height, 4, 1, file);
    fread(&info->planes, 2, 1, file);
    fread(&info->bit_count, 2, 1, file);
    fread(&info->compression, 4, 1, file);
    fread(&info->size_image, 4, 1, file);
    fread(&info->x_per_meter, 4, 1, file);
    fread(&info->y_per_meter, 4, 1, file);
    fread(&info->clr_used, 4, 1, file);
    fread(&info->clr_important, 4, 1, file);
}


BMP_file* read_bmp_from_file(const char* path) {
    FILE *file = NULL;
    char* garbage = malloc(2000);
    unsigned int height;
    unsigned int width;
    unsigned int line_width; // дополнительное количество нулей в конце строки

    BMP_file* bmp = malloc(sizeof(BMP_file));
    file = fopen(path, "rb");

    read_bmp_header(file, &bmp->header, &bmp->info);
    if (bmp->header.type != 0x4D42 && bmp->header.type != 0x424D) return NULL; // идентификатор формата BMP
    if (bmp->info.size != 40) return NULL; // поддерживается только 3 версия BMP (24 бита на пиксель)
    if (bmp->info.compression != 0) return NULL; // поддерживаются только файлы без сжатия

    width = bmp->info.width;
    height = bmp->info.height > 0 ? bmp->info.height : -bmp->info.height;
    line_width = (width * bmp->info.bit_count / 8 + 3) / 4 * 4 - width * bmp->info.bit_count / 8; // считаем количество дополняющих нулей

    bmp->data = calloc(sizeof(RGB_pixel*), height);

    for (int i = 0; i < height; i++) {
        bmp->data[i] = calloc(sizeof(RGB_pixel), width);
        for (int j = 0; j < width; j++) {
            fread(&bmp->data[i][j].blue, 1, 1, file);  // данные в BMP хранятся в порядке BlueGreenRed
            fread(&bmp->data[i][j].green, 1, 1, file);
            fread(&bmp->data[i][j].red, 1, 1, file);
        }
        if (line_width != 0) {
            fread(garbage, 1, line_width, file);
        }
    }
    fclose(file);
    free(garbage);
    return bmp;
}

unsigned short normal_byte(int v) { // нормализация байта
    if (v > 255) v = 255;
    else if (v < 0) v = 0;
    return (unsigned short)v;
}

void edit_brightness_in_bmp(BMP_file* bmp, int value) {
    int width = bmp->info.width;
    int height = bmp->info.height > 0 ? bmp->info.height : -bmp->info.height;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            bmp->data[i][j].blue = normal_byte(bmp->data[i][j].blue + value);
            bmp->data[i][j].green = normal_byte(bmp->data[i][j].green + value);
            bmp->data[i][j].red = normal_byte(bmp->data[i][j].red + value);
        }
    }
}

void edit_contrast_in_bmp(BMP_file* bmp, double value) {
    double k = 259.0f * (value + 255.0f) / 255.0f / (259.0f - value); // коэффициент контрастности
    int width = bmp->info.width;
    int height = bmp->info.height > 0 ? bmp->info.height : -bmp->info.height;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            bmp->data[i][j].blue = normal_byte((int)(k * (bmp->data[i][j].blue - 128) + 128));
            bmp->data[i][j].green = normal_byte((int)(k * (bmp->data[i][j].green - 128) + 128));
            bmp->data[i][j].red = normal_byte((int)(k * (bmp->data[i][j].red - 128) + 128));
        }
    }
}


void change_resolution(BMP_file* bmp) { // уменьшение разрешающей способности сверткой 3 на 3 пикселя
    int width = bmp->info.width;
    int height = bmp->info.height > 0 ? bmp->info.height : -bmp->info.height;
    unsigned int r, g, b, c;
    RGB_pixel p;
    RGB_pixel** new_data = calloc(sizeof(RGB_pixel*), height);

    for (int i = 0; i < height; i++) {
        new_data[i] = calloc(sizeof(RGB_pixel), width);
        for (int j = 0; j < width; j++) {
            r = g = b = c = 0;
            for (int I = i - 1; I <= i + 1; I++)
                for (int J = j - 1; J <= j + 1; J++) {
                    if (I >= 0 && J >= 0 && I < height && J < width) {
                        p = bmp->data[I][J];
                        r += p.red;
                        g += p.green;
                        b += p.blue;
                        c++;
                    }
                }
            new_data[i][j].red = r / c;
            new_data[i][j].green = g / c;
            new_data[i][j].blue = b / c;
        }
    }

    for (int i = 0; i < height; i++) {
        free(bmp->data[i]);
    }
    free(bmp->data);
    bmp->data = new_data;
}

void sphere(BMP_file* bmp) { // проецирование изображения на сферу
    int width = bmp->info.width;
    int height = bmp->info.height  > 0 ? bmp->info.height : -bmp->info.height;
    int centerX = width / 2, centerY = height / 2;
    RGB_pixel white = {255, 255, 255};
    RGB_pixel** new_data = calloc(sizeof(RGB_pixel*), height);

    for (int i = 0; i < height; i++) {
        new_data[i] = calloc(sizeof(RGB_pixel), width);
        for (int j = 0; j < width; j++) {
            new_data[i][j] = white;
        }
    }
    int R = (width < height ? width : height) / 2;
    int X, Y, Z, X1, Y1;

    for (int i = 0; i < 2 * R; i++) {
        for (int j = 0; j < 2 * R; j++) {
            X = i - R;
            Y = j - R;
            if (X * X + Y * Y < R * R) {
                Z = (int)sqrt(R * R - X * X - Y * Y);
                X1 = (int)((X * R) / (Z + R));
                Y1 = (int)((Y * R) / (Z + R));
                new_data[centerY + Y][centerX + X] = bmp->data[centerY + Y1][centerX + X1];
            }
        }
    }

    for (int i = 0; i < height; i++) {
        free(bmp->data[i]);
    }
    free(bmp->data);
    bmp->data = new_data;
}

void save_bmp_to_file(const char* path, BMP_file* bmp) {
    FILE* file = fopen(path, "wb");
    int width = bmp->info.width;
    int height = bmp->info.height > 0 ? bmp->info.height : -bmp->info.height;
    int line_width = (bmp->info.width * bmp->info.bit_count / 8 + 3) / 4 * 4 - width * bmp->info.bit_count / 8;
    unsigned char line[4] = { 0 };

    fwrite(&bmp->header.type, 2, 1, file);
    fwrite(&bmp->header.size, 4, 1, file);
    fwrite(&bmp->header.reserved1, 2, 1, file);
    fwrite(&bmp->header.reserved2, 2, 1, file);
    fwrite(&bmp->header.offset_bits, 4, 1, file);


    fwrite(&bmp->info.size, 4, 1, file);
    fwrite(&bmp->info.width, 4, 1, file);
    fwrite(&bmp->info.height, 4, 1, file);
    fwrite(&bmp->info.planes, 2, 1, file);
    fwrite(&bmp->info.bit_count, 2, 1, file);
    fwrite(&bmp->info.compression, 4, 1, file);
    fwrite(&bmp->info.size_image, 4, 1, file);
    fwrite(&bmp->info.x_per_meter, 4, 1, file);
    fwrite(&bmp->info.y_per_meter, 4, 1, file);
    fwrite(&bmp->info.clr_used, 4, 1, file);
    fwrite(&bmp->info.clr_important, 4, 1, file);

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            fwrite(&bmp->data[i][j].blue, 1, 1, file);
            fwrite(&bmp->data[i][j].green, 1, 1, file);
            fwrite(&bmp->data[i][j].red, 1, 1, file);
        }
        if (line_width != 0) {
            fwrite(line, 1, line_width, file);
        }
    }
    fflush(file);
    fclose(file);
}

void show_bmp_dump(BMP_file* bmp) { // функция для вывода дампа
    int line_width = (bmp->info.width + 3) / 4 * 4;
    int width = bmp->info.width;
    int height = bmp->info.height > 0 ? bmp->info.height : -bmp->info.height;

    printf("    ");
    for (int i = 0; i < width; i++) {
        printf("B  ");
        printf("G  ");
        printf("R    ");
    }
    printf("\n");

    for (int i = 0; i < height; i++) {
        printf("%d:  ", i + 1);
        for (int j = 0; j < width; j++) {
            printf("%02X ", bmp->data[i][j].blue);
            printf("%02X ", bmp->data[i][j].green);
            printf("%02X ", bmp->data[i][j].red);
            printf("  ");
        }
        if (line_width != width) {
            for (int q = 0; q < line_width - width; q++)
                printf("%02X ", 0);
        }
        printf("\n");
    }
}

int main(){
    char *s = calloc(sizeof(char), 100);
    int height, value, flag = 1, choice = 0;

    printf("Enter path to your image:\n"); // путь относительно расположения .exe
    gets(s);
    printf("\n\n");
    BMP_file* bmp = read_bmp_from_file(s);
    if (bmp == NULL) {
        printf("Error while reading the file...\n");
        return -1;
    }
    while (flag) {
        printf("Enter what do u want:\n1: brightness\n2: contrast\n3: resolution\n4: sphere\n\n0: save & exit\n");
        scanf("%d", &choice);
        printf("\n\n");
        switch (choice) {
            case 1:
                printf("Enter value of brightness [-255; 255]:\n");
                scanf("%d", &value);
                printf("\n\n");
                edit_brightness_in_bmp(bmp, value);
                break;
            case 2:
                printf("Enter value of contrast [-255; 255]:\n");
                scanf("%d", &value);
                printf("\n\n");
                edit_contrast_in_bmp(bmp, value);
                break;
            case 3:
                printf("Enter amount of blur iterations [1; ..]:\n");
                scanf("%d", &value);
                printf("\n\n");
                for (int i = 0; i < value; i++)
                    change_resolution(bmp);
                break;
            case 4:
                sphere(bmp);
                break;
            case 0:
                flag = 0;
                break;
        }
    }

    printf("Enter path to save your image:\n");
    scanf("%s", s);
    save_bmp_to_file(s, bmp);
    printf("Image was saved!\n");

    height = bmp->info.height > 0 ? bmp->info.height : -bmp->info.height;
    free(s);
    for (int i = 0; i < height; i++)
        free(bmp->data[i]);
    free(bmp->data);
    free(bmp);
    return 0;
}
