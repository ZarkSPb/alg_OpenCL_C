constant ushort width = 306; // ширина small изображения
constant ushort height = 256; // высота small изображения
constant ushort bordersize = 8; // величина отступов
constant ushort new_width = 306 + 2 * bordersize; // 2448 / 8 + 2 * bordersize = 320
constant ushort new_height = 256 + 2 * bordersize; // 2448 / 8 + 2 * bordersize = 320
constant ushort colsBig = 2448;

__kernel void resize(__global ushort *bmpIn,
                     __global uchar *bmpOutSmall,
                     __global uint *histogram)
{
    // определяем свои координаты и приводим их к сетке с ядром 16x16
    ushort x_out = get_global_id(0);
    ushort y_out = get_global_id(1);
    ushort x = 8 * x_out;
    ushort y = 8 * y_out;

    uint sum = 0;
    ushort current = 0;
    ulong coord;
    for (uchar row = 0; row < 8; row++)
    // TODO раскрыть цикл ???
        for (uchar col = 0; col < 8; col++) {
            coord = (y + row) * colsBig + x + col;
            current = bmpIn[coord];
            sum += current;
            // Записываем гистограмму
            atomic_add(&histogram[current], 1);
        }
    x_out += bordersize; // сдвигаем изображение на bordersize
    y_out += bordersize; // сдвигаем изображение на bordersize
    sum = (sum >> 10) + 1;
    if (sum > 254) {
        bmpOutSmall[y_out * new_width + x_out] = 255;
    } else {
        bmpOutSmall[y_out * new_width + x_out] = sum;
    }
}

// TODO разобраться с углами дополненного изображения
__kernel void pad_image_reflect(__global uchar *image) {
    // определяем свои координаты
    ushort x = get_global_id(0);
    ushort y = get_global_id(1);

    // Индекс пикселя в массиве
    ulong idx = y * new_width + x;

    // Если пиксель внутри границы, его менять не нужно
    if (y >= bordersize && y < height + bordersize &&
        x >= bordersize && x < width + bordersize) {
        return;
    }

    // Левый край
    if (x < bordersize) {
        image[idx] = image[y * new_width + 2 * bordersize - x - 1];
    }
    // Правый край
    else if (x >= width + bordersize) {
        image[idx] = image[y * new_width + 2 * (width + bordersize) - x - 1];
    }
    // Верхний край
    else if (y < bordersize) {
        image[idx] = image[(2 * bordersize - y - 1) * new_width + x];
    }
    // Нижний край
    else if (y >= height + bordersize) {
        image[idx] = image[(2 * (height + bordersize) - y - 1) * new_width + x];
    }
}

__kernel void localNorm(__global ushort *bmpIn,
                        __global uchar *smallImg,
                        __global uchar *bmpOut,
                        __global ushort *lut) {
    // определяем свои координаты
    ushort x = get_global_id(0);
    ushort y = get_global_id(1);

    uchar small_histogram[256];
    for (uchar i = 0; i < 255; i++) {
        small_histogram[i] = 0;
    }
    small_histogram[255] = 0;
    ushort ymax = y + 15;

    // Заполняем гистограмму
    ulong coord;
    uchar16 pixel_data;
    for (ushort i = y; i < ymax; i++) {
        coord = i * new_width + x;
        // Используем vload16 для загрузки 16 байт данных
        pixel_data = vload16(0, smallImg + coord);
        // for (uchar j = 0; j < 16; j++) {
        //     small_histogram[pixel_data[j]]++;}
        small_histogram[pixel_data.s0]++; small_histogram[pixel_data.s1]++; small_histogram[pixel_data.s2]++; small_histogram[pixel_data.s3]++;
        small_histogram[pixel_data.s4]++; small_histogram[pixel_data.s5]++; small_histogram[pixel_data.s6]++; small_histogram[pixel_data.s7]++;
        small_histogram[pixel_data.s8]++; small_histogram[pixel_data.s9]++; small_histogram[pixel_data.sA]++; small_histogram[pixel_data.sB]++;
        small_histogram[pixel_data.sC]++; small_histogram[pixel_data.sD]++; small_histogram[pixel_data.sE]++; small_histogram[pixel_data.sF]++;
    }
    coord = ymax * new_width + x;
    pixel_data = vload16(0, smallImg + coord);
    // for (uchar j = 0; j < 15; j++) { small_histogram[pixel_data[j]]++;
    //     // small_histogram[smallImg[coord + j]]++; }
    small_histogram[pixel_data.s0]++; small_histogram[pixel_data.s1]++; small_histogram[pixel_data.s2]++; small_histogram[pixel_data.s3]++;
    small_histogram[pixel_data.s4]++; small_histogram[pixel_data.s5]++; small_histogram[pixel_data.s6]++; small_histogram[pixel_data.s7]++;
    small_histogram[pixel_data.s8]++; small_histogram[pixel_data.s9]++; small_histogram[pixel_data.sA]++; small_histogram[pixel_data.sB]++;
    small_histogram[pixel_data.sC]++; small_histogram[pixel_data.sD]++; small_histogram[pixel_data.sE]++;
    if (small_histogram[pixel_data.sF] < 255) { small_histogram[pixel_data.sF]++; }

    // if (small_histogram[pixel_data[15]] < 255) {small_histogram[pixel_data[15]]++;}

    // считаем куммулятивную гистограмму
    ushort current, current_4;
    uchar cumhistogram[256];
    cumhistogram[255] = 255;
    cumhistogram[0] = small_histogram[0];
    for (uchar i = 1; i < 255; i++) {
        current = cumhistogram[i - 1] + small_histogram[i];
        cumhistogram[i] = (current > 254) ? 255 : current;
    }

    // записываем результирующее изображение
    x *= 8; // Переходим в пространство большого изображения
    y *= 8; // Переходим в пространство большого изображения
    for (uchar row = 0; row < 8; row++)
        for (uchar col = 0; col < 8; col++) {
            coord = (y + row) * colsBig + x + col;
            current = bmpIn[coord];
            current_4 = current >> 4;
            bmpOut[coord] = (cumhistogram[current_4] + lut[current] + current_4) >> 2;
        }
}