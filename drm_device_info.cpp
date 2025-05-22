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


inline const char* getDrmConnectionStr( const uint32_t& connection )
{
    switch (connection)
    {
    case DRM_MODE_CONNECTED:
        return "CONNECTED";
    case DRM_MODE_DISCONNECTED:
        return "DISCONNECTED";
    case DRM_MODE_UNKNOWNCONNECTION:
        return "UNKOWNCONNECTION";
    default:
        return "UNKNOWNENUM";
    }
}

inline const char* getDrmConnectorTypeStr( const uint32_t& connector_type )
{
    switch (connector_type)
    {
        case DRM_MODE_CONNECTOR_Unknown: return "Unknown";
        case DRM_MODE_CONNECTOR_VGA: return "VGA";
        case DRM_MODE_CONNECTOR_DVII: return "DVII";
        case DRM_MODE_CONNECTOR_DVID: return "DVID";
        case DRM_MODE_CONNECTOR_DVIA: return "DVIA";
        case DRM_MODE_CONNECTOR_Composite: return "Composite";
        case DRM_MODE_CONNECTOR_SVIDEO: return "SVIDEO";
        case DRM_MODE_CONNECTOR_LVDS: return "LVDS";
        case DRM_MODE_CONNECTOR_Component: return "Component";
        case DRM_MODE_CONNECTOR_9PinDIN: return "9PinDIN";
        case DRM_MODE_CONNECTOR_DisplayPort: return "DisplayPort";
        case DRM_MODE_CONNECTOR_HDMIA: return "HDMI";
        case DRM_MODE_CONNECTOR_HDMIB: return "HDMIB";
        case DRM_MODE_CONNECTOR_TV: return "TV";
        case DRM_MODE_CONNECTOR_eDP: return "eDP";
        case DRM_MODE_CONNECTOR_VIRTUAL: return "VIRTUAL";
        case DRM_MODE_CONNECTOR_DSI: return "DSI";
        case DRM_MODE_CONNECTOR_DPI: return "DPI";
        case DRM_MODE_CONNECTOR_WRITEBACK: return "WRITEBACK";
        case DRM_MODE_CONNECTOR_SPI: return "SPI";
        case DRM_MODE_CONNECTOR_USB: return "USB";
        default: return "UNKNOWNENUM";
    }
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

    for( int i = 0; i < resources->count_connectors; i++ )
    {
        drmModeConnectorPtr drm_connector = drmModeGetConnector( drm_fd, resources->connectors[i] );
        int result;
        
        if(!drm_connector)
            continue;
        
        std::cout << "Connector: " <<  \
                                        getDrmConnectionStr(drm_connector->connection) << "(" << drm_connector->connector_id << ") " << \
                                        getDrmConnectorTypeStr(drm_connector->connector_type) << "(" << drm_connector->connector_type_id << ") " << \
                                        "number of modes: " << drm_connector->count_modes << "\n";


        if( drm_connector->count_modes == 0 ) {
            std::cout << " No Valid Modes for connector!! \n\n";
            continue;
        }

        std::cout << "mode info:\n";
        for(int j = 0; j < drm_connector->count_modes; j++)
        {
            drmModeModeInfo modeInfo = drm_connector->modes[j];
            std::cout << "connector_id: " <<  drm_connector->connector_id << " " << modeInfo.hdisplay << "x" << modeInfo.vdisplay << "(" << modeInfo.vrefresh << ")\n";
        }

        std::cout << "\n\n";
    }

    return 0;
}
