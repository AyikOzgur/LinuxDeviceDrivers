#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x3f4c09e, "misc_register" },
	{ 0x122c3a7e, "_printk" },
	{ 0x546cd682, "gpiod_put" },
	{ 0x9f9a8a99, "misc_deregister" },
	{ 0x55bf416, "gpio_to_desc" },
	{ 0x7fe426b9, "gpiod_direction_output" },
	{ 0xeb6dc7f8, "gpiod_direction_input" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0x9115abcf, "gpiod_set_value" },
	{ 0xd701af9c, "gpiod_get_value" },
	{ 0x6cbbfc54, "__arch_copy_to_user" },
	{ 0xdcb764ad, "memset" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x39ff040a, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "176C2F1C6A9F508369CFA34");
