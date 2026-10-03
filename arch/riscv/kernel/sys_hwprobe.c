// SPDX-License-Identifier: GPL-2.0-only
/*
 * The hwprobe interface, for allowing userspace to probe to see which features
 * are supported by the hardware.  See Documentation/arch/riscv/hwprobe.rst for
 * more details.
 */
#include <linux/syscalls.h>
#include <asm/cacheflush.h>
#include <asm/hwprobe.h>
#include <asm/processor.h>
#include <asm/delay.h>
#include <asm/switch_to.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>
#include <vdso/vsyscall.h>

static void hwprobe_one_pair(struct riscv_hwprobe *pair,
			     const struct cpumask *cpus)
{
	pair->value = 0;

	switch (pair->key) {
	/*
	 * The kernel already assumes that the base single-letter ISA
	 * extensions are supported on all harts, and only supports the
	 * IMA base, so just cheat a bit here and tell that to
	 * userspace.
	 */
	case RISCV_HWPROBE_KEY_BASE_BEHAVIOR:
		pair->value = RISCV_HWPROBE_BASE_BEHAVIOR_IMA;
		break;

	case RISCV_HWPROBE_KEY_CPUPERF_0:
	case RISCV_HWPROBE_KEY_MISALIGNED_SCALAR_PERF:
		pair->value = RISCV_HWPROBE_MISALIGNED_SCALAR_EMULATED;
		break;

	case RISCV_HWPROBE_KEY_HIGHEST_VIRT_ADDRESS:
		pair->value = user_max_virt_addr();
		break;

	case RISCV_HWPROBE_KEY_TIME_CSR_FREQ:
		pair->value = riscv_timebase;
		break;

	/*
	 * For forward compatibility, unknown keys don't fail the whole
	 * call, but get their element key set to -1 and value set to 0
	 * indicating they're unrecognized.
	 */
	default:
		pair->key = -1;
		pair->value = 0;
		break;
	}
}

static int hwprobe_get_values(struct riscv_hwprobe __user *pairs,
			      size_t pair_count, size_t cpusetsize,
			      unsigned long __user *cpus_user,
			      unsigned int flags)
{
	size_t out;
	int ret;
	cpumask_t cpus;

	/* Check the reserved flags. */
	if (flags != 0)
		return -EINVAL;

	/*
	 * The interface supports taking in a CPU mask, and returns values that
	 * are consistent across that mask. Allow userspace to specify NULL and
	 * 0 as a shortcut to all online CPUs.
	 */
	cpumask_clear(&cpus);
	if (!cpusetsize && !cpus_user) {
		cpumask_copy(&cpus, cpu_online_mask);
	} else {
		if (cpusetsize > cpumask_size())
			cpusetsize = cpumask_size();

		ret = copy_from_user(&cpus, cpus_user, cpusetsize);
		if (ret)
			return -EFAULT;

		/*
		 * Userspace must provide at least one online CPU, without that
		 * there's no way to define what is supported.
		 */
		cpumask_and(&cpus, &cpus, cpu_online_mask);
		if (cpumask_empty(&cpus))
			return -EINVAL;
	}

	for (out = 0; out < pair_count; out++, pairs++) {
		struct riscv_hwprobe pair;

		if (get_user(pair.key, &pairs->key))
			return -EFAULT;

		pair.value = 0;
		hwprobe_one_pair(&pair, &cpus);
		ret = put_user(pair.key, &pairs->key);
		if (ret == 0)
			ret = put_user(pair.value, &pairs->value);

		if (ret)
			return -EFAULT;
	}

	return 0;
}

static int hwprobe_get_cpus(struct riscv_hwprobe __user *pairs,
			    size_t pair_count, size_t cpusetsize,
			    unsigned long __user *cpus_user,
			    unsigned int flags)
{
	cpumask_t cpus, one_cpu;
	bool clear_all = false;
	size_t i;
	int ret;

	if (flags != RISCV_HWPROBE_WHICH_CPUS)
		return -EINVAL;

	if (!cpusetsize || !cpus_user)
		return -EINVAL;

	if (cpusetsize > cpumask_size())
		cpusetsize = cpumask_size();

	cpumask_clear(&cpus);
	ret = copy_from_user(&cpus, cpus_user, cpusetsize);
	if (ret)
		return -EFAULT;

	if (cpumask_empty(&cpus))
		cpumask_copy(&cpus, cpu_online_mask);

	cpumask_and(&cpus, &cpus, cpu_online_mask);

	cpumask_clear(&one_cpu);

	for (i = 0; i < pair_count; i++) {
		struct riscv_hwprobe pair, tmp;
		int cpu;

		ret = copy_from_user(&pair, &pairs[i], sizeof(pair));
		if (ret)
			return -EFAULT;

		if (!riscv_hwprobe_key_is_valid(pair.key)) {
			clear_all = true;
			pair = (struct riscv_hwprobe){ .key = -1, };
			ret = copy_to_user(&pairs[i], &pair, sizeof(pair));
			if (ret)
				return -EFAULT;
		}

		if (clear_all)
			continue;

		tmp = (struct riscv_hwprobe){ .key = pair.key, };

		for_each_cpu(cpu, &cpus) {
			cpumask_set_cpu(cpu, &one_cpu);

			hwprobe_one_pair(&tmp, &one_cpu);

			if (!riscv_hwprobe_pair_cmp(&tmp, &pair))
				cpumask_clear_cpu(cpu, &cpus);

			cpumask_clear_cpu(cpu, &one_cpu);
		}
	}

	if (clear_all)
		cpumask_clear(&cpus);

	ret = copy_to_user(cpus_user, &cpus, cpusetsize);
	if (ret)
		return -EFAULT;

	return 0;
}

static int do_riscv_hwprobe(struct riscv_hwprobe __user *pairs,
			    size_t pair_count, size_t cpusetsize,
			    unsigned long __user *cpus_user,
			    unsigned int flags)
{
	if (flags & RISCV_HWPROBE_WHICH_CPUS)
		return hwprobe_get_cpus(pairs, pair_count, cpusetsize,
					cpus_user, flags);

	return hwprobe_get_values(pairs, pair_count, cpusetsize,
				  cpus_user, flags);
}

SYSCALL_DEFINE5(riscv_hwprobe, struct riscv_hwprobe __user *, pairs,
		size_t, pair_count, size_t, cpusetsize, unsigned long __user *,
		cpus, unsigned int, flags)
{
	return do_riscv_hwprobe(pairs, pair_count, cpusetsize,
				cpus, flags);
}
