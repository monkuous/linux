/* SPDX-License-Identifier: GPL-2.0-only */
#define pr_fmt(fmt) "xrarch-disk: " fmt
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/dma-mapping.h>
#include <linux/idr.h>
#include <linux/interrupt.h>
#include <linux/irqdomain.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/panic.h>
#include <linux/platform_device.h>

#define PART_BITS	4

#define DRIVER_NAME	"xrarch-disk"
#define DEV_NAME	"xrdisk"

#define XRDISK_CMD	0x00
#define XRDISK_DATA0	0x04
#define XRDISK_DATA1	0x08

#define XRDISK_SELECT	0x01
#define XRDISK_READ	0x02
#define XRDISK_WRITE	0x03
#define XRDISK_COMPLETE	0x04
#define XRDISK_INFO	0x05
#define XRDISK_IRQ_ON	0x06
#define XRDISK_IRQ_OFF	0x07
#define XRDISK_COUNT	0x08
#define XRDISK_DEST	0x09

#define XRDISK_MAX		8
#define XRDISK_SECT_SHIFT	9

struct xrarch_disk;

struct xrarch_disk_ctrl {
	struct device *dev;
	void __iomem *regs;
	spinlock_t lock;
	struct xrarch_disk *disks[XRDISK_MAX];
};

struct xrarch_disk {
	struct xrarch_disk_ctrl *ctrl;
	int index;
	int id;
	struct gendisk *disk;
	struct blk_mq_tag_set tag_set;
	struct request *req;
	dma_addr_t dma_addr;
	size_t dma_length;
	enum dma_data_direction dma_direction;
};

static DEFINE_IDA(xrarch_disk_id_ida);
static int xrarch_disk_major;

static void xrarch_disk_write(struct xrarch_disk_ctrl *ctrl, u32 reg, u32 value)
{
	writel(value, ctrl->regs + reg);
}

static u32 xrarch_disk_read(struct xrarch_disk_ctrl *ctrl, u32 reg)
{
	return readl(ctrl->regs + reg);
}

static irqreturn_t xrarch_disk_int(int irq, void *dev_id)
{
	struct xrarch_disk_ctrl *ctrl = dev_id;

	unsigned long flags;
	spin_lock_irqsave(&ctrl->lock, flags);

	xrarch_disk_write(ctrl, XRDISK_CMD, XRDISK_COMPLETE);
	u32 complete = xrarch_disk_read(ctrl, XRDISK_DATA1);

	for (int idx = 0; idx < XRDISK_MAX && complete != 0; idx++) {
		if ((complete & (1U << idx)) == 0) continue;

		struct xrarch_disk *xdisk = ctrl->disks[idx];
		struct request *req = xdisk->req;
		xdisk->req = NULL;

		BUG_ON(!req);

		dma_unmap_page(ctrl->dev, xdisk->dma_addr, xdisk->dma_length,
			       xdisk->dma_direction);
		blk_mq_complete_request(req);
	}

	spin_unlock_irqrestore(&ctrl->lock, flags);

	return IRQ_HANDLED;
}

static blk_status_t xrarch_disk_queue_rq(struct blk_mq_hw_ctx *hctx,
					 const struct blk_mq_queue_data *bd)
{
	struct xrarch_disk *xdisk = hctx->queue->queuedata;
	struct request *req = bd->rq;

	BUG_ON(blk_rq_nr_phys_segments(req) != 1);

	u32 submit_cmd;
	enum dma_data_direction dma_direction;

	switch (req_op(req)) {
	case REQ_OP_READ:
		submit_cmd = XRDISK_READ;
		dma_direction = DMA_FROM_DEVICE;
		break;
	case REQ_OP_WRITE:
		submit_cmd = XRDISK_WRITE;
		dma_direction = DMA_TO_DEVICE;
		break;
	default:
		WARN_ON_ONCE(1);
		return BLK_STS_IOERR;
	}

	u32 sector = blk_rq_pos(req) << (XRDISK_SECT_SHIFT - SECTOR_SHIFT);

	struct bio_vec bvec;
	struct bvec_iter iter;

	bio_for_each_segment(bvec, req->bio, iter)
		break;

	u32 size = bvec.bv_len;

	BUG_ON(size & ((1U << XRDISK_SECT_SHIFT) - 1));
	BUG_ON(size > (8U << XRDISK_SECT_SHIFT));

	dma_addr_t dma_addr = dma_map_bvec(xdisk->ctrl->dev, &bvec,
					   dma_direction, 0);
	if (dma_mapping_error(xdisk->ctrl->dev, dma_addr))
		return BLK_STS_IOERR;

	BUG_ON(dma_addr & ((1U << XRDISK_SECT_SHIFT) - 1));

	blk_mq_start_request(req);

	unsigned long flags;
	spin_lock_irqsave(&xdisk->ctrl->lock, flags);

	BUG_ON(xdisk->req);

	xdisk->req = req;
	xdisk->dma_addr = dma_addr;
	xdisk->dma_length = size;
	xdisk->dma_direction = dma_direction;

	xrarch_disk_write(xdisk->ctrl, XRDISK_DATA0, xdisk->index);
	xrarch_disk_write(xdisk->ctrl, XRDISK_CMD, XRDISK_SELECT);
	xrarch_disk_write(xdisk->ctrl, XRDISK_DATA0, dma_addr);
	xrarch_disk_write(xdisk->ctrl, XRDISK_CMD, XRDISK_DEST);
	xrarch_disk_write(xdisk->ctrl, XRDISK_DATA0, size >> XRDISK_SECT_SHIFT);
	xrarch_disk_write(xdisk->ctrl, XRDISK_CMD, XRDISK_COUNT);
	xrarch_disk_write(xdisk->ctrl, XRDISK_DATA0, sector);
	xrarch_disk_write(xdisk->ctrl, XRDISK_CMD, submit_cmd);

	spin_unlock_irqrestore(&xdisk->ctrl->lock, flags);
	return BLK_STS_OK;
}

static void xrarch_disk_complete(struct request *req)
{
	blk_mq_end_request(req, BLK_STS_OK);
}

static const struct blk_mq_ops xrarch_disk_mq_ops = {
	.queue_rq	= xrarch_disk_queue_rq,
	.complete	= xrarch_disk_complete,
};

static void xrarch_disk_free_disk(struct gendisk *disk)
{
	struct xrarch_disk *xdisk = disk->private_data;

	ida_free(&xrarch_disk_id_ida, xdisk->id);
	devm_kfree(xdisk->ctrl->dev, xdisk);
}

static const struct block_device_operations xrarch_disk_fops = {
	.owner		= THIS_MODULE,
	.free_disk	= xrarch_disk_free_disk,
};

static int minor_to_id(int minor)
{
	return minor >> PART_BITS;
}

static int id_to_minor(int id)
{
	return id << PART_BITS;
}

static int xrarch_disk_init_one(struct xrarch_disk_ctrl *ctrl, int idx,
				u32 sectors)
{
	struct queue_limits lim = {
		.features		= BLK_FEAT_ROTATIONAL,
		.max_hw_sectors		= 8,
		.max_segment_size	= 8U << XRDISK_SECT_SHIFT,
		.physical_block_size	= 1U << XRDISK_SECT_SHIFT,
		.logical_block_size	= 1U << XRDISK_SECT_SHIFT,
		.max_segments		= 1,
		.max_write_streams	= 1,
		.dma_alignment		= 1U << XRDISK_SECT_SHIFT,
	};

	struct xrarch_disk *xdisk = devm_kzalloc(ctrl->dev, sizeof(*xdisk),
						 GFP_KERNEL);
	if (!xdisk)
		return -ENOMEM;

	xdisk->ctrl = ctrl;
	xdisk->index = idx;

	int ret = ida_alloc_range(&xrarch_disk_id_ida, 0,
				  minor_to_id(1 << MINORBITS) - 1,
				  GFP_KERNEL);
	if (ret < 0)
		goto fail_free_xdisk;

	xdisk->id = ret;

	xdisk->tag_set.ops = &xrarch_disk_mq_ops;
	xdisk->tag_set.nr_maps = 1;
	xdisk->tag_set.nr_hw_queues = 1;
	xdisk->tag_set.queue_depth = 1;
	xdisk->tag_set.numa_node = NUMA_NO_NODE;
	xdisk->tag_set.driver_data = xdisk;

	ret = blk_mq_alloc_tag_set(&xdisk->tag_set);
	if (ret)
		goto fail_free_id;

	xdisk->disk = blk_mq_alloc_disk(&xdisk->tag_set, &lim, xdisk);
	if (IS_ERR(xdisk->disk)) {
		ret = PTR_ERR(xdisk->disk);
		goto fail_free_tag_set;
	}

	snprintf(xdisk->disk->disk_name, DISK_NAME_LEN, DEV_NAME "%d",
		 xdisk->id);

	xdisk->disk->major = xrarch_disk_major;
	xdisk->disk->first_minor = id_to_minor(xdisk->id);
	xdisk->disk->minors = 1 << PART_BITS;
	xdisk->disk->private_data = xdisk;
	xdisk->disk->fops = &xrarch_disk_fops;

	set_capacity(xdisk->disk, (u64)sectors << (XRDISK_SECT_SHIFT
						   - SECTOR_SHIFT));

	ctrl->disks[idx] = xdisk;

	ret = device_add_disk(ctrl->dev, xdisk->disk, NULL);
	if (ret)
		goto fail_free_disk;

	return 0;
fail_free_disk:
	put_disk(xdisk->disk);
fail_free_tag_set:
	blk_mq_free_tag_set(&xdisk->tag_set);
fail_free_id:
	ida_free(&xrarch_disk_id_ida, xdisk->id);
fail_free_xdisk:
	devm_kfree(ctrl->dev, xdisk);
	return ret;
}

static int xrarch_disk_probe(struct platform_device *pdev)
{
	struct xrarch_disk_ctrl *ctrl = devm_kzalloc(&pdev->dev, sizeof(*ctrl),
						     GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	ctrl->dev = &pdev->dev;
	spin_lock_init(&ctrl->lock);

	int ret;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		ret = -ENOMEM;
		goto fail_free_ctrl;
	}

	ctrl->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ctrl->regs)) {
		ret = PTR_ERR(ctrl->regs);
		goto fail_free_ctrl;
	}

	ret = platform_get_irq(pdev, 0);
	if (ret < 0)
		goto fail_free_regs;
	int irq = ret;

	// wait for the controller to become idle
	while (xrarch_disk_read(ctrl, XRDISK_CMD)) {
		cpu_relax();
	}

	// make sure next completion status doesn't include stale info
	xrarch_disk_write(ctrl, XRDISK_CMD, XRDISK_COMPLETE);

	// set up irq
	ret = request_irq(irq, xrarch_disk_int, 0, DRIVER_NAME, ctrl);
	if (ret)
		goto fail_free_irq;
	xrarch_disk_write(ctrl, XRDISK_CMD, XRDISK_IRQ_ON);

	// discover disks
	for (int idx = 0; idx < XRDISK_MAX; idx++) {
		unsigned long flags;
		spin_lock_irqsave(&ctrl->lock, flags);
		xrarch_disk_write(ctrl, XRDISK_DATA0, idx);
		xrarch_disk_write(ctrl, XRDISK_CMD, XRDISK_INFO);
		u32 present = xrarch_disk_read(ctrl, XRDISK_DATA0);
		u32 sectors = xrarch_disk_read(ctrl, XRDISK_DATA1);
		spin_unlock_irqrestore(&ctrl->lock, flags);

		if (!present) continue;

		int error = xrarch_disk_init_one(ctrl, idx, sectors);
		if (error)
			dev_warn(&pdev->dev,
				 "failed to initialize disk %d (%d)\n", idx,
				 error);
	}

	return 0;
fail_free_irq:
	irq_dispose_mapping(irq);
fail_free_regs:
	devm_iounmap(&pdev->dev, ctrl->regs);
fail_free_ctrl:
	devm_kfree(&pdev->dev, ctrl);
	return ret;
}

static void xrarch_disk_remove(struct platform_device *pdev)
{
	panic("TODO xrarch_disk_remove");
}

static const struct of_device_id xrarch_disk_dt_ids[] = {
	{ .compatible = "xrarch,disk-controller" },
	{}
};
MODULE_DEVICE_TABLE(of, xrarch_disk_dt_ids);

static struct platform_driver xrarch_disk_driver = {
	.probe	= xrarch_disk_probe,
	.remove	= xrarch_disk_remove,
	.driver	= {
		.name		= DRIVER_NAME,
		.of_match_table	= xrarch_disk_dt_ids,
	},
};

static int __init xrarch_disk_init(void)
{
	int error = register_blkdev(0, DEV_NAME);
	if (error < 0)
		return error;
	xrarch_disk_major = error;

	error = platform_driver_register(&xrarch_disk_driver);
	if (error)
		goto fail_unregister_blkdev;

	return 0;
fail_unregister_blkdev:
	unregister_blkdev(xrarch_disk_major, DEV_NAME);
	return error;
}

static void __exit xrarch_disk_exit(void)
{
	platform_driver_unregister(&xrarch_disk_driver);
	unregister_blkdev(xrarch_disk_major, DEV_NAME);
}

module_init(xrarch_disk_init);
module_exit(xrarch_disk_exit);

MODULE_AUTHOR("monkuous");
MODULE_DESCRIPTION("Driver for the XR/arch disk controller");
MODULE_LICENSE("GPL");
