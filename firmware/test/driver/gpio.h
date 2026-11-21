#pragma once
// desk stub of the ESP-IDF GPIO calls sensor.h uses
typedef int gpio_num_t;
inline int gpio_pulldown_en(gpio_num_t){ return 0; }
inline int gpio_pullup_dis(gpio_num_t){ return 0; }
