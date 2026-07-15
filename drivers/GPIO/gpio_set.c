#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void usage(const char* prog)
{
    printf("Usage %s <gpiochip> <line> <value> \n",prog);

}
int main(int argc ,char* argv[])
{
    const char* chip_name;
    int line_offset;
    int value;
    int hold_seconds = 5;

    struct gpiod_chip* chip;
    struct gpiod_line* line;

    if(argc < 4) 
    {
        usage(argv[0]);
        return 1;
    }    
    
    chip_name = argv[1];
    line_offset = atoi(argv[2]);
    value = atoi(argv[3]);

    if(argc >= 5) hold_seconds = atoi(argv[4]);

    // 打开GPIO控制器
    chip = gpiod_chip_open_by_name(chip_name);
    if(!chip)
    {
        perror("gpiod_chip_open_by_name");
        return 1;
    }

    // 获取GPIO控制器中的一根线
    line  = gpiod_chip_get_line(chip,line_offset);
    if(!line)
    {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(chip);
        return 1;
    }

    // 将GPIO配置成输出模式
    if(gpiod_line_request_output(line,"gpio_set",value) < 0)
    {
        perror("gpiod_line_request_output");
        gpiod_chip_close(chip);
        return 1;
    }

    printf("set %s line %d to %d,hold %d seconds\n",chip_name,line_offset,value,hold_seconds);

    // 保持一段时间
    sleep(hold_seconds);

    gpiod_line_release(line);
    gpiod_chip_close(chip);

    return 0;

}