#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>


void infoModes( const drmModeModeInfo& modeInfo )
{
    
}

int main( int argc, const char* argv[] )
{
    int drm_fd = open( "/dev/dri/card0", O_RDWR | O_NONBLOCK );    

    drmModeResPtr resources = drmModeGetResources(drm_fd);

    std::cout << "drm resouces:\n fbs: " << resources->count_fbs << "\n crtcs: " << resources->count_crtcs << "\n connectors: " << resources->count_connectors << "\n encoders: " << resources->count_encoders << "\n min_extents: " << resources->min_width << "x" << resources->min_height << "\n max_extents: " << resources->max_width << "x" << resources->max_height << "\n";

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
    }

    return 0;
}