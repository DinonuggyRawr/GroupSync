#ifndef GROUPSYNC_COLORS_H
#define GROUPSYNC_COLORS_H

/* GroupSync light-purple theme palette.
 * Add this header to Visual Studio, then:
 *     #include "GroupSync_Colors.h"
 * For Win32 controls/GDI, use GS_COLOR_* (COLORREF).
 * GS_HEX_* strings are useful for web-based interfaces.
 */
#include <windows.h>

#define GS_COLOR_PRIMARY           RGB(124, 58, 237)   /* #7C3AED */
#define GS_COLOR_PRIMARY_HOVER     RGB(109, 40, 217)   /* #6D28D9 */
#define GS_COLOR_PRIMARY_BG        RGB(239, 228, 255)  /* #EFE4FF */
#define GS_COLOR_AVAILABLE         RGB(15, 118, 110)   /* #0F766E */
#define GS_COLOR_AVAILABLE_BG      RGB(204, 251, 241)  /* #CCFBF1 */
#define GS_COLOR_MAYBE             RGB(146, 64, 14)    /* #92400E */
#define GS_COLOR_MAYBE_BG          RGB(254, 243, 199)  /* #FEF3C7 */
#define GS_COLOR_UNAVAILABLE       RGB(185, 28, 28)    /* #B91C1C */
#define GS_COLOR_UNAVAILABLE_BG    RGB(254, 226, 226)  /* #FEE2E2 */
#define GS_COLOR_WINDOW_BG         RGB(244, 236, 255)  /* #F4ECFF */
#define GS_COLOR_SURFACE           RGB(252, 248, 255)  /* #FCF8FF */
#define GS_COLOR_TEXT              RGB(15, 23, 42)     /* #0F172A */
#define GS_COLOR_TEXT_SECONDARY    RGB(93, 70, 124)    /* #5D467C */
#define GS_COLOR_BORDER            RGB(196, 167, 231)  /* #C4A7E7 */
#define GS_COLOR_BUTTON_TEXT       RGB(255, 255, 255)  /* #FFFFFF */

#define GS_HEX_PRIMARY             "#7C3AED"
#define GS_HEX_PRIMARY_HOVER       "#6D28D9"
#define GS_HEX_PRIMARY_BG          "#EFE4FF"
#define GS_HEX_AVAILABLE           "#0F766E"
#define GS_HEX_AVAILABLE_BG        "#CCFBF1"
#define GS_HEX_MAYBE               "#92400E"
#define GS_HEX_MAYBE_BG            "#FEF3C7"
#define GS_HEX_UNAVAILABLE         "#B91C1C"
#define GS_HEX_UNAVAILABLE_BG      "#FEE2E2"
#define GS_HEX_WINDOW_BG           "#F4ECFF"
#define GS_HEX_SURFACE             "#FCF8FF"
#define GS_HEX_TEXT                "#0F172A"
#define GS_HEX_TEXT_SECONDARY      "#5D467C"
#define GS_HEX_BORDER              "#C4A7E7"
#define GS_HEX_BUTTON_TEXT         "#FFFFFF"

/* Example: HBRUSH background = CreateSolidBrush(GS_COLOR_WINDOW_BG);
 * Release brushes with DeleteObject() when they are no longer needed.
 * This file defines colors; controls must apply them in their drawing code.
 */
#endif /* GROUPSYNC_COLORS_H */
