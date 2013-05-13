/* Author: Christopher R. Palmer <crpalmer@gmail.com>
 *
 * Very loosely based on a version released by HTC that was
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/msm_tsens.h>
#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/msm_tsens.h>
#include <linux/msm_thermal.h>
#include <mach/cpufreq.h>
#include <mach/perflock.h>
#include <linux/earlysuspend.h>

#define NO_RELEASE_TEMPERATURE	0
#define NO_TRIGGER_TEMPERATURE	-1

#define N_TEMP_LIMITS	4

static unsigned temp_hysteresis = 5;
static unsigned int limit_temp_degC[N_TEMP_LIMITS] = { 65, 70, 75, 85 };
static unsigned int limit_freq[N_TEMP_LIMITS] = { 1728000, 1350000, 918000, 384000 };

module_param_array(limit_temp_degC, uint, NULL, 0644);
module_param_array(limit_freq, uint, NULL, 0644);

static int throttled_bin = -1;

module_param(throttled_bin, int, 0444);

#define DEF_TEMP_SENSOR      0

//max thermal limit
#define DEF_ALLOWED_MAX_HIGH 76
#define DEF_ALLOWED_MAX_FREQ 384000

//mid thermal limit
#define DEF_ALLOWED_MID_HIGH 72
#define DEF_ALLOWED_MID_FREQ 648000

//low thermal limit
#define DEF_ALLOWED_LOW_HIGH 70
#define DEF_ALLOWED_LOW_FREQ 972000

//Sampling interval
#define DEF_THERMAL_CHECK_MS 1250


static int enabled;

//Throttling indicator, 0=not throttled, 1=low, 2=mid, 3=max
static int thermal_throttled = 0;

//Safe the cpu max freq before throttling
static int pre_throttled_max = 0;

static struct msm_thermal_data msm_thermal_info;
static struct delayed_work first_work;
static struct work_struct trip_work;

static int max_freq(int throttled_bin)

static struct msm_thermal_tuners {
	unsigned int allowed_max_high;
	unsigned int allowed_max_low;
	unsigned int allowed_max_freq;

	unsigned int allowed_mid_high;
	unsigned int allowed_mid_low;
	unsigned int allowed_mid_freq;

	unsigned int allowed_low_high;
	unsigned int allowed_low_low;
	unsigned int allowed_low_freq;

	unsigned int check_interval_ms;

} msm_thermal_tuners_ins = {
	.allowed_max_high = DEF_ALLOWED_MAX_HIGH,
	.allowed_max_low = (DEF_ALLOWED_MAX_HIGH - 5),
	.allowed_max_freq = DEF_ALLOWED_MAX_FREQ,

	.allowed_mid_high = DEF_ALLOWED_MID_HIGH,
	.allowed_mid_low = (DEF_ALLOWED_MID_HIGH - 5),
	.allowed_mid_freq = DEF_ALLOWED_MID_FREQ,

	.allowed_low_high = DEF_ALLOWED_LOW_HIGH,
	.allowed_low_low = (DEF_ALLOWED_LOW_HIGH - 6),
	.allowed_low_freq = DEF_ALLOWED_LOW_FREQ,

	.check_interval_ms = DEF_THERMAL_CHECK_MS,
};



static int update_cpu_max_freq(int cpu, uint32_t max_freq)
{
	if (throttled_bin < 0) return MSM_CPUFREQ_NO_LIMIT;
	else return limit_freq[throttled_bin];
}

static int limit_temp(int throttled_bin)
{
	if (throttled_bin < 0) return limit_temp_degC[0];
	else if (throttled_bin == N_TEMP_LIMITS-1) return NO_TRIGGER_TEMPERATURE;
	else return limit_temp_degC[throttled_bin+1];
}

static int release_temp(int throttled_bin)
{
	if (throttled_bin < 0) return NO_RELEASE_TEMPERATURE;
	else return limit_temp_degC[throttled_bin] - temp_hysteresis;
}
	
static int update_cpu_max_freq(int cpu, int throttled_bin, unsigned temp)
{
	int ret;
	int max_frequency = max_freq(throttled_bin);

	ret = msm_cpufreq_set_freq_limits(cpu, MSM_CPUFREQ_NO_LIMIT, max_frequency);
	if (ret)
		return ret;

	ret = cpufreq_update_policy(cpu);
	if (ret)
		return ret;

	if (max_frequency != MSM_CPUFREQ_NO_LIMIT) {
		struct cpufreq_policy policy;

		if ((ret = cpufreq_get_policy(&policy, cpu)) == 0)
			ret = cpufreq_driver_target(&policy, max_frequency, CPUFREQ_RELATION_L);
	}

	if (max_frequency != MSM_CPUFREQ_NO_LIMIT)
		pr_info("msm_thermal: limiting cpu%d max frequency to %d at %u degC\n",
				cpu, max_frequency, temp);
	else
		pr_info("msm_thermal: Max frequency reset for cpu%d at %u degC\n", cpu, temp);

	return ret;
}

static void
update_all_cpus_max_freq_if_changed(int new_throttled_bin, unsigned temp)
{
	int cpu;
	int ret;

	if (throttled_bin == new_throttled_bin)
		return;

#ifdef CONFIG_PERFLOCK_BOOT_LOCK
	release_boot_lock();
#endif

        struct cpufreq_policy *cpu_policy = NULL;
        struct tsens_device tsens_dev;
        unsigned long temp = 0;
        unsigned int max_freq = 0;
        int update_policy = 0;
        int cpu = 0;
        int ret = 0;

        tsens_dev.sensor_num = DEF_TEMP_SENSOR;
        ret = tsens_get_temp(&tsens_dev, &temp);
        if (ret) {
                pr_err("msm_thermal: Unable to read TSENS sensor %d\n",
                                tsens_dev.sensor_num);
                goto reschedule;
        }

        for_each_possible_cpu(cpu) {
                update_policy = 0;
                cpu_policy = cpufreq_cpu_get(cpu);
                if (!cpu_policy) {
                        pr_debug("msm_thermal: NULL policy on cpu %d\n", cpu);
                        continue;
                }

	throttled_bin = new_throttled_bin;
	
}

static void
configure_sensor_trip_points(void)

	        //low trip point
                if ((temp >= msm_thermal_tuners_ins.allowed_low_high) &&
                    (temp < msm_thermal_tuners_ins.allowed_mid_high) &&
                    (cpu_policy->max > msm_thermal_tuners_ins.allowed_low_freq)) {
                        update_policy = 1;
                        /* save pre-throttled max freq value */
                        pre_throttled_max = cpu_policy->max;
                        max_freq = msm_thermal_tuners_ins.allowed_low_freq;
                        thermal_throttled = 1;
                        pr_warn("msm_thermal: Thermal Throttled (low)! temp: %lu\n", temp);
                //low clr point
                } else if ((temp < msm_thermal_tuners_ins.allowed_low_low) &&
                           (thermal_throttled > 0)) {
                        if (cpu_policy->max < cpu_policy->cpuinfo.max_freq) {
                                if (pre_throttled_max != 0)
                                        max_freq = pre_throttled_max;
                                else {
                                        max_freq = msm_thermal_tuners_ins.allowed_low_freq;
                                        thermal_throttled = 1;
                                        pr_warn("msm_thermal: ERROR! pre_throttled_max=0, falling back to %u\n", max_freq);
                                }
                                update_policy = 1;
                                /* wait until 2nd core is unthrottled */
                                if (cpu == 1)
                                        thermal_throttled = 0;
                                pr_warn("msm_thermal: Low Thermal Throttling Ended! temp: %lu\n", temp);
                        }
                //mid trip point
                } else if ((temp >= msm_thermal_tuners_ins.allowed_low_high) &&
                           (temp < msm_thermal_tuners_ins.allowed_mid_low) &&
                           (cpu_policy->max > msm_thermal_tuners_ins.allowed_mid_freq)) {
                        update_policy = 1;
                        max_freq = msm_thermal_tuners_ins.allowed_low_freq;
                        thermal_throttled = 2;
                        pr_warn("msm_thermal: Thermal Throttled (mid)! temp: %lu\n", temp);
                //mid clr point
                } else if ( (temp < msm_thermal_tuners_ins.allowed_mid_low) &&
                           (thermal_throttled > 1)) {
                        if (cpu_policy->max < cpu_policy->cpuinfo.max_freq) {
                                max_freq = msm_thermal_tuners_ins.allowed_low_freq;
                                update_policy = 1;
                                /* wait until 2nd core is unthrottled */
                                if (cpu == 1)
                                        thermal_throttled = 1;
                                pr_warn("msm_thermal: Mid Thermal Throttling Ended! temp: %lu\n", temp);
                        }
                //max trip point
                } else if ((temp >= msm_thermal_tuners_ins.allowed_max_high) &&
                           (cpu_policy->max > msm_thermal_tuners_ins.allowed_max_freq)) {
                        update_policy = 1;
                        max_freq = msm_thermal_tuners_ins.allowed_max_freq;
                        thermal_throttled = 3;
                        pr_warn("msm_thermal: Thermal Throttled (max)! temp: %lu\n", temp);
                //max clr point
                } else if ((temp < msm_thermal_tuners_ins.allowed_max_low) &&
                           (thermal_throttled > 2)) {
                        if (cpu_policy->max < cpu_policy->cpuinfo.max_freq) {
                                max_freq = msm_thermal_tuners_ins.allowed_mid_freq;
                                update_policy = 1;
                                /* wait until 2nd core is unthrottled */
                                if (cpu == 1)
                                        thermal_throttled = 2;
                                pr_warn("msm_thermal: Max Thermal Throttling Ended! temp: %lu\n", temp);
                        }
                }


            /* Update new limits */
        for_each_possible_cpu(cpu) {
                ret = update_cpu_max_freq(cpu, max_freq);
                if (ret)
                        pr_debug("Unable to limit cpu%d max freq to %d\n",
                                        cpu, max_freq);
        }
}

reschedule:
        if (enabled)
                schedule_delayed_work(&check_temp_work,
                                msecs_to_jiffies(msm_thermal_tuners_ins.check_interval_ms));
}
static void disable_msm_thermal(void)
>>>>>>> 4ed71e6... sysfs entry for thermal
{
	int trigger_temperature = limit_temp(throttled_bin);
	int release_temperature = release_temp(throttled_bin);

	pr_info("msm_thermal: setting trip range %d..%d on sensor %d.\n", release_temperature, 			trigger_temperature, msm_thermal_info.sensor_id); 
	if (trigger_temperature != NO_TRIGGER_TEMPERATURE)
		tsens_set_tz_warm_temp_degC(msm_thermal_info.sensor_id, trigger_temperature, &trip_work);

	if (release_temperature != NO_RELEASE_TEMPERATURE)
		tsens_set_tz_cool_temp_degC(msm_thermal_info.sensor_id, release_temperature, &trip_work);
}

static int
select_throttled_bin(unsigned temp)
{
	int i;
	int new_bin = -1;

	for (i = 0; i < N_TEMP_LIMITS; i++) {
		if (temp >= limit_temp_degC[i]) new_bin = i;
	}

	if (new_bin > throttled_bin) return new_bin;
	if (temp <= release_temp(throttled_bin)) return new_bin;
	return throttled_bin;
}

static void check_temp_and_throttle_if_needed(struct work_struct *work)
{
	struct tsens_device tsens_dev;
	unsigned long temp_ul = 0;
	unsigned temp;
	int new_bin;
	int ret;

	tsens_dev.sensor_num = msm_thermal_info.sensor_id;
	ret = tsens_get_temp(&tsens_dev, &temp_ul);
	if (ret) {
		pr_warn("msm_thermal: Unable to read TSENS sensor %d\n",
				tsens_dev.sensor_num);
		return;
	}

	temp = (unsigned) temp_ul;
	new_bin = select_throttled_bin(temp);

	pr_debug("msm_thermal: TSENS sensor %d is %u degC old-bin %d new-bin %d\n",
		tsens_dev.sensor_num, temp, throttled_bin, new_bin);
	update_all_cpus_max_freq_if_changed(new_bin, temp);
}

static void check_temp(struct work_struct *work)
{
	check_temp_and_throttle_if_needed(work);
	configure_sensor_trip_points();
}

int __init msm_thermal_init(struct msm_thermal_data *pdata)
{
static struct kernel_param_ops module_ops = {
	.set = set_enabled,
	.get = param_get_bool,
};

module_param_cb(enabled, &module_ops, &enabled, 0644);
MODULE_PARM_DESC(enabled, "enforce thermal limit on cpu");
/**************************** SYSFS START ****************************/
struct kobject *msm_thermal_kobject;

#define show_one(file_name, object)					\
static ssize_t show_##file_name						\
(struct kobject *kobj, struct attribute *attr, char *buf)               \
{									\
	return sprintf(buf, "%u\n", msm_thermal_tuners_ins.object);				\
}

show_one(allowed_max_high, allowed_max_high);
show_one(allowed_max_low, allowed_max_low);
show_one(allowed_max_freq, allowed_max_freq);
show_one(allowed_mid_high, allowed_mid_high);
show_one(allowed_mid_low, allowed_mid_low);
show_one(allowed_mid_freq, allowed_mid_freq);
show_one(allowed_low_high, allowed_low_high);
show_one(allowed_low_low, allowed_low_low);
show_one(allowed_low_freq, allowed_low_freq);
show_one(check_interval_ms, check_interval_ms);

static ssize_t store_allowed_max_high(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	msm_thermal_tuners_ins.allowed_max_high = input;

	return count;
}

static ssize_t store_allowed_max_low(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	msm_thermal_tuners_ins.allowed_max_low = input;

	return count;
}

static ssize_t store_allowed_max_freq(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	msm_thermal_tuners_ins.allowed_max_freq = input;

	return count;
}

static ssize_t store_allowed_mid_high(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	msm_thermal_tuners_ins.allowed_mid_high = input;

	return count;
}

static ssize_t store_allowed_mid_low(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	msm_thermal_tuners_ins.allowed_mid_low = input;

	return count;
}

static ssize_t store_allowed_mid_freq(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	msm_thermal_tuners_ins.allowed_mid_freq = input;

	return count;
}

static ssize_t store_allowed_low_high(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	msm_thermal_tuners_ins.allowed_low_high = input;

	return count;
}

static ssize_t store_allowed_low_low(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	msm_thermal_tuners_ins.allowed_low_low = input;

	return count;
}

static ssize_t store_allowed_low_freq(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	msm_thermal_tuners_ins.allowed_low_freq = input;

	return count;
}

static ssize_t store_check_interval_ms(struct kobject *a, struct attribute *b,
				   const char *buf, size_t count)
{
	unsigned int input;
	int ret;
	ret = sscanf(buf, "%u", &input);
	if (ret != 1)
		return -EINVAL;

	msm_thermal_tuners_ins.check_interval_ms = input;

	return count;
}


define_one_global_rw(allowed_max_high);
define_one_global_rw(allowed_max_low);
define_one_global_rw(allowed_max_freq);
define_one_global_rw(allowed_mid_high);
define_one_global_rw(allowed_mid_low);
define_one_global_rw(allowed_mid_freq);
define_one_global_rw(allowed_low_high);
define_one_global_rw(allowed_low_low);
define_one_global_rw(allowed_low_freq);
define_one_global_rw(check_interval_ms);

static struct attribute *msm_thermal_attributes[] = {
	&allowed_max_high.attr,
	&allowed_max_low.attr,
	&allowed_max_freq.attr,
	&allowed_mid_high.attr,
	&allowed_mid_low.attr,
	&allowed_mid_freq.attr,
	&allowed_low_high.attr,
	&allowed_low_low.attr,
	&allowed_low_freq.attr,
	&check_interval_ms.attr,
	NULL
};


static struct attribute_group msm_thermal_attr_group = {
	.attrs = msm_thermal_attributes,
	.name = "conf",
};
/**************************** SYSFS END ****************************/

int __init msm_thermal_init(struct msm_thermal_data *pdata)
{
	int rc, ret = 0;

	BUG_ON(!pdata);
	BUG_ON(pdata->sensor_id >= TSENS_MAX_SENSORS);
	memcpy(&msm_thermal_info, pdata, sizeof(struct msm_thermal_data));


	INIT_DELAYED_WORK(&first_work, check_temp);
	INIT_WORK(&trip_work, check_temp);

	schedule_delayed_work(&first_work, msecs_to_jiffies(5*1000)); 

	return 0;

	msm_thermal_kobject = kobject_create_and_add("msm_thermal", kernel_kobj);
	if (msm_thermal_kobject) {
		rc = sysfs_create_group(msm_thermal_kobject,
							&msm_thermal_attr_group);
		if (rc) {
			pr_warn("msm_thermal: sysfs: ERROR, could not create sysfs group");
		}
	} else
		pr_warn("msm_thermal: sysfs: ERROR, could not create sysfs kobj");


	return ret;
}
