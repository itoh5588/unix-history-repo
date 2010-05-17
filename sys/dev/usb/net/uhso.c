/*-
 * Copyright (c) 2009 Fredrik Lindberg <fli@shapeshifter.se>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/tty.h>
#include <sys/sysctl.h>
#include <sys/condvar.h>
#include <sys/sx.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/bpf.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usb_cdc.h>
#include "usbdevs.h"
#define USB_DEBUG_VAR uhso_debug
#include <dev/usb/usb_debug.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_device.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>
#include <dev/usb/serial/usb_serial.h>
#include <dev/usb/usb_msctest.h>

struct uhso_tty {
	struct uhso_softc *ht_sc;
	struct usb_xfer	*ht_xfer[3];
	int		ht_muxport; /* Mux. port no */
	int		ht_open;
	char		ht_name[32];
};

struct uhso_softc {
	device_t		sc_dev;
	struct usb_device	*sc_udev;
	struct mtx		sc_mtx;
	uint32_t		sc_type;	/* Interface definition */

	struct usb_xfer		*sc_xfer[3];
	uint8_t			sc_iface_no;
	uint8_t			sc_iface_index;

	/* Control pipe */
	struct usb_xfer	*	sc_ctrl_xfer[2];
	uint8_t			sc_ctrl_iface_no;

	/* Network */
	struct usb_xfer		*sc_if_xfer[2];
	struct ifnet		*sc_ifp;
	struct mbuf		*sc_mwait;	/* Partial packet */
	size_t			sc_waitlen;	/* No. of outstanding bytes */
	struct ifqueue		sc_rxq;
	struct callout		sc_c;

	/* TTY related structures */
	struct ucom_super_softc sc_super_ucom;
	int			sc_ttys;
	struct uhso_tty		*sc_tty;
	struct ucom_softc	*sc_ucom;
	int			sc_msr;
	int			sc_lsr;
	int			sc_line;
};

#define UHSO_MAX_MTU		2048

/*
 * There are mainly two type of cards floating around.
 * The first one has 2,3 or 4 interfaces with a multiplexed serial port
 * and packet interface on the first interface and bulk serial ports
 * on the others.
 * The second type of card has several other interfaces, their purpose
 * can be detected during run-time.
 */
#define UHSO_IFACE_SPEC(usb_type, port, port_type) \
	(((usb_type) << 24) | ((port) << 16) | (port_type))

#define UHSO_IFACE_USB_TYPE(x) ((x >> 24) & 0xff)
#define UHSO_IFACE_PORT(x) ((x >> 16) & 0xff)
#define UHSO_IFACE_PORT_TYPE(x) (x & 0xff)

/*
 * USB interface types
 */
#define UHSO_IF_NET		0x01	/* Network packet interface */
#define UHSO_IF_MUX		0x02	/* Multiplexed serial port */
#define UHSO_IF_BULK		0x04	/* Bulk interface */

/*
 * Port types
 */
#define UHSO_PORT_UNKNOWN	0x00
#define UHSO_PORT_SERIAL	0x01	/* Serial port */
#define UHSO_PORT_NETWORK	0x02	/* Network packet interface */

/*
 * Multiplexed serial port destination sub-port names
 */
#define UHSO_MPORT_TYPE_CTL	0x00	/* Control port */
#define UHSO_MPORT_TYPE_APP	0x01	/* Application */
#define UHSO_MPORT_TYPE_PCSC	0x02
#define UHSO_MPORT_TYPE_GPS	0x03
#define UHSO_MPORT_TYPE_APP2	0x04	/* Secondary application */
#define UHSO_MPORT_TYPE_MAX	UHSO_MPORT_TYPE_APP2
#define UHSO_MPORT_TYPE_NOMAX	8	/* Max number of mux ports */

/*
 * Port definitions
 * Note that these definitions are arbitrary and do not match the values
 * returned by the auto config descriptor.
 */
#define UHSO_PORT_TYPE_CTL	0x01
#define UHSO_PORT_TYPE_APP	0x02
#define UHSO_PORT_TYPE_APP2	0x03
#define UHSO_PORT_TYPE_MODEM	0x04
#define UHSO_PORT_TYPE_NETWORK	0x05
#define UHSO_PORT_TYPE_DIAG	0x06
#define UHSO_PORT_TYPE_DIAG2	0x07
#define UHSO_PORT_TYPE_GPS	0x08
#define UHSO_PORT_TYPE_GPSCTL	0x09
#define UHSO_PORT_TYPE_PCSC	0x0a
#define UHSO_PORT_TYPE_MSD	0x0b
#define UHSO_PORT_TYPE_VOICE	0x0c
#define UHSO_PORT_TYPE_MAX	0x0c

static eventhandler_tag uhso_etag;

/* Overall port type */
static char *uhso_port[] = {
	"Unknown",
	"Serial",
	"Network",
	"Network/Serial"
};

/*
 * Map between interface port type read from device and description type.
 * The position in this array is a direct map to the auto config
 * descriptor values.
 */
static unsigned char uhso_port_map[] = {
	0,
	UHSO_PORT_TYPE_DIAG,
	UHSO_PORT_TYPE_GPS,
	UHSO_PORT_TYPE_GPSCTL,
	UHSO_PORT_TYPE_APP,
	UHSO_PORT_TYPE_APP2,
	UHSO_PORT_TYPE_CTL,
	UHSO_PORT_TYPE_NETWORK,
	UHSO_PORT_TYPE_MODEM,
	UHSO_PORT_TYPE_MSD,
	UHSO_PORT_TYPE_PCSC,
	UHSO_PORT_TYPE_VOICE
};
static char uhso_port_map_max = sizeof(uhso_port_map) / sizeof(char);

static unsigned char uhso_mux_port_map[] = {
	UHSO_PORT_TYPE_CTL,
	UHSO_PORT_TYPE_APP,
	UHSO_PORT_TYPE_PCSC,
	UHSO_PORT_TYPE_GPS,
	UHSO_PORT_TYPE_APP2
};

static char *uhso_port_type[] = {
	"Unknown",  /* Not a valid port */
	"Control",
	"Application",
	"Application (Secondary)",
	"Modem",
	"Network",
	"Diagnostic",
	"Diagnostic (Secondary)",
	"GPS",
	"GPS Control",
	"PC Smartcard",
	"MSD",
	"Voice",
};

static char *uhso_port_type_sysctl[] = {
	"unknown",
	"control",
	"application",
	"application",
	"modem",
	"network",
	"diagnostic",
	"diagnostic",
	"gps",
	"gps_control",
	"pcsc",
	"msd",
	"voice",
};

#define UHSO_STATIC_IFACE	0x01
#define UHSO_AUTO_IFACE		0x02

static const struct usb_device_id uhso_devs[] = {
#define	UHSO_DEV(v,p,i) { USB_VPI(USB_VENDOR_##v, USB_PRODUCT_##v##_##p, i) }
	/* Option GlobeSurfer iCON 7.2 */
	UHSO_DEV(OPTION, GSICON72, UHSO_STATIC_IFACE),
	/* Option iCON 225 */
	UHSO_DEV(OPTION, GTHSDPA, UHSO_STATIC_IFACE),
	/* Option GlobeSurfer iCON HSUPA */
	UHSO_DEV(OPTION, GSICONHSUPA, UHSO_STATIC_IFACE),
	/* Option GlobeTrotter HSUPA */
	UHSO_DEV(OPTION, GTHSUPA, UHSO_STATIC_IFACE),
	/* GE40x */
	UHSO_DEV(OPTION, GE40X, UHSO_AUTO_IFACE),
	UHSO_DEV(OPTION, GE40X_1, UHSO_AUTO_IFACE),
	UHSO_DEV(OPTION, GE40X_2, UHSO_AUTO_IFACE),
	UHSO_DEV(OPTION, GE40X_3, UHSO_AUTO_IFACE),
	/* Option GlobeSurfer iCON 401 */
	UHSO_DEV(OPTION, ICON401, UHSO_AUTO_IFACE),
	/* Option GlobeTrotter Module 382 */
	UHSO_DEV(OPTION, GMT382, UHSO_AUTO_IFACE),
	/* Option iCON EDGE */
	UHSO_DEV(OPTION, ICONEDGE, UHSO_STATIC_IFACE),
	/* Option Module HSxPA */
	UHSO_DEV(OPTION, MODHSXPA, UHSO_STATIC_IFACE),
	/* Option iCON 321 */
	UHSO_DEV(OPTION, ICON321, UHSO_STATIC_IFACE),
	/* Option iCON 322 */
	UHSO_DEV(OPTION, GTICON322, UHSO_STATIC_IFACE),
	/* Option iCON 505 */
	UHSO_DEV(OPTION, ICON505, UHSO_AUTO_IFACE),
#undef UHSO_DEV
};

SYSCTL_NODE(_hw_usb, OID_AUTO, uhso, CTLFLAG_RW, 0, "USB uhso");
static int uhso_autoswitch = 1;
SYSCTL_INT(_hw_usb_uhso, OID_AUTO, auto_switch, CTLFLAG_RW,
    &uhso_autoswitch, 0, "Automatically switch to modem mode");

#ifdef USB_DEBUG
#ifdef UHSO_DEBUG
static int uhso_debug = UHSO_DEBUG;
#else
static int uhso_debug = -1;
#endif

SYSCTL_INT(_hw_usb_uhso, OID_AUTO, debug, CTLFLAG_RW,
    &uhso_debug, 0, "Debug level");

#define UHSO_DPRINTF(n, x, ...) {\
	if (uhso_debug >= n) {\
		printf("%s: " x, __func__, ##__VA_ARGS__);\
	}\
}
#else
#define UHSO_DPRINTF(n, x, ...)
#endif

#ifdef UHSO_DEBUG_HEXDUMP
# define UHSO_HEXDUMP(_buf, _len) do { \
  { \
        size_t __tmp; \
        const char *__buf = (const char *)_buf; \
        for (__tmp = 0; __tmp < _len; __tmp++) \
                printf("%02hhx ", *__buf++); \
    printf("\n"); \
  } \
} while(0)
#else
# define UHSO_HEXDUMP(_buf, _len)
#endif

enum {
	UHSO_MUX_ENDPT_INTR = 0,
	UHSO_MUX_ENDPT_MAX
};

enum {
	UHSO_CTRL_READ = 0,
	UHSO_CTRL_WRITE,
	UHSO_CTRL_MAX
};

enum {
	UHSO_IFNET_READ = 0,
	UHSO_IFNET_WRITE,
	UHSO_IFNET_MAX
};

enum {
	UHSO_BULK_ENDPT_READ = 0,
	UHSO_BULK_ENDPT_WRITE,
	UHSO_BULK_ENDPT_INTR,
	UHSO_BULK_ENDPT_MAX
};

static usb_callback_t uhso_mux_intr_callback;
static usb_callback_t uhso_mux_read_callback;
static usb_callback_t uhso_mux_write_callback;
static usb_callback_t uhso_bs_read_callback;
static usb_callback_t uhso_bs_write_callback;
static usb_callback_t uhso_bs_intr_callback;
static usb_callback_t uhso_ifnet_read_callback;
static usb_callback_t uhso_ifnet_write_callback;

/* Config used for the default control pipes */
static const struct usb_config uhso_ctrl_config[UHSO_CTRL_MAX] = {
	[UHSO_CTRL_READ] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,
		.direction = UE_DIR_ANY,
		.flags = { .pipe_bof = 1, .short_xfer_ok = 1 },
		.bufsize = sizeof(struct usb_device_request) + 1024,
		.callback = &uhso_mux_read_callback
	},

	[UHSO_CTRL_WRITE] = {
		.type = UE_CONTROL,
		.endpoint = 0x00,
		.direction = UE_DIR_ANY,
		.flags = { .pipe_bof = 1, .force_short_xfer = 1 },
		.bufsize = sizeof(struct usb_device_request) + 1024,
		.timeout = 1000,
		.callback = &uhso_mux_write_callback
	}
};

/* Config for the multiplexed serial ports */
static const struct usb_config uhso_mux_config[UHSO_MUX_ENDPT_MAX] = {
	[UHSO_MUX_ENDPT_INTR] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = { .short_xfer_ok = 1 },
		.bufsize = 0,
		.callback = &uhso_mux_intr_callback,
	}
};

/* Config for the raw IP-packet interface */
static const struct usb_config uhso_ifnet_config[UHSO_IFNET_MAX] = {
	[UHSO_IFNET_READ] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = { .pipe_bof = 1, .short_xfer_ok = 1 },
		.bufsize = MCLBYTES,
		.callback = &uhso_ifnet_read_callback
	},
	[UHSO_IFNET_WRITE] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.flags = { .pipe_bof = 1, .force_short_xfer = 1 },
		.bufsize = MCLBYTES,
		.timeout = 5 * USB_MS_HZ,
		.callback = &uhso_ifnet_write_callback
	}
};

/* Config for interfaces with normal bulk serial ports */
static const struct usb_config uhso_bs_config[UHSO_BULK_ENDPT_MAX] = {
	[UHSO_BULK_ENDPT_READ] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = { .pipe_bof = 1, .short_xfer_ok = 1 },
		.bufsize = 4096,
		.callback = &uhso_bs_read_callback
	},

	[UHSO_BULK_ENDPT_WRITE] = {
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.flags = { .pipe_bof = 1, .force_short_xfer = 1 },
		.bufsize = 8192,
		.callback = &uhso_bs_write_callback
	},

	[UHSO_BULK_ENDPT_INTR] = {
		.type = UE_INTERRUPT,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.flags = { .short_xfer_ok = 1 },
		.bufsize = 0,
		.callback = &uhso_bs_intr_callback,
	}
};

static int  uhso_probe_iface(struct uhso_softc *, int,
    int (*probe)(struct uhso_softc *, int));
static int  uhso_probe_iface_auto(struct uhso_softc *, int);
static int  uhso_probe_iface_static(struct uhso_softc *, int);
static int  uhso_attach_muxserial(struct uhso_softc *, struct usb_interface *,
    int type);
static int  uhso_attach_bulkserial(struct uhso_softc *, struct usb_interface *,
    int type);
static int  uhso_attach_ifnet(struct uhso_softc *, struct usb_interface *,
    int type);
static void uhso_test_autoinst(void *, struct usb_device *,
		struct usb_attach_arg *);
static int  uhso_driver_loaded(struct module *, int, void *);

static void uhso_ucom_start_read(struct ucom_softc *);
static void uhso_ucom_stop_read(struct ucom_softc *);
static void uhso_ucom_start_write(struct ucom_softc *);
static void uhso_ucom_stop_write(struct ucom_softc *);
static void uhso_ucom_cfg_get_status(struct ucom_softc *, uint8_t *, uint8_t *);
static void uhso_ucom_cfg_set_dtr(struct ucom_softc *, uint8_t);
static void uhso_ucom_cfg_set_rts(struct ucom_softc *, uint8_t);
static void uhso_if_init(void *);
static void uhso_if_start(struct ifnet *);
static void uhso_if_stop(struct uhso_softc *);
static int  uhso_if_ioctl(struct ifnet *, u_long, caddr_t);
static int  uhso_if_output(struct ifnet *, struct mbuf *, struct sockaddr *,
    struct route *);
static void uhso_if_rxflush(void *);

static device_probe_t uhso_probe;
static device_attach_t uhso_attach;
static device_detach_t uhso_detach;

static device_method_t uhso_methods[] = {
	DEVMETHOD(device_probe,		uhso_probe),
	DEVMETHOD(device_attach,	uhso_attach),
	DEVMETHOD(device_detach,	uhso_detach),
	{ 0, 0 }
};

static driver_t uhso_driver = {
	"uhso",
	uhso_methods,
	sizeof(struct uhso_softc)
};

static devclass_t uhso_devclass;
DRIVER_MODULE(uhso, uhub, uhso_driver, uhso_devclass, uhso_driver_loaded, 0);
MODULE_DEPEND(uhso, ucom, 1, 1, 1);
MODULE_DEPEND(uhso, usb, 1, 1, 1);
MODULE_VERSION(uhso, 1);

static struct ucom_callback uhso_ucom_callback = {
	.ucom_cfg_get_status = &uhso_ucom_cfg_get_status,
	.ucom_cfg_set_dtr = &uhso_ucom_cfg_set_dtr,
	.ucom_cfg_set_rts = &uhso_ucom_cfg_set_rts,
	.ucom_start_read = uhso_ucom_start_read,
	.ucom_stop_read = uhso_ucom_stop_read,
	.ucom_start_write = uhso_ucom_start_write,
	.ucom_stop_write = uhso_ucom_stop_write
};

static int
uhso_probe(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (uaa->usb_mode != USB_MODE_HOST)
		return (ENXIO);
	if (uaa->info.bConfigIndex != 0)
		return (ENXIO);
	if (uaa->device->ddesc.bDeviceClass != 0xff)
		return (ENXIO);

	return (usbd_lookup_id_by_uaa(uhso_devs, sizeof(uhso_devs), uaa));
}

static int
uhso_attach(device_t self)
{
	struct uhso_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	struct usb_config_descriptor *cd;
	struct usb_interface_descriptor *id;
	struct sysctl_ctx_list *sctx;
	struct sysctl_oid *soid;
	struct sysctl_oid *tree, *tty_node;
	struct ucom_softc *ucom;
	struct uhso_tty *ht;
	int i, error, port;
	void *probe_f;
	usb_error_t uerr;
	char *desc;

	sc->sc_dev = self;
	sc->sc_udev = uaa->device;
	mtx_init(&sc->sc_mtx, "uhso", NULL, MTX_DEF);

	sc->sc_ucom = NULL;
	sc->sc_ttys = 0;

	cd = usbd_get_config_descriptor(uaa->device);
	id = usbd_get_interface_descriptor(uaa->iface);
	sc->sc_ctrl_iface_no = id->bInterfaceNumber;

	sc->sc_iface_no = uaa->info.bIfaceNum;
	sc->sc_iface_index = uaa->info.bIfaceIndex;

	/* Setup control pipe */
	uerr = usbd_transfer_setup(uaa->device,
	    &sc->sc_iface_index, sc->sc_ctrl_xfer,
	    uhso_ctrl_config, UHSO_CTRL_MAX, sc, &sc->sc_mtx);
	if (uerr) {
		device_printf(self, "Failed to setup control pipe: %s\n",
		    usbd_errstr(uerr));
		goto out;
	}

	if (USB_GET_DRIVER_INFO(uaa) == UHSO_STATIC_IFACE)
		probe_f = uhso_probe_iface_static;
	else if (USB_GET_DRIVER_INFO(uaa) == UHSO_AUTO_IFACE)
		probe_f = uhso_probe_iface_auto;
	else
		goto out;

	error = uhso_probe_iface(sc, uaa->info.bIfaceNum, probe_f);
	if (error != 0)
		goto out;

	sctx = device_get_sysctl_ctx(sc->sc_dev);
	soid = device_get_sysctl_tree(sc->sc_dev);

	SYSCTL_ADD_STRING(sctx, SYSCTL_CHILDREN(soid), OID_AUTO, "type",
	    CTLFLAG_RD, uhso_port[UHSO_IFACE_PORT(sc->sc_type)], 0,
	    "Port available at this interface");

	/*
	 * The default interface description on most Option devices isn't
	 * very helpful. So we skip device_set_usb_desc and set the
	 * device description manually.
	 */
	device_set_desc_copy(self, uhso_port_type[UHSO_IFACE_PORT_TYPE(sc->sc_type)]); 
	/* Announce device */
	device_printf(self, "<%s port> at <%s %s> on %s\n",
	    uhso_port_type[UHSO_IFACE_PORT_TYPE(sc->sc_type)],
	    uaa->device->manufacturer, uaa->device->product,
	    device_get_nameunit(uaa->device->bus->bdev));

	if (sc->sc_ttys > 0) {
		SYSCTL_ADD_INT(sctx, SYSCTL_CHILDREN(soid), OID_AUTO, "ports",
		    CTLFLAG_RD, &sc->sc_ttys, 0, "Number of attached serial ports");

		tree = SYSCTL_ADD_NODE(sctx, SYSCTL_CHILDREN(soid), OID_AUTO,
		    "port", CTLFLAG_RD, NULL, "Serial ports");
	}

	/*
	 * Loop through the number of found TTYs and create sysctl
	 * nodes for them.
	 */
	for (i = 0; i < sc->sc_ttys; i++) {
		ht = &sc->sc_tty[i];
		ucom = &sc->sc_ucom[i];

		if (UHSO_IFACE_USB_TYPE(sc->sc_type) & UHSO_IF_MUX)
			port = uhso_mux_port_map[ht->ht_muxport];
		else
			port = UHSO_IFACE_PORT_TYPE(sc->sc_type);

		desc = uhso_port_type_sysctl[port];

		tty_node = SYSCTL_ADD_NODE(sctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		    desc, CTLFLAG_RD, NULL, "");

		ht->ht_name[0] = 0;
		if (sc->sc_ttys == 1)
			snprintf(ht->ht_name, 32, "cuaU%d", ucom->sc_unit);
		else {
			snprintf(ht->ht_name, 32, "cuaU%d.%d",
			    ucom->sc_unit - ucom->sc_local_unit,
			    ucom->sc_local_unit);
		}

		desc = uhso_port_type[port];
		SYSCTL_ADD_STRING(sctx, SYSCTL_CHILDREN(tty_node), OID_AUTO,
		    "tty", CTLFLAG_RD, ht->ht_name, 0, "");
		SYSCTL_ADD_STRING(sctx, SYSCTL_CHILDREN(tty_node), OID_AUTO,
		    "desc", CTLFLAG_RD, desc, 0, "");

		if (bootverbose)
			device_printf(sc->sc_dev,
			    "\"%s\" port at %s\n", desc, ht->ht_name);
	}

	return (0);
out:
	uhso_detach(sc->sc_dev);
	return (ENXIO);
}

static int
uhso_detach(device_t self)
{
	struct uhso_softc *sc = device_get_softc(self);
	int i;

	usbd_transfer_unsetup(sc->sc_xfer, 3);
	usbd_transfer_unsetup(sc->sc_ctrl_xfer, UHSO_CTRL_MAX);
	if (sc->sc_ttys > 0) {
		ucom_detach(&sc->sc_super_ucom, sc->sc_ucom, sc->sc_ttys);

		for (i = 0; i < sc->sc_ttys; i++) {
			if (sc->sc_tty[i].ht_muxport != -1) {
				usbd_transfer_unsetup(sc->sc_tty[i].ht_xfer,
				    UHSO_CTRL_MAX);
			}
		}

		free(sc->sc_tty, M_USBDEV);
		free(sc->sc_ucom, M_USBDEV);
	}

	if (sc->sc_ifp != NULL) {
		callout_drain(&sc->sc_c);
		mtx_lock(&sc->sc_mtx);
		uhso_if_stop(sc);
		bpfdetach(sc->sc_ifp);
		if_detach(sc->sc_ifp);
		if_free(sc->sc_ifp);
		mtx_unlock(&sc->sc_mtx);
		usbd_transfer_unsetup(sc->sc_if_xfer, UHSO_IFNET_MAX);
	}

	mtx_destroy(&sc->sc_mtx);
	return (0);
}

static void
uhso_test_autoinst(void *arg, struct usb_device *udev,
    struct usb_attach_arg *uaa)
{
	struct usb_interface *iface;
	struct usb_interface_descriptor *id;

	if (uaa->dev_state != UAA_DEV_READY || !uhso_autoswitch)
		return;

	iface = usbd_get_iface(udev, 0);
	if (iface == NULL)
		return;
	id = iface->idesc;
	if (id == NULL || id->bInterfaceClass != UICLASS_MASS)
		return;
	if (usbd_lookup_id_by_uaa(uhso_devs, sizeof(uhso_devs), uaa))
		return;		/* no device match */

	if (usb_msc_eject(udev, 0, MSC_EJECT_REZERO) == 0) {
		/* success, mark the udev as disappearing */
		uaa->dev_state = UAA_DEV_EJECTING;
	}
}

static int
uhso_driver_loaded(struct module *mod, int what, void *arg)
{
	switch (what) {
	case MOD_LOAD:
		/* register our autoinstall handler */
		uhso_etag = EVENTHANDLER_REGISTER(usb_dev_configured,
		    uhso_test_autoinst, NULL, EVENTHANDLER_PRI_ANY);
		break;
	case MOD_UNLOAD:
		EVENTHANDLER_DEREGISTER(usb_dev_configured, uhso_etag);
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

/*
 * Probe the interface type by querying the device. The elements
 * of an array indicates the capabilities of a particular interface.
 * Returns a bit mask with the interface capabilities.
 */
static int
uhso_probe_iface_auto(struct uhso_softc *sc, int index)
{
	struct usb_device_request req;
	usb_error_t uerr;
	uint16_t actlen = 0;
	char port;
	char buf[17] = {0};

	req.bmRequestType = UT_READ_VENDOR_DEVICE;
	req.bRequest = 0x86;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 17);

	uerr = usbd_do_request_flags(sc->sc_udev, NULL, &req, buf,
	    0, &actlen, USB_MS_HZ);
	if (uerr != 0) {
		device_printf(sc->sc_dev, "usbd_do_request_flags failed: %s\n",
		    usbd_errstr(uerr));
		return (0);
	}

	UHSO_DPRINTF(1, "actlen=%d\n", actlen);
	UHSO_HEXDUMP(buf, 17);

	if (index < 0 || index > 16) {
		UHSO_DPRINTF(0, "Index %d out of range\n", index);
		return (0);
	}

	UHSO_DPRINTF(1, "index=%d, type=%x[%s]\n", index, buf[index],
	    uhso_port_type[(int)uhso_port_map[(int)buf[index]]]);

	if (buf[index] >= uhso_port_map_max)
		port = 0;
	else
		port = uhso_port_map[(int)buf[index]];

	switch (port) {
	case UHSO_PORT_TYPE_NETWORK:
		return (UHSO_IFACE_SPEC(UHSO_IF_NET | UHSO_IF_MUX,
		    UHSO_PORT_SERIAL | UHSO_PORT_NETWORK, port));
	case UHSO_PORT_TYPE_VOICE:
		/* Don't claim 'voice' ports */
		return (0);
	default:
		return (UHSO_IFACE_SPEC(UHSO_IF_BULK,
		    UHSO_PORT_SERIAL, port));
	}

	return (0);
}

static int
uhso_probe_iface_static(struct uhso_softc *sc, int index)
{
	struct usb_config_descriptor *cd;

	cd = usbd_get_config_descriptor(sc->sc_udev);
	if (cd->bNumInterface <= 3) {
		/* Cards with 3 or less interfaces */
		switch (index) {
		case 0:
			return UHSO_IFACE_SPEC(UHSO_IF_NET | UHSO_IF_MUX,
			    UHSO_PORT_SERIAL | UHSO_PORT_NETWORK,
			    UHSO_PORT_TYPE_NETWORK);
		case 1:
			return UHSO_IFACE_SPEC(UHSO_IF_BULK,
			    UHSO_PORT_SERIAL, UHSO_PORT_TYPE_DIAG);
		case 2:
			return UHSO_IFACE_SPEC(UHSO_IF_BULK,
			    UHSO_PORT_SERIAL, UHSO_PORT_TYPE_MODEM);
		}
	} else {
		/* Cards with 4 interfaces */
		switch (index) {
		case 0:
			return UHSO_IFACE_SPEC(UHSO_IF_NET | UHSO_IF_MUX,
			    UHSO_PORT_SERIAL | UHSO_PORT_NETWORK,
			    UHSO_PORT_TYPE_NETWORK);
		case 1:
			return UHSO_IFACE_SPEC(UHSO_IF_BULK,
			    UHSO_PORT_SERIAL, UHSO_PORT_TYPE_DIAG2);
		case 2:
			return UHSO_IFACE_SPEC(UHSO_IF_BULK,
			    UHSO_PORT_SERIAL, UHSO_PORT_TYPE_MODEM);
		case 3:
			return UHSO_IFACE_SPEC(UHSO_IF_BULK,
			    UHSO_PORT_SERIAL, UHSO_PORT_TYPE_DIAG);
		}
	}
	return (0);
}

/*
 * Probes an interface for its particular capabilities and attaches if
 * it's a supported interface.
 */
static int
uhso_probe_iface(struct uhso_softc *sc, int index,
    int (*probe)(struct uhso_softc *, int))
{
	struct usb_interface *iface;
	int type, error;

	UHSO_DPRINTF(1, "Probing for interface %d, probe_func=%p\n", index, probe);

	type = probe(sc, index);
	UHSO_DPRINTF(1, "Probe result %x\n", type);
	if (type <= 0)
		return (ENXIO);

	sc->sc_type = type;
	iface = usbd_get_iface(sc->sc_udev, index);

	if (UHSO_IFACE_PORT_TYPE(type) == UHSO_PORT_TYPE_NETWORK) {
		error = uhso_attach_ifnet(sc, iface, type);
		if (error) {
			UHSO_DPRINTF(1, "uhso_attach_ifnet failed");
			return (ENXIO);
		}

		/*
		 * If there is an additional interrupt endpoint on this
		 * interface then we most likely have a multiplexed serial port
		 * available.
		 */
		if (iface->idesc->bNumEndpoints < 3) {
			sc->sc_type = UHSO_IFACE_SPEC( 
			    UHSO_IFACE_USB_TYPE(type) & ~UHSO_IF_MUX,
			    UHSO_IFACE_PORT(type) & ~UHSO_PORT_SERIAL,
			    UHSO_IFACE_PORT_TYPE(type));
			return (0);
		}

		UHSO_DPRINTF(1, "Trying to attach mux. serial\n");
		error = uhso_attach_muxserial(sc, iface, type);
		if (error == 0 && sc->sc_ttys > 0) {
			error = ucom_attach(&sc->sc_super_ucom, sc->sc_ucom,
			    sc->sc_ttys, sc, &uhso_ucom_callback, &sc->sc_mtx);
			if (error) {
				device_printf(sc->sc_dev, "ucom_attach failed\n");
				return (ENXIO);
			}

			mtx_lock(&sc->sc_mtx);
			usbd_transfer_start(sc->sc_xfer[UHSO_MUX_ENDPT_INTR]);
			mtx_unlock(&sc->sc_mtx);
		}
	} else if ((UHSO_IFACE_USB_TYPE(type) & UHSO_IF_BULK) &&
	    UHSO_IFACE_PORT(type) & UHSO_PORT_SERIAL) {

		error = uhso_attach_bulkserial(sc, iface, type);
		if (error)
			return (ENXIO);

		error = ucom_attach(&sc->sc_super_ucom, sc->sc_ucom,
		    sc->sc_ttys, sc, &uhso_ucom_callback, &sc->sc_mtx);
		if (error) {
			device_printf(sc->sc_dev, "ucom_attach failed\n");
			return (ENXIO);
		}
	}
	else {
		UHSO_DPRINTF(0, "Unknown type %x\n", type);
		return (ENXIO);
	}

	return (0);
}

/*
 * Expands allocated memory to fit an additional TTY.
 * Two arrays are kept with matching indexes, one for ucom and one
 * for our private data.
 */
static int
uhso_alloc_tty(struct uhso_softc *sc)
{

	sc->sc_ttys++;
	sc->sc_tty = reallocf(sc->sc_tty, sizeof(struct uhso_tty) * sc->sc_ttys,
	    M_USBDEV, M_WAITOK | M_ZERO);
	if (sc->sc_tty == NULL)
		return (-1);

	sc->sc_ucom = reallocf(sc->sc_ucom,
	    sizeof(struct ucom_softc) * sc->sc_ttys, M_USBDEV, M_WAITOK | M_ZERO);
	if (sc->sc_ucom == NULL)
		return (-1);

	sc->sc_tty[sc->sc_ttys - 1].ht_sc = sc;

	UHSO_DPRINTF(1, "Allocated TTY %d\n", sc->sc_ttys - 1);	
	return (sc->sc_ttys - 1);
}

/*
 * Attach a multiplexed serial port
 * Data is read/written with requests on the default control pipe. An interrupt
 * endpoint returns when there is new data to be read.
 */
static int
uhso_attach_muxserial(struct uhso_softc *sc, struct usb_interface *iface,
    int type)
{
	struct usb_descriptor *desc;
	int i, port, tty;
	usb_error_t uerr;

	/*
	 * The class specific interface (type 0x24) descriptor subtype field
	 * contains a bitmask that specifies which (and how many) ports that
	 * are available through this multiplexed serial port.
 	 */
	desc = usbd_find_descriptor(sc->sc_udev, NULL,
	    iface->idesc->bInterfaceNumber, UDESC_CS_INTERFACE, 0xff, 0, 0);
	if (desc == NULL) {
		UHSO_DPRINTF(0, "Failed to find UDESC_CS_INTERFACE\n");
		return (ENXIO);
	}

	UHSO_DPRINTF(1, "Mux port mask %x\n", desc->bDescriptorSubtype);
	if (desc->bDescriptorSubtype == 0)
		return (ENXIO);

	/*
	 * The bitmask is one octet, loop through the number of
	 * bits that are set and create a TTY for each.
	 */
	for (i = 0; i < 8; i++) {
		port = (1 << i);
		if ((port & desc->bDescriptorSubtype) == port) {
			UHSO_DPRINTF(2, "Found mux port %x (%d)\n", port, i);
			tty = uhso_alloc_tty(sc);
			if (tty < 0)
				return (ENOMEM);
			sc->sc_tty[tty].ht_muxport = i;
			uerr = usbd_transfer_setup(sc->sc_udev,	
			    &sc->sc_iface_index, sc->sc_tty[tty].ht_xfer,
			    uhso_ctrl_config, UHSO_CTRL_MAX, sc, &sc->sc_mtx);
			if (uerr) {
				device_printf(sc->sc_dev,
				    "Failed to setup control pipe: %s\n",
				    usbd_errstr(uerr));
				return (ENXIO);
			}
		}
	}

	/* Setup the intr. endpoint */
	uerr = usbd_transfer_setup(sc->sc_udev,
	    &iface->idesc->bInterfaceNumber, sc->sc_xfer,
	    uhso_mux_config, 1, sc, &sc->sc_mtx);
	if (uerr)
		return (ENXIO);

	return (0);
}

/*
 * Interrupt callback for the multiplexed serial port. Indicates
 * which serial port has data waiting.
 */
static void
uhso_mux_intr_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct usb_page_cache *pc;
	struct usb_page_search res;
	struct uhso_softc *sc = usbd_xfer_softc(xfer);
	unsigned int i, mux;

	UHSO_DPRINTF(3, "status %d\n", USB_GET_STATE(xfer));

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		/*
		 * The multiplexed port number can be found at the first byte.
		 * It contains a bit mask, we transform this in to an integer.
		 */
		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_get_page(pc, 0, &res);

		i = *((unsigned char *)res.buffer);
		mux = 0;
		while (i >>= 1) {
			mux++;
		}

		UHSO_DPRINTF(3, "mux port %d (%d)\n", mux, i);
		if (mux > UHSO_MPORT_TYPE_NOMAX)
			break;

		/* Issue a read for this serial port */
		usbd_xfer_set_priv(
		    sc->sc_tty[mux].ht_xfer[UHSO_CTRL_READ],
		    &sc->sc_tty[mux]);
		usbd_transfer_start(sc->sc_tty[mux].ht_xfer[UHSO_CTRL_READ]);

		break;
	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		break;
	default:
		UHSO_DPRINTF(0, "error: %s\n", usbd_errstr(error));
		if (error == USB_ERR_CANCELLED)
			break;

		usbd_xfer_set_stall(xfer);
		goto tr_setup;
	}
}

static void
uhso_mux_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uhso_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	struct usb_device_request req;
	struct uhso_tty *ht;
	int actlen, len;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	UHSO_DPRINTF(3, "status %d\n", USB_GET_STATE(xfer));

	ht = usbd_xfer_get_priv(xfer);
	UHSO_DPRINTF(3, "ht=%p open=%d\n", ht, ht->ht_open);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		/* Got data, send to ucom */
		pc = usbd_xfer_get_frame(xfer, 1);
		len = usbd_xfer_frame_len(xfer, 1);

		UHSO_DPRINTF(3, "got %d bytes on mux port %d\n", len,
		    ht->ht_muxport);
		if (len <= 0) {
			usbd_transfer_start(sc->sc_xfer[UHSO_MUX_ENDPT_INTR]);
			break;
		}

		/* Deliver data if the TTY is open, discard otherwise */
		if (ht->ht_open)
			ucom_put_data(&sc->sc_ucom[ht->ht_muxport], pc, 0, len);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		bzero(&req, sizeof(struct usb_device_request));
		req.bmRequestType = UT_READ_CLASS_INTERFACE;
		req.bRequest = UCDC_GET_ENCAPSULATED_RESPONSE;
		USETW(req.wValue, 0);
		USETW(req.wIndex, ht->ht_muxport);
		USETW(req.wLength, 1024);

		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_in(pc, 0, &req, sizeof(req));

		usbd_xfer_set_frame_len(xfer, 0, sizeof(req));
		usbd_xfer_set_frame_len(xfer, 1, 1024);
		usbd_xfer_set_frames(xfer, 2);
		usbd_transfer_submit(xfer);
		break;
	default:
		UHSO_DPRINTF(0, "error: %s\n", usbd_errstr(error));
		if (error == USB_ERR_CANCELLED)
			break;
		usbd_xfer_set_stall(xfer);
		goto tr_setup;
	}
}

static void
uhso_mux_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uhso_softc *sc = usbd_xfer_softc(xfer);
	struct uhso_tty *ht;
	struct usb_page_cache *pc;
	struct usb_device_request req;
	int actlen;
	struct usb_page_search res;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	ht = usbd_xfer_get_priv(xfer);
	UHSO_DPRINTF(3, "status=%d, using mux port %d\n",
	    USB_GET_STATE(xfer), ht->ht_muxport);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		UHSO_DPRINTF(3, "wrote %zd data bytes to muxport %d\n",
		    actlen - sizeof(struct usb_device_request) ,
		    ht->ht_muxport);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
		pc = usbd_xfer_get_frame(xfer, 1);
		if (ucom_get_data(&sc->sc_ucom[ht->ht_muxport], pc,
		    0, 32, &actlen)) {

			usbd_get_page(pc, 0, &res);

			bzero(&req, sizeof(struct usb_device_request));
			req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
			req.bRequest = UCDC_SEND_ENCAPSULATED_COMMAND;
			USETW(req.wValue, 0);
			USETW(req.wIndex, ht->ht_muxport);
			USETW(req.wLength, actlen);

			pc = usbd_xfer_get_frame(xfer, 0);
			usbd_copy_in(pc, 0, &req, sizeof(req));

			usbd_xfer_set_frame_len(xfer, 0, sizeof(req));
			usbd_xfer_set_frame_len(xfer, 1, actlen);
			usbd_xfer_set_frames(xfer, 2);

			UHSO_DPRINTF(3, "Prepared %d bytes for transmit "
			    "on muxport %d\n", actlen, ht->ht_muxport);

			usbd_transfer_submit(xfer);
		}
		break;
	default:
		UHSO_DPRINTF(0, "error: %s\n", usbd_errstr(error));
		if (error == USB_ERR_CANCELLED)
			break;
		break;
	}
}

static int
uhso_attach_bulkserial(struct uhso_softc *sc, struct usb_interface *iface,
    int type)
{
	usb_error_t uerr;
	int tty;

	/* Try attaching RD/WR/INTR first */
	uerr = usbd_transfer_setup(sc->sc_udev,
	    &iface->idesc->bInterfaceNumber, sc->sc_xfer,
	    uhso_bs_config, UHSO_BULK_ENDPT_MAX, sc, &sc->sc_mtx);
	if (uerr) {
		/* Try only RD/WR */
		uerr = usbd_transfer_setup(sc->sc_udev,
		    &iface->idesc->bInterfaceNumber, sc->sc_xfer,
		    uhso_bs_config, UHSO_BULK_ENDPT_MAX - 1, sc, &sc->sc_mtx);
	}
	if (uerr) {
		UHSO_DPRINTF(0, "usbd_transfer_setup failed");
		return (-1);
	}

	tty = uhso_alloc_tty(sc);
	if (tty < 0) {
		usbd_transfer_unsetup(sc->sc_xfer, UHSO_BULK_ENDPT_MAX);
		return (ENOMEM);
	}

	sc->sc_tty[tty].ht_muxport = -1;
	return (0);
}

static void
uhso_bs_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uhso_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	UHSO_DPRINTF(3, "status %d, actlen=%d\n", USB_GET_STATE(xfer), actlen);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		pc = usbd_xfer_get_frame(xfer, 0);
		ucom_put_data(&sc->sc_ucom[0], pc, 0, actlen);
		/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
	break;
	default:
		UHSO_DPRINTF(0, "error: %s\n", usbd_errstr(error));
		if (error == USB_ERR_CANCELLED)
			break;
		usbd_xfer_set_stall(xfer);
		goto tr_setup;
	}
}

static void
uhso_bs_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uhso_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	UHSO_DPRINTF(3, "status %d, actlen=%d\n", USB_GET_STATE(xfer), actlen);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
	case USB_ST_SETUP:
tr_setup:
		pc = usbd_xfer_get_frame(xfer, 0);
		if (ucom_get_data(&sc->sc_ucom[0], pc, 0, 8192, &actlen)) {
			usbd_xfer_set_frame_len(xfer, 0, actlen);
			usbd_transfer_submit(xfer);
		}
		break;
	break;
	default:
		UHSO_DPRINTF(0, "error: %s\n", usbd_errstr(error));
		if (error == USB_ERR_CANCELLED)
			break;
		usbd_xfer_set_stall(xfer);
		goto tr_setup;
	}
}

static void
uhso_bs_cfg(struct uhso_softc *sc)
{
	struct usb_device_request req;
	usb_error_t uerr;

	if (!(UHSO_IFACE_USB_TYPE(sc->sc_type) & UHSO_IF_BULK))
		return;

	req.bmRequestType = UT_WRITE_CLASS_INTERFACE;
	req.bRequest = UCDC_SET_CONTROL_LINE_STATE;
	USETW(req.wValue, sc->sc_line);
	USETW(req.wIndex, sc->sc_iface_no);
	USETW(req.wLength, 0);

	uerr = ucom_cfg_do_request(sc->sc_udev, &sc->sc_ucom[0], &req, NULL, 0, 1000);
	if (uerr != 0) {
		device_printf(sc->sc_dev, "failed to set ctrl line state to "
		    "0x%02x: %s\n", sc->sc_line, usbd_errstr(uerr));
	}
}

static void
uhso_bs_intr_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uhso_softc *sc = usbd_xfer_softc(xfer);
	struct usb_page_cache *pc;
	int actlen;
	struct usb_cdc_notification cdc;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);
	UHSO_DPRINTF(3, "status %d, actlen=%d\n", USB_GET_STATE(xfer), actlen);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		if (actlen < UCDC_NOTIFICATION_LENGTH) {
			UHSO_DPRINTF(0, "UCDC notification too short: %d\n", actlen);
			goto tr_setup;
		}
		else if (actlen > sizeof(struct usb_cdc_notification)) {
			UHSO_DPRINTF(0, "UCDC notification too large: %d\n", actlen);
			actlen = sizeof(struct usb_cdc_notification);
		}

		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_copy_out(pc, 0, &cdc, actlen);

		if (UGETW(cdc.wIndex) != sc->sc_iface_no) {
			UHSO_DPRINTF(0, "Interface mismatch, got %d expected %d\n",
			    UGETW(cdc.wIndex), sc->sc_iface_no);
			goto tr_setup;
		}

		if (cdc.bmRequestType == UCDC_NOTIFICATION &&
		    cdc.bNotification == UCDC_N_SERIAL_STATE) {
			UHSO_DPRINTF(2, "notify = 0x%02x\n", cdc.data[0]);

			sc->sc_msr = 0;
			sc->sc_lsr = 0;
			if (cdc.data[0] & UCDC_N_SERIAL_RI)
				sc->sc_msr |= SER_RI;
			if (cdc.data[0] & UCDC_N_SERIAL_DSR)
				sc->sc_msr |= SER_DSR;	
			if (cdc.data[0] & UCDC_N_SERIAL_DCD)
				sc->sc_msr |= SER_DCD;

			ucom_status_change(&sc->sc_ucom[0]);
		}
	case USB_ST_SETUP:
tr_setup:
	default:
		if (error == USB_ERR_CANCELLED)
			break;
		usbd_xfer_set_stall(xfer);
		goto tr_setup;
	}
}

static void
uhso_ucom_cfg_get_status(struct ucom_softc *ucom, uint8_t *lsr, uint8_t *msr)
{
	struct uhso_softc *sc = ucom->sc_parent;

	*lsr = sc->sc_lsr;
	*msr = sc->sc_msr;
}

static void
uhso_ucom_cfg_set_dtr(struct ucom_softc *ucom, uint8_t onoff)
{
	struct uhso_softc *sc = ucom->sc_parent;

	if (!(UHSO_IFACE_USB_TYPE(sc->sc_type) & UHSO_IF_BULK))
		return;

	if (onoff)
		sc->sc_line |= UCDC_LINE_DTR;
	else
		sc->sc_line &= ~UCDC_LINE_DTR;

	uhso_bs_cfg(sc);
}

static void
uhso_ucom_cfg_set_rts(struct ucom_softc *ucom, uint8_t onoff)
{
	struct uhso_softc *sc = ucom->sc_parent;

	if (!(UHSO_IFACE_USB_TYPE(sc->sc_type) & UHSO_IF_BULK))
		return;

	if (onoff)
		sc->sc_line |= UCDC_LINE_RTS;
	else
		sc->sc_line &= ~UCDC_LINE_RTS;

	uhso_bs_cfg(sc);
}

static void
uhso_ucom_start_read(struct ucom_softc *ucom)
{
	struct uhso_softc *sc = ucom->sc_parent;

	UHSO_DPRINTF(3, "unit=%d, local_unit=%d\n",
	    ucom->sc_unit, ucom->sc_local_unit);

	if (UHSO_IFACE_USB_TYPE(sc->sc_type) & UHSO_IF_MUX) {
		sc->sc_tty[ucom->sc_local_unit].ht_open = 1;
		usbd_transfer_start(sc->sc_xfer[UHSO_MUX_ENDPT_INTR]);
	}
	else if (UHSO_IFACE_USB_TYPE(sc->sc_type) & UHSO_IF_BULK) {
		sc->sc_tty[0].ht_open = 1;
		usbd_transfer_start(sc->sc_xfer[UHSO_BULK_ENDPT_READ]);
		if (sc->sc_xfer[UHSO_BULK_ENDPT_INTR] != NULL)
			usbd_transfer_start(sc->sc_xfer[UHSO_BULK_ENDPT_INTR]);
	}
}

static void
uhso_ucom_stop_read(struct ucom_softc *ucom)
{

	struct uhso_softc *sc = ucom->sc_parent;

	if (UHSO_IFACE_USB_TYPE(sc->sc_type) & UHSO_IF_MUX) {
		sc->sc_tty[ucom->sc_local_unit].ht_open = 0;
		usbd_transfer_stop(
		    sc->sc_tty[ucom->sc_local_unit].ht_xfer[UHSO_CTRL_READ]);
	}
	else if (UHSO_IFACE_USB_TYPE(sc->sc_type) & UHSO_IF_BULK) {
		sc->sc_tty[0].ht_open = 0;
		usbd_transfer_start(sc->sc_xfer[UHSO_BULK_ENDPT_READ]);
		if (sc->sc_xfer[UHSO_BULK_ENDPT_INTR] != NULL)
			usbd_transfer_stop(sc->sc_xfer[UHSO_BULK_ENDPT_INTR]);
	}
}

static void
uhso_ucom_start_write(struct ucom_softc *ucom)
{
	struct uhso_softc *sc = ucom->sc_parent;

	if (UHSO_IFACE_USB_TYPE(sc->sc_type) & UHSO_IF_MUX) {
		UHSO_DPRINTF(3, "local unit %d\n", ucom->sc_local_unit);

		usbd_transfer_start(sc->sc_xfer[UHSO_MUX_ENDPT_INTR]);

		usbd_xfer_set_priv(
		    sc->sc_tty[ucom->sc_local_unit].ht_xfer[UHSO_CTRL_WRITE],
		    &sc->sc_tty[ucom->sc_local_unit]);
		usbd_transfer_start(
		    sc->sc_tty[ucom->sc_local_unit].ht_xfer[UHSO_CTRL_WRITE]);

	}
	else if (UHSO_IFACE_USB_TYPE(sc->sc_type) & UHSO_IF_BULK) {
		usbd_transfer_start(sc->sc_xfer[UHSO_BULK_ENDPT_WRITE]);
	}
}

static void
uhso_ucom_stop_write(struct ucom_softc *ucom)
{
	struct uhso_softc *sc = ucom->sc_parent;

	if (UHSO_IFACE_USB_TYPE(sc->sc_type) & UHSO_IF_MUX) {
		usbd_transfer_stop(
		    sc->sc_tty[ucom->sc_local_unit].ht_xfer[UHSO_CTRL_WRITE]);
	}
	else if (UHSO_IFACE_USB_TYPE(sc->sc_type) & UHSO_IF_BULK) {
		usbd_transfer_stop(sc->sc_xfer[UHSO_BULK_ENDPT_WRITE]);
	}
}

static int uhso_attach_ifnet(struct uhso_softc *sc, struct usb_interface *iface,
    int type)
{
	struct ifnet *ifp;
	usb_error_t uerr;
	struct sysctl_ctx_list *sctx;
	struct sysctl_oid *soid;

	uerr = usbd_transfer_setup(sc->sc_udev,
	    &iface->idesc->bInterfaceNumber, sc->sc_if_xfer,
	    uhso_ifnet_config, UHSO_IFNET_MAX, sc, &sc->sc_mtx);
	if (uerr) {
		UHSO_DPRINTF(0, "usbd_transfer_setup failed: %s\n",
		    usbd_errstr(uerr));
		return (-1);
	}

	sc->sc_ifp = ifp = if_alloc(IFT_OTHER);
	if (sc->sc_ifp == NULL) {
		device_printf(sc->sc_dev, "if_alloc() failed\n");
		return (-1);
	}

	callout_init_mtx(&sc->sc_c, &sc->sc_mtx, 0);
	mtx_lock(&sc->sc_mtx);
	callout_reset(&sc->sc_c, 1, uhso_if_rxflush, sc);
	mtx_unlock(&sc->sc_mtx);

	if_initname(ifp, device_get_name(sc->sc_dev), device_get_unit(sc->sc_dev));
	ifp->if_mtu = UHSO_MAX_MTU;
	ifp->if_ioctl = uhso_if_ioctl;
	ifp->if_init = uhso_if_init;
	ifp->if_start = uhso_if_start;
	ifp->if_output = uhso_if_output;
	ifp->if_flags = 0;
	ifp->if_softc = sc;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	if_attach(ifp);
	bpfattach(ifp, DLT_RAW, 0);

	sctx = device_get_sysctl_ctx(sc->sc_dev);
	soid = device_get_sysctl_tree(sc->sc_dev);
	/* Unlocked read... */
	SYSCTL_ADD_STRING(sctx, SYSCTL_CHILDREN(soid), OID_AUTO, "netif",
	    CTLFLAG_RD, ifp->if_xname, 0, "Attached network interface");

	return (0);
}

static void
uhso_ifnet_read_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uhso_softc *sc = usbd_xfer_softc(xfer);
	struct mbuf *m;	
	struct usb_page_cache *pc;
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	UHSO_DPRINTF(3, "status=%d, actlen=%d\n", USB_GET_STATE(xfer), actlen);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		if (actlen > 0 && (sc->sc_ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			pc = usbd_xfer_get_frame(xfer, 0);
			m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
			usbd_copy_out(pc, 0, mtod(m, uint8_t *), actlen);
			m->m_pkthdr.len = m->m_len = actlen;
			/* Enqueue frame for further processing */
			_IF_ENQUEUE(&sc->sc_rxq, m);
			if (!callout_pending(&sc->sc_c) ||
			    !callout_active(&sc->sc_c)) {
				callout_schedule(&sc->sc_c, 1);
			}
		}
	/* FALLTHROUGH */
	case USB_ST_SETUP:
tr_setup:
		usbd_xfer_set_frame_len(xfer, 0, usbd_xfer_max_len(xfer));
		usbd_transfer_submit(xfer);
		break;
	default:
		UHSO_DPRINTF(0, "error: %s\n", usbd_errstr(error));
		if (error == USB_ERR_CANCELLED)
			break;
		usbd_xfer_set_stall(xfer);
		goto tr_setup;
	}
}

/*
 * Deferred RX processing, called with mutex locked.
 *
 * Each frame we receive might contain several small ip-packets as well
 * as partial ip-packets. We need to separate/assemble them into individual
 * packets before sending them to the ip-layer.
 */
static void
uhso_if_rxflush(void *arg)
{
	struct uhso_softc *sc = arg;
	struct ifnet *ifp = sc->sc_ifp;
	uint8_t *cp;
	struct mbuf *m, *m0, *mwait;
	struct ip *ip;
#ifdef INET6
	struct ip6_hdr *ip6;
#endif
	uint16_t iplen;
	int len, isr;

	m = NULL;
	mwait = sc->sc_mwait;
	for (;;) {
		if (m == NULL) {
			_IF_DEQUEUE(&sc->sc_rxq, m);
			if (m == NULL)
				break;
			UHSO_DPRINTF(3, "dequeue m=%p, len=%d\n", m, m->m_len);
		}
		mtx_unlock(&sc->sc_mtx);

		/* Do we have a partial packet waiting? */
		if (mwait != NULL) {
			m0 = mwait;
			mwait = NULL;

			UHSO_DPRINTF(3, "partial m0=%p(%d), concat w/ m=%p(%d)\n",
			    m0, m0->m_len, m, m->m_len);
			len = m->m_len + m0->m_len;

			/* Concat mbufs and fix headers */
			m_cat(m0, m);
			m0->m_pkthdr.len = len;
			m->m_flags &= ~M_PKTHDR;

			m = m_pullup(m0, sizeof(struct ip));
			if (m == NULL) {
				ifp->if_ierrors++;
				UHSO_DPRINTF(0, "m_pullup failed\n");
				mtx_lock(&sc->sc_mtx);
				continue;
			}
			UHSO_DPRINTF(3, "Constructed mbuf=%p, len=%d\n",
			    m, m->m_pkthdr.len);
		}

		cp = mtod(m, uint8_t *);
		ip = (struct ip *)cp;
#ifdef INET6
		ip6 = (struct ip6_hdr *)cp;
#endif

		/* Check for IPv4 */
		if (ip->ip_v == IPVERSION) {
			iplen = htons(ip->ip_len);
			isr = NETISR_IP;
		}
#ifdef INET6
		/* Check for IPv6 */
		else if ((ip6->ip6_vfc & IPV6_VERSION_MASK) == IPV6_VERSION) {
			iplen = htons(ip6->ip6_plen);
			isr = NETISR_IPV6;
		}
#endif
		else {
			UHSO_DPRINTF(0, "got unexpected ip version %d, "
			    "m=%p, len=%d\n", (*cp & 0xf0) >> 4, m, m->m_len);
			ifp->if_ierrors++;
			UHSO_HEXDUMP(cp, 4);
			m_freem(m);
			m = NULL;
			mtx_lock(&sc->sc_mtx);
			continue;
		}

		if (iplen == 0) {
			UHSO_DPRINTF(0, "Zero IP length\n");
			ifp->if_ierrors++;
			m_freem(m);
			m = NULL;
			mtx_lock(&sc->sc_mtx);
			continue;
		}

		UHSO_DPRINTF(3, "m=%p, len=%d, cp=%p, iplen=%d\n",
		    m, m->m_pkthdr.len, cp, iplen);

		m0 = NULL;

		/* More IP packets in this mbuf */
		if (iplen < m->m_pkthdr.len) {
			m0 = m;

			/*
			 * Allocate a new mbuf for this IP packet and
			 * copy the IP-packet into it.
			 */
			m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
			bcopy(mtod(m0, uint8_t *), mtod(m, uint8_t *), iplen);
			m->m_pkthdr.len = m->m_len = iplen;

			/* Adjust the size of the original mbuf */
			m_adj(m0, iplen);
			m0 = m_defrag(m0, M_WAIT);

			UHSO_DPRINTF(3, "New mbuf=%p, len=%d/%d, m0=%p, "
			    "m0_len=%d/%d\n", m, m->m_pkthdr.len, m->m_len,
			    m0, m0->m_pkthdr.len, m0->m_len);
		}
		else if (iplen > m->m_pkthdr.len) {
			UHSO_DPRINTF(3, "Deferred mbuf=%p, len=%d\n",
			    m, m->m_pkthdr.len);
			mwait = m;
			m = NULL;
			mtx_lock(&sc->sc_mtx);
			continue;
		}

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;

		/* Dispatch to IP layer */
		BPF_MTAP(sc->sc_ifp, m);
		netisr_dispatch(isr, m);
		m = m0 != NULL ? m0 : NULL;
		mtx_lock(&sc->sc_mtx);
	}
	sc->sc_mwait = mwait;
}

static void
uhso_ifnet_write_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct uhso_softc *sc = usbd_xfer_softc(xfer);
	struct ifnet *ifp = sc->sc_ifp;
	struct usb_page_cache *pc;
	struct mbuf *m;
	int actlen;

	usbd_xfer_status(xfer, &actlen, NULL, NULL, NULL);

	UHSO_DPRINTF(3, "status %d, actlen=%d\n", USB_GET_STATE(xfer), actlen);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		ifp->if_opackets++;
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	case USB_ST_SETUP:
tr_setup:
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		ifp->if_drv_flags |= IFF_DRV_OACTIVE;

		if (m->m_pkthdr.len > MCLBYTES)
			m->m_pkthdr.len = MCLBYTES;

		usbd_xfer_set_frame_len(xfer, 0, m->m_pkthdr.len);
		pc = usbd_xfer_get_frame(xfer, 0);
		usbd_m_copy_in(pc, 0, m, 0, m->m_pkthdr.len);
		usbd_transfer_submit(xfer);

		BPF_MTAP(ifp, m);
		m_freem(m);
		break;
	default:
		UHSO_DPRINTF(0, "error: %s\n", usbd_errstr(error));
		if (error == USB_ERR_CANCELLED)
			break;
		usbd_xfer_set_stall(xfer);
		goto tr_setup;
	}
}

static int
uhso_if_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct uhso_softc *sc;

	sc = ifp->if_softc;

	switch (cmd) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				uhso_if_init(sc);
			}
		}
		else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				mtx_lock(&sc->sc_mtx);
				uhso_if_stop(sc);
				mtx_unlock(&sc->sc_mtx);
			}
		}
		break;
	case SIOCSIFADDR:
	case SIOCSIFDSTADDR:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;
	default:
		return (EINVAL);
	}
	return (0);
}

static void
uhso_if_init(void *priv)
{
	struct uhso_softc *sc = priv;
	struct ifnet *ifp = sc->sc_ifp;

	mtx_lock(&sc->sc_mtx);
	uhso_if_stop(sc);
	ifp = sc->sc_ifp;
	ifp->if_flags |= IFF_UP;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	mtx_unlock(&sc->sc_mtx);

	UHSO_DPRINTF(2, "ifnet initialized\n");
}

static int
uhso_if_output(struct ifnet *ifp, struct mbuf *m0, struct sockaddr *dst,
    struct route *ro)
{
	int error;

	/* Only IPv4/6 support */
	if (dst->sa_family != AF_INET
#ifdef INET6
	   && dst->sa_family != AF_INET6
#endif
	 ) {
		return (EAFNOSUPPORT);
	}

	error = (ifp->if_transmit)(ifp, m0);
	if (error) {
		ifp->if_oerrors++;
		return (ENOBUFS);
	}
	ifp->if_opackets++;
	return (0);
}

static void
uhso_if_start(struct ifnet *ifp)
{
	struct uhso_softc *sc = ifp->if_softc;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		UHSO_DPRINTF(1, "Not running\n");
		return;
	}

	mtx_lock(&sc->sc_mtx);
	usbd_transfer_start(sc->sc_if_xfer[UHSO_IFNET_READ]);
	usbd_transfer_start(sc->sc_if_xfer[UHSO_IFNET_WRITE]);
	mtx_unlock(&sc->sc_mtx);
	UHSO_DPRINTF(3, "interface started\n");
}

static void
uhso_if_stop(struct uhso_softc *sc)
{

	usbd_transfer_stop(sc->sc_if_xfer[UHSO_IFNET_READ]);
	usbd_transfer_stop(sc->sc_if_xfer[UHSO_IFNET_WRITE]);
	sc->sc_ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
}
