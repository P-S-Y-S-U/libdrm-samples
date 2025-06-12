#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <cstdlib>
#include <cstring>
#include <poll.h>

struct dumb_fb {
    uint32_t id;
    uint32_t width; uint32_t height; uint32_t stride;
    uint32_t handle; uint64_t size;

    uint8_t* data;
};

struct presenter
{
    uint32_t conn_id;
    bool connected;

    uint32_t crtc_id;
    drmModeCrtcPtr previousCrtc;

    drmModeModeInfo mode;

    uint32_t width; uint32_t height;
    uint32_t refresh;

    dumb_fb framebuffers[2];
    dumb_fb* front;
    dumb_fb* back;

    presenter* next;
};

void drawFrame( presenter* output )
{
    uint8_t color[4] = { 0x00, 0x00, 0xff, 0x00 };
    int inc = 1, dec = 2;
    
    for (int i = 0; i < 60 * 5; ++i) {
		color[inc] += 15;
		color[dec] -= 15;

		if (color[dec] == 0) {
			dec = inc;
			inc = (inc + 2) % 3;
		}
		
		dumb_fb *fb = output->back;

		for (uint32_t y = 0; y < fb->height; ++y) {
			uint8_t *row = fb->data + fb->stride * y;

			for (uint32_t x = 0; x < fb->width; ++x) {
				row[x * 4 + 0] = color[0];
				row[x * 4 + 1] = color[1];
				row[x * 4 + 2] = color[2];
				row[x * 4 + 3] = color[3];
			}
		}

		// Should give 60 FPS
		//timespec ts = { .tv_nsec = 16666667 };
		//nanosleep(&ts, NULL);
	}
}

void page_flip_handler(int fd,
				  unsigned int sequence,
				  unsigned int tv_sec,
				  unsigned int tv_usec,
				  void *user_data)
{
    presenter* output = reinterpret_cast<presenter*>(user_data);
    drawFrame(output);

    if( drmModePageFlip(fd, output->crtc_id, output->back->id, DRM_MODE_PAGE_FLIP_EVENT, output) < 0 ) {
        perror("drmModePageFlip");
    }
    
    dumb_fb* tmp = output->back;
    output->back = output->front;
    output->front = tmp;
}

uint32_t find_crtc( int drm_fd, drmModeResPtr resources, drmModeConnectorPtr connector, uint32_t& taken_crtcs )
{

    for( int i = 0; i < resources->count_encoders; i++ )
    {
        drmModeEncoderPtr encoder = drmModeGetEncoder( drm_fd, connector->encoders[i] );
        if( !encoder )
            continue;
        
        for( int j = 0; j < resources->count_crtcs; j++ )
        {
            uint32_t bit = 1 << i;

            if( ( encoder->possible_crtcs & bit ) == 0 ) 
                continue;

            
            if( taken_crtcs & bit )
                continue;
            
            drmModeFreeEncoder(encoder);
            taken_crtcs |= bit;
            return resources->crtcs[i];
        }

        drmModeFreeEncoder(encoder);
    }

    return 0;
}

bool create_fb( const int& drm_fd, dumb_fb* framebuffer, const uint32_t& width, const uint32_t& height, const uint32_t& bpp = 32 ) {
    int result;

    auto handle_fb_error = [&drm_fd](dumb_fb* fb) {
        drmModeRmFB(drm_fd, fb->id);
    };

    auto handle_dumb_error = [&drm_fd](drm_mode_create_dumb* dumb) {
        drm_mode_destroy_dumb destroy = {
            .handle = dumb->handle
        };
        drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    };

    drm_mode_create_dumb dumb = {
        .height = height,
        .width = width,
        .bpp = bpp,
    };

    result = drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &dumb);
    if( result < 0 ) {
        std::cout << "Failed to create DRM FB\n";
        handle_dumb_error(&dumb);
        return false;
    }
    
    framebuffer->height = dumb.height;
    framebuffer->width = dumb.width;
    framebuffer->stride = dumb.pitch;
    framebuffer->handle = dumb.handle;
    framebuffer->size = dumb.size;

    uint32_t handles[4] = { framebuffer->handle };
    uint32_t strides[4] = { framebuffer->stride };
    uint32_t offsets[4] = { 0 };

    result = drmModeAddFB2(
        drm_fd, framebuffer->width, framebuffer->height, DRM_FORMAT_XRGB8888,
        handles, strides, offsets,
        &framebuffer->id, 0
    );

    if( result < 0 ) {
        std::cout << " Failed to add FB\n";
        handle_fb_error(framebuffer);
        return false;
    }

    drm_mode_map_dumb map = {
        .handle = framebuffer->handle
    };
    result = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
    if( result < 0 ) {
        handle_fb_error(framebuffer);
        return false;
    }

    framebuffer->data = reinterpret_cast<uint8_t*>( mmap(0, framebuffer->size, 
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        drm_fd, map.offset
    ) );

    if( !framebuffer->data ) {
        std::cout << " Failed to Map Buffer\n";
        handle_fb_error(framebuffer);
        return false;
    }

    std::memset( framebuffer->data, 0xff, framebuffer->size );

    return true;
}

void cleanup( const int& drm_fd, presenter* output )
{
    if(!output)
        return;
    
    cleanup(drm_fd, output->next);

    if(output->connected) {
        auto l_destroyFB = [&drm_fd](dumb_fb* fb) {
            munmap(fb->data, fb->size);
            drmModeRmFB(drm_fd, fb->id);
            drm_mode_destroy_dumb destroy = {.handle = fb->handle };
            drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        };        

        l_destroyFB(&output->framebuffers[0]);
        l_destroyFB(&output->framebuffers[1]);
        
        drmModeCrtcPtr crtc = output->previousCrtc;
        if( crtc ) {
            drmModeSetCrtc(
                drm_fd,
                crtc->crtc_id, crtc->buffer_id,
                crtc->x, crtc->y,
                &output->conn_id,
                1, &output->mode
            );
            drmModeFreeCrtc(crtc);
        }
    }
    free(output);
}

int main( int argc, const char* argv[] )
{
    if( argc < 2 )
    {
        std::cout << "<program_name> <drm_device_path>\n";
        return EXIT_FAILURE;
    }

    const char* drm_device = argv[1];
    int drm_fd = open( drm_device, O_RDWR | O_NONBLOCK );    

    if( drm_fd == -1 )
        std::cout << "Failed to Open " << drm_device << " device\n";

    drmModeResPtr resources = drmModeGetResources(drm_fd);

    std::cout << "drm resouces:\n fbs: " << resources->count_fbs << "\n crtcs: " << resources->count_crtcs << "\n connectors: " << resources->count_connectors << "\n encoders: " << resources->count_encoders << "\n min_extents: " << resources->min_width << "x" << resources->min_height << "\n max_extents: " << resources->max_width << "x" << resources->max_height << "\n";

    uint32_t taken_crtc;

    presenter* output_list = new presenter;
    presenter* current_output;
    for( int i = 0; i < resources->count_connectors; i++ )
    {
        drmModeConnectorPtr drm_connector = drmModeGetConnector( drm_fd, resources->connectors[i] );
        int result;
        
        if(!drm_connector)
            continue;
        
        current_output = new presenter;
        current_output->conn_id = drm_connector->connector_id;
        current_output->connected = drm_connector->connection == DRM_MODE_CONNECTED;
            
        std::cout << "Trying Connector: " << drm_connector->connector_id;
        if( !current_output->connected ){
            std::cout << " Disconnected Connector\n";
            goto incompatible;
        }

        if( drm_connector->count_modes == 0 ) {
            std::cout << " No Valid Modes for connector: " << drm_connector->connector_id;
            current_output->connected = false;
            goto incompatible;
        }

        current_output->crtc_id = find_crtc(drm_fd, resources, drm_connector, taken_crtc );

        if(!current_output->crtc_id) {
            std::cout << "Could not find CRTC";

        }

        std::cout << " compatible crtc: " << current_output->crtc_id << "\n";

        current_output->mode = drm_connector->modes[0];
        current_output->width = current_output->mode.hdisplay;
        current_output->height = current_output->mode.vdisplay;
        current_output->refresh = current_output->mode.vrefresh;

        std::cout << "Using Mode " << current_output->width << "x" << current_output->height << "@" << current_output->refresh << "\n";
        
        create_fb(drm_fd, &current_output->framebuffers[0], current_output->width, current_output->height);
        create_fb(drm_fd, &current_output->framebuffers[1], current_output->width, current_output->height);

        if( !create_fb(drm_fd, &current_output->framebuffers[0], current_output->width, current_output->height) ||
            !create_fb(drm_fd, &current_output->framebuffers[1], current_output->width, current_output->height)
        )         
        {
            current_output->connected = false;
            goto incompatible;
        }

        current_output->front = &current_output->framebuffers[0];
        current_output->back = &current_output->framebuffers[1];
        std::cout << "Created Front & Back framebuffers" << "\n";

        current_output->previousCrtc = drmModeGetCrtc( drm_fd, current_output->crtc_id );

        result = drmModeSetCrtc(drm_fd, current_output->crtc_id, current_output->framebuffers[0].id,
            0, 0,
            &current_output->conn_id,
            1, &current_output->mode
        );

        if( result < 0 ) {
            std::cout << "Failed to set CRTC\n";
        }

        if (drmModePageFlip(drm_fd, current_output->crtc_id, current_output->front->id,
				DRM_MODE_PAGE_FLIP_EVENT, current_output) < 0) {
			perror("drmModePageFlip");
		}

        current_output->next = output_list;
        output_list = current_output;
incompatible:
            drmModeFreeConnector(drm_connector);
    }

    drmModeFreeResources(resources);

    struct pollfd pollfd = {
        .fd = drm_fd,
        .events = POLLIN,
    };

    time_t end = time(NULL) + 5;
	while (time(NULL) <= end) {
		int ret = poll(&pollfd, 1, 5000);
		if (ret < 0 && errno != EAGAIN) {
			perror("poll");
			break;
		}

		if (pollfd.revents & POLLIN) {
			drmEventContext context = {
				.version = DRM_EVENT_CONTEXT_VERSION,
				.page_flip_handler = page_flip_handler,
			};

			if (drmHandleEvent(drm_fd, &context) < 0) {
				perror("drmHandleEvent");
				break;
			}
		}
	}

    std::cout << "Presenter Set\n";

    cleanup(drm_fd, output_list);
    close(drm_fd);

    return 0;
}