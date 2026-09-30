#ifndef GROUPSYNC_COLORS_H
#define GROUPSYNC_COLORS_H

/* GroupSync light-theme palette.
 * Add this header to Visual Studio, then:
 *     #include "GroupSync_Colors.h"
 * For Win32 controls/GDI, use GS_COLOR_* (COLORREF).
 * GS_HEX_* strings are useful for web-based interfaces.
 */
#include <windows.h>

#define GS_COLOR_PRIMARY           RGB(79, 70, 229)    /* #4F46E5 */
#define GS_COLOR_PRIMARY_HOVER     RGB(67, 56, 202)    /* #4338CA */
#define GS_COLOR_PRIMARY_BG        RGB(234, 236, 254)  /* #EAECFE */
#define GS_COLOR_AVAILABLE         RGB(15, 118, 110)   /* #0F766E */
#define GS_COLOR_AVAILABLE_BG      RGB(204, 251, 241)  /* #CCFBF1 */
#define GS_COLOR_MAYBE             RGB(146, 64, 14)    /* #92400E */
#define GS_COLOR_MAYBE_BG          RGB(254, 243, 199)  /* #FEF3C7 */
#define GS_COLOR_UNAVAILABLE       RGB(185, 28, 28)    /* #B91C1C */
#define GS_COLOR_UNAVAILABLE_BG    RGB(254, 226, 226)  /* #FEE2E2 */
#define GS_COLOR_WINDOW_BG         RGB(248, 250, 252)  /* #F8FAFC */
#define GS_COLOR_SURFACE           RGB(255, 255, 255)  /* #FFFFFF */
#define GS_COLOR_TEXT              RGB(15, 23, 42)     /* #0F172A */
#define GS_COLOR_TEXT_SECONDARY    RGB(71, 85, 105)    /* #475569 */
#define GS_COLOR_BORDER            RGB(203, 213, 225)  /* #CBD5E1 */
#define GS_COLOR_BUTTON_TEXT       RGB(255, 255, 255)  /* #FFFFFF */

#define GS_HEX_PRIMARY             "#4F46E5"
#define GS_HEX_PRIMARY_HOVER       "#4338CA"
#define GS_HEX_PRIMARY_BG          "#EAECFE"
#define GS_HEX_AVAILABLE           "#0F766E"
#define GS_HEX_AVAILABLE_BG        "#CCFBF1"
#define GS_HEX_MAYBE               "#92400E"
#define GS_HEX_MAYBE_BG            "#FEF3C7"
#define GS_HEX_UNAVAILABLE         "#B91C1C"
#define GS_HEX_UNAVAILABLE_BG      "#FEE2E2"
#define GS_HEX_WINDOW_BG           "#F8FAFC"
#define GS_HEX_SURFACE             "#FFFFFF"
#define GS_HEX_TEXT                "#0F172A"
#define GS_HEX_TEXT_SECONDARY      "#475569"
#define GS_HEX_BORDER              "#CBD5E1"
#define GS_HEX_BUTTON_TEXT         "#FFFFFF"

/* Example: HBRUSH background = CreateSolidBrush(GS_COLOR_WINDOW_BG);
 * Release brushes with DeleteObject() when they are no longer needed.
 * This file defines colors; controls must apply them in their drawing code.
 */
#endif /* GROUPSYNC_COLORS_H */
