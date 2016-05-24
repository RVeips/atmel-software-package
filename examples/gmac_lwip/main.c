/* ----------------------------------------------------------------------------
 *         SAM Software Package License 
 * ----------------------------------------------------------------------------
 * Copyright (c) 2013, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */

/**
 *  \page gmac_lwip GMAC lwIP Example
 *
 *  \section Purpose
 *
 *  This project implements webserver example by using lwIP stack, It enables
 *  the device to act as a web server, sending a very short page when accessed
 *  through a browser.
 *
 *  \section Requirements
 *
 * - SAMA5D4X microcontrollers with GMAC feature.
 * - On-board ethernet interface.
 *
 *  \section Description
 *
 *  Please refer to the lwIP documentation for more information about
 *  the TCP/IP stack and the webserver example.
 *
 *  By default, the example does not use DHCP. If you want to use DHCP,
 *  please open file lwipopts.h and define "LWIP_DHCP" and "LWIP_UDP" to 1.
 *
 *  \section Usage
 *
 *  -# Build the program and download it inside the evaluation board. Please
 *     refer to the
 *     <a href="http://www.atmel.com/dyn/resources/prod_documents/6421B.pdf">
 *     SAM-BA User Guide</a>, the
 *     <a href="http://www.atmel.com/dyn/resources/prod_documents/doc6310.pdf">
 *     GNU-Based Software Development</a>
 *     application note or to the
 *     <a href="ftp://ftp.iar.se/WWWfiles/arm/Guides/EWARM_UserGuide.ENU.pdf">
 *     IAR EWARM User Guide</a>,
 *     depending on your chosen solution.
 *  -# On the computer, open and configure a terminal application
 *     (e.g. HyperTerminal on Microsoft Windows) with these settings:
 *    - 115200 bauds
 *    - 8 bits of data
 *    - No parity
 *    - 1 stop bit
 *    - No flow control
 *  -# Connect an Ethernet cable between the evaluation board and the network.
 *      The board may be connected directly to a computer; in this case,
 *      make sure to use a cross/twisted wired cable such as the one provided
 *      with SAMA5D4-EK / SAMA5D4-XULT.
 *  -# Start the application. It will display the following message on the terminal:
 *    \code
 *    -- GMAC lwIP Example xxx --
 *    -- xxxxxx-xx
 *    -- Compiled: xxx xx xxxx xx:xx:xx --
 *      MAC 3a:1f:34:08:54:54
 *    - Host IP  192.168.1.3
 *    - Gateway IP 192.168.1.2
 *    - Net Mask 255.255.255.0
 *    \endcode
 * -# Type the IP address (Host IP in the debug log) of the device in a web
 *    browser, like this:
 *    \code
 *    http://192.168.1.3
 *    \endcode
 *    The page generated by lwIP will appear in the web browser, like below:
 *    \code
 *    Small test page.#
 *    \endcode
 *
 *  \note
 *  Make sure the IP adress of the device( the board) and the computer are in the same network.
 */

/** \file
 *
 *  This file contains all the specific code for the gmac_lwip example.
 *
 */

/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/


#include "board.h"

#include "memories/at24.h"
#include "misc/console.h"
#include "peripherals/pio.h"

#include "liblwip.h"
#include "httpd.h"

#include <stdio.h>
#include <string.h>

/*----------------------------------------------------------------------------
 *        Types
 *----------------------------------------------------------------------------*/

/* Timer for calling lwIP tmr functions without system */
typedef struct _timers_info {
	uint32_t timer;
	uint32_t timer_interval;
	void (*timer_func)(void);
} timers_info;

/*---------------------------------------------------------------------------
 *         Variables
 *---------------------------------------------------------------------------*/

/* lwIP tmr functions list */
static timers_info timers_table[] = {
	/* LWIP_TCP */
	{ 0, TCP_FAST_INTERVAL,     tcp_fasttmr},
	{ 0, TCP_SLOW_INTERVAL,     tcp_slowtmr},
	/* LWIP_ARP */
	{ 0, ARP_TMR_INTERVAL,      etharp_tmr},
	/* LWIP_DHCP */
#if LWIP_DHCP
	{ 0, DHCP_COARSE_TIMER_SECS, dhcp_coarse_tmr},
	{ 0, DHCP_FINE_TIMER_MSECS,  dhcp_fine_tmr},
#endif
};

/** if AT24 is available on the board, it will be used to setup the MAC addr */
#ifdef AT24_PINS
static const struct _pin at24_pins[] = AT24_PINS;

struct _at24 at24_drv = {
	.desc = AT24_DESC,
	.sn_addr = AT24_SN_ADDR,
	.sn_offset = AT24_SN_OFFSET,
	.eui_offset = AT24_EUI48_OFFSET,
};

struct _twi_desc at24_twid = {
        .addr = AT24_ADDR,
        .freq = AT24_FREQ,
        .transfert_mode = TWID_MODE_DMA
};
#endif

/* The MAC address used for demo */
static uint8_t gMacAddress[6] = {0x3a, 0x1f, 0x34, 0x08, 0x54, 0x54};

/* The IP address used for demo (ping ...) */
//static uint8_t gIpAddress[4] = {10, 217, 2, 254};
static uint8_t gIpAddress[4] = {192, 168, 1, 3};

/* Set the default router's IP address. */
//static const uint8_t gGateWay[4] = {10, 217, 2, 250};
static const uint8_t gGateWay[4] = {192, 168, 1, 2};

/* The NetMask address */
static const uint8_t gNetMask[4] = {255, 255, 255, 0};

/*----------------------------------------------------------------------------
 *        Local functions
 *----------------------------------------------------------------------------*/
#if 1
/**
 * Process timing functions
 */
static void timers_update(void)
{
	static uint32_t last_time;
	uint32_t cur_time, time_diff, idxtimer;
	timers_info * ptmr_inf;

	cur_time = sys_get_ms();
	if (cur_time >= last_time)
	{
		time_diff = cur_time - last_time;
	}
	else
	{
		time_diff = 0xFFFFFFFF - last_time + cur_time;
	}
	if (time_diff)
	{
		last_time = cur_time;
		for(idxtimer=0;
			idxtimer < (sizeof(timers_table)/sizeof(timers_info));
			idxtimer ++)
		{
			ptmr_inf = &timers_table[idxtimer];
			ptmr_inf->timer += time_diff;
			if (ptmr_inf->timer > ptmr_inf->timer_interval)
			{
				if (ptmr_inf->timer_func)
					ptmr_inf->timer_func();
				ptmr_inf->timer -= ptmr_inf->timer_interval;
			}
		}
	}
}
#endif
/*----------------------------------------------------------------------------
 *        Exported functions
 *----------------------------------------------------------------------------*/

/**
 *  \brief gmac_lwip example entry point.
 *
 *  \return Unused (ANSI-C compatibility).
 */
int main(void)
{
	struct ip_addr ipaddr, netmask, gw;
	struct netif NetIf, *netif;

#if LWIP_DHCP
	u8_t   dhcp_state = DHCP_INIT;
#endif

	/* Output example information */
	console_example_info("GMAC lwIP Example");

#ifdef AT24_PINS
	pio_configure(at24_pins, ARRAY_SIZE(at24_pins));
	at24_configure(&at24_drv, &at24_twid);
	if (at24_get_mac_address(&at24_drv)) {
		printf("Failed reading MAC address from AT24 EEPROM");
	} else {
		memcpy(gMacAddress, at24_drv.mac_addr_48, 6);
	}
#endif

	/* Display MAC & IP settings */
	printf(" - MAC %02x:%02x:%02x:%02x:%02x:%02x\n\r",
			gMacAddress[0], gMacAddress[1], gMacAddress[2],
			gMacAddress[3], gMacAddress[4], gMacAddress[5]);

#if !LWIP_DHCP
	printf(" - Host IP  %d.%d.%d.%d\n\r",
			gIpAddress[0], gIpAddress[1],
			gIpAddress[2], gIpAddress[3]);
	printf(" - GateWay IP  %d.%d.%d.%d\n\r",
			gGateWay[0], gGateWay[1], gGateWay[2], gGateWay[3]);
	printf(" - Net Mask  %d.%d.%d.%d\n\r",
			gNetMask[0], gNetMask[1], gNetMask[2], gNetMask[3]);
#else
	printf(" - DHCP Enabled\n\r");
#endif

	/* Initialize lwIP modules */
	lwip_init();

	/* Initialize net interface for lwIP */
	gmacif_setmac((u8_t*)gMacAddress);

#if !LWIP_DHCP
	IP4_ADDR(&gw, gGateWay[0], gGateWay[1], gGateWay[2], gGateWay[3]);
	IP4_ADDR(&ipaddr, gIpAddress[0], gIpAddress[1], gIpAddress[2], gIpAddress[3]);
	IP4_ADDR(&netmask, gNetMask[0], gNetMask[1], gNetMask[2], gNetMask[3]);
#else
	IP4_ADDR(&gw, 0, 0, 0, 0);
	IP4_ADDR(&ipaddr, 0, 0, 0, 0);
	IP4_ADDR(&netmask, 0, 0, 0, 0);
#endif
	netif = netif_add(&NetIf, &ipaddr, &netmask, &gw, NULL, gmacif_init, ip_input);
	netif_set_default(netif);
	netif_set_up(netif);
	/* Initialize http server application */
	if (ERR_OK != httpd_init())
	{
		printf("httpd_init ERR_OK!");
		return -1;
	}
	printf ("Type the IP address of the device in a web browser, http://192.168.1.3 \n\r");
	while(1)
	{
		/* Run periodic tasks */
		timers_update();

		/* Run polling tasks */
		gmacif_poll(netif);
	}
}
