//==============================================================================
//  spriteLoader.h   -   NEW FILE
//
//  Shadow Chase: The Final Escape
//
//  The box artwork (LifeBox.jpg, foodbox.jpg, speedbox.jpg, explosionbox.jpg)
//  is stored as JPEG, and JPEG cannot store transparency. Every one of those
//  pictures is drawn on a solid white sheet, so drawing them directly would
//  put a white square on top of the dark game background.
//
//  This file loads such a picture and makes the white sheet see-through
//  BEFORE the texture is handed to OpenGL. The files on disk are never
//  modified - the change happens in memory only.
//
//  Why a flood fill instead of "delete every white pixel"?
//  ------------------------------------------------------
//  speedbox.jpg has a white box lid and explosionbox.jpg has a white hot
//  centre. Deleting every white pixel would punch holes straight through the
//  artwork. So we start from the border of the picture and only spread through
//  white pixels that are connected to that border. Interior white is never
//  reached, so it survives.
//
//  Include this AFTER iGraphics.h (it uses stb_image, which iGraphics.h sets up).
//==============================================================================

#ifndef SPRITE_LOADER_H
#define SPRITE_LOADER_H

// A picture that has been uploaded to the graphics card, plus its pixel size.
// The size lets us draw the picture without squashing it.
typedef struct
{
	unsigned int tex;   // OpenGL texture name
	int          w;     // width  in pixels
	int          h;     // height in pixels
} Sprite;

// A pixel counts as "background" when all three colour channels are this
// bright or brighter. JPEG compression makes the white slightly dirty, so we
// allow a little tolerance instead of testing for exactly 255.
#define SPRITE_WHITE_TOLERANCE 233

// ---------------------------------------------------------------------------
//  Two ways of recognising a background pixel.
//
//  BRIGHT - the pixel is pale on every channel. This covers plain white sheets
//           and the light grey / white transparency chequerboard that some of
//           the Level 02 artwork was exported with.
//
//  GREY   - the pixel is almost colourless (R, G and B nearly equal) and its
//           brightness falls inside a band. This is what removes the grey
//           studio backdrop behind the Level 02 woods, cactus and knife box,
//           where a brightness test alone would also eat the object.
// ---------------------------------------------------------------------------
#define SPRITE_KEY_BRIGHT 0
#define SPRITE_KEY_GREY   1

static int spriteIsBackgroundPixel(const unsigned char *px, int idx,
                                   int mode, int p1, int p2, int p3)
{
	int r = px[idx * 4 + 0];
	int g = px[idx * 4 + 1];
	int b = px[idx * 4 + 2];

	if (mode == SPRITE_KEY_GREY)
	{
		int dRG = (r > g) ? (r - g) : (g - r);
		int dGB = (g > b) ? (g - b) : (b - g);
		int dRB = (r > b) ? (r - b) : (b - r);
		int lum;

		if (dRG > p1 || dGB > p1 || dRB > p1)
			return 0;                        // too colourful to be the backdrop

		lum = (r + g + b) / 3;
		return (lum >= p2 && lum <= p3);
	}

	/* SPRITE_KEY_BRIGHT */
	return (r >= p1 && g >= p1 && b >= p1);
}

//------------------------------------------------------------------------------
// Turns the white sheet around the artwork transparent.
//
// px      - RGBA pixel buffer (4 bytes per pixel)
// w, h    - size of that buffer
//
// Classic flood fill: put every border pixel that is white on a stack, then
// keep popping pixels, making them transparent, and pushing their white
// neighbours. When the stack is empty every white pixel that could be reached
// from the border has been cleared.
//------------------------------------------------------------------------------
static void spriteKeyOutBackground(unsigned char *px, int w, int h,
                                   int mode, int p1, int p2, int p3)
{
	int  total = w * h;
	int *stack;
	unsigned char *done;
	int  top = 0;
	int  i, x, y;

	stack = (int *)malloc(sizeof(int) * total);
	done  = (unsigned char *)calloc(total, 1);

	if (stack == NULL || done == NULL)
	{
		free(stack);
		free(done);
		return;
	}

	// Is pixel i part of the backdrop? The rule depends on the mode.
	#define SPRITE_IS_WHITE(idx) spriteIsBackgroundPixel(px, (idx), mode, p1, p2, p3)

	// --- seed the stack with the four borders ---------------------------------
	for (x = 0; x < w; x++)
	{
		i = x;                       // top row
		if (!done[i] && SPRITE_IS_WHITE(i)) { done[i] = 1; stack[top++] = i; }

		i = (h - 1) * w + x;         // bottom row
		if (!done[i] && SPRITE_IS_WHITE(i)) { done[i] = 1; stack[top++] = i; }
	}
	for (y = 0; y < h; y++)
	{
		i = y * w;                   // left column
		if (!done[i] && SPRITE_IS_WHITE(i)) { done[i] = 1; stack[top++] = i; }

		i = y * w + (w - 1);         // right column
		if (!done[i] && SPRITE_IS_WHITE(i)) { done[i] = 1; stack[top++] = i; }
	}

	// --- spread inwards through connected white pixels -------------------------
	while (top > 0)
	{
		int cur = stack[--top];
		int cx  = cur % w;
		int cy  = cur / w;

		px[cur * 4 + 3] = 0;         // alpha 0 = fully see-through

		if (cx > 0)
		{
			i = cur - 1;
			if (!done[i] && SPRITE_IS_WHITE(i)) { done[i] = 1; stack[top++] = i; }
		}
		if (cx < w - 1)
		{
			i = cur + 1;
			if (!done[i] && SPRITE_IS_WHITE(i)) { done[i] = 1; stack[top++] = i; }
		}
		if (cy > 0)
		{
			i = cur - w;
			if (!done[i] && SPRITE_IS_WHITE(i)) { done[i] = 1; stack[top++] = i; }
		}
		if (cy < h - 1)
		{
			i = cur + w;
			if (!done[i] && SPRITE_IS_WHITE(i)) { done[i] = 1; stack[top++] = i; }
		}
	}

	#undef SPRITE_IS_WHITE

	free(stack);
	free(done);
}

//------------------------------------------------------------------------------
// Uploads an RGBA buffer to the graphics card and returns the texture name.
//------------------------------------------------------------------------------
static unsigned int spriteMakeTexture(unsigned char *px, int w, int h)
{
	unsigned int tex = 0;

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	// GL_CLAMP (not GL_CLAMP_TO_EDGE) because the Windows SDK 7.1A header this
	// project builds against only exposes OpenGL 1.1.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, px);

	return tex;
}

//------------------------------------------------------------------------------
// Loads a picture exactly as it is on disk (used for the backgrounds and the
// poster, which already look correct and need no transparency work).
//------------------------------------------------------------------------------
static Sprite loadSpritePlain(char *filename)
{
	Sprite s;
	int w, h, bpp;
	unsigned char *px;

	s.tex = 0; s.w = 0; s.h = 0;

	px = stbi_load(filename, &w, &h, &bpp, 4);
	if (px == NULL)
	{
		printf("SPRITE: could not load %s\n", filename);
		fflush(stdout);
		return s;
	}

	s.tex = spriteMakeTexture(px, w, h);
	s.w   = w;
	s.h   = h;

	stbi_image_free(px);
	return s;
}

//------------------------------------------------------------------------------
// Loads a picture, removes the white sheet around the artwork, then trims the
// leftover empty margin.
//
// The trim matters: speedbox.jpg is a 980x980 file where the drawing only
// occupies the middle. Without trimming, the box would look far smaller than
// the others when every pickup is drawn at the same size on screen.
//------------------------------------------------------------------------------
static Sprite loadSpriteKeyedEx(char *filename, int mode, int p1, int p2, int p3)
{
	Sprite s;
	int w, h, bpp;
	unsigned char *px;
	unsigned char *cropped;
	int minX, minY, maxX, maxY;
	int x, y, cw, ch;

	s.tex = 0; s.w = 0; s.h = 0;

	px = stbi_load(filename, &w, &h, &bpp, 4);
	if (px == NULL)
	{
		printf("SPRITE: could not load %s\n", filename);
		fflush(stdout);
		return s;
	}

	// 1. make the surrounding backdrop see-through
	spriteKeyOutBackground(px, w, h, mode, p1, p2, p3);

	// 2. find the smallest rectangle that still holds visible pixels
	minX = w; minY = h; maxX = -1; maxY = -1;

	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w; x++)
		{
			if (px[(y * w + x) * 4 + 3] > 0)
			{
				if (x < minX) minX = x;
				if (x > maxX) maxX = x;
				if (y < minY) minY = y;
				if (y > maxY) maxY = y;
			}
		}
	}

	// Nothing survived (should not happen) - fall back to the whole picture.
	if (maxX < minX || maxY < minY)
	{
		s.tex = spriteMakeTexture(px, w, h);
		s.w = w; s.h = h;
		stbi_image_free(px);
		return s;
	}

	// 3. copy that rectangle into a smaller buffer
	cw = maxX - minX + 1;
	ch = maxY - minY + 1;

	cropped = (unsigned char *)malloc(cw * ch * 4);
	if (cropped == NULL)
	{
		s.tex = spriteMakeTexture(px, w, h);
		s.w = w; s.h = h;
		stbi_image_free(px);
		return s;
	}

	for (y = 0; y < ch; y++)
	{
		memcpy(cropped + (y * cw) * 4,
		       px + ((minY + y) * w + minX) * 4,
		       cw * 4);
	}

	s.tex = spriteMakeTexture(cropped, cw, ch);
	s.w   = cw;
	s.h   = ch;

	free(cropped);
	stbi_image_free(px);

	printf("SPRITE: %s  %dx%d -> trimmed to %dx%d\n", filename, w, h, cw, ch);
	fflush(stdout);

	return s;
}

//------------------------------------------------------------------------------
// The three ways the game actually loads a keyed sprite.
//------------------------------------------------------------------------------

// Level 01's original loader. Plain white sheet behind the artwork.
static Sprite loadSpriteKeyed(char *filename)
{
	return loadSpriteKeyedEx(filename, SPRITE_KEY_BRIGHT,
	                         SPRITE_WHITE_TOLERANCE, 0, 0);
}

// Anything pale: a white sheet, or the light grey / white transparency
// chequerboard the Level 02 gems and magnet were exported with. A lower cut
// catches a darker chequer.
static Sprite loadSpriteBright(char *filename, int cut)
{
	return loadSpriteKeyedEx(filename, SPRITE_KEY_BRIGHT, cut, 0, 0);
}

// A colourless studio backdrop, as used behind the Level 02 woods, cactus and
// knife box. satTol is how far R, G and B may differ before the pixel counts
// as coloured (and so part of the object); lo/hi bound the grey's brightness.
static Sprite loadSpriteGrey(char *filename, int satTol, int lo, int hi)
{
	return loadSpriteKeyedEx(filename, SPRITE_KEY_GREY, satTol, lo, hi);
}

//------------------------------------------------------------------------------
// Draws a sprite so that its longest side becomes boxSize pixels, centred on
// (cx, cy). Keeping the original width/height ratio stops the pickups from
// looking stretched.
//------------------------------------------------------------------------------
static void drawSpriteFit(Sprite s, double cx, double cy, double boxSize)
{
	double dw, dh, scale;

	if (s.tex == 0 || s.w <= 0 || s.h <= 0)
		return;

	scale = (s.w >= s.h) ? (boxSize / s.w) : (boxSize / s.h);

	dw = s.w * scale;
	dh = s.h * scale;

	iShowImage((int)(cx - dw / 2.0), (int)(cy - dh / 2.0),
	           (int)dw, (int)dh, s.tex);
}

#endif // SPRITE_LOADER_H
