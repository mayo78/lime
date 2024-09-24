#define WUFFS_IMPLEMENTATION

extern "C" {

	#include <png.h>
	#include <pngstruct.h>
	#define PNG_SIG_SIZE 8
}

#define MAX_PNG_SIZE (64 * 1024 * 1024)  // 64 MB
#define MAX_DIMENSION 4096

#include "wuffs-v0.3.c"
#include <stdio.h> // For printf
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <setjmp.h>
#include <graphics/format/PNG.h>
#include <graphics/ImageBuffer.h>
#include <system/System.h>
#include <utils/Bytes.h>
#include <utils/QuickVec.h>


namespace lime {


	struct ReadBuffer {

		ReadBuffer (const unsigned char* data, int length) : data (data), length (length), position (0) {}

		bool Read (unsigned char* out, int count) {

			if (position >= length) return false;

			if (count > length - position) {

				memcpy (out, data + position, length - position);
				position = length;

			} else {

				memcpy (out, data + position, count);
				position += count;

			}

			return true;

		}

		char unused; // the first byte gets corrupted when passed to libpng?
		const unsigned char* data;
		int length;
		int position;

	};

	static void user_error_fn (png_structp png_ptr, png_const_charp error_msg) {

			longjmp (png_ptr->jmp_buf_local, 1);

		}


	static void user_read_data_fn (png_structp png_ptr, png_bytep data, png_size_t length) {

		ReadBuffer* buffer = (ReadBuffer*)png_get_io_ptr (png_ptr);
		if (!buffer->Read (data, length)) {
			png_error (png_ptr, "Read Error");
		}

	}

	static void user_warning_fn (png_structp png_ptr, png_const_charp warning_msg) {}


	void user_write_data (png_structp png_ptr, png_bytep data, png_size_t length) {

		QuickVec<unsigned char> *buffer = (QuickVec<unsigned char> *)png_get_io_ptr (png_ptr);
		buffer->append ((unsigned char *)data,(int)length);

	}

	void user_flush_data (png_structp png_ptr) {}

	bool ReadInput(Resource* resource, wuffs_base__io_buffer* io_buffer) {
    size_t data_size = 0;
    uint8_t* data = nullptr;

    if (resource->path) {
        FILE_HANDLE* file = lime::fopen(resource->path, "rb");
        if (!file) {
            return false;
        }

        // Assuming lime::fseek, lime::ftell, and lime::fread exist
        lime::fseek(file, 0, SEEK_END);
        data_size = lime::ftell(file);
        lime::fseek(file, 0, SEEK_SET);

        if (data_size > MAX_PNG_SIZE) {
            lime::fclose(file);
            return false;
        }

        data = (uint8_t*)malloc(data_size);
        if (!data) {
            lime::fclose(file);
            return false;
        }

        if (lime::fread(data, 1, data_size, file) != data_size) {
            free(data);
            lime::fclose(file);
            return false;
        }

        lime::fclose(file);
    } else if (resource->data && resource->data->length > 0) {
        data_size = resource->data->length;
        if (data_size > MAX_PNG_SIZE) {
            return false;
        }

        data = (uint8_t*)malloc(data_size);
        if (!data) {
            return false;
        }

        memcpy(data, resource->data->b, data_size);
    } else {
        return false;
    }

    io_buffer->data.ptr = data;
    io_buffer->data.len = data_size;
    io_buffer->meta.wi = data_size;
    io_buffer->meta.ri = 0;
    io_buffer->meta.pos = 0;
    io_buffer->meta.closed = true;

    return true;
	}

	bool AllocateBuffers(wuffs_png__decoder* decoder,
                     wuffs_base__image_config* ic,
                     uint8_t** pixel_buffer,
                     uint8_t** workbuf,
                     size_t* workbuf_len) {
    uint32_t width = wuffs_base__pixel_config__width(&ic->pixcfg);
    uint32_t height = wuffs_base__pixel_config__height(&ic->pixcfg);

    if (width > MAX_DIMENSION || height > MAX_DIMENSION) {
        return false;
    }

    size_t pixel_buffer_size = width * height * 4;  // 4 bytes per pixel for BGRA_PREMUL
    *pixel_buffer = (uint8_t*)malloc(pixel_buffer_size);
    if (!*pixel_buffer) {
        return false;
    }

    wuffs_base__range_ii_u64 workbuf_len_range = wuffs_png__decoder__workbuf_len(decoder);
    *workbuf_len = workbuf_len_range.max_incl;
    *workbuf = (uint8_t*)malloc(*workbuf_len);
    if (!*workbuf) {
        free(*pixel_buffer);
        *pixel_buffer = nullptr;
        return false;
    }

    return true;
	}

	bool PNG::Decode(Resource* resource, ImageBuffer* imageBuffer, bool decodeData) {
    wuffs_png__decoder decoder;
    wuffs_base__image_config ic;
    wuffs_base__io_buffer io_buffer = {0};
    uint8_t* pixel_buffer = nullptr;
    uint8_t* workbuf = nullptr;
    size_t workbuf_len = 0;
    bool success = false;


    // Initialize the decoder
    wuffs_base__status status = wuffs_png__decoder__initialize(&decoder, sizeof(decoder), WUFFS_VERSION, 0);
    if (status.repr != NULL) {
        return false;
    }

    // Read input data
    if (!ReadInput(resource, &io_buffer)) {
        return false;
    }

    // Decode the image configuration
    status = wuffs_png__decoder__decode_image_config(&decoder, &ic, &io_buffer);
    if (status.repr != NULL) {
        goto cleanup;
    }

    // Set pixel format explicitly
    wuffs_base__pixel_config__set(
        &ic.pixcfg, WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL,
        WUFFS_BASE__PIXEL_SUBSAMPLING__NONE,
        wuffs_base__pixel_config__width(&ic.pixcfg),
        wuffs_base__pixel_config__height(&ic.pixcfg));

    // Allocate buffers
    if (!AllocateBuffers(&decoder, &ic, &pixel_buffer, &workbuf, &workbuf_len)) {
        goto cleanup;
    }

    // Update ImageBuffer dimensions
    imageBuffer->width = wuffs_base__pixel_config__width(&ic.pixcfg);
    imageBuffer->height = wuffs_base__pixel_config__height(&ic.pixcfg);

    if (decodeData) {
        wuffs_base__pixel_buffer pb = {0};
        status = wuffs_base__pixel_buffer__set_from_slice(
            &pb, &ic.pixcfg,
            wuffs_base__make_slice_u8(pixel_buffer, imageBuffer->width * imageBuffer->height * 4));

        if (status.repr != NULL) {
            printf("Failed to set up pixel buffer: %s\n", status.repr);
            goto cleanup;
        }

        status = wuffs_png__decoder__decode_frame(
            &decoder, &pb, &io_buffer,
            WUFFS_BASE__PIXEL_BLEND__SRC,
            wuffs_base__make_slice_u8(workbuf, workbuf_len),
            NULL);

        if (status.repr != NULL) {
            printf("Failed to decode frame: %s\n", status.repr);
            goto cleanup;
        }

        // Resize and copy decoded data to imageBuffer
        imageBuffer->Resize(imageBuffer->width, imageBuffer->height, 32);
        memcpy(imageBuffer->data->buffer->b, pixel_buffer, imageBuffer->width * imageBuffer->height * 4);
    }

    success = true;

		cleanup:
				free(workbuf);
				free(pixel_buffer);
				free(io_buffer.data.ptr);

				return success;
		}

	bool PNG::Encode (ImageBuffer *imageBuffer, Bytes* bytes) {

		png_structp png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, user_error_fn, user_warning_fn);

		if (!png_ptr) {

			return false;

		}

		png_infop info_ptr = png_create_info_struct (png_ptr);

		if (!info_ptr) {

			return false;

		}

		if (setjmp (png_jmpbuf (png_ptr))) {

			png_destroy_write_struct (&png_ptr, &info_ptr);
			return false;

		}

		QuickVec<unsigned char> out_buffer;

		png_set_write_fn (png_ptr, &out_buffer, user_write_data, user_flush_data);

		int w = imageBuffer->width;
		int h = imageBuffer->height;

		int bit_depth = 8;
		//int color_type = (inSurface->Format () & pfHasAlpha) ? PNG_COLOR_TYPE_RGB_ALPHA : PNG_COLOR_TYPE_RGB;
		int color_type = PNG_COLOR_TYPE_RGB_ALPHA;
		png_set_IHDR (png_ptr, info_ptr, w, h, bit_depth, color_type, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

		png_write_info (png_ptr, info_ptr);

		bool do_alpha = (color_type == PNG_COLOR_TYPE_RGBA);
		unsigned char* imageData = imageBuffer->data->buffer->b;
		int stride = imageBuffer->Stride ();

		{
			QuickVec<unsigned char> row_data (w * 4);
			png_bytep row = &row_data[0];

			for (int y = 0; y < h; y++) {

				unsigned char *buf = &row_data[0];
				const unsigned char *src = (const unsigned char *)(imageData + (stride * y));

				for (int x = 0; x < w; x++) {

					buf[0] = src[0];
					buf[1] = src[1];
					buf[2] = src[2];
					src += 3;
					buf += 3;

					if (do_alpha) {

						*buf++ = *src;

					}

					src++;

				}

				png_write_rows (png_ptr, &row, 1);

			}

		}

		png_write_end (png_ptr, NULL);

		int size = out_buffer.size ();

		if (size > 0) {

			bytes->Resize (size);
			memcpy (bytes->b, &out_buffer[0], size);

		}

		return true;

	}


}
