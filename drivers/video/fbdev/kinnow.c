/* SPDX-License-Identifier: GPL-2.0-only */
#include <asm/io.h>
#include <linux/aperture.h>
#include <linux/fb.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#define KINNOW_REGS_OFFSET	0x3000
#define KINNOW_REGS_SIZE	8

#define KINNOW_DISPLAY_SIZE	0x00
#define KINNOW_FEATURES		0x20

#define KINNOW_FEATURE_PALETTE	0x01

#define KINNOW_PALETTE_OFFSET	0x4000

#define KINNOW_MEM_OFFSET	0x100000

struct kinnowfb_par {
	void __iomem *palette;
};

static const struct fb_fix_screeninfo kinnowfb_fix = {
	.id	= "kinnow",
	.type	= FB_TYPE_PACKED_PIXELS,
	.visual	= FB_VISUAL_PSEUDOCOLOR,
	.accel	= FB_ACCEL_NONE,
};

static const struct fb_var_screeninfo kinnowfb_var = {
	.height		= -1,
	.width		= -1,
	.activate	= FB_ACTIVATE_NOW,
	.vmode		= FB_VMODE_NONINTERLACED,
	.bits_per_pixel	= 8,
	.red		= { 0, 8, 0 },
	.green		= { 0, 8, 0 },
	.blue		= { 0, 8, 0 },
};

static const u32 kinnowfb_palette[] = {
	0x000000, 0x220022, 0x000044, 0x220066, 0x000088, 0x2200aa, 0x0000cc,
	0x2200ee, 0x440000, 0x660022, 0x440044, 0x660066, 0x440088, 0x6600aa,
	0x4400cc, 0x6600ee, 0x880000, 0xaa0022, 0x880044, 0xaa0066, 0x880088,
	0xaa00aa, 0x8800cc, 0xaa00ee, 0xcc0000, 0xee0022, 0xcc0044, 0xee0066,
	0xcc0088, 0xee00aa, 0xcc00cc, 0xee00ee, 0x002200, 0x222222, 0x002244,
	0x222266, 0x002288, 0x2222aa, 0x0022cc, 0x2222ee, 0x442200, 0x662222,
	0x442244, 0x662266, 0x442288, 0x6622aa, 0x4422cc, 0x6622ee, 0x882200,
	0xaa2222, 0x882244, 0xaa2266, 0x882288, 0xaa22aa, 0x8822cc, 0xaa22ee,
	0xcc2200, 0xee2222, 0xcc2244, 0xee2266, 0xcc2288, 0xee22aa, 0xcc22cc,
	0xee22ee, 0x004400, 0x224422, 0x004444, 0x224466, 0x004488, 0x2244aa,
	0x0044cc, 0x2244ee, 0x444400, 0x664422, 0x444444, 0x664466, 0x444488,
	0x6644aa, 0x4444cc, 0x6644ee, 0x884400, 0xaa4422, 0x884444, 0xaa4466,
	0x884488, 0xaa44aa, 0x8844cc, 0xaa44ee, 0xcc4400, 0xee4422, 0xcc4444,
	0xee4466, 0xcc4488, 0xee44aa, 0xcc44cc, 0xee44ee, 0x006600, 0x226622,
	0x006644, 0x226666, 0x006688, 0x2266aa, 0x0066cc, 0x2266ee, 0x446600,
	0x666622, 0x446644, 0x666666, 0x446688, 0x6666aa, 0x4466cc, 0x6666ee,
	0x886600, 0xaa6622, 0x886644, 0xaa6666, 0x886688, 0xaa66aa, 0x8866cc,
	0xaa66ee, 0xcc6600, 0xee6622, 0xcc6644, 0xee6666, 0xcc6688, 0xee66aa,
	0xcc66cc, 0xee66ee, 0x008800, 0x228822, 0x008844, 0x228866, 0x008888,
	0x2288aa, 0x0088cc, 0x2288ee, 0x448800, 0x668822, 0x448844, 0x668866,
	0x448888, 0x6688aa, 0x4488cc, 0x6688ee, 0x888800, 0xaa8822, 0x888844,
	0xaa8866, 0x888888, 0xaa88aa, 0x8888cc, 0xaa88ee, 0xcc8800, 0xee8822,
	0xcc8844, 0xee8866, 0xcc8888, 0xee88aa, 0xcc88cc, 0xee88ee, 0x00aa00,
	0x22aa22, 0x00aa44, 0x22aa66, 0x00aa88, 0x22aaaa, 0x00aacc, 0x22aaee,
	0x44aa00, 0x66aa22, 0x44aa44, 0x66aa66, 0x44aa88, 0x66aaaa, 0x44aacc,
	0x66aaee, 0x88aa00, 0xaaaa22, 0x88aa44, 0xaaaa66, 0x88aa88, 0xaaaaaa,
	0x88aacc, 0xaaaaee, 0xccaa00, 0xeeaa22, 0xccaa44, 0xeeaa66, 0xccaa88,
	0xeeaaaa, 0xccaacc, 0xeeaaee, 0x00cc00, 0x22cc22, 0x00cc44, 0x22cc66,
	0x00cc88, 0x22ccaa, 0x00cccc, 0x22ccee, 0x44cc00, 0x66cc22, 0x44cc44,
	0x66cc66, 0x44cc88, 0x66ccaa, 0x44cccc, 0x66ccee, 0x88cc00, 0xaacc22,
	0x88cc44, 0xaacc66, 0x88cc88, 0xaaccaa, 0x88cccc, 0xaaccee, 0xcccc00,
	0xeecc22, 0xcccc44, 0xeecc66, 0xcccc88, 0xeeccaa, 0xcccccc, 0xeeccee,
	0x00ee00, 0x22ee22, 0x00ee44, 0x22ee66, 0x00ee88, 0x22eeaa, 0x00eecc,
	0x22eeee, 0x44ee00, 0x66ee22, 0x44ee44, 0x66ee66, 0x44ee88, 0x66eeaa,
	0x44eecc, 0x66eeee, 0x88ee00, 0xaaee22, 0x88ee44, 0xaaee66, 0x88ee88,
	0xaaeeaa, 0x88eecc, 0xaaeeee, 0xccee00, 0xeeee22, 0xccee44, 0xeeee66,
	0xccee88, 0xeeeeaa, 0xcceecc, 0xffffff
};

static int kinnowfb_setcolreg(unsigned regno, unsigned red, unsigned green,
			      unsigned blue, unsigned transp,
			      struct fb_info *info)
{
	struct kinnowfb_par *par = info->par;

	if (!par->palette)
		return 0;

	if (regno >= ARRAY_SIZE(kinnowfb_palette))
		return -EINVAL;

	red >>= 8;
	red &= 0xff;

	green >>= 8;
	green &= 0xff;

	blue >>= 8;
	blue &= 0xff;

	unsigned value = (red << 16) | (green << 8) | blue;
	writel(value, par->palette + regno * 4);

	return 0;
}

static int kinnowfb_setcmap(struct fb_cmap *cmap, struct fb_info *info)
{
	struct kinnowfb_par *par = info->par;

	if (!par->palette)
		return 0;

	if (cmap->start + cmap->len < cmap->start
	    || cmap->start + cmap->len > ARRAY_SIZE(kinnowfb_palette))
		return -EINVAL;

	for (u32 i = 0; i < cmap->len; i++) {
		u32 red = (cmap->red[i] >> 8) & 0xff;
		u32 green = (cmap->green[i] >> 8) & 0xff;
		u32 blue = (cmap->blue[i] >> 8) & 0xff;

		u32 value = (red << 16) | (green << 8) | blue;
		writel(value, par->palette + (cmap->start + i) * 4);
	}

	return 0;
}

static void kinnowfb_destroy(struct fb_info *info)
{
	struct kinnowfb_par *par = info->par;

	fb_dealloc_cmap(&info->cmap);

	if (par->palette)
		iounmap(par->palette);

	if (info->screen_base)
		iounmap(info->screen_base);

	framebuffer_release(info);
}

static const struct fb_ops kinnowfb_ops = {
	.owner		= THIS_MODULE,
	FB_DEFAULT_IOMEM_OPS,
	.fb_setcolreg	= kinnowfb_setcolreg,
	.fb_setcmap	= kinnowfb_setcmap,
	.fb_destroy	= kinnowfb_destroy,
};

static u16 kinnowfb_translate_color(u8 color)
{
	return ((u16)color << 8) | color;
}

static int kinnowfb_probe(struct platform_device *pdev)
{
	if (fb_get_options("kinnowfb", NULL))
		return -ENODEV;

	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "no memory resource\n");
		return -EINVAL;
	}

	void __iomem *regs = ioremap(res->start + KINNOW_REGS_OFFSET,
				     KINNOW_REGS_SIZE);
	if (!regs) {
		dev_err(&pdev->dev, "failed to map registers\n");
		return -ENOMEM;
	}

	u32 display_size = readl(regs + KINNOW_DISPLAY_SIZE);
	u32 width = display_size & 0xfff;
	u32 height = (display_size >> 12) & 0xfff;
	u32 features = readl(regs + KINNOW_FEATURES);

	iounmap(regs);

	int ret;
	struct fb_info *info = framebuffer_alloc(sizeof(struct kinnowfb_par),
						 &pdev->dev);
	if (!info)
		return -ENOMEM;
	platform_set_drvdata(pdev, info);

	struct kinnowfb_par *par = info->par;

	info->fix = kinnowfb_fix;
	info->fix.smem_start = res->start + KINNOW_MEM_OFFSET;
	info->fix.smem_len = width * height;
	info->fix.line_length = width;

	info->var = kinnowfb_var;
	info->var.xres = width;
	info->var.yres = height;
	info->var.xres_virtual = width;
	info->var.yres_virtual = height;

	info->fbops = &kinnowfb_ops;
	info->screen_base = ioremap_wc(info->fix.smem_start,
				       info->fix.smem_len);
	if (!info->screen_base) {
		ret = -ENOMEM;
		goto fail_free_info;
	}

	if (features & KINNOW_FEATURE_PALETTE) {
		par->palette = ioremap(res->start + KINNOW_PALETTE_OFFSET,
				       sizeof(kinnowfb_palette));

		if (par->palette) {
			for (int i = 0; i < ARRAY_SIZE(kinnowfb_palette); i++) {
				writel(kinnowfb_palette[i],
				       par->palette + i * 4);
			}
		} else {
			// ioremap failure is not critical, it'll just make
			// console colors incorrect
			dev_warn(&pdev->dev, "failed to map palette");
		}
	}

	if (!par->palette)
		dev_warn(&pdev->dev,
			 "palette is read-only, colors may be incorrect");

	ret = fb_alloc_cmap(&info->cmap, ARRAY_SIZE(kinnowfb_palette), 0);
	if (ret)
		goto fail_free_info;

	for (int i = 0; i < ARRAY_SIZE(kinnowfb_palette); i++) {
		u32 color = kinnowfb_palette[i];
		info->cmap.red[i] = kinnowfb_translate_color(color >> 16);
		info->cmap.green[i] = kinnowfb_translate_color(color >> 8);
		info->cmap.blue[i] = kinnowfb_translate_color(color);
	}

	dev_info(&pdev->dev, "%ux%u framebuffer at 0x%lx, 0x%x bytes\n",
		 width, height, info->fix.smem_start, info->fix.smem_len);

	ret = devm_aperture_acquire_for_platform_device(pdev,
							info->fix.smem_start,
							info->fix.smem_len);
	if (ret) {
		dev_err(&pdev->dev, "unable to acquire aperture (%d)\n", ret);
		goto fail_free_info;
	}

	ret = register_framebuffer(info);
	if (ret < 0) {
		dev_err(&pdev->dev, "unable to register framebuffer (%d)\n",
			ret);
		goto fail_free_info;
	}

	return 0;

fail_free_info:
	framebuffer_release(info);
	return ret;
}

static void kinnowfb_remove(struct platform_device *pdev)
{
	struct fb_info *info = platform_get_drvdata(pdev);

	/* kinnowfb_destroy takes care of info cleanup */
	unregister_framebuffer(info);
}

static const struct of_device_id kinnowfb_of_match[] = {
	{ .compatible = "xrarch,expansion-4b494e36" },
	{ },
};
MODULE_DEVICE_TABLE(of, kinnowfb_of_match);

static struct platform_driver kinnowfb_driver = {
	.driver	= {
		.name		= "kinnow-framebuffer",
		.of_match_table	= kinnowfb_of_match,
	},
	.probe	= kinnowfb_probe,
	.remove	= kinnowfb_remove,
};

module_platform_driver(kinnowfb_driver);

MODULE_AUTHOR("monkuous");
MODULE_DESCRIPTION("XR/arch Kinnow framebuffer driver");
MODULE_LICENSE("GPL");
