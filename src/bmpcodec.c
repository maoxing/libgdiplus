/*
 * Copyright (c) 2004 Ximian
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
 * NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * bmpcodec.c : Contains function definitions for encoding decoding bmp images
 *
 * Authors:
 *      Jordi Mas i Hernandez (jordi@ximian.com)
 *      Sanjay Gupta (gsanjay@novell.com)
 *      Mark Steele (ms@rapidsys.com)
 *
 * Useful documentation about bitmaps
 *
 *      http://msdn.microsoft.com/library/default.asp?url=/library/en-us/gdi/bitmaps_4v1h.asp
 *      http://www.csdn.net/Dev/Format/windows/Bmp.html
 *      http://www.fortunecity.com/skyscraper/windows/364/bmpffrmt.html
 *
 *      Header structure
 *              BITMAPFILEHEADER
 *              BITMAPINFOHEADER or BITMAPV4HEADER or BITMAPV5HEADER or BITMAPCOREHEADER
 *              RGBQUADS or RGBTRIPLE (optional)
 *              Bitmap data
 */

#include <stdio.h>
#include "bmpcodec.h"
#include "gdip.h"


/* Codecinfo related data*/
static ImageCodecInfo bmp_codec;
static const WCHAR bmp_codecname[] = {'B', 'u', 'i','l', 't', '-','i', 'n', ' ', 'B', 'M', 'P', 0}; /* Built-in BMP */
static const WCHAR bmp_extension[] = {'*','.','B', 'M', 'P',';', '*','.', 'D','I', 'B',';', '*','.', 'R', 'L', 'E',0}; /* *.BMP;*.DIB;*.RLE */
static const WCHAR bmp_mimetype[] = {'i', 'm', 'a','g', 'e', '/', 'b', 'm', 'p', 0}; /* image/bmp */
static const WCHAR bmp_format[] = {'B', 'M', 'P', 0}; /* BMP */


ImageCodecInfo *
gdip_getcodecinfo_bmp ()
{
        bmp_codec.Clsid = (CLSID) { 0x557cf400, 0x1a04, 0x11d3, { 0x9a, 0x73, 0x0, 0x0, 0xf8, 0x1e, 0xf3, 0x2e } };
        bmp_codec.FormatID = gdip_bmp_image_format_guid;
        bmp_codec.CodecName = (const WCHAR*) bmp_codecname;            
        bmp_codec.DllName = NULL;
        bmp_codec.FormatDescription = (const WCHAR*) bmp_format;
        bmp_codec.FilenameExtension = (const WCHAR*) bmp_extension;
        bmp_codec.MimeType = (const WCHAR*) bmp_mimetype;
        bmp_codec.Flags = Encoder | Decoder | SupportBitmap | Builtin;
        bmp_codec.Version = 1;
        bmp_codec.SigCount = 0;
        bmp_codec.SigSize = 0;
        bmp_codec.SigPattern = 0;
        bmp_codec.SigMask = 0;

        return &bmp_codec; 
}

int
gdip_get_pixelformat (WORD bitcount)
{
        switch (bitcount) {
        case 32:
                return Format32bppArgb;
        case 24:
                return Format24bppRgb;
        case 8:
                return Format8bppIndexed;
        case 4:
                return Format4bppIndexed;
        default:
                return 0;
        }            
}

void 
gdip_bitmap_fill_info_header (GpBitmap *bitmap, PBITMAPINFOHEADER bmi)
{
	int  bitmapLen = bitmap->data.Stride * bitmap->data.Height;
	memset (bmi, 0, sizeof (BITMAPINFOHEADER));
#ifdef WORDS_BIGENDIAN
	bmi->biSize = GUINT32_FROM_LE (sizeof (BITMAPINFOHEADER));
	bmi->biWidth = GULONG_FROM_LE (bitmap->data.Width);
	bmi->biHeight = GULONG_FROM_LE (bitmap->data.Height);
	bmi->biPlanes = GUINT16_FROM_LE (1);
	bmi->biBitCount = GUINT16_FROM_LE (32); /*PIXEL_FORMAT_BPP (bitmap->image.pixFormat); */
	bmi->biCompression = GUINT32_FROM_LE (BI_RGB);
	bmi->biSizeImage =  GUINT32_FROM_LE (0); /* Many tools expect this may be set to zero for BI_RGB bitmaps */
	bmi->biXPelsPerMeter = GULONG_FROM_LE ((int) (0.5f + ((gdip_get_display_dpi() * 3937) / 100)));
	bmi->biYPelsPerMeter = GULONG_FROM_LE ((int) (0.5f + ((gdip_get_display_dpi() * 3937) / 100))); /* 1 meter is = 39.37 */       
#else
	bmi->biSize = sizeof (BITMAPINFOHEADER);
	bmi->biWidth = bitmap->data.Width;
	bmi->biHeight = bitmap->data.Height;
	bmi->biPlanes = 1;
	bmi->biBitCount = 32; /*PIXEL_FORMAT_BPP (bitmap->image.pixFormat); */
	bmi->biCompression = BI_RGB;
	bmi->biSizeImage =  0; /* Many tools expect this may be set to zero for BI_RGB bitmaps */
	bmi->biXPelsPerMeter = (int) (0.5f + ((gdip_get_display_dpi() * 3937) / 100));
	bmi->biYPelsPerMeter = (int) (0.5f + ((gdip_get_display_dpi() * 3937) / 100)); /* 1 meter is = 39.37 */       
#endif
}                                                           

void 
gdip_bitmap_save_bmp (const char *name, GpBitmap *bitmap)
{
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER bmi;
	int  bitmapLen = bitmap->data.Stride * bitmap->data.Height;
	FILE *fp;
	
	bmfh.bfReserved1 = bmfh.bfReserved2 = 0;
	bmfh.bfType = BFT_BITMAP;
	bmfh.bfOffBits = (14 + 40 + 0 * 4);
	bmfh.bfSize = (bmfh.bfOffBits + bitmapLen);
	fp = fopen (name, "w+b");
	fwrite (&bmfh, sizeof (bmfh), 1, fp);
	gdip_bitmap_fill_info_header (bitmap, &bmi);
	bmi.biHeight = -bmi.biHeight;
	fwrite (&bmi, sizeof (bmi), 1, fp);
	fwrite (bitmap->data.Scan0, bitmapLen, 1, fp);
	fclose (fp);
}

GpStatus 
gdip_load_bmp_image_from_file (FILE *fp, GpImage **image)
{
        return gdip_read_bmp_image_from_file_stream ( (void*)fp,
							image, TRUE);
}

GpStatus 
gdip_load_bmp_image_from_stream_delegate (GetBytesDelegate getBytesFunc,
                                          SeekDelegate seeknFunc,
                                          GpImage **image)
{
	return gdip_read_bmp_image_from_file_stream ( (void*)getBytesFunc,
							image, FALSE);
}

#define palette_lookup(x)	img->image.palette->Entries[(x)]

GpStatus 
gdip_read_bmp_image_from_file_stream (void *pointer, GpImage **image, bool useFile)
{
	BITMAPFILEHEADER bmfh;
        BITMAPINFOHEADER bmi;
        GpBitmap	*img = NULL;
        guchar		*pixels = NULL;
        guchar		*linep = NULL;
        int		i;
	int		j;
	int		format;
	int		colours;
        BOOL		os2format = FALSE;
	BOOL		upsidedown = TRUE;
        int		size;
	int		size_read;
	byte		*data_read;
	int		line;
	int		loop;
	long		pixel;
	long		index;
		
	size = sizeof(bmfh);
	data_read = (byte*) GdipAlloc(size);
	memset (data_read, 0, size);
	size_read = gdip_read_bmp_data (pointer, data_read, size, useFile);
	if (size_read < size) {
		GdipFree(data_read);
		return InvalidParameter;
	}
	
	bmfh.bfType = ((data_read[1]<<8)|data_read[0]);
	if (bmfh.bfType != BFT_BITMAP) {
		GdipFree(data_read);
		return UnknownImageFormat;
	}

	bmfh.bfSize = (data_read[5]<<24 | data_read[4]<<16 | data_read[3]<<8 | data_read[2]);
	bmfh.bfReserved1 = ((data_read[7]<<8)|data_read[6]);
	bmfh.bfReserved1 = ((data_read[9]<<8)|data_read[8]);
	bmfh.bfOffBits = (data_read[13]<<24 | data_read[12]<<16 | data_read[11]<<8 | data_read[10]);
	GdipFree(data_read);

	size = sizeof(DWORD);
	data_read = (byte*)GdipAlloc(size);
	size_read = gdip_read_bmp_data (pointer, data_read, size, useFile);

	if (size_read < size) {
		GdipFree(data_read);
		return InvalidParameter;
	}

	bmi.biSize = (data_read[3]<<24 | data_read[2]<<16 | data_read[1]<<8 | data_read[0]);

	if (bmi.biSize > BITMAPCOREHEADER_SIZE){   /* New Windows headers can be bigger */ 
		memset (data_read, 0, size);
		size_read = gdip_read_bmp_data (pointer, data_read, size, useFile);
		if (size_read < size) {
			GdipFree(data_read);
			return InvalidParameter;
		}
		bmi.biWidth = (data_read[3]<<24 | data_read[2]<<16 | data_read[1]<<8 | data_read[0]);

		memset (data_read, 0, size);
		size_read = gdip_read_bmp_data (pointer, data_read, size, useFile);
		if (size_read < size) {
			GdipFree(data_read);
			return InvalidParameter;
		}
		bmi.biHeight = (data_read[3]<<24 | data_read[2]<<16 | data_read[1]<<8 | data_read[0]);
 	}
        else  {
		if (bmi.biSize ==  BITMAPCOREHEADER_SIZE) {
			/* Old OS/2 format. Width and Height fields are WORDs instead of DWORDS */
                        memset (data_read, 0, size);
			size_read = gdip_read_bmp_data (pointer, data_read, size, useFile);
			if (size_read < size) {
				GdipFree(data_read);
				return InvalidParameter;
			}
			bmi.biWidth = (data_read[1]<<8 | data_read[0]);
			bmi.biHeight = (data_read[3]<<8 | data_read[2]);
                        os2format = TRUE;
                }
                else {
			GdipFree(data_read);
                        return UnknownImageFormat;
		}
        }

	memset (data_read, 0, size);
	size_read = gdip_read_bmp_data (pointer, data_read, size, useFile);
	if (size_read < size) {
		GdipFree(data_read);
		return InvalidParameter;
	}
	bmi.biPlanes = (data_read[1]<<8 | data_read[0]); 
	bmi.biBitCount = (data_read[3]<<8 | data_read[2]); 

	memset (data_read, 0, size);
	size_read = gdip_read_bmp_data (pointer, data_read, size, useFile);
	if (size_read < size) {
		GdipFree(data_read);
		return InvalidParameter;
	}
	bmi.biCompression = (data_read[3]<<24 | data_read[2]<<16 | data_read[1]<<8 | data_read[0]);
		
        if (bmi.biCompression == BI_RLE4 || bmi.biCompression == BI_RLE8) {
		GdipFree(data_read);
                return NotImplemented; /* We do not support RLE for now*/
	}

        if (bmi.biHeight < 0) { /* Negative height indicates that the bitmap is sideup*/
                upsidedown = FALSE;
                bmi.biHeight =  -bmi.biHeight;
        }

	memset (data_read, 0, size);
	size_read = gdip_read_bmp_data (pointer, data_read, size, useFile);
	if (size_read < size) {
		GdipFree(data_read);
		return InvalidParameter;
	}
	bmi.biSizeImage = (data_read[3]<<24 | data_read[2]<<16 | data_read[1]<<8 | data_read[0]);
		
	memset (data_read, 0, size);
	size_read = gdip_read_bmp_data (pointer, data_read, size, useFile);
	if (size_read < size) {
		GdipFree(data_read);
		return InvalidParameter;
	}
	bmi.biXPelsPerMeter = (data_read[3]<<24 | data_read[2]<<16 | data_read[1]<<8 | data_read[0]);
	
	memset (data_read, 0, size);
	size_read = gdip_read_bmp_data (pointer, data_read, size, useFile);
	if (size_read < size) {
		GdipFree(data_read);
		return InvalidParameter;
	}
	bmi.biYPelsPerMeter = (data_read[3]<<24 | data_read[2]<<16 | data_read[1]<<8 | data_read[0]);
		
	memset (data_read, 0, size);
	size_read = gdip_read_bmp_data (pointer, data_read, size, useFile);
	if (size_read < size) {
		GdipFree(data_read);
		return InvalidParameter;
	}
	bmi.biClrUsed = (data_read[3]<<24 | data_read[2]<<16 | data_read[1]<<8 | data_read[0]);
		
	memset (data_read, 0, size);
	size_read = gdip_read_bmp_data (pointer, data_read, size, useFile);
	if (size_read < size) {
		GdipFree(data_read);
		return InvalidParameter;
	}

	bmi.biClrImportant = (data_read[3]<<24 | data_read[2]<<16 | data_read[1]<<8 | data_read[0]);
		
	colours =  (bmi.biClrUsed == 0 && bmi.biBitCount <= 8) ? (1 << bmi.biBitCount) : bmi.biClrUsed;
        
        format = gdip_get_pixelformat (bmi.biBitCount);
                                               
        img = gdip_bitmap_new ();
        img->image.type = imageBitmap;
        img->image.width = bmi.biWidth;
        img->image.height = bmi.biHeight;

        img->image.pixFormat = img->data.PixelFormat = Format32bppArgb;
        img->data.Width = img->image.width;
        img->data.Height = img->image.height;

	// We always assume 32 bit and translate into 32 bit from source format
        img->data.Stride = (32 * img->image.width) / 8;
        img->data.Stride = (img->data.Stride + 3) & ~3;

	GdipFree(data_read);
     
        if (colours) {
                img->image.palette = GdipAlloc (sizeof(ColorPalette) + sizeof(ARGB) * colours);
                img->image.palette->Flags = 0;
                img->image.palette->Count = colours;
                       
                /* Read optional colour table*/
                if (os2format) {  /* RGBTRIPLE */
			size = sizeof(byte)*3;
			data_read = (byte*) GdipAlloc(size);
                        for (i = 0; i < colours; i++) {
				memset(data_read, 0, size);
				size_read = gdip_read_bmp_data (pointer, data_read, size, useFile);
				if (size_read < size) {
					GdipFree(data_read);
					return InvalidParameter;
				}
				img->image.palette->Entries[i] = 
					(((data_read[0]&0xff)<<16) | 	// R
					((data_read[1]&0xff)<<8) | 	// G
					(data_read[2]&0xff));		// B
                        }
			GdipFree(data_read);
                }
                else { /* RGBSquads */
			size = sizeof(byte)*4;
			data_read = (byte*) GdipAlloc(size);
                        for (i = 0; i < colours; i++) {
                                size_read = gdip_read_bmp_data (pointer, data_read, size, useFile);
				if (size_read < size) {
					return InvalidParameter;
				}

                                img->image.palette->Entries[i] = 
					(((data_read[0]&0xff)<<16)  | 	// R
					((data_read[1]&0xff)<<8) | 	// G
					((data_read[2]&0xff)) | 	// B
					((data_read[3]& 0xff)<<24));	// Alpha
                        }
			GdipFree(data_read);
                }
        } else {
                img->image.palette = NULL;
	}
       
        pixels = GdipAlloc (img->data.Stride * img->data.Height);

	// Size contains the size of the lines on disk
	switch (bmi.biBitCount) {
		case 1: {
			size = ((img->image.width + 31) & ~31) / 8;
			loop = img->image.width / 8;
			// we assume 0s for 1 bit, only setting 1s
			memset(pixels, 0, img->data.Stride * img->data.Height);
			break;
		}

		case 4: {
			size = ((bmi.biBitCount * img->image.width + 31) & ~31) / 8;
			loop = ((img->image.width + 1) &~1) / 2;
			break;
		}

		case 8: {
			size = (((bmi.biBitCount * img->image.width) + 31) & ~31) / 8;
			loop = img->image.width;
			break;
		}

		case 24: {
			size = (((bmi.biBitCount * img->image.width) + 31) & ~31) / 8;
			loop = img->image.width * 3;
			break;
		}

		default: {
			size = (((bmi.biBitCount * img->image.width) + 31) & ~31) / 8;
			loop = size; 
			break;
		}
	}

	data_read = (byte*) GdipAlloc(size);

	for (i = 0; i < img->data.Height; i++){ 
		if (upsidedown) {
			line = img->data.Height - i - 1;
		} else {
			line = i;
		}

		size_read = gdip_read_bmp_data (pointer, data_read, size, useFile);

		if (size_read < size) {
			GdipFree(data_read);
			return InvalidParameter;
		}

		switch(bmi.biBitCount) {
			case 1: {
				int	c;
				int	bit;

				for (c = 0; c < loop; c++) {
					for (bit = 0; bit < 8; bit++) {
						index = (line * img->data.Stride) + (c*8 + bit) * 4;

						if (((data_read[c] << bit) & 0x80) != 0) {
							set_pixel_bgra(pixels, index, 0xff, 0xff, 0xff, 0xff);
						} else {
							set_pixel_bgra(pixels, index, 0x00, 0x00, 0x00, 0xff);
						}
					}
				}

				for (bit = 0; bit < img->image.width % 8; bit++) {
					index = (line * img->data.Stride) + (c*8 + bit) * 4;

					if (((data_read[c] << bit) & 0x80) != 0) {
						set_pixel_bgra(pixels, index, 0xff, 0xff, 0xff, 0xff);
					} else {
						set_pixel_bgra(pixels, index, 0x00, 0x00, 0x00, 0xff);
					}
				}
				continue;
			}

			case 4: {
				int c;

				for (c = 0; c < loop; c++) {
					pixel = palette_lookup((data_read[c] & 0xf0) >> 4);

					index = (line * img->data.Stride) + c*8;

					set_pixel_bgra(pixels, index, (pixel & 0xff0000) >> 16, (pixel & 0xff00) >> 8, pixel & 0xff, 0xff);

					pixel = palette_lookup(data_read[c] & 0xf);

					set_pixel_bgra(pixels, index+4, (pixel & 0xff0000) >> 16, (pixel & 0xff00) >> 8, pixel & 0xff, 0xff);
				}
				continue;
			}

			case 8: {
				int	c;

				for (c = 0; c < loop; c++) {
					pixel = palette_lookup(data_read[c]);

					index = (line * img->data.Stride) + c*4;

					set_pixel_bgra(pixels, index, (pixel & 0xff0000) >> 16, (pixel & 0xff00) >> 8, pixel & 0xff, 0xff);
				}
				continue;
			}

			case 24: {
				int	src;
				int	dest;

				src = 0;
				dest = 0;

				while (src < loop) {
					index = (line * img->data.Stride);
					set_pixel_bgra(pixels, index+dest, data_read[src+0], data_read[src+1], data_read[src+2], 0xff);
					dest += 4;
					src += 3;
				}
				continue;
			}

			case 32: {
				int	src;
				int	dest;

				src = 0;
				dest = 0;

				while (src < loop) {
					index = (line * img->data.Stride);
					set_pixel_bgra(pixels, index+dest, data_read[src+0], data_read[src+1], data_read[src+2], data_read[src+3]);
					dest += 4;
					src += 4;
				}
				continue;
			}
		}
	}

	GdipFree(data_read);

	if (img->image.palette) {
		GdipFree(img->image.palette);
		img->image.palette = NULL;
	}

	img->data.Scan0 = pixels;
        img->data.Reserved = GBD_OWN_SCAN0;
        img->image.surface = cairo_surface_create_for_image ((char *)pixels,
                                                         img->cairo_format,
                                                         img->image.width,
                                                         img->image.height,
                                                         img->data.Stride);
        img->image.imageFlags =
                ImageFlagsReadOnly |
                ImageFlagsHasRealPixelSize |
                ImageFlagsColorSpaceRGB;
        img->image.horizontalResolution = 0;
        img->image.verticalResolution = 0;
        img->image.propItems = NULL;

        *image = (GpImage *) img;
        return Ok;
}

int 
gdip_read_bmp_data (void *pointer, byte *data, int size, bool useFile)
{
	if (useFile) {
		return fread (data, 1, size, (FILE*) pointer);
	} else {
		// Streams are not required to return the number of bytes
		// requested, they could return less yet our code seems to assume
		// it will always get what it's asking for; lets loop until we
		// get what was requested or we get an error
		int got;
		int total;

		total = 0;
		
		do {
			got = ((GetBytesDelegate)(pointer))(data + total, size - total, 0);
			if (got < 1) {	// 0 = end of stream, -1 = error
				return total;
			}
			total += got;
		} while (total < size);

		return total;
	}
}

GpStatus 
gdip_save_bmp_image_to_file (FILE *fp, GpImage *image)
{
	return gdip_save_bmp_image_to_file_stream ( (void *)fp, image, TRUE);
}

GpStatus 
gdip_save_bmp_image_to_stream_delegate (PutBytesDelegate putBytesFunc,
                                        GpImage *image)
{	
        return gdip_save_bmp_image_to_file_stream ( (void *)putBytesFunc, image, FALSE);
}

GpStatus 
gdip_save_bmp_image_to_file_stream (void *pointer,
                                        GpImage *image,
					bool useFile)
{
	BITMAPFILEHEADER bmfh;
        BITMAPINFOHEADER bmi;
        GpBitmap *bitmap = (GpBitmap *) image;
        int bitmapLen = bitmap->data.Stride * bitmap->data.Height;
        int i;
        byte b;
        ARGB color;
        int colours = 0;

        if (bitmap->image.palette)
                colours = bitmap->image.palette->Count;

#ifdef WORDS_BIGENDIAN
	bmfh.bfReserved1 = bmfh.bfReserved2 = GUINT16_FROM_LE (0);
	bmfh.bfType = GUINT16_FROM_LE (BFT_BITMAP);
	bmfh.bfOffBits = GUINT32_FROM_LE (14 + 40 + colours * 4);
	bmfh.bfSize = GUINT32_FROM_LE (bmfh.bfOffBits + bitmapLen);        
#else
        bmfh.bfReserved1 = bmfh.bfReserved2 = 0;
        bmfh.bfType = BFT_BITMAP;
        bmfh.bfOffBits = (14 + 40 + colours * 4);
        bmfh.bfSize = (bmfh.bfOffBits + bitmapLen);

#endif
        gdip_write_bmp_data (pointer, (byte *)&bmfh, sizeof (bmfh), useFile);
        
	gdip_bitmap_fill_info_header (bitmap, &bmi);
	gdip_write_bmp_data (pointer, (byte *)&bmi, sizeof (bmi), useFile);

        if (colours) {

                /* Write palette on disk on BGRA*/
                for (i = 0; bitmap->image.palette->Count; i++) {
                        color = bitmap->image.palette->Entries[i];
#ifdef WORDS_BIGENDIAN
                        b = color >> 24;
			gdip_write_bmp_data (pointer, &b, 1, useFile);
                        b = (color >> 16) & 0xff;
			gdip_write_bmp_data (pointer, &b, 1, useFile);
                        b = (color >> 8) & 0xff;
			gdip_write_bmp_data (pointer, &b, 1, useFile);
                        b =  color & 0xff;
			gdip_write_bmp_data (pointer, &b, 1, useFile);
#else
                        b =  color & 0xff;
			gdip_write_bmp_data (pointer, &b, 1, useFile);
                        b = (color >> 8) & 0xff;
			gdip_write_bmp_data (pointer, &b, 1, useFile);
                        b = (color >> 16) & 0xff;
			gdip_write_bmp_data (pointer, &b, 1, useFile);
                        b = color >> 24;
			gdip_write_bmp_data (pointer, &b, 1, useFile);
#endif
                }	
        }
        
        /* Writes bitmap upside down. Many tools can only process bmp stored this way*/        
#ifdef WORDS_BIGENDIAN
	{
		int j;
		guchar *row_pointer = GdipAlloc (image->width * 4);
		
		for (i = image->height -1; i >= 0; i--) {
			for (j = 0; j < image->width; j++) {
				row_pointer[j*4] = *((guchar *)bitmap->data.Scan0 + (bitmap->data.Stride * i) + (j*4) + 3); 
				row_pointer[j*4+1] = *((guchar *)bitmap->data.Scan0 + (bitmap->data.Stride * i) + (j*4) + 2); 
				row_pointer[j*4+2] = *((guchar *)bitmap->data.Scan0 + (bitmap->data.Stride * i) + (j*4) + 1); 
				row_pointer[j*4+3] = *((guchar *)bitmap->data.Scan0 + (bitmap->data.Stride * i) + (j*4) + 0); 
			}
			gdip_write_bmp_data (pointer, row_pointer, bitmap->data.Stride, useFile);
		}
		GdipFree (row_pointer);
	}
#else
	
        for (i = bitmap->data.Height - 1; i >= 0; i--) {
		gdip_write_bmp_data (pointer, bitmap->data.Scan0 + i *bitmap->data.Stride, bitmap->data.Stride, useFile);
	}
#endif
        return Ok;
}

void 
gdip_write_bmp_data (void *pointer, byte *data, int size, bool useFile)
{
	if (useFile)
		fwrite (data, 1, size, (FILE*) pointer);
	else 	
		((PutBytesDelegate)(pointer))(data, size);	
}


