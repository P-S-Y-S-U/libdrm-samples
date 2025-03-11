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

    dumb_fb fb;
};

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

bool create_fb( const int& drm_fd, presenter& output ) {
    int result;

    auto handle_fb_error = [&output, &drm_fd]() {
        drmModeRmFB(drm_fd, output.fb.id);
    };

    auto handle_dumb_error = [&output, &drm_fd]() {
        drm_mode_destroy_dumb destroy = {
            .handle = output.fb.handle
        };
        drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    };

    drm_mode_create_dumb dumb = {
        .height = output.height,
        .width = output.width,
        .bpp = 32,
    };

    result = drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &dumb);
    if( result < 0 ) {
        std::cout << "Failed to create DRM FB\n";
        handle_dumb_error();
        return false;
    }

    output.fb.height = output.height;
    output.fb.width = output.width;
    output.fb.stride = dumb.pitch;
    output.fb.handle = dumb.handle;
    output.fb.size = dumb.size;

    uint32_t handles[4] = { output.fb.handle };
    uint32_t strides[4] = { output.fb.stride };
    uint32_t offsets[4] = { 0 };

    result = drmModeAddFB2(
        drm_fd, output.fb.width, output.fb.height, DRM_FORMAT_XRGB8888,
        handles, strides, offsets,
        &output.fb.id, 0
    );

    if( result < 0 ) {
        std::cout << " Failed to add FB\n";
        handle_fb_error();
        return false;
    }

    drm_mode_map_dumb map = {
        .handle = output.fb.handle
    };
    result = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map);
    if( result < 0 ) {
        handle_fb_error();
        return false;
    }

    output.fb.data = reinterpret_cast<uint8_t*>( mmap(0, output.fb.size, 
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        drm_fd, map.offset
    ) );

    if( !output.fb.data ) {
        std::cout << " Failed to Map Buffer\n";
        handle_fb_error();
        return false;
    }

    std::memset( output.fb.data, 0xff, output.fb.size );

    return true;
}

int main( int argc, const char* argv[] )
{
    int drm_fd = open( "/dev/dri/card0", O_RDWR | O_NONBLOCK );    

    drmModeResPtr resources = drmModeGetResources(drm_fd);

    std::cout << "drm resouces:\n fbs: " << resources->count_fbs << "\n crtcs: " << resources->count_crtcs << "\n connectors: " << resources->count_connectors << "\n encoders: " << resources->count_encoders << "\n min_extents: " << resources->min_width << "x" << resources->min_height << "\n max_extents: " << resources->max_width << "x" << resources->max_height << "\n";

    uint32_t taken_crtc;

    presenter output;

    for( int i = 0; i < resources->count_connectors; i++ )
    {
        drmModeConnectorPtr drm_connector = drmModeGetConnector( drm_fd, resources->connectors[i] );
        
        if(!drm_connector)
            continue;
        
        output.conn_id = drm_connector->connector_id;
        output.connected = drm_connector->connection == DRM_MODE_CONNECTED;
        
        std::cout << "Trying Connector: " << drm_connector->connector_id;
        if( !output.connected ){
            std::cout << " Disconnected Connector\n";
            goto incompatible;
        }

        if( drm_connector->count_modes == 0 ) {
            std::cout << " No Valid Modes for connector: " << drm_connector->connector_id;
            output.connected = false;
            goto incompatible;
        }
/*            
        std::cout << " connector: " << connector->connector_id << " modes: " << connector->count_modes << "\n";
        std::cout << "mode info:";
        for(int j = 0; j < connector->count_modes; j++)
        {
            drmModeModeInfo modeInfo = connector->modes[j];
            std::cout <<"\n "<< modeInfo.hdisplay << "x" << modeInfo.vdisplay << "(" << modeInfo.vrefresh << ")\n";
        }
*/

        output.crtc_id = find_crtc(drm_fd, resources, drm_connector, taken_crtc );

        if(!output.crtc_id) {
            std::cout << "Could not find CRTC";

        }

        std::cout << " compatible crtc: " << output.crtc_id << "\n";

        output.mode = drm_connector->modes[0];
        output.width = output.mode.hdisplay;
        output.height = output.mode.vdisplay;
        output.refresh = output.mode.vrefresh;

        std::cout << "Using Mode " << output.width << "x" << output.height << "@" << output.refresh << "\n";

        if( !create_fb(drm_fd, output) )
        {
            output.connected = false;
            goto incompatible;
        }

        std::cout << "Created FB with id " << output.fb.id << "\n";

        output.previousCrtc = drmModeGetCrtc( drm_fd, output.crtc_id );
incompatible:
            drmModeFreeConnector(drm_connector);
    }

    auto l_cleanup = [&drm_fd]( presenter& output ) {
        if(output.connected) {
            munmap(output.fb.data, output.fb.size);
            drmModeRmFB(drm_fd, output.fb.id);
            drm_mode_destroy_dumb destroy = {.handle = output.fb.handle };
            drmIoctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);

            drmModeCrtcPtr crtc = output.previousCrtc;
            if( crtc ) {
                drmModeSetCrtc(
                    drm_fd,
                    crtc->crtc_id, crtc->buffer_id,
                    crtc->x, crtc->y,
                    &output.conn_id,
                    1, &output.mode
                );
                drmModeFreeCrtc(crtc);
            }
        }
    };

    l_cleanup(output);
    close(drm_fd);

    return 0;
}