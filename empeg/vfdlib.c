/*

vfdlib - a graphics api library for empeg userland apps
Copyright (C) 2002  Richard Kirkby

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

Send comments, suggestions, bug reports to rkirkby (at) cs.waikato.ac.nz

*/

#include <stdlib.h> /* needed for malloc/free in drawSolidPolygon */
#include "vfdlib.h"

#define MASK_LO_NYBBLE   0x0F
#define MASK_HI_NYBBLE   0xF0


/* built-in fonts */

typedef struct {
  short offset;
  char width;
} FontInfo;

static unsigned char tinyFontBitmap[] = {
  BITMAP_1BPP, 0, 180, 0, 3,
  0xF9, 0xEF, 0xF5, 0xFB, 0x6F, 0x7F, 0xB3, 0xF6, 0xD6, 0xDC, 0xB6,
  0xD5, 0xDA, 0xF6, 0x4E, 0xD5, 0x40, 0x35, 0xEE, 0xDC, 0xD1, 0x7C,
  0x80, 0xFE, 0x5D, 0xAF, 0xAD, 0x56, 0xDE, 0xB2, 0x56, 0xD5, 0x75,
  0x52, 0x7D, 0x6F, 0xE0, 0x9D, 0x2E, 0xEE, 0x42, 0x17, 0x72, 0x68,
  0xD7, 0x50, 0xBD, 0xEF, 0x3D, 0xEB, 0xD6, 0xFB, 0xEE, 0x4D, 0x6E,
  0xA6, 0xBB, 0xC6, 0x6F, 0x41, 0x28, 0xD5, 0x51, 0x9D, 0xE4, 0xF9,
  0xC6, 0x7C, 0x20
};

static FontInfo tinyFontInfo[] = {
  {106, 1}, /* ! */ {107, 3}, /* " */ {  0, 0}, /* # */ {  0, 0}, /* $ */
  {110, 5}, /* % */ {115, 3}, /* & */ {118, 1}, /* ' */ {119, 2}, /* ( */
  {121, 2}, /* ) */ {123, 5}, /* * */ {128, 3}, /* + */ {131, 2}, /* , */
  {133, 2}, /* - */ {135, 1}, /* . */ {136, 3}, /* / */ { 79, 3}, /* 0 */
  { 82, 3}, /* 1 */ { 85, 3}, /* 2 */ { 88, 3}, /* 3 */ { 91, 3}, /* 4 */
  { 94, 3}, /* 5 */ { 97, 2}, /* 6 */ { 99, 2}, /* 7 */ {101, 3}, /* 8 */
  {104, 2}, /* 9 */ {139, 1}, /* : */ {140, 2}, /* ; */ {142, 2}, /* < */
  {144, 2}, /* = */ {146, 2}, /* > */ {148, 3}, /* ? */ {151, 4}, /* @ */
  {  0, 3}, /* A */ {  3, 3}, /* B */ {  6, 3}, /* C */ {  9, 3}, /* D */
  { 12, 3}, /* E */ { 15, 3}, /* F */ { 18, 3}, /* G */ { 21, 3}, /* H */
  { 24, 1}, /* I */ { 25, 3}, /* J */ { 28, 3}, /* K */ { 31, 2}, /* L */
  { 33, 5}, /* M */ { 38, 3}, /* N */ { 41, 3}, /* O */ { 44, 2}, /* P */
  { 46, 4}, /* Q */ { 50, 3}, /* R */ { 53, 3}, /* S */ { 56, 3}, /* T */
  { 59, 3}, /* U */ { 62, 3}, /* V */ { 65, 5}, /* W */ { 70, 3}, /* X */
  { 73, 3}, /* Y */ { 76, 3}, /* Z */ {155, 2}, /* [ */ {157, 3}, /* \ */
  {160, 2}, /* ] */ {162, 3}, /* ^ */ {165, 2}, /* _ */ {167, 1}, /* ` */
  {  0, 3}, /* a */ {  3, 3}, /* b */ {  6, 3}, /* c */ {  9, 3}, /* d */
  { 12, 3}, /* e */ { 15, 3}, /* f */ { 18, 3}, /* g */ { 21, 3}, /* h */
  { 24, 1}, /* i */ { 25, 3}, /* j */ { 28, 3}, /* k */ { 31, 2}, /* l */
  { 33, 5}, /* m */ { 38, 3}, /* n */ { 41, 3}, /* o */ { 44, 2}, /* p */
  { 46, 4}, /* q */ { 50, 3}, /* r */ { 53, 3}, /* s */ { 56, 3}, /* t */
  { 59, 3}, /* u */ { 62, 3}, /* v */ { 65, 5}, /* w */ { 70, 3}, /* x */
  { 73, 3}, /* y */ { 76, 3}, /* z */ {168, 3}, /* { */ {171, 1}, /* | */
  {172, 3}, /* } */ {175, 5}  /* ~ */
};

static unsigned char smallFontBitmap[] = {
  BITMAP_1BPP, 0, 252, 0, 5,
  0x59, 0xDE, 0xAF, 0xB7, 0xB2, 0xCB, 0x7E, 0xDA, 0xDB, 0x88, 0x10,
  0x92, 0xC8, 0x00, 0x00, 0x08, 0x00, 0x00, 0x3A, 0xBB, 0xBF, 0xFE,
  0xA9, 0xC9, 0xD8, 0x00, 0x01, 0x27, 0x0C, 0x60, 0xBE, 0x00, 0xBE,
  0xBD, 0x3D, 0xD5, 0x6D, 0xB6, 0xCA, 0xDA, 0xAE, 0xBC, 0xBD, 0x78,
  0xDB, 0xD9, 0x7B, 0x7E, 0xDA, 0xDB, 0xAE, 0x7B, 0x43, 0xFC, 0x7F,
  0x12, 0xA6, 0xA0, 0x3A, 0xD1, 0x6A, 0x28, 0xAA, 0xD0, 0xF6, 0xB7,
  0x6D, 0xB5, 0x6D, 0xD7, 0x2A, 0xAA, 0xD5, 0x5B, 0x5D, 0xF6, 0xEA,
  0xB6, 0xFA, 0x4A, 0xDA, 0xA6, 0xAA, 0x9E, 0xF5, 0x48, 0x7D, 0xA7,
  0x25, 0x76, 0x44, 0x0A, 0xD9, 0x34, 0x49, 0xB0, 0xBD, 0xDC, 0xEE,
  0xBD, 0x6A, 0x8E, 0xE9, 0xAF, 0x55, 0xFC, 0xB7, 0x37, 0x56, 0xB5,
  0x4E, 0xC5, 0xAF, 0x52, 0xFF, 0xF3, 0x75, 0xF4, 0x2B, 0xCA, 0x26,
  0xA9, 0x9A, 0xD0, 0xB8, 0xA0, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x48,
  0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x08,
  0x09, 0x22, 0x7C, 0x63, 0x3E, 0x00
};

static FontInfo smallFontInfo[] = {
  {173, 1}, /* ! */ {174, 3}, /* " */ {177, 5}, /* # */ {182, 3}, /* $ */
  {185, 4}, /* % */ {189, 4}, /* & */ {193, 1}, /* ' */ {194, 2}, /* ( */
  {196, 2}, /* ) */ {198, 3}, /* * */ {201, 3}, /* + */ {204, 1}, /* , */
  {205, 2}, /* - */ {207, 1}, /* . */ {208, 3}, /* / */ {146, 3}, /* 0 */
  {149, 3}, /* 1 */ {152, 2}, /* 2 */ {154, 2}, /* 3 */ {156, 3}, /* 4 */
  {159, 2}, /* 5 */ {161, 3}, /* 6 */ {164, 3}, /* 7 */ {167, 3}, /* 8 */
  {170, 3}, /* 9 */ {211, 1}, /* : */ {212, 1}, /* ; */ {213, 3}, /* < */
  {216, 2}, /* = */ {218, 3}, /* > */ {221, 3}, /* ? */ {224, 4}, /* @ */
  {  0, 3}, /* A */ {  3, 3}, /* B */ {  6, 2}, /* C */ {  8, 3}, /* D */
  { 11, 2}, /* E */ { 13, 2}, /* F */ { 15, 3}, /* G */ { 18, 3}, /* H */
  { 21, 1}, /* I */ { 22, 2}, /* J */ { 24, 3}, /* K */ { 27, 2}, /* L */
  { 29, 5}, /* M */ { 34, 3}, /* N */ { 37, 3}, /* O */ { 40, 3}, /* P */
  { 43, 3}, /* Q */ { 46, 3}, /* R */ { 49, 2}, /* S */ { 51, 3}, /* T */
  { 54, 3}, /* U */ { 57, 3}, /* V */ { 60, 5}, /* W */ { 65, 3}, /* X */
  { 68, 3}, /* Y */ { 71, 2}, /* Z */ {228, 2}, /* [ */ {230, 3}, /* \ */
  {233, 2}, /* ] */ {235, 3}, /* ^ */ {238, 2}, /* _ */ {240, 1}, /* ` */
  { 73, 3}, /* a */ { 76, 3}, /* b */ { 79, 2}, /* c */ { 81, 3}, /* d */
  { 84, 3}, /* e */ { 87, 2}, /* f */ { 89, 2}, /* g */ { 91, 3}, /* h */
  { 94, 1}, /* i */ { 95, 2}, /* j */ { 97, 3}, /* k */ {100, 2}, /* l */
  {102, 5}, /* m */ {107, 3}, /* n */ {110, 3}, /* o */ {113, 2}, /* p */
  {115, 3}, /* q */ {118, 2}, /* r */ {120, 3}, /* s */ {123, 3}, /* t */
  {126, 3}, /* u */ {129, 3}, /* v */ {132, 5}, /* w */ {137, 3}, /* x */
  {140, 3}, /* y */ {143, 3}, /* z */ {241, 3}, /* { */ {244, 1}, /* | */
  {245, 3}, /* } */ {248, 4}  /* ~ */
};

static unsigned char mediumFontBitmap[] = {
  BITMAP_1BPP, 1, 10, 0, 6,
  0x59, 0xDE, 0xAD, 0xB7, 0xB2, 0xCB, 0x7E, 0xDA, 0xDB, 0x88, 0x10,
  0x89, 0x64, 0x00, 0x00, 0x04, 0x00, 0x00, 0x12, 0xAB, 0xBD, 0x2D,
  0x52, 0x04, 0x99, 0x50, 0x00, 0x04, 0x98, 0xCC, 0x30, 0x13, 0xE0,
  0x00, 0xB6, 0xB5, 0x2D, 0xB5, 0x6D, 0xB6, 0xCA, 0xDA, 0xDA, 0xBC,
  0xB7, 0x3C, 0x6D, 0xEC, 0xB3, 0x7F, 0x6D, 0x6D, 0xEE, 0x5B, 0x46,
  0xDD, 0xFF, 0x9A, 0xA4, 0xE4, 0x01, 0xAB, 0x45, 0x2A, 0x12, 0x0A,
  0xA6, 0x80, 0xFA, 0xBF, 0x3D, 0xD5, 0x6D, 0xD7, 0x2A, 0xDA, 0xAF,
  0x5B, 0x5B, 0xDB, 0x75, 0x5B, 0x6D, 0x55, 0x6D, 0x55, 0x6A, 0x6E,
  0xF9, 0x38, 0x56, 0x24, 0x43, 0xFE, 0x62, 0x10, 0x3A, 0xD9, 0x15,
  0x04, 0x9B, 0x00, 0xB6, 0xB5, 0x6D, 0xB5, 0x6D, 0x96, 0xAA, 0xDA,
  0xD5, 0x5B, 0x5D, 0x7B, 0x6D, 0x5B, 0x77, 0x4D, 0x55, 0x6B, 0xAA,
  0x92, 0xDA, 0x90, 0xFB, 0x4A, 0x42, 0xE4, 0x04, 0x0B, 0x53, 0x58,
  0x98, 0x82, 0xA0, 0x00, 0xB9, 0xDC, 0xEE, 0xBD, 0x6A, 0x8E, 0xE9,
  0xAF, 0x55, 0xFC, 0xB7, 0x1B, 0x6B, 0x5A, 0xA1, 0xDA, 0xD7, 0xA9,
  0xD7, 0xE3, 0x3B, 0xE8, 0x57, 0x9F, 0x25, 0x50, 0x98, 0xA4, 0x81,
  0xE8, 0x50, 0x03, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x80, 0x00, 0x21, 0x00, 0x00,
  0x06, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x18, 0x01, 0x00, 0x40,
  0x10, 0xCC, 0x30, 0x60, 0x80, 0x00
};

static FontInfo mediumFontInfo[] = {
  {172, 1}, /* ! */ {173, 3}, /* " */ {176, 5}, /* # */ {181, 3}, /* $ */
  {184, 4}, /* % */ {188, 4}, /* & */ {192, 1}, /* ' */ {193, 3}, /* ( */
  {196, 3}, /* ) */ {199, 5}, /* * */ {204, 3}, /* + */ {207, 2}, /* , */
  {209, 2}, /* - */ {211, 1}, /* . */ {212, 4}, /* / */ {146, 3}, /* 0 */
  {149, 3}, /* 1 */ {152, 2}, /* 2 */ {154, 2}, /* 3 */ {156, 3}, /* 4 */
  {159, 2}, /* 5 */ {161, 3}, /* 6 */ {164, 2}, /* 7 */ {166, 3}, /* 8 */
  {169, 3}, /* 9 */ {216, 1}, /* : */ {217, 2}, /* ; */ {219, 3}, /* < */
  {222, 2}, /* = */ {224, 3}, /* > */ {227, 3}, /* ? */ {230, 6}, /* @ */
  {  0, 3}, /* A */ {  3, 3}, /* B */ {  6, 2}, /* C */ {  8, 3}, /* D */
  { 11, 2}, /* E */ { 13, 2}, /* F */ { 15, 3}, /* G */ { 18, 3}, /* H */
  { 21, 1}, /* I */ { 22, 2}, /* J */ { 24, 3}, /* K */ { 27, 2}, /* L */
  { 29, 5}, /* M */ { 34, 3}, /* N */ { 37, 3}, /* O */ { 40, 3}, /* P */
  { 43, 3}, /* Q */ { 46, 3}, /* R */ { 49, 2}, /* S */ { 51, 3}, /* T */
  { 54, 3}, /* U */ { 57, 3}, /* V */ { 60, 5}, /* W */ { 65, 3}, /* X */
  { 68, 3}, /* Y */ { 71, 2}, /* Z */ {236, 2}, /* [ */ {238, 4}, /* \ */
  {242, 2}, /* ] */ {244, 5}, /* ^ */ {249, 2}, /* _ */ {251, 2}, /* ` */
  { 73, 3}, /* a */ { 76, 3}, /* b */ { 79, 2}, /* c */ { 81, 3}, /* d */
  { 84, 3}, /* e */ { 87, 2}, /* f */ { 89, 3}, /* g */ { 92, 3}, /* h */
  { 95, 1}, /* i */ { 96, 2}, /* j */ { 98, 3}, /* k */ {101, 2}, /* l */
  {103, 5}, /* m */ {108, 3}, /* n */ {111, 3}, /* o */ {114, 3}, /* p */
  {117, 4}, /* q */ {121, 2}, /* r */ {123, 2}, /* s */ {125, 2}, /* t */
  {127, 3}, /* u */ {130, 3}, /* v */ {133, 5}, /* w */ {138, 3}, /* x */
  {141, 3}, /* y */ {144, 2}, /* z */ {253, 3}, /* { */ {256, 1}, /* | */
  {257, 3}, /* } */ {260, 5}  /* ~ */
};


/* global variables */

static int g_clipXLeft = 0;
static int g_clipYTop = 0;
static int g_clipXRight = VFD_WIDTH;
static int g_clipYBottom = VFD_HEIGHT;


/* private utility prototypes */

static void getFontInfo(int font, FontInfo **fInfo, unsigned char **fBitmap);
static void drawPixel4WaySymmetricClipped(char *buffer, int cX, int cY,
					  int x, int y, char shade);
static void drawPixel4WaySymmetricUnclipped(char *buffer, int cX, int cY,
					    int x, int y, char shade);
static void drawLineHMaxClipped(char *buffer, int x0, int y0, int dx, int dy,
				int d, int xMax, int yMax, char shade);
static void drawLineVMaxClipped(char *buffer, int x0, int y0, int dx, int dy,
				int d, int xMax, int yMax, char shade);
static void drawLineHMaxUnclipped(char *buffer, int x0, int y0,
				  int dx, int dy, char shade);
static void drawLineVMaxUnclipped(char *buffer, int x0, int y0,
				  int dx, int dy, char shade);
static void drawBitmap1BPP(char *buffer, char *bitmap,
			   int bitmapWidth, int bitmapHeight,
			   int destX, int destY,
			   int sourceX, int sourceY,
			   int width, int height,
			   signed char shade, int isTransparent);
static void drawBitmap2BPP(char *buffer, char *bitmap,
			   int bitmapWidth, int bitmapHeight,
			   int destX, int destY,
			   int sourceX, int sourceY,
			   int width, int height,
			   signed char shade, int isTransparent);
static void drawBitmap4BPP(char *buffer, char *bitmap,
			   int bitmapWidth, int bitmapHeight,
			   int destX, int destY,
			   int sourceX, int sourceY,
			   int width, int height,
			   signed char shade, int isTransparent);


/* public methods */

/*
SETCLIPAREA

Sets the clip rectangle, outside which all clipped drawing operations will be
discarded.

Parameters:
 xLeft - the left hand side of the clip rectangle
 yTop - the top of the clip rectangle
 xRight - the right hand side of the clip rectangle
 yBottom - the bottom of the clip rectangle

Notes:
 Checks are in place to ensure the clip rectangle cannot reach beyond the
 display area.

*/
void vfdlib_setClipArea(int xLeft, int yTop, int xRight, int yBottom) {

  /* sanity checks */
  if (yTop > yBottom || xLeft > xRight) return;
  if (xLeft < 0) xLeft = 0;
  if (yTop < 0) yTop = 0;
  if (xRight > VFD_WIDTH) xRight = VFD_WIDTH;
  if (yBottom > VFD_HEIGHT) yBottom = VFD_HEIGHT;

  /* assign new values */
  g_clipXLeft = xLeft;
  g_clipYTop = yTop;
  g_clipXRight = xRight;
  g_clipYBottom = yBottom;
}

/*
CLEAR

Clears the area inside the clip area to a specified colour.

Parameters:
 buffer - pointer to the display buffer
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

*/
void vfdlib_clear(char *buffer, char shade) {

  vfdlib_drawSolidRectangleUnclipped(buffer, g_clipXLeft, g_clipYTop,
				     g_clipXRight - g_clipXLeft,
				     g_clipYBottom - g_clipYTop,
				     shade);
}

/*
DRAWPOINTCLIPPED

Sets a (clipped) pixel in the display buffer to a specified colour.

Parameters:
 buffer - pointer to the display buffer
 xPos - the x-coordinate of the pixel
 yPos - the y-coordinate of the pixel
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.

*/
void vfdlib_drawPointClipped(char *buffer, int xPos, int yPos, char shade) {

  if (xPos >= g_clipXLeft && xPos < g_clipXRight &&
      yPos >= g_clipYTop  && yPos < g_clipYBottom) {

    vfdlib_drawPointUnclipped(buffer, xPos, yPos, shade);
  }
}

/*
DRAWPOINTUNCLIPPED

Sets a pixel in the display buffer to a specified colour.

Parameters:
 buffer - pointer to the display buffer
 xPos - the x-coordinate of the pixel
 yPos - the y-coordinate of the pixel
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.

*/
void vfdlib_drawPointUnclipped(char *buffer, int xPos, int yPos, char shade) {

  int byteOffset = (yPos << 6) + (xPos >> 1);
  if (xPos & 0x1) { /* RHS pixel */
    buffer[byteOffset] &= MASK_LO_NYBBLE;
    buffer[byteOffset] |= (shade << 4);
  } else { /* LHS pixel */
    buffer[byteOffset] &= MASK_HI_NYBBLE;
    buffer[byteOffset] |= shade;
  }
}

/*
DRAWLINEHORIZCLIPPED

Draws a (clipped) horizontal line in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 yPos - the y-coordinate of the line
 xLeft - the x-coordinate of the left hand side of the line
 xRight - the x-coordinate of the right hand side of the line
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.

*/
void vfdlib_drawLineHorizClipped(char *buffer, int yPos, int xLeft, int xRight,
				 char shade) {

  if (xLeft < xRight &&
      xRight >= g_clipXLeft && xLeft < g_clipXRight &&
      yPos >= g_clipYTop  && yPos < g_clipYBottom) {

    int left = xLeft > g_clipXLeft ? xLeft : g_clipXLeft;
    vfdlib_drawLineHorizUnclipped(buffer, yPos, left,
				  (xRight < g_clipXRight ?
				   xRight : g_clipXRight) - left,
				  shade);
  }
}

/*
DRAWLINEHORIZUNCLIPPED

Draws a horizontal line in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 yPos - the y-coordinate of the line
 xLeft - the x-coordinate of the left hand side of the line
 length - the width of the line
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.

*/
void vfdlib_drawLineHorizUnclipped(char *buffer, int yPos, int xLeft,
				   int length, char shade) {

  char doubleShade = shade | (shade << 4);
  
  if (length < 1) return;
  
  buffer += (yPos << 6) + (xLeft >> 1);

  /* do left */
  if (xLeft & 0x1) {
    *buffer &= MASK_LO_NYBBLE;
    *buffer |= (shade << 4);
    length--;
    buffer++;
  }

  /* do middle */
  while (length > 1) {
    *(buffer++) = doubleShade;
    length -= 2;
  }

  /* do right */
  if (length > 0) {
    *buffer &= MASK_HI_NYBBLE;
    *buffer |= shade;
  }
}

/*
DRAWLINEVERTCLIPPED

Draws a (clipped) vertical line in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 xPos - the x-coordinate of the line
 yTop - the y-coordinate of the top of the line
 yBottom - the y-coordinate of the bottom of the line
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.

*/
void vfdlib_drawLineVertClipped(char *buffer, int xPos, int yTop, int yBottom,
				char shade) {

  if (yTop < yBottom &&
      xPos >= g_clipXLeft && xPos < g_clipXRight &&
      yBottom >= g_clipYTop  && yTop < g_clipYBottom) {

    int top = yTop > g_clipYTop ? yTop : g_clipYTop;
    vfdlib_drawLineVertUnclipped(buffer, xPos,
				 top,
				 (yBottom < g_clipYBottom ?
				  yBottom : g_clipYBottom) - top,
				 shade);
  }
}

/*
DRAWLINEVERTUNCLIPPED

Draws a vertical line in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 xPos - the x-coordinate of the line
 yTop - the y-coordinate of the top of the line
 length - the height of the line
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.

*/
void vfdlib_drawLineVertUnclipped(char *buffer, int xPos, int yTop, int length,
				  char shade) {

  if (length < 1) return;

  buffer += (yTop << 6) + (xPos >> 1);

  if (xPos & 0x1) { /* LHS pixel */
    while (length-- > 0) {
      *buffer &= MASK_LO_NYBBLE;
      *buffer |= (shade << 4);
      buffer += VFD_BYTES_PER_SCANLINE;
    }
  } else { /* RHS pixel */
    while (length-- > 0) {
      *buffer &= MASK_HI_NYBBLE;
      *buffer |= shade;
      buffer += VFD_BYTES_PER_SCANLINE;
    }
  }
}

/*
DRAWLINECLIPPED

Draws a (clipped) line between two points in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 x0 - the x-coordinate of the first endpoint
 y0 - the y-coordinate of the first endpoint
 x1 - the x-coordinate of the second endpoint
 y1 - the y-coordinate of the second endpoint
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.

*/
void vfdlib_drawLineClipped(char *buffer, int x0, int y0, int x1, int y1,
			    char shade) {

  int dx = x1 - x0;
  int dy = y1 - y0;
  int distToHEntry, distToVEntry, distToHExit, distToVExit, xDelta, yDelta;
  int dyAbs, xMax, yMax;

  /* make sure x is positive */
  if (dx < 0) {
    /* swap points */ 
    x0 = x1;
    y0 = y1;
    x1 -= dx;
    y1 -= dy;
    dx = -dx;
    dy = -dy;
  }
  distToHEntry = g_clipXLeft - x0;
  if (dx < distToHEntry) return;
  
  if (dy > 0) {
    distToVEntry = g_clipYTop - y0;
    if (dy < distToVEntry) return;
    distToVExit = g_clipYBottom - y0 - 1;
    dyAbs = dy;
  } else {
    distToVEntry = y0 - g_clipYBottom + 1;
    if (-dy < distToVEntry) return;
    distToVExit = y0 - g_clipYTop;
    dyAbs = -dy;
  }
  if (distToVExit < 0) return;
  distToHExit = g_clipXRight - x0 - 1;
  if (distToHExit < 0) return;

  xDelta = 0; yDelta = 0;

  if (dx > dyAbs) { /* hmax line */
    if (distToHEntry > 0) {
      /* intersect LH edge */ 
      int temp = distToHEntry * dyAbs;
      yDelta += temp / dx;
      if (temp % dx > (dx >> 1)) yDelta++; /* round up */
      xDelta += distToHEntry;
      if (distToVExit - yDelta < 0) return;
    }
    if (distToVEntry - yDelta > 0) {
      /* intersect top/bottom - step back 1/2 a pixel */
      xDelta = (((distToVEntry << 1) - 1) * dx) / (dyAbs << 1) + 1;
      yDelta = distToVEntry;

      /* reject if intersection past RH edge */
      if (xDelta > distToHExit) return;
    }
    if (dy > 0) y0 += yDelta;
    else y0 -= yDelta;

    distToHExit -= xDelta;
    xMax = dx - xDelta;
    if (distToHExit < xMax) xMax = distToHExit;

    distToVExit -= yDelta;    
    yMax = dyAbs - yDelta;
    if (distToVExit < yMax) yMax = distToVExit;

    drawLineHMaxClipped(buffer, x0 + xDelta, y0, dx, dy,
			((2 + (xDelta << 1)) * dyAbs) -
			((1 + (yDelta << 1)) * dx),
			xMax, yMax, shade);
  } else { /* vmax line */
    if (distToVEntry > 0) {
      /* intersect top/bottom */
      int temp = distToVEntry * dx;
      xDelta += temp / dyAbs;
      if (temp % dyAbs > (dyAbs >> 1)) xDelta++; /* round up */
      yDelta += distToVEntry;
      if (distToHExit - xDelta < 0) return;
    }
    if (distToHEntry - xDelta > 0) {
      /* intersect LH edge - step back 1/2 a pixel */
      yDelta = ((distToHEntry << 1) - 1) * dyAbs / (dx << 1) + 1;
      xDelta = distToHEntry;

      /* reject if intersection not on left edge */
      if (yDelta > distToVExit) return;
    }
    if (dy > 0) y0 += yDelta;
    else y0 -= yDelta;

    distToHExit -= xDelta;
    xMax = dx - xDelta;
    if (distToHExit < xMax) xMax = distToHExit;

    distToVExit -= yDelta;    
    yMax = dyAbs - yDelta;
    if (distToVExit < yMax) yMax = distToVExit;

    drawLineVMaxClipped(buffer, x0 + xDelta, y0, dx, dy,
			((2 + (yDelta << 1)) * dx) -
			((1 + (xDelta << 1)) * dyAbs),
			xMax, yMax, shade);
  }
}

/*
DRAWLINEUNCLIPPED

Draws a line between two points in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 x0 - the x-coordinate of the first endpoint
 y0 - the y-coordinate of the first endpoint
 x1 - the x-coordinate of the second endpoint
 y1 - the y-coordinate of the second endpoint
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.

*/
void vfdlib_drawLineUnclipped(char *buffer, int x0, int y0, int x1, int y1,
			      char shade) {

  int dx = x1 - x0;
  int dy = y1 - y0;

  if (dx == 0) {
    /* vertical line */
    if (dy >= 0)
      vfdlib_drawLineVertUnclipped(buffer, x0, y0, dy+1, shade);
    else vfdlib_drawLineVertUnclipped(buffer, x0, y1, -dy+1, shade);
    return;
  }
  if (dy == 0) {
    /* horizontal line */
    if (dx >= 0)
      vfdlib_drawLineHorizUnclipped(buffer, y0, x0, dx+1, shade);
    else vfdlib_drawLineHorizUnclipped(buffer, y0, x1, -dx+1, shade);
    return;
  }
  if (dx < 0) {
    /* swap points */
    x0 = x1;
    y0 = y1;
    dy = -dy;
    dx = -dx;
  }
  if (dy < 0) {
    if (dx > -dy) drawLineHMaxUnclipped(buffer, x0, y0, dx, dy, shade);
    else drawLineVMaxUnclipped(buffer, x0, y0, dx, dy, shade);
  } else {
    if (dx > dy) drawLineHMaxUnclipped(buffer, x0, y0, dx, dy, shade);
    else drawLineVMaxUnclipped(buffer, x0, y0, dx, dy, shade);
  }
}

/*
DRAWOUTLINERECTANGLECLIPPED

Draws the (clipped) outline of a rectangle in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 xLeft - the x-coordinate of the lefthand side of the rectangle
 yTop - the y-coordinate of the top of the rectangle
 xRight - the x-coordinate of the righthand side of the rectangle
 yBottom - the y-coordinate of the bottom of the rectangle
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.

*/
void vfdlib_drawOutlineRectangleClipped(char *buffer, int xLeft, int yTop,
					int xRight, int yBottom, char shade) {

  vfdlib_drawLineHorizClipped(buffer, yTop, xLeft, xRight, shade);/* top */
  vfdlib_drawLineVertClipped(buffer, xLeft, yTop, yBottom, shade);/* left */
  vfdlib_drawLineHorizClipped(buffer, yBottom-1, xLeft, xRight,
			      shade);/* bottom */
  vfdlib_drawLineVertClipped(buffer, xRight-1, yTop, yBottom,
			     shade);/* right */  
}

/*
DRAWOUTLINERECTANGLEUNCLIPPED

Draws the outline of a rectangle in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 xLeft - the x-coordinate of the lefthand side of the rectangle
 yTop - the y-coordinate of the top of the rectangle
 xWidth - the width of the rectangle
 yHeight - the height of the rectangle
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.

*/
void vfdlib_drawOutlineRectangleUnclipped(char *buffer, int xLeft, int yTop,
					  int xWidth, int yHeight, char shade)
{

  vfdlib_drawLineHorizUnclipped(buffer, yTop, xLeft, xWidth, shade);/* top */
  vfdlib_drawLineVertUnclipped(buffer, xLeft, yTop, yHeight, shade);/* left */
  vfdlib_drawLineHorizUnclipped(buffer, yTop+yHeight-1, xLeft, xWidth,
				shade);/* bottom */
  vfdlib_drawLineVertUnclipped(buffer, xLeft+xWidth-1, yTop, yHeight,
			       shade);/* right */  
}

/*
DRAWSOLIDRECTANGLECLIPPED

Draws a (clipped) solid rectangle in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 xLeft - the x-coordinate of the lefthand side of the rectangle
 yTop - the y-coordinate of the top of the rectangle
 xRight - the x-coordinate of the righthand side of the rectangle
 yBottom - the y-coordinate of the bottom of the rectangle
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.

*/
void vfdlib_drawSolidRectangleClipped(char *buffer, int xLeft, int yTop,
				      int xRight, int yBottom, char shade) {

  int width, height;
  if (xLeft < g_clipXLeft) xLeft = g_clipXLeft;
  if (yTop < g_clipYTop) yTop = g_clipYTop;
  if (xRight > g_clipXRight) xRight = g_clipXRight;
  if (yBottom > g_clipYBottom) yBottom = g_clipYBottom;
  width = xRight - xLeft;
  height = yBottom - yTop;
  if (width < 1 || height < 1) return;
  vfdlib_drawSolidRectangleUnclipped(buffer, xLeft, yTop, width, height,
				     shade);
}

/*
DRAWSOLIDRECTANGLEUNCLIPPED

Draws a solid rectangle in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 xLeft - the x-coordinate of the lefthand side of the rectangle
 yTop - the y-coordinate of the top of the rectangle
 xWidth - the width of the rectangle
 yHeight - the height of the rectangle
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.

*/
void vfdlib_drawSolidRectangleUnclipped(char *buffer, int xLeft, int yTop,
					int xWidth, int yHeight, char shade) {

  int widthRemaining;
  char doubleShade = shade | (shade << 4);
  char *lineStart;
  lineStart = buffer + (yTop << 6) + (xLeft >> 1);

  while (yHeight-- > 0) {

    buffer = lineStart;
    widthRemaining = xWidth;

    /* do left */
    if (xLeft & 0x1) {
      *buffer &= MASK_LO_NYBBLE;
      *buffer |= (shade << 4);
      widthRemaining--;
      buffer++;
    }
    
    /* do middle */
    while (widthRemaining > 1) {
      *(buffer++) = doubleShade;
      widthRemaining -= 2;
    }
    
    /* do right */
    if (widthRemaining > 0) {
      *buffer &= MASK_HI_NYBBLE;
      *buffer |= shade;
	  buffer++;
    }

    lineStart += VFD_BYTES_PER_SCANLINE;
  }
}

/*
DRAWOUTLINEELLIPSECLIPPED

Draws the (clipped) outline of an ellipse in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 cX - the x centre-coordinate of the ellipse
 cY - the y centre-coordinate of the ellipse
 a - half the width of the ellipse
 b - half the height of the ellipse
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.
 Uses an incremental integer midpoint algorithm for speed.

*/
void vfdlib_drawOutlineEllipseClipped(char *buffer, int cX, int cY,
				      int a, int b, char shade) {

  int d2, deltaS, x, y, a_2, b_2, a2_2, b2_2, a4_2, b4_2;
  int a8_2, b8_2, a8_2plusb8_2, d1, deltaE, deltaSE;
  
  if ((cX - a) > g_clipXLeft && (cX + a) < g_clipXRight &&
      (cY - b) > g_clipYTop && (cY + b) < g_clipYBottom) {

    vfdlib_drawOutlineEllipseUnclipped(buffer, cX, cY, a, b, shade);
    return;
  }

  x=0;
  y=b;
  
  /* precalculate values for speed */
  a_2 = a * a;
  b_2 = b * b;
  a2_2 = a_2 << 1;
  b2_2 = b_2 << 1;
  a4_2 = a2_2 << 1;
  b4_2 = b2_2 << 1;
  a8_2 = a4_2 << 1;
  b8_2 = b4_2 << 1;
  a8_2plusb8_2 = a8_2 + b8_2;
  
  /* region 1 */
  d1 = b4_2 - (a4_2 * b) + a_2;
  deltaE = 12 * b_2;
  deltaSE = deltaE - a8_2 * b + a8_2;
  
  drawPixel4WaySymmetricClipped(buffer, cX, cY, x, y, shade);
  
  while (a2_2 * y - a_2 > b2_2 * (x + 1)) {
    if (d1 < 0) { /* choose E */
      d1 += deltaE;
      deltaE += b8_2;
      deltaSE += b8_2;
    }
    else { /* choose SE */
      d1 += deltaSE;
      deltaE += b8_2;
      deltaSE += a8_2plusb8_2;
      y--;
    }
    x++;
    drawPixel4WaySymmetricClipped(buffer, cX, cY, x, y, shade);
  }
  
  /* region 2 */
  d2 =  (b4_2 * (x * x + x) + b_2) + (a4_2 * (y - 1) * (y - 1)) -
    (a4_2 * b_2);
  deltaS = a4_2 * (-2 * y + 3);
  deltaSE = b4_2 * (2 * x + 2) + deltaS;
  
  while (y > 0) {
    if (d2 < 0) { /* choose SE */
      d2 += deltaSE;
      deltaSE += a8_2plusb8_2;
      deltaS += a8_2;
      x++;
    } else {/* choose S */
      d2 += deltaS;
      deltaSE += a8_2;
      deltaS += a8_2;
    }
    y--;
    drawPixel4WaySymmetricClipped(buffer, cX, cY, x, y, shade);
  }
}

/*
DRAWOUTLINEELLIPSEUNCLIPPED

Draws the outline of an ellipse in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 cX - the x centre-coordinate of the ellipse
 cY - the y centre-coordinate of the ellipse
 a - half the width of the ellipse
 b - half the height of the ellipse
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.
 Uses an incremental integer midpoint algorithm for speed.

*/
void vfdlib_drawOutlineEllipseUnclipped(char *buffer, int cX, int cY,
					int a, int b, char shade) {

  int d2, deltaS;
  
  int x=0;
  int y=b;
  
  /* precalculate values for speed */
  int a_2 = a * a;
  int b_2 = b * b;
  int a2_2 = a_2 << 1;
  int b2_2 = b_2 << 1;
  int a4_2 = a2_2 << 1;
  int b4_2 = b2_2 << 1;
  int a8_2 = a4_2 << 1;
  int b8_2 = b4_2 << 1;
  int a8_2plusb8_2 = a8_2 + b8_2;
  
  /* region 1 */
  int d1 = b4_2 - (a4_2 * b) + a_2;
  int deltaE = 12 * b_2;
  int deltaSE = deltaE - a8_2 * b + a8_2;
  
  drawPixel4WaySymmetricUnclipped(buffer, cX, cY, x, y, shade);
  
  while (a2_2 * y - a_2 > b2_2 * (x + 1)) {
    if (d1 < 0) { /* choose E */
      d1 += deltaE;
      deltaE += b8_2;
      deltaSE += b8_2;
    }
    else { /* choose SE */
      d1 += deltaSE;
      deltaE += b8_2;
      deltaSE += a8_2plusb8_2;
      y--;
    }
    x++;
    drawPixel4WaySymmetricUnclipped(buffer, cX, cY, x, y, shade);
  }
  
  /* region 2 */
  d2 =  (b4_2 * (x * x + x) + b_2) + (a4_2 * (y - 1) * (y - 1)) - (a4_2 * b_2);
  deltaS = a4_2 * (-2 * y + 3);
  deltaSE = b4_2 * (2 * x + 2) + deltaS;
  
  while (y > 0) {
    if (d2 < 0) { /* choose SE */
      d2 += deltaSE;
      deltaSE += a8_2plusb8_2;
      deltaS += a8_2;
      x++;
    } else {/* choose S */
      d2 += deltaS;
      deltaSE += a8_2;
      deltaS += a8_2;
    }
    y--;
    drawPixel4WaySymmetricUnclipped(buffer, cX, cY, x, y, shade);
  }
}

/*
DRAWSOLIDELLIPSECLIPPED

Draws a (clipped) solid ellipse in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 cX - the x centre-coordinate of the ellipse
 cY - the y centre-coordinate of the ellipse
 a - half the width of the ellipse
 b - half the height of the ellipse
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.
 Uses an incremental integer midpoint algorithm for speed.

*/
void vfdlib_drawSolidEllipseClipped(char *buffer, int cX, int cY,
				    int a, int b, char shade) {

  int d2, deltaS, x, y, a_2, b_2, a2_2, b2_2, a4_2, b4_2;
  int a8_2, b8_2, a8_2plusb8_2, d1, deltaE, deltaSE;
  
  if ((cX - a) > g_clipXLeft && (cX + a) < g_clipXRight &&
      (cY - b) > g_clipYTop && (cY + b) < g_clipYBottom) {

    vfdlib_drawSolidEllipseUnclipped(buffer, cX, cY, a, b, shade);
    return;
  }

  x=0;
  y=b;

  /* precalculate values for speed */
  a_2 = a * a;
  b_2 = b * b;
  a2_2 = a_2 << 1;
  b2_2 = b_2 << 1;
  a4_2 = a2_2 << 1;
  b4_2 = b2_2 << 1;
  a8_2 = a4_2 << 1;
  b8_2 = b4_2 << 1;
  a8_2plusb8_2 = a8_2 + b8_2;
  
  /* region 1 */
  d1 = b4_2 - (a4_2 * b) + a_2;
  deltaE = 12 * b_2;
  deltaSE = deltaE - a8_2 * b + a8_2;

  while (a2_2 * y - a_2 > b2_2 * (x + 1)) {
    if (d1 < 0) { /* choose E */
      d1 += deltaE;
      deltaE += b8_2;
      deltaSE += b8_2;
    }
    else { /* choose SE */
      d1 += deltaSE;
      deltaE += b8_2;
      deltaSE += a8_2plusb8_2;
      vfdlib_drawLineHorizClipped(buffer, cY + y, cX - x, cX + x, shade);
      vfdlib_drawLineHorizClipped(buffer, cY - y, cX - x, cX + x, shade);
      y--;
    }
    x++;
  }

  /* region 2 */
  d2 = (b4_2 * (x * x + x) + b_2) + (a4_2 * (y - 1) * (y - 1)) - (a4_2 * b_2);
  deltaS = a4_2 * (-2 * y + 3);
  deltaSE = b4_2 * (2 * x + 2) + deltaS;
	
  while (y > 0) {
    if (d2 < 0) { /* choose SE */
      d2 += deltaSE;
      deltaSE += a8_2plusb8_2;
      deltaS += a8_2;
      x++;
    } else {/* choose S */
      d2 += deltaS;
      deltaSE += a8_2;
      deltaS += a8_2;
    }
    vfdlib_drawLineHorizClipped(buffer, cY + y, cX - x, cX + x, shade);
    vfdlib_drawLineHorizClipped(buffer, cY - y, cX - x, cX + x, shade);
    y--;
  }
  vfdlib_drawLineHorizClipped(buffer, cY, cX - x, cX + x, shade);
}

/*
DRAWSOLIDELLIPSEUNCLIPPED

Draws a solid ellipse in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 cX - the x centre-coordinate of the ellipse
 cY - the y centre-coordinate of the ellipse
 a - half the width of the ellipse
 b - half the height of the ellipse
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.
 Uses an incremental integer midpoint algorithm for speed.

*/
void vfdlib_drawSolidEllipseUnclipped(char *buffer, int cX, int cY,
				      int a, int b, char shade) {

  int d2, deltaS;

  int x=0;
  int y=b;

  /* precalculate values for speed */
  int a_2 = a * a;
  int b_2 = b * b;
  int a2_2 = a_2 << 1;
  int b2_2 = b_2 << 1;
  int a4_2 = a2_2 << 1;
  int b4_2 = b2_2 << 1;
  int a8_2 = a4_2 << 1;
  int b8_2 = b4_2 << 1;
  int a8_2plusb8_2 = a8_2 + b8_2;

  /* region 1 */
  int d1 = b4_2 - (a4_2 * b) + a_2;
  int deltaE = 12 * b_2;
  int deltaSE = deltaE - a8_2 * b + a8_2;

  while (a2_2 * y - a_2 > b2_2 * (x + 1)) {
    if (d1 < 0) { /* choose E */
      d1 += deltaE;
      deltaE += b8_2;
      deltaSE += b8_2;
    }
    else { /* choose SE */
      d1 += deltaSE;
      deltaE += b8_2;
      deltaSE += a8_2plusb8_2;
      vfdlib_drawLineHorizUnclipped(buffer, cY + y, cX - x, x << 1, shade);
      vfdlib_drawLineHorizUnclipped(buffer, cY - y, cX - x, x << 1, shade);
      y--;
    }
    x++;
  }

  /* region 2 */
  d2 =  (b4_2 * (x * x + x) + b_2) + (a4_2 * (y - 1) * (y - 1)) - (a4_2 * b_2);
  deltaS = a4_2 * (-2 * y + 3);
  deltaSE = b4_2 * (2 * x + 2) + deltaS;
	
  while (y > 0) {
    if (d2 < 0) { /* choose SE */
      d2 += deltaSE;
      deltaSE += a8_2plusb8_2;
      deltaS += a8_2;
      x++;
    } else {/* choose S */
      d2 += deltaS;
      deltaSE += a8_2;
      deltaS += a8_2;
    }
    vfdlib_drawLineHorizUnclipped(buffer, cY + y, cX - x, x << 1, shade);
    vfdlib_drawLineHorizUnclipped(buffer, cY - y, cX - x, x << 1, shade);
    y--;
  }
  vfdlib_drawLineHorizUnclipped(buffer, cY, cX - x, x << 1, shade);
}

/*
DRAWOUTLINEPOLYGONCLIPPED

Draws the (clipped) outline of a polygon in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 coordsData - pointer to an array of numPoints pairs of integers;
              x0, y0, x1, y1...
 numPoints - the number of vertices in the polygon
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.
 Polygons are closed automatically - the first point is drawn connected to
 the last point.

*/
void vfdlib_drawOutlinePolygonClipped(char *buffer, int *coordsData,
				      int numPoints, char shade) {

  int x0, y0, x1, y1;
  int lastIndex = (numPoints-1) << 1; 
  int currIndex;
  int prevX = coordsData[lastIndex];
  int prevY = coordsData[lastIndex+1];

  for (currIndex = 0; currIndex<=lastIndex;) {
    x0 = prevX;
    y0 = prevY;
    prevX = x1 = coordsData[currIndex++];
    prevY = y1 = coordsData[currIndex++];
    vfdlib_drawLineClipped(buffer, x0, y0, x1, y1, shade);
  }
}

/*
DRAWOUTLINEPOLYGONUNCLIPPED

Draws the outline of a polygon in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 coordsData - pointer to an array of numPoints pairs of integers;
              x0, y0, x1, y1...
 numPoints - the number of vertices in the polygon
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is unclipped for speed, but if any pixels fall outside the
 display there will be adverse effects.
 Polygons are closed automatically - the first point is drawn connected to
 the last point.

*/
void vfdlib_drawOutlinePolygonUnclipped(char *buffer, int *coordsData,
					int numPoints, char shade) {

  int x0, y0, x1, y1;
  int lastIndex = (numPoints-1) << 1; 
  int currIndex;
  int prevX = coordsData[lastIndex];
  int prevY = coordsData[lastIndex+1];

  for (currIndex = 0; currIndex<=lastIndex;) {
    x0 = prevX;
    y0 = prevY;
    prevX = x1 = coordsData[currIndex++];
    prevY = y1 = coordsData[currIndex++];
    vfdlib_drawLineUnclipped(buffer, x0, y0, x1, y1, shade);
  }
}

/*
DRAWSOLIDPOLYGON

Draws a (clipped) solid polygon in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 coordsData - pointer to an array of numPoints pairs of integers;
              x0, y0, x1, y1...
 numPoints - the number of vertices in the polygon
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.
 Polygons are closed automatically - the first point is drawn connected to
 the last point.

*/
void vfdlib_drawSolidPolygon(char *buffer, int *coordsData, int numPoints,
			     char shade) {

  struct sETentry {
    int ymax;
    int x;
    int invm_numerator;
    int invm_denominator;
    int increment;
    struct sETentry *next;
  };
  typedef struct sETentry ETentry;
  static ETentry *edgeTable[VFD_HEIGHT];
  int i, y, x0, y0, x1, y1, tx, ty, sorted;
  int lastIndex = (numPoints-1) << 1; 
  int currIndex;
  int prevX = coordsData[lastIndex];
  int prevY = coordsData[lastIndex+1];
  int polyMinY = g_clipYBottom;
  int polyMaxY = g_clipYTop;
  ETentry *prev, *curr, *next, *newEdge, *dead, *AEtable;
	
  /* sanity check */
  if (numPoints < 3) return;

  /* clear the edge table */
  for (i=0; i<VFD_HEIGHT; i++) edgeTable[i] = NULL;

  /* add edges to edge table */
  for (currIndex = 0; currIndex<=lastIndex;) {

    /* get endpoints of edge */
    x0 = prevX;
    y0 = prevY;
    prevX = x1 = coordsData[currIndex++];
    prevY = y1 = coordsData[currIndex++];

    /* add a new entry to the edge table, clipping if necessary */
    if (y0 == y1) continue; /* skip horizontal spans */
    
    /* create new entry for table */
    newEdge = (ETentry *) malloc(sizeof(ETentry));
    if (y0 > y1) {
      /* swap points so first coord is topmost */
      tx = x0; ty = y0;
      x0 = x1; y0 = y1;
      x1 = tx; y1 = ty;
    }
    
    /* do clipping - only needed for top parts of lines */
    if (y1 < g_clipYTop || y0 >= g_clipYBottom) continue;

    newEdge->invm_numerator = x1 - x0;
    newEdge->invm_denominator = y1 - y0;
    
    if (y0 < g_clipYTop) {
      ty = g_clipYTop - y0;
      /* jump start by ty */
      if (newEdge->invm_numerator < 0) {
	newEdge->increment =
	  (ty * -newEdge->invm_numerator) % newEdge->invm_denominator;
	newEdge->x = x0 -
	   (ty * -newEdge->invm_numerator) / newEdge->invm_denominator;
      }
      else { 
	newEdge->increment =
	   (ty * newEdge->invm_numerator) % newEdge->invm_denominator;
	newEdge->x = x0 +
	   (ty * newEdge->invm_numerator) / newEdge->invm_denominator;
      }
      y0 = g_clipYTop;
    }
    else {
      newEdge->increment = 0;
      newEdge->x = x0;
    }
    newEdge->ymax = y1;
    /* add entry to head of linked list */
    newEdge->next = edgeTable[y0];
    edgeTable[y0] = newEdge;
    
    /* update polyMinY & polyMaxY */
    if (y0 < polyMinY) polyMinY = y0;
    if (y1 > polyMaxY) polyMaxY = y1;
  }

  AEtable = NULL;
  if (polyMaxY > g_clipYBottom-1) polyMaxY = g_clipYBottom-1;

  /* scan conversion loop */
  for (y = polyMinY; y <= polyMaxY; y++) {
    /* add new edges to active edge table */
    newEdge = edgeTable[y];
    while (newEdge != NULL) {
      ETentry *nextNewEdge = newEdge->next;
      /* insert new edge into AET */
      prev = NULL;
      curr = AEtable;
      /* find x-sorted insertion point */
      while (curr != NULL && curr->x < newEdge->x) {
	prev = curr;
	curr = curr->next;
      }
      if (prev == NULL) {
	/* add to head of list */
	AEtable = newEdge;
      } else {
	prev->next = newEdge;
      }
      newEdge->next = curr;
      newEdge = nextNewEdge;
    }
    
    /* delete ETentrys as they expire */
    prev = NULL;
    curr = AEtable;
    while (curr != NULL) {
      if (curr->ymax == y) {
	if (prev == NULL) {
	  /* delete from head */
	  AEtable = curr->next;
	  dead = curr;
	  prev = NULL;
	  curr = curr->next;
	  free(dead);
	} else {
	  dead = curr;
	  prev->next = curr->next;
	  curr = curr->next;
	  free(dead);
	}
      } else {
	prev = curr;
	curr = curr->next;
      }
    }
    
    /* draw scanlines */
    curr = AEtable;
    while (curr != NULL) {
      if (curr->next != NULL) {
	/* draw scissored scanline */
	tx = curr->x > g_clipXLeft ? curr->x : g_clipXLeft;
	vfdlib_drawLineHorizUnclipped(buffer, y,
				      tx,
				      (curr->next->x < g_clipXRight-1 ?
				       curr->next->x : g_clipXRight-1) -
				      tx + 1,
				      shade);
	curr = curr->next->next;
      } else curr = NULL;
    }
    
    /* update x values */
    curr = AEtable;
    while (curr != NULL) {
      if (curr->invm_numerator == 0) ; /* vertical - do nothing */
      else if (curr->invm_numerator > 0) {
	if (curr->invm_numerator < curr->invm_denominator) {
	  /* high slope right */
	  curr->increment += curr->invm_numerator;
	  if (curr->increment >= curr->invm_denominator) {
	    curr->x++;
	    curr->increment -= curr->invm_denominator;
	  }
	} else {
	  /* low slope right */
	  curr->increment += curr->invm_numerator;
	  tx = curr->increment / curr->invm_denominator;
	  curr->x += tx;
	  curr->increment -= curr->invm_denominator * tx;
	}
      } else {
	if (-curr->invm_numerator < curr->invm_denominator) {
	  /* high slope left */
	  curr->increment -= curr->invm_numerator;
	  if (curr->increment >= curr->invm_denominator) {
	    curr->x--;
	    curr->increment -= curr->invm_denominator;
	  }
	} else {
	  /* low slope left */
	  curr->increment -= curr->invm_numerator;
	  tx = curr->increment / curr->invm_denominator;
	  curr->x -= tx;
	  curr->increment -= curr->invm_denominator * tx;
	}
      }
      curr = curr->next;
    }
    
    /* bubble sort active edge table */
    sorted = 0;
    while (!sorted) {
      sorted = 1;
      prev = NULL;
      curr = AEtable;
      while (curr != NULL) {
	if (curr->next != NULL && curr->x > curr->next->x) {
	  /* found unsorted */
	  sorted = 0;
	  /* swap the entries */
	  if (prev == NULL) {
	    /* beginning of list */
	    prev = curr->next;
	    next = curr->next->next;
	    AEtable = curr->next;
	    curr->next->next = curr;
	    curr->next = next;
	  } else {
	    ETentry *newPrev = curr->next;
	    next = curr->next->next;
	    prev->next = curr->next;
	    curr->next->next = curr;
	    curr->next = next;
	    prev = newPrev;
	  }
	} else {
	  prev = curr;
	  curr = curr->next;
	}
      }
    }
  }
  
  /* delete any edges left in active edge table */
  curr = AEtable;
  while (curr != NULL) {
    dead = curr;
    curr = curr->next;
    free(dead);
  }
}

/*
DRAWTEXT

Draws a (clipped) string of text in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 string - the string of text to display
 xLeft - the x left-hand position of the string
 yTop - the y topmost position of the string
 font - the built-in font to use; VFDFONT_TINY, VFDFONT_SMALL or
        VFDFONT_MEDIUM 
 shade - the shade to draw with, either VFDSHADE_BLACK, VFDSHADE_DIM,
         VFDSHADE_MEDIUM or VFDSHADE_BRIGHT

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.
 Each font has characters in the ASCII range '!' (33) - '~' (126) inclusive.
 0: VFDFONT_TINY (capitals only, some symbols missing - 3 pixels high)
 1: VFDFONT_SMALL (4 pixels high)
 2: VFDFONT_MEDIUM (5 pixels high)
 Use the get functions to determine the pixel size required for a string.
*/
void vfdlib_drawText(char *buffer, char *string, int xLeft, int yTop,
		     int font, char shade) {

  FontInfo *fInfo;
  unsigned char *fBitmap;

  getFontInfo(font, &fInfo, &fBitmap);

  while (*string != '\0' && xLeft < g_clipXRight) {
    if (*string >= '!' && *string <= '~') {
      FontInfo ci = fInfo[*string-'!'];
      if (ci.width > 0) {
	vfdlib_drawBitmap(buffer, fBitmap, xLeft, yTop, ci.offset, 0,
			  ci.width, -1, shade, 1);
      }
      xLeft += ci.width + 1;
    } else {
      xLeft++;
    }
    string++;
  }
}

/*
GETTEXTHEIGHT

Gets the pixel height of a string.

Parameters:
 font - the built-in font to be used; VFDFONT_TINY, VFDFONT_SMALL or
        VFDFONT_MEDIUM 

*/
int vfdlib_getTextHeight(int font) {

  FontInfo *fInfo;
  unsigned char *fBitmap;

  getFontInfo(font, &fInfo, &fBitmap);

  return ((int) fBitmap[4] | ((int) fBitmap[3] << 8));
}

/*
GETTEXTWIDTH

Gets the pixel width of a string.

Parameters:
 string - the string of text to be displayed
 font - the built-in font to be used; VFDFONT_TINY, VFDFONT_SMALL or
        VFDFONT_MEDIUM 

*/
int vfdlib_getTextWidth(char *string, int font) {

  int width = 0;
  FontInfo *fInfo;
  unsigned char *fBitmap;

  getFontInfo(font, &fInfo, &fBitmap);

  while (*string != '\0') {
    if (*string >= '!' && *string <= '~') {
      FontInfo ci = fInfo[*string-'!'];
      width += ci.width + 1;
    } else {
      width++;
    }
    string++;
  }

  return width;
}

/*
DRAWBITMAP

Draws a (clipped) bitmap in the display buffer.

Parameters:
 buffer - pointer to the display buffer
 bitmap - pointer to the bitmap to display
 destX - the left hand side x-coordinate of the destination in the display
 destY - the top y-coordinate of the destination in the display
 sourceX - the left hand side x-coordinate of the source in the bitmap
 sourceY - the top y-coordinate of the source in the bitmap
 width - the width of the area to copy, if <1 then the entire width of the
         bitmap will be copied
 height - the height of the area to copy, if <1 then the entire height of the
          bitmap will be copied 
 shade - the shade to override with, if <0 then the original shades of the
         bitmap will be used
 isTransparent - if nonzero:
                 pixels of shade 0 will be treated as transparent
                 - unless the bitmap is 4BPP in which case the extra
                 transparency information that is encoded will be obeyed.

Notes:
 This method is clipped so that any pixels falling outside the clip area will
 not be affected.
 See the header file (vfdlib.h) for a description of the bitmap formats.

*/
void vfdlib_drawBitmap(char *buffer, unsigned char *bitmap,
		       int destX, int destY,
		       int sourceX, int sourceY,
		       int width, int height,
		       signed char shade, int isTransparent) {

  int bitmapWidth = (int) bitmap[2] | ((int) bitmap[1] << 8);
  int bitmapHeight = (int) bitmap[4] | ((int) bitmap[3] << 8);

  /* clip LHS */
  if (width < 1) width = bitmapWidth; 
  if (destX < g_clipXLeft) { 
    int diff = g_clipXLeft - destX;
    sourceX += diff;
    width -= diff;
    destX = g_clipXLeft;
  }

  /* clip top */
  if (height < 1) height = bitmapHeight;
  if (destY < g_clipYTop) {
    int diff = g_clipYTop - destY;
    sourceY += diff;
    height -= diff;
    destY = g_clipYTop;
  }

  /* clip RHS */
  if (width > bitmapWidth) width = bitmapWidth;
  if (destX + width > g_clipXRight) width = g_clipXRight - destX;

  /* clip bottom */
  if (height > bitmapHeight) height = bitmapHeight;
  if (destY + height > g_clipYBottom) height = g_clipYBottom - destY;

  if (width > 0 && height > 0) {
    switch (bitmap[0]) {
    case BITMAP_1BPP:
      drawBitmap1BPP(buffer, (char *)bitmap+5, bitmapWidth, bitmapHeight, destX, destY,
		     sourceX, sourceY, width, height, shade, isTransparent);
      break;
    case BITMAP_2BPP:
      drawBitmap2BPP(buffer, (char *)bitmap+5, bitmapWidth, bitmapHeight, destX, destY,
		     sourceX, sourceY, width, height, shade, isTransparent);
      break;
    case BITMAP_4BPP:
      drawBitmap4BPP(buffer, (char *)bitmap+5, bitmapWidth, bitmapHeight, destX, destY,
		     sourceX, sourceY, width, height, shade, isTransparent);
      break;
    }
  }
}


/* private utility functions */

static void getFontInfo(int font, FontInfo **fInfo, unsigned char **fBitmap) {

  switch (font) {
  case VFDFONT_TINY:
    *fInfo = tinyFontInfo;
    *fBitmap = tinyFontBitmap;
    break;
  case VFDFONT_SMALL:
    *fInfo = smallFontInfo;
    *fBitmap = smallFontBitmap;
    break;
  default: /* VFDFONT_MEDIUM */
    *fInfo = mediumFontInfo;
    *fBitmap = mediumFontBitmap;
    break;
  }
} 

static void drawPixel4WaySymmetricClipped(char *buffer, int cX, int cY,
					  int x, int y, char shade) {

  vfdlib_drawPointClipped(buffer, cX+x,cY+y, shade);
  vfdlib_drawPointClipped(buffer, cX+x,cY-y, shade);
  vfdlib_drawPointClipped(buffer, cX-x,cY+y, shade);
  vfdlib_drawPointClipped(buffer, cX-x,cY-y, shade);
}

static void drawPixel4WaySymmetricUnclipped(char *buffer, int cX, int cY,
					    int x, int y, char shade) {

  vfdlib_drawPointUnclipped(buffer, cX+x,cY+y, shade);
  vfdlib_drawPointUnclipped(buffer, cX+x,cY-y, shade);
  vfdlib_drawPointUnclipped(buffer, cX-x,cY+y, shade);
  vfdlib_drawPointUnclipped(buffer, cX-x,cY-y, shade);
}

static void drawLineHMaxClipped(char *buffer, int x0, int y0, int dx, int dy,
				int d, int xMax, int yMax, char shade) {

  int incrH, incrHV, addV;
  int pixelPos = x0 & 0x1;
  int loPixel = 0;
  int hiPixel = 0;
  char hiShade = shade << 4;
  char biShade = shade | hiShade;

  if (dy > 0) addV = VFD_BYTES_PER_SCANLINE;
  else {
    addV = -VFD_BYTES_PER_SCANLINE;
    dy = -dy;
  }

  incrH = 2 * dy;
  incrHV = 2 * (dy - dx);

  buffer += (y0 << 6) + (x0 >> 1);

  /* write initial pixel */
  if (pixelPos == 0) loPixel = 1;
  else hiPixel = 1;

  while (xMax-- > 0) {
    if (d <= 0) {
      d += incrH;
      pixelPos++;
      if (pixelPos == 2) {
	if (loPixel && hiPixel) {
	  *buffer = biShade;
	} else if (loPixel) {
	  *buffer &= MASK_HI_NYBBLE;
	  *buffer |= shade;
	} else { /* hiPixel */
	  *buffer &= MASK_LO_NYBBLE;
	  *buffer |= hiShade;
	}
	buffer++;
	pixelPos = 0;
	hiPixel = 0;
	loPixel = 1;
      } else { /* pixelPos == 1 */
	hiPixel = 1;
      }
    } else {
      d += incrHV;
      
      /* draw outstanding pixels */
      if (loPixel && hiPixel) {
	*buffer = biShade;
      } else if (loPixel) {
	*buffer &= MASK_HI_NYBBLE;
	*buffer |= shade;
      } else { /* hiPixel */
	*buffer &= MASK_LO_NYBBLE;
	*buffer |= hiShade;
      }
      
      /* h++ */
      pixelPos++;
      if (pixelPos == 2) {
	buffer++;
	pixelPos = 0;
	loPixel = 1;
	hiPixel = 0;
      } else { /* pixelPos == 1 */
	loPixel = 0;
	hiPixel = 1;
      }

      /* v++ */
      if (!yMax--) return;
      buffer += addV;
    }
  }
  
  /* draw outstanding pixels */
  if (loPixel && hiPixel) {
    *buffer = biShade;
  } else if (loPixel) {
    *buffer &= MASK_HI_NYBBLE;
    *buffer |= shade;
  } else { /* hiPixel */
    *buffer &= MASK_LO_NYBBLE;
    *buffer |= hiShade;
  }
}

static void drawLineVMaxClipped(char *buffer, int x0, int y0, int dx, int dy,
				int d, int xMax, int yMax, char shade) {

  int incrV, incrHV, addV;
  int pixelPos = x0 & 0x1;
  char hiShade = shade << 4;

  if (dy > 0) addV = VFD_BYTES_PER_SCANLINE;
  else {
    addV = -VFD_BYTES_PER_SCANLINE;
    dy = -dy;
  }

  incrV = 2 * dx;
  incrHV = 2 * (dx - dy);
  buffer += (y0 << 6) + (x0 >> 1);

  /* write initial pixel */
  if (pixelPos == 0) {
    *buffer &= MASK_HI_NYBBLE;
    *buffer |= shade;
  } else {
    *buffer &= MASK_LO_NYBBLE;
    *buffer |= hiShade;
  }

  while (yMax-- > 0) {
    if (d <= 0) {
      d += incrV;
    } else {
      d += incrHV;   

      /* h++ */
      if (!xMax--) return;
      if (++pixelPos > 1) {
	buffer++;
	pixelPos = 0;
      }
    }

    /* v++ */
    buffer += addV;

    /* write pixel */
    if (pixelPos == 0) {
      *buffer &= MASK_HI_NYBBLE;
      *buffer |= shade;
    } else {
      *buffer &= MASK_LO_NYBBLE;
      *buffer |= hiShade;
    }
  }
}

static void drawLineHMaxUnclipped(char *buffer, int x0, int y0,
				  int dx, int dy, char shade) {

  int d, incrH, incrHV, addV;
  int pixelPos = x0 & 0x1;
  int loPixel = 0;
  int hiPixel = 0;
  char hiShade = shade << 4;
  char biShade = shade | hiShade;

  if (dy > 0) addV = VFD_BYTES_PER_SCANLINE;
  else {
    addV = -VFD_BYTES_PER_SCANLINE;
    dy = -dy;
  }

  d = 2 * dy - dx;
  incrH = 2 * dy;
  incrHV = 2 * (dy - dx);

  buffer += (y0 << 6) + (x0 >> 1);

  /* write initial pixel */
  if (pixelPos == 0) loPixel = 1;
  else hiPixel = 1;

  while (dx-- > 0)
    if (d <= 0) {
      d += incrH;
      pixelPos++;
      if (pixelPos == 2) {
	if (loPixel && hiPixel) {
	  *buffer = biShade;
	} else if (loPixel) {
	  *buffer &= MASK_HI_NYBBLE;
	  *buffer |= shade;
	} else { /* hiPixel */
	  *buffer &= MASK_LO_NYBBLE;
	  *buffer |= hiShade;
	}
	buffer++;
	pixelPos = 0;
	hiPixel = 0;
	loPixel = 1;
      } else { /* pixelPos == 1 */
	hiPixel = 1;
      }
    } else {
      d += incrHV;
      
      /* draw outstanding pixels */
      if (loPixel && hiPixel) {
	*buffer = biShade;
      } else if (loPixel) {
	*buffer &= MASK_HI_NYBBLE;
	*buffer |= shade;
      } else { /* hiPixel */
	*buffer &= MASK_LO_NYBBLE;
	*buffer |= hiShade;
      }
      
      /* h++ */
      pixelPos++;
      if (pixelPos == 2) {
	buffer++;
	pixelPos = 0;
	loPixel = 1;
	hiPixel = 0;
      } else { /* pixelPos == 1 */
	loPixel = 0;
	hiPixel = 1;
      }

      /* v++ */
      buffer += addV;
    }
  
  /* draw outstanding pixels */
  if (loPixel && hiPixel) {
    *buffer = biShade;
  } else if (loPixel) {
    *buffer &= MASK_HI_NYBBLE;
    *buffer |= shade;
  } else { /* hiPixel */
    *buffer &= MASK_LO_NYBBLE;
    *buffer |= hiShade;
  }
}

static void drawLineVMaxUnclipped(char *buffer, int x0, int y0,
				  int dx, int dy, char shade) {

  int d, incrV, incrHV, addV;
  int pixelPos = x0 & 0x1;
  char hiShade = shade << 4;

  if (dy > 0) addV = VFD_BYTES_PER_SCANLINE;
  else {
    addV = -VFD_BYTES_PER_SCANLINE;
    dy = -dy;
  }
  
  d = 2 * dx - dy;
  incrV = 2 * dx;
  incrHV = 2 * (dx - dy);
  buffer += (y0 << 6) + (x0 >> 1);

  /* write initial pixel */
  if (pixelPos == 0) {
    *buffer &= MASK_HI_NYBBLE;
    *buffer |= shade;
  } else {
    *buffer &= MASK_LO_NYBBLE;
    *buffer |= hiShade;
  }

  while (dy-- > 0) {
    if (d <= 0) {
      d += incrV;
    } else {
      d += incrHV;   

      /* h++ */
      if (++pixelPos > 1) {
	buffer++;
	pixelPos = 0;
      }
    }

    /* v++ */
    buffer += addV;

    /* write pixel */
    if (pixelPos == 0) {
      *buffer &= MASK_HI_NYBBLE;
      *buffer |= shade;
    } else {
      *buffer &= MASK_LO_NYBBLE;
      *buffer |= hiShade;
    }
  }
}

static void drawBitmap1BPP(char *buffer, char *bitmap,
			   int bitmapWidth, int bitmapHeight,
			   int destX, int destY,
			   int sourceX, int sourceY,
			   int width, int height,
			   signed char shade, int isTransparent) {

  int startPos, widthLeft;
  char *startBitmap, *startBuffer;
  unsigned char startMask;
  int bitmapScanlineSkip = ((bitmapWidth-1) >> 3) + 1;
  unsigned char bitmask = 0x80 >> (sourceX & 0x07);
  int pixelPos = destX & 0x1;
  char hiShade;
  if (shade < 0) shade = VFDSHADE_BRIGHT;
  hiShade = shade << 4;
  bitmap += (sourceY * bitmapScanlineSkip) + (sourceX >> 3);
  buffer += (destY << 6) + (destX >> 1);
  
  startMask = bitmask;
  startPos = pixelPos;
  startBitmap = bitmap;
  startBuffer = buffer;

  while (height-- > 0) {
    widthLeft = width;
    while (widthLeft-- > 0) {

      /* set pixel */
      if (*bitmap & bitmask) {
	if (pixelPos == 0) {
	  *buffer &= MASK_HI_NYBBLE;
	  *buffer |= shade;
	} else {
	  *buffer &= MASK_LO_NYBBLE;
	  *buffer |= hiShade;
	}
      } else if (!isTransparent) {
	if (pixelPos == 0) {
	  *buffer &= MASK_HI_NYBBLE;
	} else {
	  *buffer &= MASK_LO_NYBBLE;
	}
      }
      
      /* sourceX++ */
      bitmask >>= 1;
      if (bitmask == 0) {
	bitmask = 0x80;
	bitmap++;
      }
      /* destX++ */
      if (++pixelPos > 1) {
	buffer++;
	pixelPos = 0;
      }      
    }
    /* sourceY++ */
    bitmask = startMask;
    startBitmap += bitmapScanlineSkip;
    bitmap = startBitmap;
    /* destY++ */
    pixelPos = startPos;
    startBuffer += VFD_BYTES_PER_SCANLINE;
    buffer = startBuffer;
  }
}

static void drawBitmap2BPP(char *buffer, char *bitmap,
			   int bitmapWidth, int bitmapHeight,
			   int destX, int destY,
			   int sourceX, int sourceY,
			   int width, int height,
			   signed char shade, int isTransparent) {

  int startPos, startShift, widthLeft;
  char currentBitmap, currentShade;
  char *startBitmap, *startBuffer;
  int bitmapScanlineSkip = ((bitmapWidth-1) >> 2) + 1;
  int bitmapShift = (sourceX & 0x03) << 1;
  int pixelPos = destX & 0x1;
  bitmap += (sourceY * bitmapScanlineSkip) + (sourceX >> 2);
  buffer += (destY << 6) + (destX >> 1);
  
  startShift = bitmapShift;
  startPos = pixelPos;
  startBitmap = bitmap;
  startBuffer = buffer;

  while (height-- > 0) {
    widthLeft = width;
    currentBitmap = *bitmap >> bitmapShift;
    while (widthLeft-- > 0) {

      currentShade = currentBitmap & 0x03;

      if (currentShade != 0 || !isTransparent) {
	 /* set pixel */
	if (shade >= 0) currentShade = shade;
	if (pixelPos == 0) {
	  *buffer &= MASK_HI_NYBBLE;
	  *buffer |= currentShade;
	} else {
	  *buffer &= MASK_LO_NYBBLE;
	  *buffer |= currentShade << 4;
	}
      }
      
      /* sourceX++ */
      bitmapShift += 2;
      if (bitmapShift > 6) {
	bitmapShift = 0;
	currentBitmap = *(++bitmap);
      } else currentBitmap >>= 2;

      /* destX++ */
      if (++pixelPos > 1) {
	buffer++;
	pixelPos = 0;
      }      
    }
    /* sourceY++ */
    bitmapShift = startShift;
    startBitmap += bitmapScanlineSkip;
    bitmap = startBitmap;
    /* destY++ */
    pixelPos = startPos;
    startBuffer += VFD_BYTES_PER_SCANLINE;
    buffer = startBuffer;
  }
}

static void drawBitmap4BPP(char *buffer, char *bitmap,
			   int bitmapWidth, int bitmapHeight,
			   int destX, int destY,
			   int sourceX, int sourceY,
			   int width, int height,
			   signed char shade, int isTransparent) {

  int startSourcePos, startDestPos, widthLeft;
  char currentShade, currentTrans;
  char *startBitmap, *startBuffer;
  int bitmapScanlineSkip = ((bitmapWidth-1) >> 1) + 1;
  int pixelSourcePos = sourceX & 0x1;
  int pixelDestPos = destX & 0x1;
  bitmap += (sourceY * bitmapScanlineSkip) + (sourceX >> 1);
  buffer += (destY << 6) + (destX >> 1);
  
  startSourcePos = pixelSourcePos;
  startDestPos = pixelDestPos;
  startBitmap = bitmap;
  startBuffer = buffer;

  if (isTransparent) {
    while (height-- > 0) {
      widthLeft = width;
      while (widthLeft-- > 0) {
	
	if (pixelSourcePos == 0) {
	  currentShade = *bitmap & 0x03;
	  currentTrans = *bitmap & 0x0C;
	}
	else {
	  currentShade = (*bitmap >> 4) & 0x03;
	  currentTrans = *bitmap & 0xC0;
	}
	
	if (!currentTrans) {
	  /* set pixel */
	  if (shade >= 0) currentShade = shade;
	  if (pixelDestPos == 0) {
	    *buffer &= MASK_HI_NYBBLE;
	    *buffer |= currentShade;
	  } else {
	    *buffer &= MASK_LO_NYBBLE;
	    *buffer |= currentShade << 4;
	  }
	}
	
	/* sourceX++ */
	if (++pixelSourcePos > 1) {
	  bitmap++;
	  pixelSourcePos = 0;
	}   
	/* destX++ */
	if (++pixelDestPos > 1) {
	  buffer++;
	  pixelDestPos = 0;
	}      
      }
      /* sourceY++ */
      pixelSourcePos = startSourcePos;;
      startBitmap += bitmapScanlineSkip;
      bitmap = startBitmap;
      /* destY++ */
      pixelDestPos = startDestPos;
      startBuffer += VFD_BYTES_PER_SCANLINE;
      buffer = startBuffer;
    }
  } else {
    while (height-- > 0) {
      widthLeft = width;
      while (widthLeft-- > 0) {
	
	if (pixelSourcePos == 0) currentShade = *bitmap & 0x03;
	else currentShade = (*bitmap >> 4) & 0x03;
	
	/* set pixel */
	if (pixelDestPos == 0) {
	  *buffer &= MASK_HI_NYBBLE;
	  *buffer |= currentShade;
	} else {
	  *buffer &= MASK_LO_NYBBLE;
	  *buffer |= currentShade << 4;
	}
      
	/* sourceX++ */
	if (++pixelSourcePos > 1) {
	  bitmap++;
	  pixelSourcePos = 0;
	}   
	/* destX++ */
	if (++pixelDestPos > 1) {
	  buffer++;
	  pixelDestPos = 0;
	}      
      }
      /* sourceY++ */
      pixelSourcePos = startSourcePos;;
      startBitmap += bitmapScanlineSkip;
      bitmap = startBitmap;
      /* destY++ */
      pixelDestPos = startDestPos;
      startBuffer += VFD_BYTES_PER_SCANLINE;
      buffer = startBuffer;
    }
  }
}
