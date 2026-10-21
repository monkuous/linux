/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/console.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/tty_flip.h>

#define DRIVER_NAME	"xrarch-serial"
#define DEV_NAME	"ttyXRS"
#define UART_NR		2

#define UART_CMD_REG	0x00
#define UART_DATA_REG	0x04

#define UART_IRQ_ON	0x03
#define UART_IRQ_OFF	0x04

#define UART_TX_FIFO_SIZE	16

struct xrarch_port {
	struct uart_port port;
	u32 default_speed;
};

static struct xrarch_port *xrarch_serial_ports[UART_NR];

static void xrarch_serial_write(struct uart_port *port, unsigned long reg,
				u32 v)
{
	writel(v, port->membase + reg);
}

static u32 xrarch_serial_read(struct uart_port *port, unsigned long reg)
{
	return readl(port->membase + reg);
}

static bool xrarch_serial_tx_full(struct uart_port *port)
{
	return xrarch_serial_read(port, UART_CMD_REG) != 0;
}

static void xrarch_serial_put_char(struct uart_port *port, u8 c)
{
	xrarch_serial_write(port, UART_DATA_REG, c);
}

static void xrarch_serial_put_char_sync(struct uart_port *port, u8 c)
{
	unsigned long timeout = jiffies + HZ;

	while (xrarch_serial_tx_full(port)) {
		if (time_after(jiffies, timeout)) {
			dev_warn(port->dev, "timeout waiting for TX\n");
			return;
		}

		cpu_relax();
	}

	xrarch_serial_put_char(port, c);
}

static void xrarch_serial_do_rx(struct uart_port *port)
{
	struct tty_port *tty_port = &port->state->port;
	unsigned long flags;

	spin_lock_irqsave(&port->lock, flags);

	u32 value;
	u32 count = 0;

	while ((value = xrarch_serial_read(port, UART_DATA_REG)) != 0xFFFF) {
		count += 1;

		if (uart_handle_sysrq_char(port, value))
			continue;

		tty_insert_flip_char(tty_port, value, TTY_NORMAL);
		port->icount.rx += 1;
	}

	spin_unlock_irqrestore(&port->lock, flags);

	if (count)
		tty_flip_buffer_push(tty_port);
}

static void xrarch_serial_do_tx(struct uart_port *port)
{
	u8 ch;
	uart_port_tx(port, ch, !xrarch_serial_tx_full(port),
		     xrarch_serial_put_char(port, ch));
}

static irqreturn_t xrarch_serial_int(int irq, void *dev_id)
{
	struct uart_port *port = dev_id;

	xrarch_serial_do_rx(port);
	xrarch_serial_do_tx(port);

	return IRQ_HANDLED;
}

static unsigned int xrarch_serial_tx_empty(struct uart_port *port) {
	return TIOCSER_TEMT;
}

static void xrarch_serial_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
}

static unsigned int xrarch_serial_get_mctrl(struct uart_port *port)
{
	return TIOCM_CAR | TIOCM_DSR | TIOCM_CTS;
}

static void xrarch_serial_stop_tx(struct uart_port *port)
{
}

static void xrarch_serial_start_tx(struct uart_port *port)
{
	xrarch_serial_do_tx(port);
}

static void xrarch_serial_stop_rx(struct uart_port *port)
{
}

static int xrarch_serial_startup(struct uart_port *port)
{
	int ret = request_irq(port->irq, xrarch_serial_int, 0, DRIVER_NAME,
			      port);
	if (ret)
		return ret;

	xrarch_serial_write(port, UART_CMD_REG, UART_IRQ_ON);

	return ret;
}

static void xrarch_serial_shutdown(struct uart_port *port)
{
	xrarch_serial_write(port, UART_CMD_REG, UART_IRQ_OFF);
	free_irq(port->irq, port);
}

static void xrarch_serial_set_termios(struct uart_port *port,
				      struct ktermios *termios,
				      const struct ktermios *old)
{
}

static const char *xrarch_serial_type(struct uart_port *port)
{
	return "XR/arch UART";
}

static void xrarch_serial_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE)
		port->type = PORT_GENERIC;
}

#ifdef CONFIG_CONSOLE_POLL
static void xrarch_serial_poll_put_char(struct uart_port *port, unsigned char c)
{
	xrarch_serial_put_char_sync(port, c);
}

static int xrarch_serial_poll_get_char(struct uart_port *port)
{
	u32 value = xrarch_serial_read(port, UART_DATA_REG);

	if (value != 0xFFFF)
		return value;
	else
		return NO_POLL_CHAR;

}
#endif

static const struct uart_ops xrarch_serial_pops = {
	.tx_empty	= xrarch_serial_tx_empty,
	.set_mctrl	= xrarch_serial_set_mctrl,
	.get_mctrl	= xrarch_serial_get_mctrl,
	.stop_tx	= xrarch_serial_stop_tx,
	.start_tx	= xrarch_serial_start_tx,
	.stop_rx	= xrarch_serial_stop_rx,
	.startup	= xrarch_serial_startup,
	.shutdown	= xrarch_serial_shutdown,
	.set_termios	= xrarch_serial_set_termios,
	.type		= xrarch_serial_type,
	.config_port	= xrarch_serial_config_port,
#ifdef CONFIG_CONSOLE_POLL
	.poll_put_char	= xrarch_serial_poll_put_char,
	.poll_get_char	= xrarch_serial_poll_get_char,
#endif
};

static void xrarch_serial_console_putchar(struct uart_port *port, u8 c)
{
	xrarch_serial_put_char_sync(port, c);
}

static void xrarch_serial_string_write(struct uart_port *port, const char *s,
				       unsigned int count)
{
	uart_console_write(port, s, count, xrarch_serial_console_putchar);
}

static void xrarch_serial_console_write(struct console *co, const char *s,
					unsigned int count)
{
	struct xrarch_port *sport = xrarch_serial_ports[co->index];
	struct uart_port *port = &sport->port;
	bool locked = true;
	unsigned long flags;

	if (port->sysrq)
		locked = false;
	else if (oops_in_progress)
		locked = spin_trylock_irqsave(&port->lock, flags);
	else
		spin_lock_irqsave(&port->lock, flags);

	xrarch_serial_string_write(port, s, count);

	if (locked)
		spin_unlock_irqrestore(&port->lock, flags);
}

static int xrarch_serial_console_setup(struct console *co, char *options)
{
	if (co->index == -1 || co->index >= ARRAY_SIZE(xrarch_serial_ports))
		co->index = 0;

	struct xrarch_port *sport = xrarch_serial_ports[co->index];
	if (!sport)
		return -ENODEV;

	int baud = sport->default_speed;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (options)
		uart_parse_options(options, &baud, &parity, &bits, &flow);

	return uart_set_options(&sport->port, co, baud, parity, bits, flow);
}

static struct uart_driver xrarch_serial_reg;
static struct console xrarch_serial_console = {
	.name	= DEV_NAME,
	.write	= xrarch_serial_console_write,
	.device	= uart_console_device,
	.setup	= xrarch_serial_console_setup,
	.flags	= CON_PRINTBUFFER,
	.index	= -1,
	.data	= &xrarch_serial_reg,
};

static struct uart_driver xrarch_serial_reg = {
	.owner		= THIS_MODULE,
	.driver_name	= DRIVER_NAME,
	.dev_name	= DEV_NAME,
	.nr		= ARRAY_SIZE(xrarch_serial_ports),
	.cons		= &xrarch_serial_console,
};

static int xrarch_serial_probe(struct platform_device *pdev)
{
	struct xrarch_port *sport = devm_kzalloc(&pdev->dev, sizeof(*sport),
						 GFP_KERNEL);
	if (!sport)
		return -ENOMEM;

	struct uart_port *port = &sport->port;
	struct device_node *np = pdev->dev.of_node;

	int ret = of_alias_get_id(np, "serial");
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to get alias id, errno %d\n", ret);
		goto fail_free_port;
	}
	if (ret >= UART_NR) {
		dev_err(&pdev->dev, "driver limited to %d ports\n", UART_NR);
		ret = -ENOMEM;
		goto fail_free_port;
	}

	port->line = ret;

	ret = of_property_read_u32_index(np, "current-speed", 0,
					 &sport->default_speed);
	if (ret)
		goto fail_free_port;

	port->uartclk = sport->default_speed * 16;

	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENOMEM;
		goto fail_free_port;
	}

	port->mapbase = res->start;
	port->membase = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(port->membase)) {
		ret = PTR_ERR(port->membase);
		goto fail_free_port;
	}

	port->dev = &pdev->dev;
	port->type = PORT_GENERIC;
	port->iotype = UPIO_MEM;
	port->irq = platform_get_irq(pdev, 0);
	port->ops = &xrarch_serial_pops;
	port->flags = UPF_BOOT_AUTOCONF;
	port->has_sysrq = 1;
	port->fifosize = UART_TX_FIFO_SIZE;

	xrarch_serial_ports[port->line] = sport;

	platform_set_drvdata(pdev, port);

	return uart_add_one_port(&xrarch_serial_reg, port);
fail_free_port:
	devm_kfree(&pdev->dev, sport);
	return ret;
}

static void xrarch_serial_remove(struct platform_device *pdev)
{
	struct uart_port *port = platform_get_drvdata(pdev);

	uart_remove_one_port(&xrarch_serial_reg, port);
}

static const struct of_device_id xrarch_serial_dt_ids[] = {
	{ .compatible = "xrarch,serial" },
	{}
};
MODULE_DEVICE_TABLE(of, xrarch_serial_dt_ids);

static struct platform_driver xrarch_serial_driver = {
	.probe	= xrarch_serial_probe,
	.remove	= xrarch_serial_remove,
	.driver	= {
		.name		= DRIVER_NAME,
		.of_match_table	= xrarch_serial_dt_ids,
	},
};

static int __init xrarch_serial_init(void)
{
	int ret;

	ret = uart_register_driver(&xrarch_serial_reg);
	if (ret)
		return ret;

	ret = platform_driver_register(&xrarch_serial_driver);
	if (ret)
		uart_unregister_driver(&xrarch_serial_reg);

	return ret;
}

static void __exit xrarch_serial_exit(void)
{
	platform_driver_unregister(&xrarch_serial_driver);
	uart_unregister_driver(&xrarch_serial_reg);
}

module_init(xrarch_serial_init);
module_exit(xrarch_serial_exit);

static void xrarch_serial_earlycon_putchar(struct uart_port *port, u8 c)
{
	xrarch_serial_put_char_sync(port, c);
}

static void xrarch_serial_earlycon_write(struct console *con, const char *s,
				         unsigned int n)
{
	struct earlycon_device *dev = con->data;

	uart_console_write(&dev->port, s, n, xrarch_serial_earlycon_putchar);
}

#ifdef CONFIG_CONSOLE_POLL
static int xrarch_serial_earlycon_read(struct console *con, char *s,
				       unsigned int n)
{
	struct earlycon_device *dev = con->data;
	unsigned int num_read = 0;

	while (num_read < n) {
		int c = xrarch_serial_poll_get_char(&dev->port);

		if (c == NO_POLL_CHAR)
			break;

		s[num_read++] = c;
	}

	return num_read;
}
#endif

static int __init xrarch_serial_earlycon_setup(struct earlycon_device *device,
					       const char *options)
{
	if (!device->port.membase)
		return -ENODEV;

	device->con->write = xrarch_serial_earlycon_write;
#ifdef CONFIG_CONSOLE_POLL
	device->con->read = xrarch_serial_earlycon_read;
#endif

	return 0;
}

OF_EARLYCON_DECLARE(xrarch_serial, "xrarch,serial",
		    xrarch_serial_earlycon_setup);

MODULE_AUTHOR("monkuous");
MODULE_DESCRIPTION("XR/arch UART support");
MODULE_LICENSE("GPL");
