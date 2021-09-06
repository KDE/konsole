/*
 * HSLuv-C: Human-friendly HSL
 * <https://github.com/hsluv/hsluv-c>
 * <https://www.hsluv.org/>
 *
 * SPDX-FileCopyrightText: 2015 Alexei Boronine <alexei@boronine.com> (original idea, JavaScript implementation)
 * SPDX-FileCopyrightText: 2015 Roger Tallada <roger.tallada@gmail.com> (Obj-C implementation)
 * SPDX-FileCopyrightText: 2017 Martin Mitas <mity@morous.org> (C implementation, based on Obj-C implementation)
 * SPDX-License-Identifier: MIT
 */

#ifndef HSLUV_H
#define HSLUV_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Convert HSLuv to RGB.
 *
 * @param h Hue. Between 0.0 and 360.0.
 * @param s Saturation. Between 0.0 and 100.0.
 * @param l Lightness. Between 0.0 and 100.0.
 * @param[out] pr Red component. Between 0.0 and 1.0.
 * @param[out] pg Green component. Between 0.0 and 1.0.
 * @param[out] pb Blue component. Between 0.0 and 1.0.
 */
void hsluv2rgb(double h, double s, double l, double *pr, double *pg, double *pb);

/**
 * Convert RGB to HSLuv.
 *
 * @param r Red component. Between 0.0 and 1.0.
 * @param g Green component. Between 0.0 and 1.0.
 * @param b Blue component. Between 0.0 and 1.0.
 * @param[out] ph Hue. Between 0.0 and 360.0.
 * @param[out] ps Saturation. Between 0.0 and 100.0.
 * @param[out] pl Lightness. Between 0.0 and 100.0.
 */
void rgb2hsluv(double r, double g, double b, double *ph, double *ps, double *pl);

/**
 * Convert HPLuv to RGB.
 *
 * @param h Hue. Between 0.0 and 360.0.
 * @param s Saturation. Between 0.0 and 100.0.
 * @param l Lightness. Between 0.0 and 100.0.
 * @param[out] pr Red component. Between 0.0 and 1.0.
 * @param[out] pg Green component. Between 0.0 and 1.0.
 * @param[out] pb Blue component. Between 0.0 and 1.0.
 */
void hpluv2rgb(double h, double s, double l, double *pr, double *pg, double *pb);

/**
 * Convert RGB to HPLuv.
 *
 * @param r Red component. Between 0.0 and 1.0.
 * @param g Green component. Between 0.0 and 1.0.
 * @param b Blue component. Between 0.0 and 1.0.
 * @param[out] ph Hue. Between 0.0 and 360.0.
 * @param[out] ps Saturation. Between 0.0 and 100.0.
 * @param[out] pl Lightness. Between 0.0 and 100.0.
 *
 * Note that HPLuv does not contain all the colors of RGB, so converting
 * arbitrary RGB to it may generate invalid HPLuv colors.
 */
void rgb2hpluv(double r, double g, double b, double *ph, double *ps, double *pl);

#ifdef __cplusplus
}
#endif

#endif /* HSLUV_H */
