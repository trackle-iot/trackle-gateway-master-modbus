# SIM7600E driver for ESP32

![banner](doc/src/images/banner.png "banner")

## Content of the repository

- ```include```: public interface headers;
- ```src```: source files;
    - ```include```: private headers;
- ```doc```
    - ```src```: additional source files specifically for documentation;
- ```tools```: tools useful for debug and development of the driver.

## Integration of the component

For the integration of the component in your project, it is recommended to rise the size of the main task's stack to <b>at least 8192 bytes</b>.

In order to do so, inside your ```sdkconfig.<env>``` (where ```<env>``` is the name of your build environment inside ```platformio.ini```) you can change ```CONFIG_MAIN_TASK_STACK_SIZE``` and ```CONFIG_ESP_MAIN_TASK_STACK_SIZE``` as follows:

```
...
CONFIG_MAIN_TASK_STACK_SIZE=8192
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
...
```

## Further documentation

To be done.
