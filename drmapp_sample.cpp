#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>


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
            std::cout << " compatible crtc: " << resources->crtcs[i] << "\n";
            return resources->crtcs[i];
        }

        drmModeFreeEncoder(encoder);
    }

    return 0;
}

int main( int argc, const char* argv[] )
{
    int drm_fd = open( "/dev/dri/card0", O_RDWR | O_NONBLOCK );    

    drmModeResPtr resources = drmModeGetResources(drm_fd);

    std::cout << "drm resouces:\n fbs: " << resources->count_fbs << "\n crtcs: " << resources->count_crtcs << "\n connectors: " << resources->count_connectors << "\n encoders: " << resources->count_encoders << "\n min_extents: " << resources->min_width << "x" << resources->min_height << "\n max_extents: " << resources->max_width << "x" << resources->max_height << "\n";

    uint32_t taken_crtc;

    for( int i = 0; i < resources->count_connectors; i++ )
    {
        drmModeConnectorPtr connector = drmModeGetConnector( drm_fd, resources->connectors[i] );
        
        if(!connector)
            continue;

        std::cout << " connector: " << connector->connector_id << " modes: " << connector->count_modes << "\n";
        std::cout << "mode info:";
        for(int j = 0; j < connector->count_modes; j++)
        {
            drmModeModeInfo modeInfo = connector->modes[j];
            std::cout <<"\n "<< modeInfo.hdisplay << "x" << modeInfo.vdisplay << "(" << modeInfo.vrefresh << ")\n";
        }

        uint32_t crtc_index = find_crtc(drm_fd, resources, connector, taken_crtc );
    }

    return 0;
}