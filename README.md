This repository shows implementation of device drivers simple device drivers for a raspberry pi. Under ./KernelModules folder there are 4 different driver implementations. These are : 

- charDeviceModule :  Implementation of simple character device driver. In order to test it, you should create related character device node by using mknode command and then load driver by using insmode command.

- miscDeviceModule : Implementation of miscellaneous device driver. Thanks to miscellaneous device, we do not need to provide device major number and also it creates the device file for us. We dont need mknode command anymore, insmode still required to load driver.

- platformDeviceModule : Implementation of platform device driver. Platform device drivers upload automatically if there is maching device node on device tree. So it removes necessity of insmode. So we need neither mknode nor insmode thanks to platform devices. Only requirement is having maching node on device tree.

- gpioPlatformDriver : Demonstration of platform device driver by blinking a led on raspberry pi zero 2. If you have different raspberry pi model, ensure that you have proper physical address definitions.