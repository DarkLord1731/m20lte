/*
 * Copyright (c) Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/clk-provider.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_iovmm.h>
#include <linux/of_address.h>
#include <linux/pinctrl/consumer.h>
#include <linux/irq.h>
#include <media/v4l2-subdev.h>
#include <linux/reboot.h>
#include <linux/debugfs.h>

#include "decon.h"
#include "dsim.h"
#include "dpp.h"
#include "../../../../../kernel/irq/internals.h"

#define abd_printf(m, x...)	\
{	if (m) seq_printf(m, x); else decon_info(x);	}	\

void decon_abd_save_log_fto(struct abd_protect *abd, struct sync_fence *fence)
{
	struct abd_trace *first = &abd->f_first;
	struct abd_trace *lcdon = &abd->f_lcdon;
	struct abd_trace *event = &abd->f_event;

	struct abd_log *first_log = &first->log[(first->count % ABD_LOG_MAX)];
	struct abd_log *lcdon_log = &lcdon->log[(lcdon->count % ABD_LOG_MAX)];
	struct abd_log *event_log = &event->log[(event->count % ABD_LOG_MAX)];

	memset(event_log, 0, sizeof(struct abd_log));
	event_log->stamp = ktime_to_ns(ktime_get());
	memcpy(&event_log->fence, fence, sizeof(struct sync_fence));

	if (!first->count) {
		memset(first_log, 0, sizeof(struct abd_log));
		memcpy(first_log, event_log, sizeof(struct abd_log));
		first->count++;
	}

	if (!lcdon->lcdon_flag) {
		memset(lcdon_log, 0, sizeof(struct abd_log));
		memcpy(lcdon_log, event_log, sizeof(struct abd_log));
		lcdon->count++;
		lcdon->lcdon_flag++;
	}

	event->count++;
}

void decon_abd_save_log_udr(struct abd_protect *abd, unsigned long mif, unsigned long iint, unsigned long disp)
{
	struct decon_device *decon = container_of(abd, struct decon_device, abd);
	struct abd_trace *first = &abd->u_first;
	struct abd_trace *lcdon = &abd->u_lcdon;
	struct abd_trace *event = &abd->u_event;

	struct abd_log *first_log = &first->log[(first->count) % ABD_LOG_MAX];
	struct abd_log *lcdon_log = &lcdon->log[(lcdon->count) % ABD_LOG_MAX];
	struct abd_log *event_log = &event->log[(event->count) % ABD_LOG_MAX];

	memset(event_log, 0, sizeof(struct abd_log));
	event_log->stamp = ktime_to_ns(ktime_get());
	event_log->frm_status = decon->frm_status;
	event_log->mif = mif;
	event_log->iint = iint;
	event_log->disp = disp;
	memcpy(&event_log->bts, &decon->bts, sizeof(struct decon_bts));

	if (!first->count) {
		memset(first_log, 0, sizeof(struct abd_log));
		memcpy(first_log, event_log, sizeof(struct abd_log));
		first->count++;
	}

	if (!lcdon->lcdon_flag) {
		memset(lcdon_log, 0, sizeof(struct abd_log));
		memcpy(lcdon_log, event_log, sizeof(struct abd_log));
		lcdon->count++;
		lcdon->lcdon_flag++;
	}

	event->count++;
}

static void decon_abd_clear_pending_bit(int irq)
{
	struct irq_desc *desc;

	desc = irq_to_desc(irq);
	if (desc->irq_data.chip->irq_ack) {
		desc->irq_data.chip->irq_ack(&desc->irq_data);
		desc->istate &= ~IRQS_PENDING;
	}
}

static void decon_abd_save_log_pin(struct decon_device *decon, struct abd_pin *pin, struct abd_trace *trace, bool on)
{
	struct abd_trace *first = &pin->p_first;

	struct abd_log *first_log = &first->log[(first->count) % ABD_LOG_MAX];
	struct abd_log *trace_log = &trace->log[(trace->count) % ABD_LOG_MAX];

	trace_log->stamp = ktime_to_ns(ktime_get());
	trace_log->level = pin->level;
	trace_log->state = decon->state;
	trace_log->onoff = on;

	if (!first->count) {
		memset(first_log, 0, sizeof(struct abd_log));
		memcpy(first_log, trace_log, sizeof(struct abd_log));
		first->count++;
	}

	trace->count++;
}

static void decon_abd_enable_interrupt(struct decon_device *decon, struct abd_pin *pin, bool on)
{
	struct abd_trace *trace = &pin->p_lcdon;

	if (!pin || !pin->irq)
		return;

	pin->level = gpio_get_value(pin->gpio);

	decon_info("%s: on: %d, %s(%d) level: %d, count: %d, state: %d\n", __func__, on, pin->name, pin->irq, pin->level, trace->count, decon->state);

	if (pin->level == pin->active_level) {
		decon_abd_save_log_pin(decon, pin, trace, on);
		if (pin->name && !strcmp(pin->name, "pcd")) {
			decon->ignore_vsync = 1;
			decon_info("%s: ignore_vsync: %d\n", __func__, decon->ignore_vsync);
		}
	}

	if (on) {
		decon_abd_clear_pending_bit(pin->irq);
		enable_irq(pin->irq);
	} else
		disable_irq_nosync(pin->irq);
}

void decon_abd_enable(struct decon_device *decon, int enable)
{
	struct abd_protect *abd = &decon->abd;

	if (!abd)
		return;

	if (enable) {
		if (abd->irq_enable == 1) {
			decon_info("%s: already enabled irq_enable: %d\n", __func__, abd->irq_enable);
			return;
		}

		abd->f_lcdon.lcdon_flag = 0;
		abd->u_lcdon.lcdon_flag = 0;

		abd->irq_enable = 1;
	} else {
		if (abd->irq_enable == 0) {
			decon_info("%s: already disabled irq_enable: %d\n", __func__, abd->irq_enable);
			return;
		}

		abd->irq_enable = 0;
	}

	decon_abd_enable_interrupt(decon, &abd->pin[ABD_PIN_PCD], enable);
	decon_abd_enable_interrupt(decon, &abd->pin[ABD_PIN_DET], enable);
	decon_abd_enable_interrupt(decon, &abd->pin[ABD_PIN_ERR], enable);
}

irqreturn_t decon_abd_pcd_handler(int irq, void *dev_id)
{
	struct decon_device *decon = (struct decon_device *)dev_id;
	struct abd_pin *pin = &decon->abd.pin[ABD_PIN_PCD];
	struct abd_trace *trace = &pin->p_event;

	pin->level = gpio_get_value(pin->gpio);

	decon_info("%s: %s(%d) level: %d, count: %d, state: %d\n", __func__, pin->name, pin->irq, pin->level, trace->count, decon->state);

	decon_abd_save_log_pin(decon, pin, trace, 1);

	if (pin->active_level != pin->level)
		goto exit;

	decon->ignore_vsync = 1;

	if (pin->handler)
		pin->handler(irq, pin->dev_id);

exit:
	return IRQ_HANDLED;
}

irqreturn_t decon_abd_det_handler(int irq, void *dev_id)
{
	struct decon_device *decon = (struct decon_device *)dev_id;
	struct abd_pin *pin = &decon->abd.pin[ABD_PIN_DET];
	struct abd_trace *trace = &pin->p_event;

	pin->level = gpio_get_value(pin->gpio);

	decon_info("%s: %s(%d) level: %d, count: %d, state: %d\n", __func__, pin->name, pin->irq, pin->level, trace->count, decon->state);

	decon_abd_save_log_pin(decon, pin, trace, 1);

	if (pin->active_level != pin->level)
		goto exit;

	if (pin->handler)
		pin->handler(irq, pin->dev_id);

exit:
	return IRQ_HANDLED;
}

irqreturn_t decon_abd_err_handler(int irq, void *dev_id)
{
	struct decon_device *decon = (struct decon_device *)dev_id;
	struct abd_pin *pin = &decon->abd.pin[ABD_PIN_ERR];
	struct abd_trace *trace = &pin->p_event;

	pin->level = gpio_get_value(pin->gpio);

	decon_info("%s: %s(%d) level: %d, count: %d, state: %d\n", __func__, pin->name, pin->irq, pin->level, trace->count, decon->state);

	decon_abd_save_log_pin(decon, pin, trace, 1);

	if (pin->active_level != pin->level)
		goto exit;

	if (pin->handler)
		pin->handler(irq, pin->dev_id);

exit:
	return IRQ_HANDLED;
}

int decon_abd_register_pin_handler(int irq, irq_handler_t handler, void *dev_id)
{
	struct decon_device *decon = get_decon_drvdata(0);
	struct abd_pin *pin = NULL;
	unsigned int i;

	if (!irq) {
		decon_info("%s: irq(%d) invalid\n", __func__, irq);
		return -EINVAL;
	}

	for (i = 0; i < ABD_PIN_MAX; i++) {
		pin = &decon->abd.pin[i];
		if (pin && irq == pin->irq) {
			pin->handler = handler;
			pin->dev_id = dev_id;
			decon_info("%s: register handler for %s irq(%d)\n", __func__, pin->name, irq);
			break;
		}
	}

	if (i == ABD_PIN_MAX) {
		decon_info("%s: irq(%d) is not in abd\n", __func__, irq);
		return -EINVAL;
	}

	return 0;
}

static int decon_debug_pin_log_print(struct seq_file *m, struct abd_trace *trace)
{
	int i = 0;
	struct timeval tv;
	struct abd_log *log;

	if (!trace->count)
		return 0;

	abd_printf(m, "%s total count: %d\n", trace->name, trace->count);
	for (i = 0; i < ABD_LOG_MAX; i++) {
		log = &trace->log[i];
		if (!log->stamp)
			continue;
		tv = ns_to_timeval(log->stamp);
		abd_printf(m, "time: %lu.%06lu level: %d onoff: %d state: %d\n",
			(unsigned long)tv.tv_sec, tv.tv_usec, log->level, log->onoff, log->state);
	}

	return 0;
}

static int decon_debug_pin_print(struct seq_file *m, struct abd_pin *pin)
{
	if (!pin->irq)
		return 0;

	abd_printf(m, "[%s]\n", pin->name);

	decon_debug_pin_log_print(m, &pin->p_first);
	decon_debug_pin_log_print(m, &pin->p_lcdon);
	decon_debug_pin_log_print(m, &pin->p_event);

	return 0;
}

static const char *sync_status_str(int status)
{
	if (status == 0)
		return "signaled";

	if (status > 0)
		return "active";

	return "error";
}

static int decon_debug_fto_print(struct seq_file *m, struct abd_trace *trace)
{
	int i = 0;
	struct timeval tv;
	struct abd_log *log;

	if (!trace->count)
		return 0;

	abd_printf(m, "%s total count: %d\n", trace->name, trace->count);
	for (i = 0; i < ABD_LOG_MAX; i++) {
		log = &trace->log[i];
		if (!log->stamp)
			continue;
		tv = ns_to_timeval(log->stamp);
		abd_printf(m, "time: %lu.%06lu, %d, %s: %s\n",
			(unsigned long)tv.tv_sec, tv.tv_usec, log->winid, log->fence.name, sync_status_str(atomic_read(&log->fence.status)));
	}

	return 0;
}

static int decon_debug_udr_print(struct seq_file *m, struct abd_trace *trace)
{
	int i = 0, idx;
	struct timeval tv;
	struct abd_log *log;
	struct decon_bts *bts;
	struct bts_decon_info *bts_info;

	if (!trace->count)
		return 0;

	abd_printf(m, "%s total count: %d\n", trace->name, trace->count);
	for (i = 0; i < ABD_LOG_MAX; i++) {
		log = &trace->log[i];
		if (!log->stamp)
			continue;
		tv = ns_to_timeval(log->stamp);
		abd_printf(m, "time: %lu.%06lu, %d\n",
			(unsigned long)tv.tv_sec, tv.tv_usec, log->frm_status);

		abd_printf(m, "MIF(%lu), INT(%lu), DISP(%lu)\n", log->mif, log->iint, log->disp);

		bts = &log->bts;
		bts_info = &log->bts.bts_info;
		abd_printf(m, "total(%u %u), max(%u %u), peak(%u)\n",
				bts->prev_total_bw,
				bts->total_bw,
				bts->prev_max_disp_freq,
				bts->max_disp_freq,
				bts->peak);

		for (idx = 0; idx < BTS_DPP_MAX; ++idx) {
			if (!bts_info->dpp[idx].used)
				continue;

			abd_printf(m, "DPP[%d] (%d) b(%d) s(%4d %4d) d(%4d %4d %4d %4d)\n",
				idx, bts_info->dpp[idx].idma_type, bts_info->dpp[idx].bpp,
				bts_info->dpp[idx].src_w, bts_info->dpp[idx].src_h,
				bts_info->dpp[idx].dst.x1, bts_info->dpp[idx].dst.x2,
				bts_info->dpp[idx].dst.y1, bts_info->dpp[idx].dst.y2);
		}
	}

	return 0;
}

static int decon_debug_ss_log_print(struct seq_file *m)
{
	unsigned int ss_log_max = 200, i, idx;
	struct timeval tv;
	struct decon_device *decon = m ? m->private : get_decon_drvdata(0);
	int start = atomic_read(&decon->d.event_log_idx);
	struct dpu_log *log;

	start = (start > ss_log_max) ? start - ss_log_max + 1 : 0;

	for (i = 0; i < ss_log_max; i++) {
		idx = (start + i) % DPU_EVENT_LOG_MAX;
		log = &decon->d.event_log[idx];

		if (!ktime_to_ns(log->time))
			continue;
		tv = ktime_to_timeval(log->time);
		if (i && !(i % 10))
			abd_printf(m, "\n");
		abd_printf(m, "%lu.%06lu %2u, ", (unsigned long)tv.tv_sec, tv.tv_usec, log->type);
	}

	abd_printf(m, "\n");

	return 0;
}

static int decon_debug_show(struct seq_file *m, void *unused)
{
	struct decon_device *decon = m ? m->private : get_decon_drvdata(0);
	struct abd_protect *abd = &decon->abd;

	abd_printf(m, "========== LCD DEBUG ==========\n");
	abd_printf(m, "isync: %d\n", decon->ignore_vsync);
	decon_debug_pin_print(m, &abd->pin[ABD_PIN_PCD]);
	decon_debug_pin_print(m, &abd->pin[ABD_PIN_DET]);
	decon_debug_pin_print(m, &abd->pin[ABD_PIN_ERR]);

	abd_printf(m, "========== FTO DEBUG ==========\n");
	decon_debug_fto_print(m, &abd->f_first);
	decon_debug_fto_print(m, &abd->f_lcdon);
	decon_debug_fto_print(m, &abd->f_event);

	abd_printf(m, "========== UDR DEBUG ==========\n");
	decon_debug_udr_print(m, &abd->u_first);
	decon_debug_udr_print(m, &abd->u_lcdon);
	decon_debug_udr_print(m, &abd->u_event);

	abd_printf(m, "===============================\n");
	decon_debug_ss_log_print(m);

	return 0;
}

static int decon_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, decon_debug_show, inode->i_private);
}

static const struct file_operations decon_debug_fops = {
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
	.open = decon_debug_open,
};

static int decon_abd_reboot_notifier(struct notifier_block *this,
		unsigned long code, void *unused)
{
	struct abd_protect *abd = container_of(this, struct abd_protect, reboot_notifier);
	struct decon_device *decon = container_of(abd, struct decon_device, abd);

	decon_info("++ %s: %lu\n",  __func__, code);

	decon_abd_enable(decon, 0);

	abd_printf(NULL, "isync: %d\n", decon->ignore_vsync);
	decon_debug_pin_print(NULL, &abd->pin[ABD_PIN_PCD]);
	decon_debug_pin_print(NULL, &abd->pin[ABD_PIN_DET]);
	decon_debug_pin_print(NULL, &abd->pin[ABD_PIN_ERR]);

	decon_debug_fto_print(NULL, &abd->f_first);
	decon_debug_fto_print(NULL, &abd->f_lcdon);
	decon_debug_fto_print(NULL, &abd->f_event);

	decon_debug_udr_print(NULL, &abd->u_first);
	decon_debug_udr_print(NULL, &abd->u_lcdon);
	decon_debug_udr_print(NULL, &abd->u_event);

	decon_info("-- %s: %lu\n",  __func__, code);

	return NOTIFY_DONE;
}

static int decon_abd_register_function(struct decon_device *decon, struct abd_pin *pin, char *keyword,
		irqreturn_t func(int irq, void *dev_id))
{
	int ret = 0, gpio = 0;
	enum of_gpio_flags flags;
	struct device_node *np = NULL;
	struct device *dev = decon->dev;
	unsigned int irqf_type = IRQF_TRIGGER_RISING;
	struct abd_trace *trace = &pin->p_lcdon;
	char *prefix_gpio = "gpio_";
	char dts_name[10] = {0, };

	np = dev->of_node;
	if (!np) {
		decon_warn("device node not exist\n");
		goto exit;
	}

	if (strlen(keyword) + strlen(prefix_gpio) >= sizeof(dts_name)) {
		decon_warn("%s: %s is too log(%zu)\n", __func__, keyword, strlen(keyword));
		goto exit;
	}

	scnprintf(dts_name, sizeof(dts_name), "%s%s", prefix_gpio, keyword);

	gpio = of_get_named_gpio_flags(np, dts_name, 0, &flags);
	if (gpio_is_valid(gpio)) {
		decon_info("%s: found %s(%d) success\n", __func__, dts_name, gpio);
		pin->gpio = gpio;
		pin->irq = gpio_to_irq(pin->gpio);
		pin->active_level = !(flags & OF_GPIO_ACTIVE_LOW);
		irqf_type = (flags & OF_GPIO_ACTIVE_LOW) ? IRQF_TRIGGER_FALLING : IRQF_TRIGGER_RISING;
		decon_info("%s: %s is active %s, %s\n", __func__, keyword, pin->active_level ? "high" : "low",
			(irqf_type == IRQF_TRIGGER_RISING) ? "rising" : "falling");
		ret++;

		if (pin->irq) {
			pin->name = keyword;
			pin->p_first.name = "first";
			pin->p_lcdon.name = "lcdon";
			pin->p_event.name = "event";

			irq_set_irq_type(pin->irq, irqf_type);
			irq_set_status_flags(pin->irq, _IRQ_NOAUTOEN);
			decon_abd_clear_pending_bit(pin->irq);

			if (devm_request_irq(dev, pin->irq, func, irqf_type, keyword, decon)) {
				decon_err("%s: failed to request irq for %s\n", __func__, keyword);
				pin->irq = 0;
				ret--;
			}

			pin->level = gpio_get_value(pin->gpio);
			if (pin->level == pin->active_level) {
				decon_info("%s: %s(%d) is already %s(%d)\n", __func__, keyword, pin->gpio,
					(pin->active_level) ? "high" : "low", pin->level);

				decon_abd_save_log_pin(decon, pin, trace, 1);

				if (pin->name && !strcmp(pin->name, "pcd")) {
					decon->ignore_vsync = 1;
					decon_info("%s: ignore_vsync: %d\n", __func__, decon->ignore_vsync);
				}
			}
		}
	}

exit:
	return ret;
}

int decon_abd_register(struct decon_device *decon)
{
	int ret = 0;
	struct abd_protect *abd = &decon->abd;

	decon_info("%s: ++\n", __func__);

	ret += decon_abd_register_function(decon, &abd->pin[ABD_PIN_PCD], "pcd", decon_abd_pcd_handler);
	ret += decon_abd_register_function(decon, &abd->pin[ABD_PIN_DET], "det", decon_abd_det_handler);
	ret += decon_abd_register_function(decon, &abd->pin[ABD_PIN_ERR], "err", decon_abd_err_handler);

	abd->u_first.name = abd->f_first.name = "first";
	abd->u_lcdon.name = abd->f_lcdon.name = "lcdon";
	abd->u_event.name = abd->f_event.name = "event";

	if (ret < 0)
		goto exit;

	debugfs_create_file("debug", 0444, decon->d.debug_root, decon, &decon_debug_fops);

	abd->reboot_notifier.notifier_call = decon_abd_reboot_notifier;
	register_reboot_notifier(&abd->reboot_notifier);
exit:
	decon_info("%s: -- %d entity was registered\n", __func__, ret);

	return ret;
}

