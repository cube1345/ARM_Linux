#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void usage(const char* prog)
{
    printf("Usage %s <gpiochip> <line> <delay_ms>",prog);
}
int main(int argc ,char *argv[])
{
    const char* chip_name;
    int line_offset;
    int times;
    int delay_ms;
    int value = 0;

    struct gpiod_chip *chip;
    struct gpiod_line *line;

    if(argc < 5)
    {
        usage(argv[0]);
        return 1;
    }

    chip_name = argv[1];
    line_offset = atoi(argv[2]);
    times = atoi(argv[3]);
    delay_ms = atoi(argv[4]);


    chip = gpiod_chip_open_by_name(chip_name);
    if(!chip)
    {
        perror("gpiod_chip_open_by_name");
        return 1;
    }

    line  = gpiod_chip_get_line(chip,line_offset);
    if(!line)
    {
        perror("gpiod_line_get_line");
        return 1;
    }

    // 申请 GPIO 为输出
    if(gpiod_line_request_output(line,"gpio_blink",0) < 0)
    {
        perror("gpiod_line_request_output");
        gpiod_chip_close(chip);
        return 1;
    }    

    // 循环反转GPIO电平
    for(int i = 0; i < times; i++)
    {
        value = !value;
        if(gpiod_line_set_value(line,value) < 0)
        {
            perror("gpiod_line_set_value");
            gpiod_line_release(line);
            gpiod_chip_close(chip);
            return 1;
        }
        printf("gpiochip: %s line %d value %d\n",chip_name,line_offset,value);
        usleep(delay_ms * 1000);
    }
    gpiod_line_release(line);
    gpiod_chip_close(chip);
    return 0;


}