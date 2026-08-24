#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>

static void usage(const char *prog)
{
    printf("Usage %s <gpiochip> <line> \n",prog);
    printf("Example %s gpiochip0 0\n",prog);
}

int main(int argc,char *argv[])
{
    const char *chip_name;
    int line_offset;
    int value;

    struct gpiod_chip *chip;
    struct gpiod_line *line;

    if(argc < 3)
    {
        usage(argv[0]);
        return 1;
    }
    chip_name = argv[1];
    line_offset = atoi(argv[2]);

    // 打开GPIO
    chip = gpiod_chip_open_by_name(chip_name);
    if(!chip)
    {
        perror("gpiod_chip_open_by_name");
        return 1;
    }

    // 获取指定GPIO line
    line = gpiod_chip_get_line(chip,line_offset);
    if(!line)
    {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(chip);
        return 1;
    }

    // 配置GPIO为输入模式
    if(gpiod_line_request_input(line,"gpio_get") < 0)
    {
        perror("gpiod_line_request_input");
        gpiod_chip_close(chip);
        return 1;
    }

    // 读取电平
    value = gpiod_line_get_value(line);
    if(value < 0)
    {
        perror("gpiod_line_get_value");
        gpiod_line_release(line);
        gpiod_chip_close(chip);
        return 1;
    }

    printf("%s line %d value: %d\n",chip_name,line_offset,value);

    gpiod_line_release(line);
    gpiod_chip_close(chip);

    return 0;
}