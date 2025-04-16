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

    return 0;
}
