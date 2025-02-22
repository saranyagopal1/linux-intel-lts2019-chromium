// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[drm-dp] %s: " fmt, __func__

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include "dp_power.h"

struct dp_power_private {
	struct dp_parser *parser;
	struct platform_device *pdev;
	struct clk *link_clk_src;
	struct clk *pixel_provider;
	struct clk *link_provider;
	struct regulator_bulk_data supplies[DP_DEV_REGULATOR_MAX];

	struct dp_power dp_power;
};

static void dp_power_regulator_disable(struct dp_power_private *power)
{
	struct regulator_bulk_data *s = power->supplies;
	const struct dp_reg_entry *regs = power->parser->regulator_cfg->regs;
	int num = power->parser->regulator_cfg->num;
	int i;

	DBG("");
	for (i = num - 1; i >= 0; i--)
		if (regs[i].disable_load >= 0)
			regulator_set_load(s[i].consumer,
					   regs[i].disable_load);

	regulator_bulk_disable(num, s);
}

static int dp_power_regulator_enable(struct dp_power_private *power)
{
	struct regulator_bulk_data *s = power->supplies;
	const struct dp_reg_entry *regs = power->parser->regulator_cfg->regs;
	int num = power->parser->regulator_cfg->num;
	int ret, i;

	DBG("");
	for (i = 0; i < num; i++) {
		if (regs[i].enable_load >= 0) {
			ret = regulator_set_load(s[i].consumer,
						 regs[i].enable_load);
			if (ret < 0) {
				pr_err("regulator %d set op mode failed, %d\n",
					i, ret);
				goto fail;
			}
		}
	}

	ret = regulator_bulk_enable(num, s);
	if (ret < 0) {
		pr_err("regulator enable failed, %d\n", ret);
		goto fail;
	}

	return 0;

fail:
	for (i--; i >= 0; i--)
		regulator_set_load(s[i].consumer, regs[i].disable_load);
	return ret;
}

static int dp_power_regulator_init(struct dp_power_private *power)
{
	struct regulator_bulk_data *s = power->supplies;
	const struct dp_reg_entry *regs = power->parser->regulator_cfg->regs;
	struct platform_device *pdev = power->pdev;
	int num = power->parser->regulator_cfg->num;
	int i, ret;

	for (i = 0; i < num; i++)
		s[i].supply = regs[i].name;

	ret = devm_regulator_bulk_get(&pdev->dev, num, s);
	if (ret < 0) {
		pr_err("%s: failed to init regulator, ret=%d\n",
						__func__, ret);
		return ret;
	}

	return 0;
}

static int dp_power_clk_init(struct dp_power_private *power)
{
	int rc = 0;
	struct dss_module_power *core, *ctrl, *stream;
	struct device *dev = &power->pdev->dev;

	core = &power->parser->mp[DP_CORE_PM];
	ctrl = &power->parser->mp[DP_CTRL_PM];
	stream = &power->parser->mp[DP_STREAM_PM];

	if (power->parser->pll && power->parser->pll->get_provider) {
		rc = power->parser->pll->get_provider(power->parser->pll,
			&power->link_provider, &power->pixel_provider);
		if (rc) {
			DRM_ERROR("%s:provider failed,don't set parent\n",
								__func__);
			return 0;
		}
	}

	rc = msm_dss_get_clk(dev, core->clk_config, core->num_clk);
	if (rc) {
		DRM_ERROR("failed to get %s clk. err=%d\n",
			dp_parser_pm_name(DP_CORE_PM), rc);
		return rc;
	}

	rc = msm_dss_get_clk(dev, ctrl->clk_config, ctrl->num_clk);
	if (rc) {
		DRM_ERROR("failed to get %s clk. err=%d\n",
			dp_parser_pm_name(DP_CTRL_PM), rc);
		msm_dss_put_clk(core->clk_config, core->num_clk);
		return -ENODEV;
	}

	rc = msm_dss_get_clk(dev, stream->clk_config, stream->num_clk);
	if (rc) {
		DRM_ERROR("failed to get %s clk. err=%d\n",
			dp_parser_pm_name(DP_CTRL_PM), rc);
		msm_dss_put_clk(core->clk_config, core->num_clk);
		return -ENODEV;
	}

	return 0;
}

static int dp_power_clk_deinit(struct dp_power_private *power)
{
	struct dss_module_power *core, *ctrl, *stream;

	core = &power->parser->mp[DP_CORE_PM];
	ctrl = &power->parser->mp[DP_CTRL_PM];
	stream = &power->parser->mp[DP_STREAM_PM];

	if (!core || !ctrl || !stream) {
		DRM_ERROR("invalid power_data\n");
		return -EINVAL;
	}

	msm_dss_put_clk(ctrl->clk_config, ctrl->num_clk);
	msm_dss_put_clk(core->clk_config, core->num_clk);
	msm_dss_put_clk(stream->clk_config, stream->num_clk);
	return 0;
}

static int dp_power_clk_set_rate(struct dp_power_private *power,
		enum dp_pm_type module, bool enable)
{
	int rc = 0;
	struct dss_module_power *mp = &power->parser->mp[module];

	if (enable) {
		rc = msm_dss_clk_set_rate(mp->clk_config, mp->num_clk);
		if (rc) {
			DRM_ERROR("failed to set clks rate.\n");
			return rc;
		}
	}

	rc = msm_dss_enable_clk(mp->clk_config, mp->num_clk, enable);
	if (rc) {
		DRM_ERROR("failed to %d clks, err: %d\n", enable, rc);
		return rc;
	}

	return 0;
}

int dp_power_clk_status(struct dp_power *dp_power, enum dp_pm_type pm_type)
{
	if (pm_type == DP_CORE_PM)
		return dp_power->core_clks_on;

	if (pm_type == DP_CTRL_PM)
		return dp_power->link_clks_on;

	if (pm_type == DP_STREAM_PM)
		return dp_power->stream_clks_on;

	return 0;
}

int dp_power_clk_enable(struct dp_power *dp_power,
		enum dp_pm_type pm_type, bool enable)
{
	int rc = 0;
	struct dp_power_private *power;

	power = container_of(dp_power, struct dp_power_private, dp_power);

	if (pm_type != DP_CORE_PM && pm_type != DP_CTRL_PM &&
			pm_type != DP_STREAM_PM) {
		DRM_ERROR("unsupported power module: %s\n",
				dp_parser_pm_name(pm_type));
		return -EINVAL;
	}

	if (enable) {
		if (pm_type == DP_CORE_PM && dp_power->core_clks_on) {
			DRM_DEBUG_DP("core clks already enabled\n");
			return 0;
		}

		if (pm_type == DP_CTRL_PM && dp_power->link_clks_on) {
			DRM_DEBUG_DP("links clks already enabled\n");
			return 0;
		}

		if (pm_type == DP_STREAM_PM && dp_power->stream_clks_on) {
			DRM_DEBUG_DP("pixel clks already enabled\n");
			return 0;
		}

		if ((pm_type == DP_CTRL_PM) && (!dp_power->core_clks_on)) {
			DRM_DEBUG_DP("Enable core clks before link clks\n");

			rc = dp_power_clk_set_rate(power, DP_CORE_PM, enable);
			if (rc) {
				DRM_ERROR("fail to enable clks: %s. err=%d\n",
					dp_parser_pm_name(DP_CORE_PM), rc);
				return rc;
			}
			dp_power->core_clks_on = true;
		}
	}

	rc = dp_power_clk_set_rate(power, pm_type, enable);
	if (rc) {
		DRM_ERROR("failed to '%s' clks for: %s. err=%d\n",
			enable ? "enable" : "disable",
			dp_parser_pm_name(pm_type), rc);
			return rc;
	}

	if (pm_type == DP_CORE_PM)
		dp_power->core_clks_on = enable;
	else if (pm_type == DP_STREAM_PM)
		dp_power->stream_clks_on = enable;
	else
		dp_power->link_clks_on = enable;

	DRM_DEBUG_DP("%s clocks for %s\n",
			enable ? "enable" : "disable",
			dp_parser_pm_name(pm_type));
	DRM_DEBUG_DP("strem_clks:%s link_clks:%s core_clks:%s\n",
		dp_power->stream_clks_on ? "on" : "off",
		dp_power->link_clks_on ? "on" : "off",
		dp_power->core_clks_on ? "on" : "off");

	return 0;
}

int dp_power_client_init(struct dp_power *dp_power)
{
	int rc = 0;
	struct dp_power_private *power;

	if (!dp_power) {
		DRM_ERROR("invalid power data\n");
		return -EINVAL;
	}

	power = container_of(dp_power, struct dp_power_private, dp_power);

	pm_runtime_enable(&power->pdev->dev);

	rc = dp_power_regulator_init(power);
	if (rc) {
		DRM_ERROR("failed to init regulators %d\n", rc);
		goto error;
	}

	rc = dp_power_clk_init(power);
	if (rc) {
		DRM_ERROR("failed to init clocks %d\n", rc);
		goto error;
	}
	return 0;

error:
	pm_runtime_disable(&power->pdev->dev);
	return rc;
}

void dp_power_client_deinit(struct dp_power *dp_power)
{
	struct dp_power_private *power;

	if (!dp_power) {
		DRM_ERROR("invalid power data\n");
		return;
	}

	power = container_of(dp_power, struct dp_power_private, dp_power);

	dp_power_clk_deinit(power);
	pm_runtime_disable(&power->pdev->dev);

}

int dp_power_set_link_clk_parent(struct dp_power *dp_power)
{
	int rc = 0;
	struct dp_power_private *power;
	u32 num;
	struct dss_clk *cfg;
	char *name = "ctrl_link";

	if (!dp_power) {
		DRM_ERROR("invalid power data\n");
		rc = -EINVAL;
		goto exit;
	}

	power = container_of(dp_power, struct dp_power_private, dp_power);

	num = power->parser->mp[DP_CTRL_PM].num_clk;
	cfg = power->parser->mp[DP_CTRL_PM].clk_config;

	while (num && strcmp(cfg->clk_name, name)) {
		num--;
		cfg++;
	}

exit:
	return rc;
}

int dp_power_init(struct dp_power *dp_power, bool flip)
{
	int rc = 0;
	struct dp_power_private *power = NULL;

	if (!dp_power) {
		DRM_ERROR("invalid power data\n");
		return -EINVAL;
	}

	power = container_of(dp_power, struct dp_power_private, dp_power);

	pm_runtime_get_sync(&power->pdev->dev);
	rc = dp_power_regulator_enable(power);
	if (rc) {
		DRM_ERROR("failed to enable regulators, %d\n", rc);
		goto exit;
	}

	rc = dp_power_clk_enable(dp_power, DP_CORE_PM, true);
	if (rc) {
		DRM_ERROR("failed to enable DP core clocks, %d\n", rc);
		goto err_clk;
	}

	return 0;

err_clk:
	dp_power_regulator_disable(power);
exit:
	pm_runtime_put_sync(&power->pdev->dev);
	return rc;
}

int dp_power_deinit(struct dp_power *dp_power)
{
	struct dp_power_private *power;

	power = container_of(dp_power, struct dp_power_private, dp_power);

	dp_power_clk_enable(dp_power, DP_CORE_PM, false);
	dp_power_regulator_disable(power);
	pm_runtime_put_sync(&power->pdev->dev);
	return 0;
}

struct dp_power *dp_power_get(struct dp_parser *parser)
{
	struct dp_power_private *power;
	struct dp_power *dp_power;

	if (!parser) {
		DRM_ERROR("invalid input\n");
		return ERR_PTR(-EINVAL);
	}

	power = devm_kzalloc(&parser->pdev->dev, sizeof(*power), GFP_KERNEL);
	if (!power)
		return ERR_PTR(-ENOMEM);

	power->parser = parser;
	power->pdev = parser->pdev;

	dp_power = &power->dp_power;

	return dp_power;
}
