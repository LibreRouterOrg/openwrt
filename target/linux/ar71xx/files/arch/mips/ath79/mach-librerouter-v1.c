/*
 * LibreRouter v1 support (based on AP135)
 *
 * Copyright (C) 2017 Daniel Golle <daniel@makrotopia.org>
 *
 * Based on mach-wpj558.c and mach-tl-wr1043nd-v2.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <linux/pci.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/ar8216_platform.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>

#include <asm/mach-ath79/ath79.h>
#include <asm/mach-ath79/ar71xx_regs.h>

#include "common.h"
#include "dev-ap9x-pci.h"
#include "dev-eth.h"
#include "dev-leds-gpio.h"
#include "dev-gpio-buttons.h"
#include "dev-m25p80.h"
#include "dev-nfc.h"
#include "dev-spi.h"
#include "dev-usb.h"
#include "dev-wmac.h"
#include "machtypes.h"
#include "pci.h"

#define LIBREROUTERV1_MAC0_OFFSET		0x0
#define LIBREROUTERV1_MAC1_OFFSET		0x6
#define LIBREROUTERV1_WMAC_CALDATA_OFFSET	0x1000

#define LIBREROUTERV1_GPIO_BTN_RESET	17

#define LIBREROUTERV1_KEYS_POLL_INTERVAL		20
#define LIBREROUTERV1_KEYS_DEBOUNCE_INTERVAL	(3 * LIBREROUTERV1_KEYS_POLL_INTERVAL)


static struct gpio_keys_button librerouter_gpio_keys[] __initdata = {
	{
		.desc		= "reset",
		.type		= EV_KEY,
		.code		= KEY_RESTART,
		.debounce_interval = LIBREROUTERV1_KEYS_DEBOUNCE_INTERVAL,
		.gpio		= LIBREROUTERV1_GPIO_BTN_RESET,
		.active_low	= 1,
	}
};

static struct mtd_partition librerouter_spi_partitions[] = {
	{
		.name		= "uboot",
		.offset		= 0,
		.size		= 0x0040000,
	}, {
		.name		= "uboot-env",
		.offset		= 0x0040000,
		.size		= 0x0010000,
	}, {
		.name		= "firmware",
		.offset		= 0x0050000,
		.size		= 0x07c0000,
	}, {
		.name		= "art",
		.offset		= 0x0ff0000,
		.size		= 0x0010000,
	}
};

static struct flash_platform_data librerouter_spi_flash_data = {
	.parts		= librerouter_spi_partitions,
	.nr_parts	= ARRAY_SIZE(librerouter_spi_partitions),
};

static struct spi_board_info librerouter_spi_info[] = {
	{
		.bus_num	= 0,
		.chip_select	= 0,
		.max_speed_hz	= 25000000,
		.modalias	= "w25q256",
		.platform_data	= &librerouter_spi_flash_data,
	},
};

static struct ath79_spi_platform_data librerouter_spi_data = {
	.bus_num		= 0,
	.num_chipselect		= 1,
};

static struct resource librerouter_uart1_resources[] = {
	{
		.start	= QCA955X_UART1_BASE,
		.end	= QCA955X_UART1_BASE + QCA955X_UART1_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= ATH79_MISC_IRQ(6),
		.end	= ATH79_MISC_IRQ(6),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device librerouter_uart1_device = {
	.name		= "ar933x-uart",
	.id		= -1,
	.resource	= librerouter_uart1_resources,
	.num_resources	= ARRAY_SIZE(librerouter_uart1_resources),
};

static const struct ar8327_led_info librerouter_leds_qca8337[] = {
	AR8327_LED_INFO(PHY0_0, HW, "librerouter:green:link0"),
	AR8327_LED_INFO(PHY1_0, HW, "librerouter:green:link1"),
};

/* Blink rate: 1 Gbps -> 8 hz, 100 Mbs -> 4 Hz, 10 Mbps -> 2 Hz */
static struct ar8327_led_cfg librerouter_qca8337_led_cfg = {
	.led_ctrl0 = 0xcf37cf37,
	.led_ctrl1 = 0xcf37cf37,
	.led_ctrl2 = 0xcf37cf37,
	.led_ctrl3 = 0x0,
	.open_drain = true,
};

/* QCA8337 GMAC0 is connected with QCA9558 over RGMII */
static struct ar8327_pad_cfg librerouter_qca8337_pad0_cfg = {
	.mode = AR8327_PAD_MAC_RGMII,
	.txclk_delay_en = true,
	.rxclk_delay_en = true,
	.txclk_delay_sel = AR8327_CLK_DELAY_SEL1,
	.rxclk_delay_sel = AR8327_CLK_DELAY_SEL2,
	.mac06_exchange_dis = false,
};

/* QCA8337 GMAC6 is connected with QCA9558 over SGMII */
static struct ar8327_pad_cfg librerouter_qca8337_pad6_cfg = {
	.mode = AR8327_PAD_MAC_SGMII,
	.sgmii_delay_en = true,
	.rxclk_delay_sel = AR8327_CLK_DELAY_SEL0,
};


static struct ar8327_platform_data librerouter_qca8337_data = {
	.pad0_cfg = &librerouter_qca8337_pad0_cfg,
	.pad6_cfg = &librerouter_qca8337_pad6_cfg,
	.port0_cfg = {
		.force_link = 1,
		.speed = AR8327_PORT_SPEED_1000,
		.duplex = 1,
		.txpause = 1,
		.rxpause = 1,
	},
	.port6_cfg = {
		.force_link = 1,
		.speed = AR8327_PORT_SPEED_1000,
		.duplex = 1,
		.txpause = 1,
		.rxpause = 1,
	},
	.led_cfg = &librerouter_qca8337_led_cfg,
	.num_leds = ARRAY_SIZE(librerouter_leds_qca8337),
	.leds = librerouter_leds_qca8337,
};

static struct mdio_board_info librerouter_mdio0_info[] = {
	{
		.bus_id = "ag71xx-mdio.0",
		.phy_addr = 0,
		.platform_data = &librerouter_qca8337_data,
	},
};

static void __init librerouter_setup_qca955x_eth_cfg(u32 mask,
                                             unsigned int rxd,
                                             unsigned int rxdv,
                                             unsigned int txd,
                                             unsigned int txe)
{
	void __iomem *base;
	u32 t;

	base = ioremap(QCA955X_GMAC_BASE, QCA955X_GMAC_SIZE);

	t = mask;
	t |= rxd << QCA955X_ETH_CFG_RXD_DELAY_SHIFT;
	t |= rxdv << QCA955X_ETH_CFG_RDV_DELAY_SHIFT;
	t |= txd << QCA955X_ETH_CFG_TXD_DELAY_SHIFT;
	t |= txe << QCA955X_ETH_CFG_TXE_DELAY_SHIFT;

	__raw_writel(t, base + QCA955X_GMAC_REG_ETH_CFG);

	iounmap(base);
}

static void __init librerouter_v1_setup(void)
{
	u8 *art = (u8 *) KSEG1ADDR(0x1fff0000);

	ath79_gpio_function_enable(AR934X_GPIO_FUNC_JTAG_DISABLE);
	ath79_register_spi(&librerouter_spi_data,
			   librerouter_spi_info,
			   ARRAY_SIZE(librerouter_spi_info));

	mdiobus_register_board_info(librerouter_mdio0_info,
				    ARRAY_SIZE(librerouter_mdio0_info));
	ath79_register_mdio(0, 0x0);

	librerouter_setup_qca955x_eth_cfg(QCA955X_ETH_CFG_RGMII_EN, 2, 2, 0, 0);

	/* QCA9558 GMAC0 is connected to RMGII interface */
	ath79_eth0_data.phy_if_mode = PHY_INTERFACE_MODE_RGMII;
	ath79_eth0_data.phy_mask = BIT(0);
	ath79_eth0_data.mii_bus_dev = &ath79_mdio0_device.dev;
	ath79_eth0_pll_data.pll_1000 = 0xa6000000;

	ath79_init_mac(ath79_eth0_data.mac_addr, art + LIBREROUTERV1_MAC0_OFFSET, 0);
	ath79_register_eth(0);

	/* QCA9558 GMAC1 is connected to SGMII interface */
	ath79_eth1_data.phy_if_mode = PHY_INTERFACE_MODE_SGMII;
	ath79_eth1_data.speed = SPEED_1000;
	ath79_eth1_data.duplex = DUPLEX_FULL;
	ath79_eth1_pll_data.pll_1000 = 0x03000101;

	ath79_init_mac(ath79_eth1_data.mac_addr, art + LIBREROUTERV1_MAC1_OFFSET, 0);

	ath79_register_eth(1);

	ath79_register_gpio_keys_polled(-1, LIBREROUTERV1_KEYS_POLL_INTERVAL,
					ARRAY_SIZE(librerouter_gpio_keys),
					librerouter_gpio_keys);

	ath79_register_wmac(art + LIBREROUTERV1_WMAC_CALDATA_OFFSET, NULL);

	ath79_register_pci();
	ath79_register_usb();
	ath79_register_nfc();

	platform_device_register(&librerouter_uart1_device);
}

MIPS_MACHINE(ATH79_MACH_LIBREROUTERV1, "LIBREROUTERV1", "LibreRouter v1", librerouter_v1_setup);
