>基于国民技术MCU N32L406RB（FLASH 128KB, RAM 24KB）的IAP boot loader工程
>该flash一共有64页，每页2KB大小
>page0~page2共三页，共6KB，作为boot loader代码区域
>page3作为启动信息区，目前实际使用了前四个字节，bootloader代码每次上电运行时检查该区前四个字节
>如果启动信息区前四个字节值为0xAAAAAAAA，则认为需要更新APP代码，否则，直接跳转至APP代码区域运行APP
>page4~page33共30页60KB，用于存储APP代码
>page34~page63共30页60KB，用于存储要更新的APP固件
>在APP代码中，如果监测到IAP请求，接收更新固件并存储至page34~page63，再将page3前四个字节置为0xAAAAAAAA，在进行自身复位
>复位后，运行bootloader代码，检测前四个字节，如果为0xAAAAAAAA，则将page34~page63整个复制到page4~page33
>复制完成后，将page34~page63全部擦除，将page3擦除，跳转至APP运行
