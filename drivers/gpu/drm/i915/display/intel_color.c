/*
 * Copyright © 2016 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include "intel_color.h"
#include "intel_display_types.h"

#define CTM_COEFF_SIGN	(1ULL << 63)

#define CTM_COEFF_1_0	(1ULL << 32)
#define CTM_COEFF_2_0	(CTM_COEFF_1_0 << 1)
#define CTM_COEFF_4_0	(CTM_COEFF_2_0 << 1)
#define CTM_COEFF_8_0	(CTM_COEFF_4_0 << 1)
#define CTM_COEFF_0_5	(CTM_COEFF_1_0 >> 1)
#define CTM_COEFF_0_25	(CTM_COEFF_0_5 >> 1)
#define CTM_COEFF_0_125	(CTM_COEFF_0_25 >> 1)

#define CTM_COEFF_LIMITED_RANGE ((235ULL - 16ULL) * CTM_COEFF_1_0 / 255)

#define CTM_COEFF_NEGATIVE(coeff)	(((coeff) & CTM_COEFF_SIGN) != 0)
#define CTM_COEFF_ABS(coeff)		((coeff) & (CTM_COEFF_SIGN - 1))

#define LEGACY_LUT_LENGTH		256

/*
 * ILK+ csc matrix:
 *
 * |R/Cr|   | c0 c1 c2 |   ( |R/Cr|   |preoff0| )   |postoff0|
 * |G/Y | = | c3 c4 c5 | x ( |G/Y | + |preoff1| ) + |postoff1|
 * |B/Cb|   | c6 c7 c8 |   ( |B/Cb|   |preoff2| )   |postoff2|
 *
 * ILK/SNB don't have explicit post offsets, and instead
 * CSC_MODE_YUV_TO_RGB and CSC_BLACK_SCREEN_OFFSET are used:
 *  CSC_MODE_YUV_TO_RGB=0 + CSC_BLACK_SCREEN_OFFSET=0 -> 1/2, 0, 1/2
 *  CSC_MODE_YUV_TO_RGB=0 + CSC_BLACK_SCREEN_OFFSET=1 -> 1/2, 1/16, 1/2
 *  CSC_MODE_YUV_TO_RGB=1 + CSC_BLACK_SCREEN_OFFSET=0 -> 0, 0, 0
 *  CSC_MODE_YUV_TO_RGB=1 + CSC_BLACK_SCREEN_OFFSET=1 -> 1/16, 1/16, 1/16
 */

/*
 * Extract the CSC coefficient from a CTM coefficient (in U32.32 fixed point
 * format). This macro takes the coefficient we want transformed and the
 * number of fractional bits.
 *
 * We only have a 9 bits precision window which slides depending on the value
 * of the CTM coefficient and we write the value from bit 3. We also round the
 * value.
 */
#define ILK_CSC_COEFF_FP(coeff, fbits)	\
	(clamp_val(((coeff) >> (32 - (fbits) - 3)) + 4, 0, 0xfff) & 0xff8)

#define ILK_CSC_COEFF_LIMITED_RANGE 0x0dc0
#define ILK_CSC_COEFF_1_0 0x7800

#define ILK_CSC_POSTOFF_LIMITED_RANGE (16 * (1 << 12) / 255)

/* Nop pre/post offsets */
static const u16 ilk_csc_off_zero[3] = {};

/* Identity matrix */
static const u16 ilk_csc_coeff_identity[9] = {
	ILK_CSC_COEFF_1_0, 0, 0,
	0, ILK_CSC_COEFF_1_0, 0,
	0, 0, ILK_CSC_COEFF_1_0,
};

/* Limited range RGB post offsets */
static const u16 ilk_csc_postoff_limited_range[3] = {
	ILK_CSC_POSTOFF_LIMITED_RANGE,
	ILK_CSC_POSTOFF_LIMITED_RANGE,
	ILK_CSC_POSTOFF_LIMITED_RANGE,
};

/* Full range RGB -> limited range RGB matrix */
static const u16 ilk_csc_coeff_limited_range[9] = {
	ILK_CSC_COEFF_LIMITED_RANGE, 0, 0,
	0, ILK_CSC_COEFF_LIMITED_RANGE, 0,
	0, 0, ILK_CSC_COEFF_LIMITED_RANGE,
};

/* BT.709 full range RGB -> limited range YCbCr matrix */
static const u16 ilk_csc_coeff_rgb_to_ycbcr[9] = {
	0x1e08, 0x9cc0, 0xb528,
	0x2ba8, 0x09d8, 0x37e8,
	0xbce8, 0x9ad8, 0x1e08,
};

/* Limited range YCbCr post offsets */
static const u16 ilk_csc_postoff_rgb_to_ycbcr[3] = {
	0x0800, 0x0100, 0x0800,
};

static bool lut_is_legacy(const struct drm_property_blob *lut)
{
	return drm_color_lut_size(lut) == LEGACY_LUT_LENGTH;
}

static bool crtc_state_is_legacy_gamma(const struct intel_crtc_state *crtc_state)
{
	return !crtc_state->hw.degamma_lut &&
		!crtc_state->hw.ctm &&
		crtc_state->hw.gamma_lut &&
		lut_is_legacy(crtc_state->hw.gamma_lut);
}

/*
 * When using limited range, multiply the matrix given by userspace by
 * the matrix that we would use for the limited range.
 */
static u64 *ctm_mult_by_limited(u64 *result, const u64 *input)
{
	int i;

	for (i = 0; i < 9; i++) {
		u64 user_coeff = input[i];
		u32 limited_coeff = CTM_COEFF_LIMITED_RANGE;
		u32 abs_coeff = clamp_val(CTM_COEFF_ABS(user_coeff), 0,
					  CTM_COEFF_4_0 - 1) >> 2;

		/*
		 * By scaling every co-efficient with limited range (16-235)
		 * vs full range (0-255) the final o/p will be scaled down to
		 * fit in the limited range supported by the panel.
		 */
		result[i] = mul_u32_u32(limited_coeff, abs_coeff) >> 30;
		result[i] |= user_coeff & CTM_COEFF_SIGN;
	}

	return result;
}

static void ilk_update_pipe_csc(struct intel_crtc *crtc,
				const u16 preoff[3],
				const u16 coeff[9],
				const u16 postoff[3])
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;

	intel_de_write(dev_priv, PIPE_CSC_PREOFF_HI(pipe), preoff[0]);
	intel_de_write(dev_priv, PIPE_CSC_PREOFF_ME(pipe), preoff[1]);
	intel_de_write(dev_priv, PIPE_CSC_PREOFF_LO(pipe), preoff[2]);

	intel_de_write(dev_priv, PIPE_CSC_COEFF_RY_GY(pipe),
		       coeff[0] << 16 | coeff[1]);
	intel_de_write(dev_priv, PIPE_CSC_COEFF_BY(pipe), coeff[2] << 16);

	intel_de_write(dev_priv, PIPE_CSC_COEFF_RU_GU(pipe),
		       coeff[3] << 16 | coeff[4]);
	intel_de_write(dev_priv, PIPE_CSC_COEFF_BU(pipe), coeff[5] << 16);

	intel_de_write(dev_priv, PIPE_CSC_COEFF_RV_GV(pipe),
		       coeff[6] << 16 | coeff[7]);
	intel_de_write(dev_priv, PIPE_CSC_COEFF_BV(pipe), coeff[8] << 16);

	if (INTEL_GEN(dev_priv) >= 7) {
		intel_de_write(dev_priv, PIPE_CSC_POSTOFF_HI(pipe),
			       postoff[0]);
		intel_de_write(dev_priv, PIPE_CSC_POSTOFF_ME(pipe),
			       postoff[1]);
		intel_de_write(dev_priv, PIPE_CSC_POSTOFF_LO(pipe),
			       postoff[2]);
	}
}

static void icl_update_output_csc(struct intel_crtc *crtc,
				  const u16 preoff[3],
				  const u16 coeff[9],
				  const u16 postoff[3])
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;

	intel_de_write(dev_priv, PIPE_CSC_OUTPUT_PREOFF_HI(pipe), preoff[0]);
	intel_de_write(dev_priv, PIPE_CSC_OUTPUT_PREOFF_ME(pipe), preoff[1]);
	intel_de_write(dev_priv, PIPE_CSC_OUTPUT_PREOFF_LO(pipe), preoff[2]);

	intel_de_write(dev_priv, PIPE_CSC_OUTPUT_COEFF_RY_GY(pipe),
		       coeff[0] << 16 | coeff[1]);
	intel_de_write(dev_priv, PIPE_CSC_OUTPUT_COEFF_BY(pipe),
		       coeff[2] << 16);

	intel_de_write(dev_priv, PIPE_CSC_OUTPUT_COEFF_RU_GU(pipe),
		       coeff[3] << 16 | coeff[4]);
	intel_de_write(dev_priv, PIPE_CSC_OUTPUT_COEFF_BU(pipe),
		       coeff[5] << 16);

	intel_de_write(dev_priv, PIPE_CSC_OUTPUT_COEFF_RV_GV(pipe),
		       coeff[6] << 16 | coeff[7]);
	intel_de_write(dev_priv, PIPE_CSC_OUTPUT_COEFF_BV(pipe),
		       coeff[8] << 16);

	intel_de_write(dev_priv, PIPE_CSC_OUTPUT_POSTOFF_HI(pipe), postoff[0]);
	intel_de_write(dev_priv, PIPE_CSC_OUTPUT_POSTOFF_ME(pipe), postoff[1]);
	intel_de_write(dev_priv, PIPE_CSC_OUTPUT_POSTOFF_LO(pipe), postoff[2]);
}

static bool ilk_csc_limited_range(const struct intel_crtc_state *crtc_state)
{
	struct drm_i915_private *dev_priv = to_i915(crtc_state->uapi.crtc->dev);

	/*
	 * FIXME if there's a gamma LUT after the CSC, we should
	 * do the range compression using the gamma LUT instead.
	 */
	return crtc_state->limited_color_range &&
		(IS_HASWELL(dev_priv) || IS_BROADWELL(dev_priv) ||
		 IS_GEN_RANGE(dev_priv, 9, 10));
}

static void ilk_csc_convert_ctm(const struct intel_crtc_state *crtc_state,
				u16 coeffs[9])
{
	const struct drm_color_ctm *ctm = crtc_state->hw.ctm->data;
	const u64 *input;
	u64 temp[9];
	int i;

	if (ilk_csc_limited_range(crtc_state))
		input = ctm_mult_by_limited(temp, ctm->matrix);
	else
		input = ctm->matrix;

	/*
	 * Convert fixed point S31.32 input to format supported by the
	 * hardware.
	 */
	for (i = 0; i < 9; i++) {
		u64 abs_coeff = ((1ULL << 63) - 1) & input[i];

		/*
		 * Clamp input value to min/max supported by
		 * hardware.
		 */
		abs_coeff = clamp_val(abs_coeff, 0, CTM_COEFF_4_0 - 1);

		coeffs[i] = 0;

		/* sign bit */
		if (CTM_COEFF_NEGATIVE(input[i]))
			coeffs[i] |= 1 << 15;

		if (abs_coeff < CTM_COEFF_0_125)
			coeffs[i] |= (3 << 12) |
				ILK_CSC_COEFF_FP(abs_coeff, 12);
		else if (abs_coeff < CTM_COEFF_0_25)
			coeffs[i] |= (2 << 12) |
				ILK_CSC_COEFF_FP(abs_coeff, 11);
		else if (abs_coeff < CTM_COEFF_0_5)
			coeffs[i] |= (1 << 12) |
				ILK_CSC_COEFF_FP(abs_coeff, 10);
		else if (abs_coeff < CTM_COEFF_1_0)
			coeffs[i] |= ILK_CSC_COEFF_FP(abs_coeff, 9);
		else if (abs_coeff < CTM_COEFF_2_0)
			coeffs[i] |= (7 << 12) |
				ILK_CSC_COEFF_FP(abs_coeff, 8);
		else
			coeffs[i] |= (6 << 12) |
				ILK_CSC_COEFF_FP(abs_coeff, 7);
	}
}

static void ilk_load_csc_matrix(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	bool limited_color_range = ilk_csc_limited_range(crtc_state);

	if (crtc_state->hw.ctm) {
		u16 coeff[9];

		ilk_csc_convert_ctm(crtc_state, coeff);
		ilk_update_pipe_csc(crtc, ilk_csc_off_zero, coeff,
				    limited_color_range ?
				    ilk_csc_postoff_limited_range :
				    ilk_csc_off_zero);
	} else if (crtc_state->output_format != INTEL_OUTPUT_FORMAT_RGB) {
		ilk_update_pipe_csc(crtc, ilk_csc_off_zero,
				    ilk_csc_coeff_rgb_to_ycbcr,
				    ilk_csc_postoff_rgb_to_ycbcr);
	} else if (limited_color_range) {
		ilk_update_pipe_csc(crtc, ilk_csc_off_zero,
				    ilk_csc_coeff_limited_range,
				    ilk_csc_postoff_limited_range);
	} else if (crtc_state->csc_enable) {
		/*
		 * On GLK+ both pipe CSC and degamma LUT are controlled
		 * by csc_enable. Hence for the cases where the degama
		 * LUT is needed but CSC is not we need to load an
		 * identity matrix.
		 */
		drm_WARN_ON(&dev_priv->drm, !IS_CANNONLAKE(dev_priv) &&
			    !IS_GEMINILAKE(dev_priv));

		ilk_update_pipe_csc(crtc, ilk_csc_off_zero,
				    ilk_csc_coeff_identity,
				    ilk_csc_off_zero);
	}

	intel_de_write(dev_priv, PIPE_CSC_MODE(crtc->pipe),
		       crtc_state->csc_mode);
}

static void icl_load_csc_matrix(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);

	if (crtc_state->hw.ctm) {
		u16 coeff[9];

		ilk_csc_convert_ctm(crtc_state, coeff);
		ilk_update_pipe_csc(crtc, ilk_csc_off_zero,
				    coeff, ilk_csc_off_zero);
	}

	if (crtc_state->output_format != INTEL_OUTPUT_FORMAT_RGB) {
		icl_update_output_csc(crtc, ilk_csc_off_zero,
				      ilk_csc_coeff_rgb_to_ycbcr,
				      ilk_csc_postoff_rgb_to_ycbcr);
	} else if (crtc_state->limited_color_range) {
		icl_update_output_csc(crtc, ilk_csc_off_zero,
				      ilk_csc_coeff_limited_range,
				      ilk_csc_postoff_limited_range);
	}

	intel_de_write(dev_priv, PIPE_CSC_MODE(crtc->pipe),
		       crtc_state->csc_mode);
}

/*
 * Set up the pipe CSC unit on CherryView.
 */
static void cherryview_load_csc_matrix(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;

	if (crtc_state->hw.ctm) {
		const struct drm_color_ctm *ctm = crtc_state->hw.ctm->data;
		u16 coeffs[9] = {};
		int i;

		for (i = 0; i < ARRAY_SIZE(coeffs); i++) {
			u64 abs_coeff =
				((1ULL << 63) - 1) & ctm->matrix[i];

			/* Round coefficient. */
			abs_coeff += 1 << (32 - 13);
			/* Clamp to hardware limits. */
			abs_coeff = clamp_val(abs_coeff, 0, CTM_COEFF_8_0 - 1);

			/* Write coefficients in S3.12 format. */
			if (ctm->matrix[i] & (1ULL << 63))
				coeffs[i] = 1 << 15;
			coeffs[i] |= ((abs_coeff >> 32) & 7) << 12;
			coeffs[i] |= (abs_coeff >> 20) & 0xfff;
		}

		intel_de_write(dev_priv, CGM_PIPE_CSC_COEFF01(pipe),
			       coeffs[1] << 16 | coeffs[0]);
		intel_de_write(dev_priv, CGM_PIPE_CSC_COEFF23(pipe),
			       coeffs[3] << 16 | coeffs[2]);
		intel_de_write(dev_priv, CGM_PIPE_CSC_COEFF45(pipe),
			       coeffs[5] << 16 | coeffs[4]);
		intel_de_write(dev_priv, CGM_PIPE_CSC_COEFF67(pipe),
			       coeffs[7] << 16 | coeffs[6]);
		intel_de_write(dev_priv, CGM_PIPE_CSC_COEFF8(pipe), coeffs[8]);
	}

	intel_de_write(dev_priv, CGM_PIPE_MODE(pipe), crtc_state->cgm_mode);
}

/* i965+ "10.6" bit interpolated format "even DW" (low 8 bits) */
static u32 i965_lut_10p6_ldw(const struct drm_color_lut *color)
{
	return (color->red & 0xff) << 16 |
		(color->green & 0xff) << 8 |
		(color->blue & 0xff);
}

/* i965+ "10.6" interpolated format "odd DW" (high 8 bits) */
static u32 i965_lut_10p6_udw(const struct drm_color_lut *color)
{
	return (color->red >> 8) << 16 |
		(color->green >> 8) << 8 |
		(color->blue >> 8);
}

static u32 ilk_lut_10(const struct drm_color_lut *color)
{
	return drm_color_lut_extract(color->red, 10) << 20 |
		drm_color_lut_extract(color->green, 10) << 10 |
		drm_color_lut_extract(color->blue, 10);
}

/* Loads the legacy palette/gamma unit for the CRTC. */
static void i9xx_load_luts_internal(const struct intel_crtc_state *crtc_state,
				    const struct drm_property_blob *blob)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;
	int i;

	if (HAS_GMCH(dev_priv)) {
		if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_DSI))
			assert_dsi_pll_enabled(dev_priv);
		else
			assert_pll_enabled(dev_priv, pipe);
	}

	if (blob) {
		const struct drm_color_lut *lut = blob->data;

		for (i = 0; i < 256; i++) {
			u32 word =
				(drm_color_lut_extract(lut[i].red, 8) << 16) |
				(drm_color_lut_extract(lut[i].green, 8) << 8) |
				drm_color_lut_extract(lut[i].blue, 8);

			if (HAS_GMCH(dev_priv))
				intel_de_write(dev_priv, PALETTE(pipe, i),
					       word);
			else
				intel_de_write(dev_priv, LGC_PALETTE(pipe, i),
					       word);
		}
	}
}

static void i9xx_load_luts(const struct intel_crtc_state *crtc_state)
{
	i9xx_load_luts_internal(crtc_state, crtc_state->hw.gamma_lut);
}

static void i9xx_color_commit(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;
	u32 val;

	val = intel_de_read(dev_priv, PIPECONF(pipe));
	val &= ~PIPECONF_GAMMA_MODE_MASK_I9XX;
	val |= PIPECONF_GAMMA_MODE(crtc_state->gamma_mode);
	intel_de_write(dev_priv, PIPECONF(pipe), val);
}

static void ilk_color_commit(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;
	u32 val;

	val = intel_de_read(dev_priv, PIPECONF(pipe));
	val &= ~PIPECONF_GAMMA_MODE_MASK_ILK;
	val |= PIPECONF_GAMMA_MODE(crtc_state->gamma_mode);
	intel_de_write(dev_priv, PIPECONF(pipe), val);

	ilk_load_csc_matrix(crtc_state);
}

static void hsw_color_commit(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);

	intel_de_write(dev_priv, GAMMA_MODE(crtc->pipe),
		       crtc_state->gamma_mode);

	ilk_load_csc_matrix(crtc_state);
}

static void skl_color_commit(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;
	u32 val = 0;

	/*
	 * We don't (yet) allow userspace to control the pipe background color,
	 * so force it to black, but apply pipe gamma and CSC appropriately
	 * so that its handling will match how we program our planes.
	 */
	if (crtc_state->gamma_enable)
		val |= SKL_BOTTOM_COLOR_GAMMA_ENABLE;
	if (crtc_state->csc_enable)
		val |= SKL_BOTTOM_COLOR_CSC_ENABLE;
	intel_de_write(dev_priv, SKL_BOTTOM_COLOR(pipe), val);

	intel_de_write(dev_priv, GAMMA_MODE(crtc->pipe),
		       crtc_state->gamma_mode);

	if (INTEL_GEN(dev_priv) >= 11)
		icl_load_csc_matrix(crtc_state);
	else
		ilk_load_csc_matrix(crtc_state);
}

static void i965_load_lut_10p6(struct intel_crtc *crtc,
			       const struct drm_property_blob *blob)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	const struct drm_color_lut *lut = blob->data;
	int i, lut_size = drm_color_lut_size(blob);
	enum pipe pipe = crtc->pipe;

	for (i = 0; i < lut_size - 1; i++) {
		intel_de_write(dev_priv, PALETTE(pipe, 2 * i + 0),
			       i965_lut_10p6_ldw(&lut[i]));
		intel_de_write(dev_priv, PALETTE(pipe, 2 * i + 1),
			       i965_lut_10p6_udw(&lut[i]));
	}

	intel_de_write(dev_priv, PIPEGCMAX(pipe, 0), lut[i].red);
	intel_de_write(dev_priv, PIPEGCMAX(pipe, 1), lut[i].green);
	intel_de_write(dev_priv, PIPEGCMAX(pipe, 2), lut[i].blue);
}

static void i965_load_luts(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	const struct drm_property_blob *gamma_lut = crtc_state->hw.gamma_lut;

	if (crtc_state->gamma_mode == GAMMA_MODE_MODE_8BIT)
		i9xx_load_luts(crtc_state);
	else
		i965_load_lut_10p6(crtc, gamma_lut);
}

static void ilk_load_lut_10(struct intel_crtc *crtc,
			    const struct drm_property_blob *blob)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	const struct drm_color_lut *lut = blob->data;
	int i, lut_size = drm_color_lut_size(blob);
	enum pipe pipe = crtc->pipe;

	for (i = 0; i < lut_size; i++)
		intel_de_write(dev_priv, PREC_PALETTE(pipe, i),
		               ilk_lut_10(&lut[i]));
}

static void ilk_load_luts(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	const struct drm_property_blob *gamma_lut = crtc_state->hw.gamma_lut;

	if (crtc_state->gamma_mode == GAMMA_MODE_MODE_8BIT)
		i9xx_load_luts(crtc_state);
	else
		ilk_load_lut_10(crtc, gamma_lut);
}

static int ivb_lut_10_size(u32 prec_index)
{
	if (prec_index & PAL_PREC_SPLIT_MODE)
		return 512;
	else
		return 1024;
}

/*
 * IVB/HSW Bspec / PAL_PREC_INDEX:
 * "Restriction : Index auto increment mode is not
 *  supported and must not be enabled."
 */
static void ivb_load_lut_10(struct intel_crtc *crtc,
			    const struct drm_property_blob *blob,
			    u32 prec_index)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	int hw_lut_size = ivb_lut_10_size(prec_index);
	const struct drm_color_lut *lut = blob->data;
	int i, lut_size = drm_color_lut_size(blob);
	enum pipe pipe = crtc->pipe;

	for (i = 0; i < hw_lut_size; i++) {
		/* We discard half the user entries in split gamma mode */
		const struct drm_color_lut *entry =
			&lut[i * (lut_size - 1) / (hw_lut_size - 1)];

		intel_de_write(dev_priv, PREC_PAL_INDEX(pipe), prec_index++);
		intel_de_write(dev_priv, PREC_PAL_DATA(pipe),
			       ilk_lut_10(entry));
	}

	/*
	 * Reset the index, otherwise it prevents the legacy palette to be
	 * written properly.
	 */
	intel_de_write(dev_priv, PREC_PAL_INDEX(pipe), 0);
}

/* On BDW+ the index auto increment mode actually works */
static void bdw_load_lut_10(struct intel_crtc *crtc,
			    const struct drm_property_blob *blob,
			    u32 prec_index)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	int hw_lut_size = ivb_lut_10_size(prec_index);
	const struct drm_color_lut *lut = blob->data;
	int i, lut_size = drm_color_lut_size(blob);
	enum pipe pipe = crtc->pipe;

	intel_de_write(dev_priv, PREC_PAL_INDEX(pipe),
		       prec_index | PAL_PREC_AUTO_INCREMENT);

	for (i = 0; i < hw_lut_size; i++) {
		/* We discard half the user entries in split gamma mode */
		const struct drm_color_lut *entry =
			&lut[i * (lut_size - 1) / (hw_lut_size - 1)];

		intel_de_write(dev_priv, PREC_PAL_DATA(pipe),
			       ilk_lut_10(entry));
	}

	/*
	 * Reset the index, otherwise it prevents the legacy palette to be
	 * written properly.
	 */
	intel_de_write(dev_priv, PREC_PAL_INDEX(pipe), 0);
}

static void ivb_load_lut_ext_max(struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	struct intel_dsb *dsb = intel_dsb_get(crtc);
	enum pipe pipe = crtc->pipe;

	/* Program the max register to clamp values > 1.0. */
	intel_dsb_reg_write(dsb, PREC_PAL_EXT_GC_MAX(pipe, 0), 1 << 16);
	intel_dsb_reg_write(dsb, PREC_PAL_EXT_GC_MAX(pipe, 1), 1 << 16);
	intel_dsb_reg_write(dsb, PREC_PAL_EXT_GC_MAX(pipe, 2), 1 << 16);

	/*
	 * Program the gc max 2 register to clamp values > 1.0.
	 * ToDo: Extend the ABI to be able to program values
	 * from 3.0 to 7.0
	 */
	if (INTEL_GEN(dev_priv) >= 10 || IS_GEMINILAKE(dev_priv)) {
		intel_dsb_reg_write(dsb, PREC_PAL_EXT2_GC_MAX(pipe, 0),
				    1 << 16);
		intel_dsb_reg_write(dsb, PREC_PAL_EXT2_GC_MAX(pipe, 1),
				    1 << 16);
		intel_dsb_reg_write(dsb, PREC_PAL_EXT2_GC_MAX(pipe, 2),
				    1 << 16);
	}

	intel_dsb_put(dsb);
}

static void ivb_load_luts(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	const struct drm_property_blob *gamma_lut = crtc_state->hw.gamma_lut;
	const struct drm_property_blob *degamma_lut = crtc_state->hw.degamma_lut;

	if (crtc_state->gamma_mode == GAMMA_MODE_MODE_8BIT) {
		i9xx_load_luts(crtc_state);
	} else if (crtc_state->gamma_mode == GAMMA_MODE_MODE_SPLIT) {
		ivb_load_lut_10(crtc, degamma_lut, PAL_PREC_SPLIT_MODE |
				PAL_PREC_INDEX_VALUE(0));
		ivb_load_lut_ext_max(crtc);
		ivb_load_lut_10(crtc, gamma_lut, PAL_PREC_SPLIT_MODE |
				PAL_PREC_INDEX_VALUE(512));
	} else {
		const struct drm_property_blob *blob = gamma_lut ?: degamma_lut;

		ivb_load_lut_10(crtc, blob,
				PAL_PREC_INDEX_VALUE(0));
		ivb_load_lut_ext_max(crtc);
	}
}

static void bdw_load_luts(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	const struct drm_property_blob *gamma_lut = crtc_state->hw.gamma_lut;
	const struct drm_property_blob *degamma_lut = crtc_state->hw.degamma_lut;

	if (crtc_state->gamma_mode == GAMMA_MODE_MODE_8BIT) {
		i9xx_load_luts(crtc_state);
	} else if (crtc_state->gamma_mode == GAMMA_MODE_MODE_SPLIT) {
		bdw_load_lut_10(crtc, degamma_lut, PAL_PREC_SPLIT_MODE |
				PAL_PREC_INDEX_VALUE(0));
		ivb_load_lut_ext_max(crtc);
		bdw_load_lut_10(crtc, gamma_lut, PAL_PREC_SPLIT_MODE |
				PAL_PREC_INDEX_VALUE(512));
	} else {
		const struct drm_property_blob *blob = gamma_lut ?: degamma_lut;

		bdw_load_lut_10(crtc, blob,
				PAL_PREC_INDEX_VALUE(0));
		ivb_load_lut_ext_max(crtc);
	}
}

static void glk_load_degamma_lut(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;
	const u32 lut_size = INTEL_INFO(dev_priv)->color.degamma_lut_size;
	const struct drm_color_lut *lut = crtc_state->hw.degamma_lut->data;
	u32 i;

	/*
	 * When setting the auto-increment bit, the hardware seems to
	 * ignore the index bits, so we need to reset it to index 0
	 * separately.
	 */
	intel_de_write(dev_priv, PRE_CSC_GAMC_INDEX(pipe), 0);
	intel_de_write(dev_priv, PRE_CSC_GAMC_INDEX(pipe),
		       PRE_CSC_GAMC_AUTO_INCREMENT);

	for (i = 0; i < lut_size; i++) {
		/*
		 * First 33 entries represent range from 0 to 1.0
		 * 34th and 35th entry will represent extended range
		 * inputs 3.0 and 7.0 respectively, currently clamped
		 * at 1.0. Since the precision is 16bit, the user
		 * value can be directly filled to register.
		 * The pipe degamma table in GLK+ onwards doesn't
		 * support different values per channel, so this just
		 * programs green value which will be equal to Red and
		 * Blue into the lut registers.
		 * ToDo: Extend to max 7.0. Enable 32 bit input value
		 * as compared to just 16 to achieve this.
		 */
		intel_de_write(dev_priv, PRE_CSC_GAMC_DATA(pipe),
		               lut[i].green);
	}

	/* Clamp values > 1.0. */
	while (i++ < 35)
		intel_de_write(dev_priv, PRE_CSC_GAMC_DATA(pipe), 1 << 16);
}

static void glk_load_degamma_lut_linear(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;
	const u32 lut_size = INTEL_INFO(dev_priv)->color.degamma_lut_size;
	u32 i;

	/*
	 * When setting the auto-increment bit, the hardware seems to
	 * ignore the index bits, so we need to reset it to index 0
	 * separately.
	 */
	intel_de_write(dev_priv, PRE_CSC_GAMC_INDEX(pipe), 0);
	intel_de_write(dev_priv, PRE_CSC_GAMC_INDEX(pipe),
		       PRE_CSC_GAMC_AUTO_INCREMENT);

	for (i = 0; i < lut_size; i++) {
		u32 v = (i << 16) / (lut_size - 1);

		intel_de_write(dev_priv, PRE_CSC_GAMC_DATA(pipe), v);
	}

	/* Clamp values > 1.0. */
	while (i++ < 35)
		intel_de_write(dev_priv, PRE_CSC_GAMC_DATA(pipe), 1 << 16);
}

static void glk_load_luts(const struct intel_crtc_state *crtc_state)
{
	const struct drm_property_blob *gamma_lut = crtc_state->hw.gamma_lut;
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);

	/*
	 * On GLK+ both pipe CSC and degamma LUT are controlled
	 * by csc_enable. Hence for the cases where the CSC is
	 * needed but degamma LUT is not we need to load a
	 * linear degamma LUT. In fact we'll just always load
	 * the degama LUT so that we don't have to reload
	 * it every time the pipe CSC is being enabled.
	 */
	if (crtc_state->hw.degamma_lut)
		glk_load_degamma_lut(crtc_state);
	else
		glk_load_degamma_lut_linear(crtc_state);

	if (crtc_state->gamma_mode == GAMMA_MODE_MODE_8BIT) {
		i9xx_load_luts(crtc_state);
	} else {
		bdw_load_lut_10(crtc, gamma_lut, PAL_PREC_INDEX_VALUE(0));
		ivb_load_lut_ext_max(crtc);
	}
}

/* ilk+ "12.4" interpolated format (high 10 bits) */
static u32 ilk_lut_12p4_udw(const struct drm_color_lut *color)
{
	return (color->red >> 6) << 20 | (color->green >> 6) << 10 |
		(color->blue >> 6);
}

/* ilk+ "12.4" interpolated format (low 6 bits) */
static u32 ilk_lut_12p4_ldw(const struct drm_color_lut *color)
{
	return (color->red & 0x3f) << 24 | (color->green & 0x3f) << 14 |
		(color->blue & 0x3f) << 4;
}

static void
icl_load_gcmax(const struct intel_crtc_state *crtc_state,
	       const struct drm_color_lut *color)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct intel_dsb *dsb = intel_dsb_get(crtc);
	enum pipe pipe = crtc->pipe;

	/* Fixme: LUT entries are 16 bit only, so we can prog 0xFFFF max */
	intel_dsb_reg_write(dsb, PREC_PAL_GC_MAX(pipe, 0), color->red);
	intel_dsb_reg_write(dsb, PREC_PAL_GC_MAX(pipe, 1), color->green);
	intel_dsb_reg_write(dsb, PREC_PAL_GC_MAX(pipe, 2), color->blue);
	intel_dsb_put(dsb);
}

static void
icl_program_gamma_superfine_segment(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	const struct drm_property_blob *blob = crtc_state->hw.gamma_lut;
	const struct drm_color_lut *lut = blob->data;
	struct intel_dsb *dsb = intel_dsb_get(crtc);
	enum pipe pipe = crtc->pipe;
	u32 i;

	/*
	 * Program Super Fine segment (let's call it seg1)...
	 *
	 * Super Fine segment's step is 1/(8 * 128 * 256) and it has
	 * 9 entries, corresponding to values 0, 1/(8 * 128 * 256),
	 * 2/(8 * 128 * 256) ... 8/(8 * 128 * 256).
	 */
	intel_dsb_reg_write(dsb, PREC_PAL_MULTI_SEG_INDEX(pipe),
			    PAL_PREC_AUTO_INCREMENT);

	for (i = 0; i < 9; i++) {
		const struct drm_color_lut *entry = &lut[i];

		intel_dsb_indexed_reg_write(dsb, PREC_PAL_MULTI_SEG_DATA(pipe),
					    ilk_lut_12p4_ldw(entry));
		intel_dsb_indexed_reg_write(dsb, PREC_PAL_MULTI_SEG_DATA(pipe),
					    ilk_lut_12p4_udw(entry));
	}

	intel_dsb_put(dsb);
}

static void
icl_program_gamma_multi_segment(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	const struct drm_property_blob *blob = crtc_state->hw.gamma_lut;
	const struct drm_color_lut *lut = blob->data;
	const struct drm_color_lut *entry;
	struct intel_dsb *dsb = intel_dsb_get(crtc);
	enum pipe pipe = crtc->pipe;
	u32 i;

	/*
	 * Program Fine segment (let's call it seg2)...
	 *
	 * Fine segment's step is 1/(128 * 256) i.e. 1/(128 * 256), 2/(128 * 256)
	 * ... 256/(128 * 256). So in order to program fine segment of LUT we
	 * need to pick every 8th entry in the LUT, and program 256 indexes.
	 *
	 * PAL_PREC_INDEX[0] and PAL_PREC_INDEX[1] map to seg2[1],
	 * seg2[0] being unused by the hardware.
	 */
	intel_dsb_reg_write(dsb, PREC_PAL_INDEX(pipe), PAL_PREC_AUTO_INCREMENT);
	for (i = 1; i < 257; i++) {
		entry = &lut[i * 8];
		intel_dsb_indexed_reg_write(dsb, PREC_PAL_DATA(pipe),
					    ilk_lut_12p4_ldw(entry));
		intel_dsb_indexed_reg_write(dsb, PREC_PAL_DATA(pipe),
					    ilk_lut_12p4_udw(entry));
	}

	/*
	 * Program Coarse segment (let's call it seg3)...
	 *
	 * Coarse segment starts from index 0 and it's step is 1/256 ie 0,
	 * 1/256, 2/256 ... 256/256. As per the description of each entry in LUT
	 * above, we need to pick every (8 * 128)th entry in LUT, and
	 * program 256 of those.
	 *
	 * Spec is not very clear about if entries seg3[0] and seg3[1] are
	 * being used or not, but we still need to program these to advance
	 * the index.
	 */
	for (i = 0; i < 256; i++) {
		entry = &lut[i * 8 * 128];
		intel_dsb_indexed_reg_write(dsb, PREC_PAL_DATA(pipe),
					    ilk_lut_12p4_ldw(entry));
		intel_dsb_indexed_reg_write(dsb, PREC_PAL_DATA(pipe),
					    ilk_lut_12p4_udw(entry));
	}

	/* The last entry in the LUT is to be programmed in GCMAX */
	entry = &lut[256 * 8 * 128];
	icl_load_gcmax(crtc_state, entry);
	ivb_load_lut_ext_max(crtc);
	intel_dsb_put(dsb);
}

static void icl_load_luts(const struct intel_crtc_state *crtc_state)
{
	const struct drm_property_blob *gamma_lut = crtc_state->hw.gamma_lut;
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct intel_dsb *dsb = intel_dsb_get(crtc);

	if (crtc_state->hw.degamma_lut)
		glk_load_degamma_lut(crtc_state);

	switch (crtc_state->gamma_mode & GAMMA_MODE_MODE_MASK) {
	case GAMMA_MODE_MODE_8BIT:
		i9xx_load_luts(crtc_state);
		break;
	case GAMMA_MODE_MODE_12BIT_MULTI_SEGMENTED:
		icl_program_gamma_superfine_segment(crtc_state);
		icl_program_gamma_multi_segment(crtc_state);
		break;
	default:
		bdw_load_lut_10(crtc, gamma_lut, PAL_PREC_INDEX_VALUE(0));
		ivb_load_lut_ext_max(crtc);
	}

	intel_dsb_commit(dsb);
	intel_dsb_put(dsb);
}

static u32 chv_cgm_degamma_ldw(const struct drm_color_lut *color)
{
	return drm_color_lut_extract(color->green, 14) << 16 |
		drm_color_lut_extract(color->blue, 14);
}

static u32 chv_cgm_degamma_udw(const struct drm_color_lut *color)
{
	return drm_color_lut_extract(color->red, 14);
}

static void chv_load_cgm_degamma(struct intel_crtc *crtc,
				 const struct drm_property_blob *blob)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	const struct drm_color_lut *lut = blob->data;
	int i, lut_size = drm_color_lut_size(blob);
	enum pipe pipe = crtc->pipe;

	for (i = 0; i < lut_size; i++) {
		intel_de_write(dev_priv, CGM_PIPE_DEGAMMA(pipe, i, 0),
			       chv_cgm_degamma_ldw(&lut[i]));
		intel_de_write(dev_priv, CGM_PIPE_DEGAMMA(pipe, i, 1),
			       chv_cgm_degamma_udw(&lut[i]));
	}
}

static u32 chv_cgm_gamma_ldw(const struct drm_color_lut *color)
{
	return drm_color_lut_extract(color->green, 10) << 16 |
		drm_color_lut_extract(color->blue, 10);
}

static u32 chv_cgm_gamma_udw(const struct drm_color_lut *color)
{
	return drm_color_lut_extract(color->red, 10);
}

static void chv_load_cgm_gamma(struct intel_crtc *crtc,
			       const struct drm_property_blob *blob)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	const struct drm_color_lut *lut = blob->data;
	int i, lut_size = drm_color_lut_size(blob);
	enum pipe pipe = crtc->pipe;

	for (i = 0; i < lut_size; i++) {
		intel_de_write(dev_priv, CGM_PIPE_GAMMA(pipe, i, 0),
			       chv_cgm_gamma_ldw(&lut[i]));
		intel_de_write(dev_priv, CGM_PIPE_GAMMA(pipe, i, 1),
			       chv_cgm_gamma_udw(&lut[i]));
	}
}

static void chv_load_luts(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	const struct drm_property_blob *gamma_lut = crtc_state->hw.gamma_lut;
	const struct drm_property_blob *degamma_lut = crtc_state->hw.degamma_lut;

	cherryview_load_csc_matrix(crtc_state);

	if (crtc_state_is_legacy_gamma(crtc_state)) {
		i9xx_load_luts(crtc_state);
		return;
	}

	if (degamma_lut)
		chv_load_cgm_degamma(crtc, degamma_lut);

	if (gamma_lut)
		chv_load_cgm_gamma(crtc, gamma_lut);
}

void intel_color_load_luts(const struct intel_crtc_state *crtc_state)
{
	struct drm_i915_private *dev_priv = to_i915(crtc_state->uapi.crtc->dev);

	dev_priv->display.load_luts(crtc_state);
}

void intel_color_commit(const struct intel_crtc_state *crtc_state)
{
	struct drm_i915_private *dev_priv = to_i915(crtc_state->uapi.crtc->dev);

	dev_priv->display.color_commit(crtc_state);
}

static bool intel_can_preload_luts(const struct intel_crtc_state *new_crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(new_crtc_state->uapi.crtc);
	struct intel_atomic_state *state =
		to_intel_atomic_state(new_crtc_state->uapi.state);
	const struct intel_crtc_state *old_crtc_state =
		intel_atomic_get_old_crtc_state(state, crtc);

	return !old_crtc_state->hw.gamma_lut &&
		!old_crtc_state->hw.degamma_lut;
}

static bool chv_can_preload_luts(const struct intel_crtc_state *new_crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(new_crtc_state->uapi.crtc);
	struct intel_atomic_state *state =
		to_intel_atomic_state(new_crtc_state->uapi.state);
	const struct intel_crtc_state *old_crtc_state =
		intel_atomic_get_old_crtc_state(state, crtc);

	/*
	 * CGM_PIPE_MODE is itself single buffered. We'd have to
	 * somehow split it out from chv_load_luts() if we wanted
	 * the ability to preload the CGM LUTs/CSC without tearing.
	 */
	if (old_crtc_state->cgm_mode || new_crtc_state->cgm_mode)
		return false;

	return !old_crtc_state->hw.gamma_lut;
}

static bool glk_can_preload_luts(const struct intel_crtc_state *new_crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(new_crtc_state->uapi.crtc);
	struct intel_atomic_state *state =
		to_intel_atomic_state(new_crtc_state->uapi.state);
	const struct intel_crtc_state *old_crtc_state =
		intel_atomic_get_old_crtc_state(state, crtc);

	/*
	 * The hardware degamma is active whenever the pipe
	 * CSC is active. Thus even if the old state has no
	 * software degamma we need to avoid clobbering the
	 * linear hardware degamma mid scanout.
	 */
	return !old_crtc_state->csc_enable &&
		!old_crtc_state->hw.gamma_lut;
}

int intel_color_check(struct intel_crtc_state *crtc_state)
{
	struct drm_i915_private *dev_priv = to_i915(crtc_state->uapi.crtc->dev);

	return dev_priv->display.color_check(crtc_state);
}

void intel_color_get_config(struct intel_crtc_state *crtc_state)
{
	struct drm_i915_private *dev_priv = to_i915(crtc_state->uapi.crtc->dev);

	if (dev_priv->display.read_luts)
		dev_priv->display.read_luts(crtc_state);
}

static bool need_plane_update(struct intel_plane *plane,
			      const struct intel_crtc_state *crtc_state)
{
	struct drm_i915_private *dev_priv = to_i915(plane->base.dev);

	/*
	 * On pre-SKL the pipe gamma enable and pipe csc enable for
	 * the pipe bottom color are configured via the primary plane.
	 * We have to reconfigure that even if the plane is inactive.
	 */
	return crtc_state->active_planes & BIT(plane->id) ||
		(INTEL_GEN(dev_priv) < 9 &&
		 plane->id == PLANE_PRIMARY);
}

static int
intel_color_add_affected_planes(struct intel_crtc_state *new_crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(new_crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	struct intel_atomic_state *state =
		to_intel_atomic_state(new_crtc_state->uapi.state);
	const struct intel_crtc_state *old_crtc_state =
		intel_atomic_get_old_crtc_state(state, crtc);
	struct intel_plane *plane;

	if (!new_crtc_state->hw.active ||
	    drm_atomic_crtc_needs_modeset(&new_crtc_state->uapi))
		return 0;

	if (new_crtc_state->gamma_enable == old_crtc_state->gamma_enable &&
	    new_crtc_state->csc_enable == old_crtc_state->csc_enable)
		return 0;

	for_each_intel_plane_on_crtc(&dev_priv->drm, crtc, plane) {
		struct intel_plane_state *plane_state;

		if (!need_plane_update(plane, new_crtc_state))
			continue;

		plane_state = intel_atomic_get_plane_state(state, plane);
		if (IS_ERR(plane_state))
			return PTR_ERR(plane_state);

		new_crtc_state->update_planes |= BIT(plane->id);
	}

	return 0;
}

static int check_lut_size(const struct drm_property_blob *lut, int expected)
{
	int len;

	if (!lut)
		return 0;

	len = drm_color_lut_size(lut);
	if (len != expected) {
		DRM_DEBUG_KMS("Invalid LUT size; got %d, expected %d\n",
			      len, expected);
		return -EINVAL;
	}

	return 0;
}

static int check_luts(const struct intel_crtc_state *crtc_state)
{
	struct drm_i915_private *dev_priv = to_i915(crtc_state->uapi.crtc->dev);
	const struct drm_property_blob *gamma_lut = crtc_state->hw.gamma_lut;
	const struct drm_property_blob *degamma_lut = crtc_state->hw.degamma_lut;
	int gamma_length, degamma_length;
	u32 gamma_tests, degamma_tests;

	/* Always allow legacy gamma LUT with no further checking. */
	if (crtc_state_is_legacy_gamma(crtc_state))
		return 0;

	/* C8 relies on its palette being stored in the legacy LUT */
	if (crtc_state->c8_planes) {
		DRM_DEBUG_KMS("C8 pixelformat requires the legacy LUT\n");
		return -EINVAL;
	}

	degamma_length = INTEL_INFO(dev_priv)->color.degamma_lut_size;
	gamma_length = INTEL_INFO(dev_priv)->color.gamma_lut_size;
	degamma_tests = INTEL_INFO(dev_priv)->color.degamma_lut_tests;
	gamma_tests = INTEL_INFO(dev_priv)->color.gamma_lut_tests;

	if (check_lut_size(degamma_lut, degamma_length) ||
	    check_lut_size(gamma_lut, gamma_length))
		return -EINVAL;

	if (drm_color_lut_check(degamma_lut, degamma_tests) ||
	    drm_color_lut_check(gamma_lut, gamma_tests))
		return -EINVAL;

	return 0;
}

static u32 i9xx_gamma_mode(struct intel_crtc_state *crtc_state)
{
	if (!crtc_state->gamma_enable ||
	    crtc_state_is_legacy_gamma(crtc_state))
		return GAMMA_MODE_MODE_8BIT;
	else
		return GAMMA_MODE_MODE_10BIT; /* i965+ only */
}

static int i9xx_color_check(struct intel_crtc_state *crtc_state)
{
	int ret;

	ret = check_luts(crtc_state);
	if (ret)
		return ret;

	crtc_state->gamma_enable =
		crtc_state->hw.gamma_lut &&
		!crtc_state->c8_planes;

	crtc_state->gamma_mode = i9xx_gamma_mode(crtc_state);

	ret = intel_color_add_affected_planes(crtc_state);
	if (ret)
		return ret;

	crtc_state->preload_luts = intel_can_preload_luts(crtc_state);

	return 0;
}

static u32 chv_cgm_mode(const struct intel_crtc_state *crtc_state)
{
	u32 cgm_mode = 0;

	if (crtc_state_is_legacy_gamma(crtc_state))
		return 0;

	if (crtc_state->hw.degamma_lut)
		cgm_mode |= CGM_PIPE_MODE_DEGAMMA;
	if (crtc_state->hw.ctm)
		cgm_mode |= CGM_PIPE_MODE_CSC;
	if (crtc_state->hw.gamma_lut)
		cgm_mode |= CGM_PIPE_MODE_GAMMA;

	return cgm_mode;
}

/*
 * CHV color pipeline:
 * u0.10 -> CGM degamma -> u0.14 -> CGM csc -> u0.14 -> CGM gamma ->
 * u0.10 -> WGC csc -> u0.10 -> pipe gamma -> u0.10
 *
 * We always bypass the WGC csc and use the CGM csc
 * instead since it has degamma and better precision.
 */
static int chv_color_check(struct intel_crtc_state *crtc_state)
{
	int ret;

	ret = check_luts(crtc_state);
	if (ret)
		return ret;

	/*
	 * Pipe gamma will be used only for the legacy LUT.
	 * Otherwise we bypass it and use the CGM gamma instead.
	 */
	crtc_state->gamma_enable =
		crtc_state_is_legacy_gamma(crtc_state) &&
		!crtc_state->c8_planes;

	crtc_state->gamma_mode = GAMMA_MODE_MODE_8BIT;

	crtc_state->cgm_mode = chv_cgm_mode(crtc_state);

	ret = intel_color_add_affected_planes(crtc_state);
	if (ret)
		return ret;

	crtc_state->preload_luts = chv_can_preload_luts(crtc_state);

	return 0;
}

static u32 ilk_gamma_mode(const struct intel_crtc_state *crtc_state)
{
	if (!crtc_state->gamma_enable ||
	    crtc_state_is_legacy_gamma(crtc_state))
		return GAMMA_MODE_MODE_8BIT;
	else
		return GAMMA_MODE_MODE_10BIT;
}

static u32 ilk_csc_mode(const struct intel_crtc_state *crtc_state)
{
	/*
	 * CSC comes after the LUT in RGB->YCbCr mode.
	 * RGB->YCbCr needs the limited range offsets added to
	 * the output. RGB limited range output is handled by
	 * the hw automagically elsewhere.
	 */
	if (crtc_state->output_format != INTEL_OUTPUT_FORMAT_RGB)
		return CSC_BLACK_SCREEN_OFFSET;

	return CSC_MODE_YUV_TO_RGB |
		CSC_POSITION_BEFORE_GAMMA;
}

static int ilk_color_check(struct intel_crtc_state *crtc_state)
{
	int ret;

	ret = check_luts(crtc_state);
	if (ret)
		return ret;

	crtc_state->gamma_enable =
		crtc_state->hw.gamma_lut &&
		!crtc_state->c8_planes;

	/*
	 * We don't expose the ctm on ilk/snb currently, also RGB
	 * limited range output is handled by the hw automagically.
	 */
	crtc_state->csc_enable =
		crtc_state->output_format != INTEL_OUTPUT_FORMAT_RGB;

	crtc_state->gamma_mode = ilk_gamma_mode(crtc_state);

	crtc_state->csc_mode = ilk_csc_mode(crtc_state);

	ret = intel_color_add_affected_planes(crtc_state);
	if (ret)
		return ret;

	crtc_state->preload_luts = intel_can_preload_luts(crtc_state);

	return 0;
}

static u32 ivb_gamma_mode(const struct intel_crtc_state *crtc_state)
{
	if (!crtc_state->gamma_enable ||
	    crtc_state_is_legacy_gamma(crtc_state))
		return GAMMA_MODE_MODE_8BIT;
	else if (crtc_state->hw.gamma_lut &&
		 crtc_state->hw.degamma_lut)
		return GAMMA_MODE_MODE_SPLIT;
	else
		return GAMMA_MODE_MODE_10BIT;
}

static u32 ivb_csc_mode(const struct intel_crtc_state *crtc_state)
{
	bool limited_color_range = ilk_csc_limited_range(crtc_state);

	/*
	 * CSC comes after the LUT in degamma, RGB->YCbCr,
	 * and RGB full->limited range mode.
	 */
	if (crtc_state->hw.degamma_lut ||
	    crtc_state->output_format != INTEL_OUTPUT_FORMAT_RGB ||
	    limited_color_range)
		return 0;

	return CSC_POSITION_BEFORE_GAMMA;
}

static int ivb_color_check(struct intel_crtc_state *crtc_state)
{
	bool limited_color_range = ilk_csc_limited_range(crtc_state);
	int ret;

	ret = check_luts(crtc_state);
	if (ret)
		return ret;

	crtc_state->gamma_enable =
		(crtc_state->hw.gamma_lut ||
		 crtc_state->hw.degamma_lut) &&
		!crtc_state->c8_planes;

	crtc_state->csc_enable =
		crtc_state->output_format != INTEL_OUTPUT_FORMAT_RGB ||
		crtc_state->hw.ctm || limited_color_range;

	crtc_state->gamma_mode = ivb_gamma_mode(crtc_state);

	crtc_state->csc_mode = ivb_csc_mode(crtc_state);

	ret = intel_color_add_affected_planes(crtc_state);
	if (ret)
		return ret;

	crtc_state->preload_luts = intel_can_preload_luts(crtc_state);

	return 0;
}

static u32 glk_gamma_mode(const struct intel_crtc_state *crtc_state)
{
	if (!crtc_state->gamma_enable ||
	    crtc_state_is_legacy_gamma(crtc_state))
		return GAMMA_MODE_MODE_8BIT;
	else
		return GAMMA_MODE_MODE_10BIT;
}

static int glk_color_check(struct intel_crtc_state *crtc_state)
{
	int ret;

	ret = check_luts(crtc_state);
	if (ret)
		return ret;

	crtc_state->gamma_enable =
		crtc_state->hw.gamma_lut &&
		!crtc_state->c8_planes;

	/* On GLK+ degamma LUT is controlled by csc_enable */
	crtc_state->csc_enable =
		crtc_state->hw.degamma_lut ||
		crtc_state->output_format != INTEL_OUTPUT_FORMAT_RGB ||
		crtc_state->hw.ctm || crtc_state->limited_color_range;

	crtc_state->gamma_mode = glk_gamma_mode(crtc_state);

	crtc_state->csc_mode = 0;

	ret = intel_color_add_affected_planes(crtc_state);
	if (ret)
		return ret;

	crtc_state->preload_luts = glk_can_preload_luts(crtc_state);

	return 0;
}

static u32 icl_gamma_mode(const struct intel_crtc_state *crtc_state)
{
	u32 gamma_mode = 0;

	if (crtc_state->hw.degamma_lut)
		gamma_mode |= PRE_CSC_GAMMA_ENABLE;

	if (crtc_state->hw.gamma_lut &&
	    !crtc_state->c8_planes)
		gamma_mode |= POST_CSC_GAMMA_ENABLE;

	if (!crtc_state->hw.gamma_lut ||
	    crtc_state_is_legacy_gamma(crtc_state))
		gamma_mode |= GAMMA_MODE_MODE_8BIT;
	else
		gamma_mode |= GAMMA_MODE_MODE_12BIT_MULTI_SEGMENTED;

	return gamma_mode;
}

static u32 icl_csc_mode(const struct intel_crtc_state *crtc_state)
{
	u32 csc_mode = 0;

	if (crtc_state->hw.ctm)
		csc_mode |= ICL_CSC_ENABLE;

	if (crtc_state->output_format != INTEL_OUTPUT_FORMAT_RGB ||
	    crtc_state->limited_color_range)
		csc_mode |= ICL_OUTPUT_CSC_ENABLE;

	return csc_mode;
}

static int icl_color_check(struct intel_crtc_state *crtc_state)
{
	int ret;

	ret = check_luts(crtc_state);
	if (ret)
		return ret;

	crtc_state->gamma_mode = icl_gamma_mode(crtc_state);

	crtc_state->csc_mode = icl_csc_mode(crtc_state);

	crtc_state->preload_luts = intel_can_preload_luts(crtc_state);

	return 0;
}

static int i9xx_gamma_precision(const struct intel_crtc_state *crtc_state)
{
	if (!crtc_state->gamma_enable)
		return 0;

	switch (crtc_state->gamma_mode) {
	case GAMMA_MODE_MODE_8BIT:
		return 8;
	case GAMMA_MODE_MODE_10BIT:
		return 16;
	default:
		MISSING_CASE(crtc_state->gamma_mode);
		return 0;
	}
}

static int ilk_gamma_precision(const struct intel_crtc_state *crtc_state)
{
	if (!crtc_state->gamma_enable)
		return 0;

	if ((crtc_state->csc_mode & CSC_POSITION_BEFORE_GAMMA) == 0)
		return 0;

	switch (crtc_state->gamma_mode) {
	case GAMMA_MODE_MODE_8BIT:
		return 8;
	case GAMMA_MODE_MODE_10BIT:
		return 10;
	default:
		MISSING_CASE(crtc_state->gamma_mode);
		return 0;
	}
}

static int chv_gamma_precision(const struct intel_crtc_state *crtc_state)
{
	if (crtc_state->cgm_mode & CGM_PIPE_MODE_GAMMA)
		return 10;
	else
		return i9xx_gamma_precision(crtc_state);
}

static int glk_gamma_precision(const struct intel_crtc_state *crtc_state)
{
	if (!crtc_state->gamma_enable)
		return 0;

	switch (crtc_state->gamma_mode) {
	case GAMMA_MODE_MODE_8BIT:
		return 8;
	case GAMMA_MODE_MODE_10BIT:
		return 10;
	default:
		MISSING_CASE(crtc_state->gamma_mode);
		return 0;
	}
}

int intel_color_get_gamma_bit_precision(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);

	if (HAS_GMCH(dev_priv)) {
		if (IS_CHERRYVIEW(dev_priv))
			return chv_gamma_precision(crtc_state);
		else
			return i9xx_gamma_precision(crtc_state);
	} else {
		if (IS_CANNONLAKE(dev_priv) || IS_GEMINILAKE(dev_priv))
			return glk_gamma_precision(crtc_state);
		else if (IS_IRONLAKE(dev_priv))
			return ilk_gamma_precision(crtc_state);
	}

	return 0;
}

static bool err_check(struct drm_color_lut *lut1,
		      struct drm_color_lut *lut2, u32 err)
{
	return ((abs((long)lut2->red - lut1->red)) <= err) &&
		((abs((long)lut2->blue - lut1->blue)) <= err) &&
		((abs((long)lut2->green - lut1->green)) <= err);
}

static bool intel_color_lut_entry_equal(struct drm_color_lut *lut1,
					struct drm_color_lut *lut2,
					int lut_size, u32 err)
{
	int i;

	for (i = 0; i < lut_size; i++) {
		if (!err_check(&lut1[i], &lut2[i], err))
			return false;
	}

	return true;
}

bool intel_color_lut_equal(struct drm_property_blob *blob1,
			   struct drm_property_blob *blob2,
			   u32 gamma_mode, u32 bit_precision)
{
	struct drm_color_lut *lut1, *lut2;
	int lut_size1, lut_size2;
	u32 err;

	if (!blob1 != !blob2)
		return false;

	if (!blob1)
		return true;

	lut_size1 = drm_color_lut_size(blob1);
	lut_size2 = drm_color_lut_size(blob2);

	/* check sw and hw lut size */
	switch (gamma_mode) {
	case GAMMA_MODE_MODE_8BIT:
	case GAMMA_MODE_MODE_10BIT:
		if (lut_size1 != lut_size2)
			return false;
		break;
	default:
		MISSING_CASE(gamma_mode);
			return false;
	}

	lut1 = blob1->data;
	lut2 = blob2->data;

	err = 0xffff >> bit_precision;

	/* check sw and hw lut entry to be equal */
	switch (gamma_mode) {
	case GAMMA_MODE_MODE_8BIT:
	case GAMMA_MODE_MODE_10BIT:
		if (!intel_color_lut_entry_equal(lut1, lut2,
						 lut_size2, err))
			return false;
		break;
	default:
		MISSING_CASE(gamma_mode);
			return false;
	}

	return true;
}

/* convert hw value with given bit_precision to lut property val */
static u32 intel_color_lut_pack(u32 val, u32 bit_precision)
{
	u32 max = 0xffff >> (16 - bit_precision);

	val = clamp_val(val, 0, max);

	if (bit_precision < 16)
		val <<= 16 - bit_precision;

	return val;
}

static struct drm_property_blob *
i9xx_read_lut_8(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;
	struct drm_property_blob *blob;
	struct drm_color_lut *blob_data;
	u32 i, val;

	blob = drm_property_create_blob(&dev_priv->drm,
					sizeof(struct drm_color_lut) * LEGACY_LUT_LENGTH,
					NULL);
	if (IS_ERR(blob))
		return NULL;

	blob_data = blob->data;

	for (i = 0; i < LEGACY_LUT_LENGTH; i++) {
		if (HAS_GMCH(dev_priv))
			val = intel_de_read(dev_priv, PALETTE(pipe, i));
		else
			val = intel_de_read(dev_priv, LGC_PALETTE(pipe, i));

		blob_data[i].red = intel_color_lut_pack(REG_FIELD_GET(
							LGC_PALETTE_RED_MASK, val), 8);
		blob_data[i].green = intel_color_lut_pack(REG_FIELD_GET(
							  LGC_PALETTE_GREEN_MASK, val), 8);
		blob_data[i].blue = intel_color_lut_pack(REG_FIELD_GET(
							 LGC_PALETTE_BLUE_MASK, val), 8);
	}

	return blob;
}

static void i9xx_read_luts(struct intel_crtc_state *crtc_state)
{
	if (!crtc_state->gamma_enable)
		return;

	crtc_state->hw.gamma_lut = i9xx_read_lut_8(crtc_state);
}

static struct drm_property_blob *
i965_read_lut_10p6(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	u32 lut_size = INTEL_INFO(dev_priv)->color.gamma_lut_size;
	enum pipe pipe = crtc->pipe;
	struct drm_property_blob *blob;
	struct drm_color_lut *blob_data;
	u32 i, val1, val2;

	blob = drm_property_create_blob(&dev_priv->drm,
					sizeof(struct drm_color_lut) * lut_size,
					NULL);
	if (IS_ERR(blob))
		return NULL;

	blob_data = blob->data;

	for (i = 0; i < lut_size - 1; i++) {
		val1 = intel_de_read(dev_priv, PALETTE(pipe, 2 * i + 0));
		val2 = intel_de_read(dev_priv, PALETTE(pipe, 2 * i + 1));

		blob_data[i].red = REG_FIELD_GET(PALETTE_RED_MASK, val2) << 8 |
						 REG_FIELD_GET(PALETTE_RED_MASK, val1);
		blob_data[i].green = REG_FIELD_GET(PALETTE_GREEN_MASK, val2) << 8 |
						   REG_FIELD_GET(PALETTE_GREEN_MASK, val1);
		blob_data[i].blue = REG_FIELD_GET(PALETTE_BLUE_MASK, val2) << 8 |
						  REG_FIELD_GET(PALETTE_BLUE_MASK, val1);
	}

	blob_data[i].red = REG_FIELD_GET(PIPEGCMAX_RGB_MASK,
					 intel_de_read(dev_priv, PIPEGCMAX(pipe, 0)));
	blob_data[i].green = REG_FIELD_GET(PIPEGCMAX_RGB_MASK,
					   intel_de_read(dev_priv, PIPEGCMAX(pipe, 1)));
	blob_data[i].blue = REG_FIELD_GET(PIPEGCMAX_RGB_MASK,
					  intel_de_read(dev_priv, PIPEGCMAX(pipe, 2)));

	return blob;
}

static void i965_read_luts(struct intel_crtc_state *crtc_state)
{
	if (!crtc_state->gamma_enable)
		return;

	if (crtc_state->gamma_mode == GAMMA_MODE_MODE_8BIT)
		crtc_state->hw.gamma_lut = i9xx_read_lut_8(crtc_state);
	else
		crtc_state->hw.gamma_lut = i965_read_lut_10p6(crtc_state);
}

static struct drm_property_blob *
chv_read_cgm_lut(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	u32 lut_size = INTEL_INFO(dev_priv)->color.gamma_lut_size;
	enum pipe pipe = crtc->pipe;
	struct drm_property_blob *blob;
	struct drm_color_lut *blob_data;
	u32 i, val;

	blob = drm_property_create_blob(&dev_priv->drm,
					sizeof(struct drm_color_lut) * lut_size,
					NULL);
	if (IS_ERR(blob))
		return NULL;

	blob_data = blob->data;

	for (i = 0; i < lut_size; i++) {
		val = intel_de_read(dev_priv, CGM_PIPE_GAMMA(pipe, i, 0));
		blob_data[i].green = intel_color_lut_pack(REG_FIELD_GET(
							  CGM_PIPE_GAMMA_GREEN_MASK, val), 10);
		blob_data[i].blue = intel_color_lut_pack(REG_FIELD_GET(
							 CGM_PIPE_GAMMA_BLUE_MASK, val), 10);

		val = intel_de_read(dev_priv, CGM_PIPE_GAMMA(pipe, i, 1));
		blob_data[i].red = intel_color_lut_pack(REG_FIELD_GET(
							CGM_PIPE_GAMMA_RED_MASK, val), 10);
	}

	return blob;
}

static void chv_read_luts(struct intel_crtc_state *crtc_state)
{
	if (crtc_state->cgm_mode & CGM_PIPE_MODE_GAMMA)
		crtc_state->hw.gamma_lut = chv_read_cgm_lut(crtc_state);
	else
		i965_read_luts(crtc_state);
}

static struct drm_property_blob *
ilk_read_lut_10(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	u32 lut_size = INTEL_INFO(dev_priv)->color.gamma_lut_size;
	enum pipe pipe = crtc->pipe;
	struct drm_property_blob *blob;
	struct drm_color_lut *blob_data;
	u32 i, val;

	blob = drm_property_create_blob(&dev_priv->drm,
					sizeof(struct drm_color_lut) * lut_size,
					NULL);
	if (IS_ERR(blob))
		return NULL;

	blob_data = blob->data;

	for (i = 0; i < lut_size; i++) {
		val = intel_de_read(dev_priv, PREC_PALETTE(pipe, i));

		blob_data[i].red = intel_color_lut_pack(REG_FIELD_GET(
							PREC_PALETTE_RED_MASK, val), 10);
		blob_data[i].green = intel_color_lut_pack(REG_FIELD_GET(
							  PREC_PALETTE_GREEN_MASK, val), 10);
		blob_data[i].blue = intel_color_lut_pack(REG_FIELD_GET(
							 PREC_PALETTE_BLUE_MASK, val), 10);
	}

	return blob;
}

static void ilk_read_luts(struct intel_crtc_state *crtc_state)
{
	if (!crtc_state->gamma_enable)
		return;

	if ((crtc_state->csc_mode & CSC_POSITION_BEFORE_GAMMA) == 0)
		return;

	if (crtc_state->gamma_mode == GAMMA_MODE_MODE_8BIT)
		crtc_state->hw.gamma_lut = i9xx_read_lut_8(crtc_state);
	else
		crtc_state->hw.gamma_lut = ilk_read_lut_10(crtc_state);
}

static struct drm_property_blob *
glk_read_lut_10(const struct intel_crtc_state *crtc_state, u32 prec_index)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	int hw_lut_size = ivb_lut_10_size(prec_index);
	enum pipe pipe = crtc->pipe;
	struct drm_property_blob *blob;
	struct drm_color_lut *blob_data;
	u32 i, val;

	blob = drm_property_create_blob(&dev_priv->drm,
					sizeof(struct drm_color_lut) * hw_lut_size,
					NULL);
	if (IS_ERR(blob))
		return NULL;

	blob_data = blob->data;

	intel_de_write(dev_priv, PREC_PAL_INDEX(pipe),
		       prec_index | PAL_PREC_AUTO_INCREMENT);

	for (i = 0; i < hw_lut_size; i++) {
		val = intel_de_read(dev_priv, PREC_PAL_DATA(pipe));

		blob_data[i].red = intel_color_lut_pack(REG_FIELD_GET(
							PREC_PAL_DATA_RED_MASK, val), 10);
		blob_data[i].green = intel_color_lut_pack(REG_FIELD_GET(
							PREC_PAL_DATA_GREEN_MASK, val), 10);
		blob_data[i].blue = intel_color_lut_pack(REG_FIELD_GET(
							PREC_PAL_DATA_BLUE_MASK, val), 10);
	}

	intel_de_write(dev_priv, PREC_PAL_INDEX(pipe), 0);

	return blob;
}

static void glk_read_luts(struct intel_crtc_state *crtc_state)
{
	if (!crtc_state->gamma_enable)
		return;

	if (crtc_state->gamma_mode == GAMMA_MODE_MODE_8BIT)
		crtc_state->hw.gamma_lut = i9xx_read_lut_8(crtc_state);
	else
		crtc_state->hw.gamma_lut = glk_read_lut_10(crtc_state, PAL_PREC_INDEX_VALUE(0));
}

void intel_color_init(struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	bool has_ctm = INTEL_INFO(dev_priv)->color.degamma_lut_size != 0;

	drm_mode_crtc_set_gamma_size(&crtc->base, 256);

	if (HAS_GMCH(dev_priv)) {
		if (IS_CHERRYVIEW(dev_priv)) {
			dev_priv->display.color_check = chv_color_check;
			dev_priv->display.color_commit = i9xx_color_commit;
			dev_priv->display.load_luts = chv_load_luts;
			dev_priv->display.read_luts = chv_read_luts;
		} else if (INTEL_GEN(dev_priv) >= 4) {
			dev_priv->display.color_check = i9xx_color_check;
			dev_priv->display.color_commit = i9xx_color_commit;
			dev_priv->display.load_luts = i965_load_luts;
			dev_priv->display.read_luts = i965_read_luts;
		} else {
			dev_priv->display.color_check = i9xx_color_check;
			dev_priv->display.color_commit = i9xx_color_commit;
			dev_priv->display.load_luts = i9xx_load_luts;
			dev_priv->display.read_luts = i9xx_read_luts;
		}
	} else {
		if (INTEL_GEN(dev_priv) >= 11)
			dev_priv->display.color_check = icl_color_check;
		else if (INTEL_GEN(dev_priv) >= 10 || IS_GEMINILAKE(dev_priv))
			dev_priv->display.color_check = glk_color_check;
		else if (INTEL_GEN(dev_priv) >= 7)
			dev_priv->display.color_check = ivb_color_check;
		else
			dev_priv->display.color_check = ilk_color_check;

		if (INTEL_GEN(dev_priv) >= 9)
			dev_priv->display.color_commit = skl_color_commit;
		else if (IS_BROADWELL(dev_priv) || IS_HASWELL(dev_priv))
			dev_priv->display.color_commit = hsw_color_commit;
		else
			dev_priv->display.color_commit = ilk_color_commit;

		if (INTEL_GEN(dev_priv) >= 11) {
			dev_priv->display.load_luts = icl_load_luts;
		} else if (IS_CANNONLAKE(dev_priv) || IS_GEMINILAKE(dev_priv)) {
			dev_priv->display.load_luts = glk_load_luts;
			dev_priv->display.read_luts = glk_read_luts;
		} else if (INTEL_GEN(dev_priv) >= 8) {
			dev_priv->display.load_luts = bdw_load_luts;
		} else if (INTEL_GEN(dev_priv) >= 7) {
			dev_priv->display.load_luts = ivb_load_luts;
		} else {
			dev_priv->display.load_luts = ilk_load_luts;
			dev_priv->display.read_luts = ilk_read_luts;
		}
	}

	drm_crtc_enable_color_mgmt(&crtc->base,
				   INTEL_INFO(dev_priv)->color.degamma_lut_size,
				   has_ctm,
				   INTEL_INFO(dev_priv)->color.gamma_lut_size);
}
