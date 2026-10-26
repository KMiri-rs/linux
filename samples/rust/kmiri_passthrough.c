// SPDX-License-Identifier: GPL-2.0

#include <linux/bitmap.h>
#include <linux/find.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#define KMIRI_PT_NAME "kmiri_pt"
#define KMIRI_MAX_CMD (128 * 1024)
#define KMIRI_MAX_RESP (128 * 1024)

static struct proc_dir_entry *kmiri_pt_entry;
static DEFINE_MUTEX(kmiri_pt_lock);
static char *kmiri_resp;
static size_t kmiri_resp_len;

static void kmiri_set_resp_err(const char *msg)
{
	kmiri_resp_len = scnprintf(kmiri_resp, KMIRI_MAX_RESP, "ERR %s\n", msg);
}

static int kmiri_hex_val(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int kmiri_hex_to_bytes(const char *hex, u8 *out, size_t out_len)
{
	size_t i;

	for (i = 0; i < out_len; i++) {
		int h = kmiri_hex_val(hex[i * 2]);
		int l = kmiri_hex_val(hex[i * 2 + 1]);

		if (h < 0 || l < 0)
			return -EINVAL;
		out[i] = (h << 4) | l;
	}
	if (hex[out_len * 2] != '\0')
		return -EINVAL;
	return 0;
}

static int kmiri_bytes_to_hex(const u8 *in, size_t in_len, char *out, size_t out_len)
{
	static const char tab[] = "0123456789abcdef";
	size_t i;

	if (out_len < in_len * 2 + 1)
		return -ENOSPC;
	for (i = 0; i < in_len; i++) {
		u8 v = in[i];
		out[i * 2] = tab[v >> 4];
		out[i * 2 + 1] = tab[v & 0xf];
	}
	out[in_len * 2] = '\0';
	return 0;
}

static int kmiri_parse_ulong(const char *s, unsigned long *v)
{
	int ret;

	ret = kstrtoul(s, 0, v);
	if (ret)
		return ret;
	return 0;
}

static int kmiri_parse_u32(const char *s, u32 *v)
{
	int ret;
	unsigned long tmp;

	ret = kstrtoul(s, 0, &tmp);
	if (ret)
		return ret;
	if (tmp > U32_MAX)
		return -ERANGE;
	*v = (u32)tmp;
	return 0;
}

static void kmiri_handle_cmd(char *cmd)
{
	char *op, *a, *b, *c, *d;

	op = strsep(&cmd, " \t\n");
	if (!op || *op == '\0') {
		kmiri_set_resp_err("empty");
		return;
	}

	if (!strcmp(op, "ping")) {
		kmiri_resp_len = scnprintf(kmiri_resp, KMIRI_MAX_RESP, "OK pong\n");
		return;
	}

	if (!strcmp(op, "bitmap_zalloc")) {
		u32 nbits;
		unsigned long flags;
		unsigned long *ptr;

		a = strsep(&cmd, " \t\n");
		b = strsep(&cmd, " \t\n");
		if (!a || !b || kmiri_parse_u32(a, &nbits) || kmiri_parse_ulong(b, &flags)) {
			kmiri_set_resp_err("bitmap_zalloc_args");
			return;
		}
		ptr = bitmap_zalloc(nbits, (gfp_t)flags);
		if (ptr)
			bitmap_free(ptr);
		kmiri_resp_len = scnprintf(kmiri_resp, KMIRI_MAX_RESP, "OK %u\n", ptr ? 1 : 0);
		return;
	}

	if (!strcmp(op, "bitmap_free")) {
		kmiri_resp_len = scnprintf(kmiri_resp, KMIRI_MAX_RESP, "OK 0\n");
		return;
	}

	if (!strcmp(op, "find_next_bit") || !strcmp(op, "find_next_zero_bit") ||
	    !strcmp(op, "find_last_bit") || !strcmp(op, "set_bit") ||
	    !strcmp(op, "clear_bit") || !strcmp(op, "set_bit_atomic") ||
	    !strcmp(op, "clear_bit_atomic")) {
		unsigned long nbits, start_or_index = 0;
		size_t nbytes;
		u8 *buf;
		unsigned long idx;
		char *hex_out;
		int err;

		a = strsep(&cmd, " \t\n");
		if (!a || kmiri_parse_ulong(a, &nbits)) {
			kmiri_set_resp_err("bitmap_nbits");
			return;
		}

		if (strcmp(op, "find_last_bit")) {
			b = strsep(&cmd, " \t\n");
			if (!b || kmiri_parse_ulong(b, &start_or_index)) {
				kmiri_set_resp_err("bitmap_index");
				return;
			}
			c = strsep(&cmd, " \t\n");
		} else {
			c = strsep(&cmd, " \t\n");
		}

		if (!c || *c == '\0') {
			kmiri_set_resp_err("bitmap_hex");
			return;
		}

		nbytes = DIV_ROUND_UP(nbits, 8UL);
		buf = kcalloc(nbytes ? nbytes : 1, sizeof(u8), GFP_KERNEL);
		if (!buf) {
			kmiri_set_resp_err("nomem");
			return;
		}
		err = kmiri_hex_to_bytes(c, buf, nbytes);
		if (err) {
			kfree(buf);
			kmiri_set_resp_err("bitmap_hex_decode");
			return;
		}

		if (!strcmp(op, "find_next_bit")) {
			idx = _find_next_bit((unsigned long *)buf, nbits, start_or_index);
			kmiri_resp_len = scnprintf(kmiri_resp, KMIRI_MAX_RESP, "OK %lu\n", idx);
			kfree(buf);
			return;
		}
		if (!strcmp(op, "find_next_zero_bit")) {
			idx = _find_next_zero_bit((unsigned long *)buf, nbits, start_or_index);
			kmiri_resp_len = scnprintf(kmiri_resp, KMIRI_MAX_RESP, "OK %lu\n", idx);
			kfree(buf);
			return;
		}
		if (!strcmp(op, "find_last_bit")) {
			idx = _find_last_bit((unsigned long *)buf, nbits);
			kmiri_resp_len = scnprintf(kmiri_resp, KMIRI_MAX_RESP, "OK %lu\n", idx);
			kfree(buf);
			return;
		}

		if (!strcmp(op, "set_bit") || !strcmp(op, "set_bit_atomic"))
			set_bit(start_or_index, (unsigned long *)buf);
		else
			clear_bit(start_or_index, (unsigned long *)buf);

		hex_out = kmalloc(nbytes * 2 + 1, GFP_KERNEL);
		if (!hex_out) {
			kfree(buf);
			kmiri_set_resp_err("nomem_hex");
			return;
		}
		err = kmiri_bytes_to_hex(buf, nbytes, hex_out, nbytes * 2 + 1);
		if (err) {
			kfree(hex_out);
			kfree(buf);
			kmiri_set_resp_err("bitmap_hex_encode");
			return;
		}
		kmiri_resp_len = scnprintf(kmiri_resp, KMIRI_MAX_RESP, "OK 0 %s\n", hex_out);
		kfree(hex_out);
		kfree(buf);
		return;
	}

	if (!strcmp(op, "bitmap_copy_and_extend")) {
		unsigned long src_bits, dst_bits;
		size_t src_bytes, dst_bytes;
		u8 *src, *dst;
		char *hex_out;
		int err;
		unsigned int count;

		a = strsep(&cmd, " \t\n");
		b = strsep(&cmd, " \t\n");
		c = strsep(&cmd, " \t\n");
		d = strsep(&cmd, " \t\n");
		if (!a || !b || !c || !d || kmiri_parse_ulong(a, &src_bits) ||
		    kmiri_parse_ulong(b, &dst_bits)) {
			kmiri_set_resp_err("copy_args");
			return;
		}

		src_bytes = DIV_ROUND_UP(src_bits, 8UL);
		dst_bytes = DIV_ROUND_UP(dst_bits, 8UL);
		src = kcalloc(src_bytes ? src_bytes : 1, sizeof(u8), GFP_KERNEL);
		dst = kcalloc(dst_bytes ? dst_bytes : 1, sizeof(u8), GFP_KERNEL);
		if (!src || !dst) {
			kfree(src);
			kfree(dst);
			kmiri_set_resp_err("nomem");
			return;
		}
		err = kmiri_hex_to_bytes(c, src, src_bytes);
		if (!err)
			err = kmiri_hex_to_bytes(d, dst, dst_bytes);
		if (err) {
			kfree(src);
			kfree(dst);
			kmiri_set_resp_err("copy_hex_decode");
			return;
		}

		count = min(src_bits, dst_bits);
		bitmap_copy_and_extend((unsigned long *)dst, (const unsigned long *)src,
				       count, dst_bits);

		hex_out = kmalloc(dst_bytes * 2 + 1, GFP_KERNEL);
		if (!hex_out) {
			kfree(src);
			kfree(dst);
			kmiri_set_resp_err("nomem_hex");
			return;
		}
		err = kmiri_bytes_to_hex(dst, dst_bytes, hex_out, dst_bytes * 2 + 1);
		if (err) {
			kfree(hex_out);
			kfree(src);
			kfree(dst);
			kmiri_set_resp_err("copy_hex_encode");
			return;
		}
		kmiri_resp_len = scnprintf(kmiri_resp, KMIRI_MAX_RESP, "OK 0 %s\n", hex_out);
		kfree(hex_out);
		kfree(src);
		kfree(dst);
		return;
	}

	kmiri_set_resp_err("unknown_op");
}

static ssize_t kmiri_pt_write(struct file *file, const char __user *buf,
		size_t count, loff_t *ppos)
{
	char *kbuf;

	if (!count || count > KMIRI_MAX_CMD)
		return -EINVAL;

	kbuf = memdup_user_nul(buf, count);
	if (IS_ERR(kbuf))
		return PTR_ERR(kbuf);

	mutex_lock(&kmiri_pt_lock);
	kmiri_handle_cmd(kbuf);
	mutex_unlock(&kmiri_pt_lock);

	kfree(kbuf);
	*ppos = 0;
	return count;
}

static ssize_t kmiri_pt_read(struct file *file, char __user *buf, size_t count,
		loff_t *ppos)
{
	ssize_t ret;

	mutex_lock(&kmiri_pt_lock);
	ret = simple_read_from_buffer(buf, count, ppos, kmiri_resp, kmiri_resp_len);
	mutex_unlock(&kmiri_pt_lock);
	return ret;
}

static const struct proc_ops kmiri_pt_ops = {
	.proc_read = kmiri_pt_read,
	.proc_write = kmiri_pt_write,
};

static int __init kmiri_passthrough_init(void)
{
	kmiri_resp = kzalloc(KMIRI_MAX_RESP, GFP_KERNEL);
	if (!kmiri_resp)
		return -ENOMEM;

	kmiri_resp_len = scnprintf(kmiri_resp, KMIRI_MAX_RESP, "OK boot\n");
	kmiri_pt_entry = proc_create(KMIRI_PT_NAME, 0666, NULL, &kmiri_pt_ops);
	if (!kmiri_pt_entry) {
		kfree(kmiri_resp);
		kmiri_resp = NULL;
		return -ENOMEM;
	}
	pr_info("kmiri_passthrough: /proc/%s ready\n", KMIRI_PT_NAME);
	return 0;
}

static void __exit kmiri_passthrough_exit(void)
{
	if (kmiri_pt_entry)
		proc_remove(kmiri_pt_entry);
	kfree(kmiri_resp);
	kmiri_resp = NULL;
}

late_initcall(kmiri_passthrough_init);
module_exit(kmiri_passthrough_exit);

MODULE_AUTHOR("KMiri experiment");
MODULE_DESCRIPTION("KMiri passthrough helper");
MODULE_LICENSE("GPL");
