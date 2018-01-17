#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include <bcm_host.h>
#include <interface/vcos/vcos.h>
#include <IL/OMX_Broadcom.h>

#include "dump.h"
#include "../common_util/common_util.h"

#define OMX_INIT_STRUCTURE(x) \
  memset (&(x), 0, sizeof (x)); \
  (x).nSize = sizeof (x); \
  (x).nVersion.nVersion = OMX_VERSION; \
  (x).nVersion.s.nVersionMajor = OMX_VERSION_MAJOR; \
  (x).nVersion.s.nVersionMinor = OMX_VERSION_MINOR; \
  (x).nVersion.s.nRevision = OMX_VERSION_REVISION; \
  (x).nVersion.s.nStep = OMX_VERSION_STEP

#define VIDEO_FRAMERATE 10
#define VIDEO_BITRATE 140000
#define VIDEO_IDR_PERIOD 1
#define VIDEO_SEI OMX_FALSE
#define VIDEO_EEDE OMX_FALSE
#define VIDEO_EEDE_LOSS_RATE 0
#define VIDEO_QP OMX_FALSE
#define VIDEO_QP_I 0 //1 .. 51, 0 means off
#define VIDEO_QP_P 0 //1 .. 51, 0 means off
#define VIDEO_PROFILE OMX_VIDEO_AVCProfileBaseline
#define VIDEO_INLINE_HEADERS OMX_FALSE

//Some settings doesn't work well
#define CAM_WIDTH 320
#define CAM_HEIGHT 240
#define CAM_SHARPNESS 0 //-100 .. 100
#define CAM_CONTRAST 0 //-100 .. 100
#define CAM_BRIGHTNESS 50 //0 .. 100
#define CAM_SATURATION 0 //-100 .. 100
#define CAM_SHUTTER_SPEED_AUTO OMX_TRUE
#define CAM_SHUTTER_SPEED 1.0/8.0
#define CAM_ISO_AUTO OMX_TRUE
#define CAM_ISO 100 //100 .. 800
#define CAM_EXPOSURE OMX_ExposureControlAuto
#define CAM_EXPOSURE_COMPENSATION 0 //-24 .. 24
#define CAM_MIRROR OMX_MirrorNone
#define CAM_ROTATION 0 //0 90 180 270
#define CAM_COLOR_ENABLE OMX_FALSE
#define CAM_COLOR_U 128 //0 .. 255
#define CAM_COLOR_V 128 //0 .. 255
#define CAM_NOISE_REDUCTION OMX_TRUE
#define CAM_FRAME_STABILIZATION OMX_FALSE
#define CAM_METERING OMX_MeteringModeAverage
#define CAM_WHITE_BALANCE OMX_WhiteBalControlAuto
//The gains are used if the white balance is set to off
#define CAM_WHITE_BALANCE_RED_GAIN 1000 //0 ..
#define CAM_WHITE_BALANCE_BLUE_GAIN 1000 //0 ..
#define CAM_IMAGE_FILTER OMX_ImageFilterNone
#define CAM_ROI_TOP 0 //0 .. 100
#define CAM_ROI_LEFT 0 //0 .. 100
#define CAM_ROI_WIDTH 100 //0 .. 100
#define CAM_ROI_HEIGHT 100 //0 .. 100
#define CAM_DRC OMX_DynRangeExpOff

/*
Possible values:

CAM_EXPOSURE
  OMX_ExposureControlOff
  OMX_ExposureControlAuto
  OMX_ExposureControlNight
  OMX_ExposureControlBackLight
  OMX_ExposureControlSpotlight
  OMX_ExposureControlSports
  OMX_ExposureControlSnow
  OMX_ExposureControlBeach
  OMX_ExposureControlLargeAperture
  OMX_ExposureControlSmallAperture
  OMX_ExposureControlVeryLong
  OMX_ExposureControlFixedFps
  OMX_ExposureControlNightWithPreview
  OMX_ExposureControlAntishake
  OMX_ExposureControlFireworks

CAM_IMAGE_FILTER
  OMX_ImageFilterNone
  OMX_ImageFilterEmboss
  OMX_ImageFilterNegative
  OMX_ImageFilterSketch
  OMX_ImageFilterOilPaint
  OMX_ImageFilterHatch
  OMX_ImageFilterGpen
  OMX_ImageFilterSolarize
  OMX_ImageFilterWatercolor
  OMX_ImageFilterPastel
  OMX_ImageFilterFilm
  OMX_ImageFilterBlur
  OMX_ImageFilterColourSwap
  OMX_ImageFilterWashedOut
  OMX_ImageFilterColourPoint
  OMX_ImageFilterPosterise
  OMX_ImageFilterColourBalance
  OMX_ImageFilterCartoon

CAM_METERING
  OMX_MeteringModeAverage
  OMX_MeteringModeSpot
  OMX_MeteringModeMatrix
  OMX_MeteringModeBacklit

CAM_MIRROR
  OMX_MirrorNone
  OMX_MirrorHorizontal
  OMX_MirrorVertical
  OMX_MirrorBoth

CAM_WHITE_BALANCE
  OMX_WhiteBalControlOff
  OMX_WhiteBalControlAuto
  OMX_WhiteBalControlSunLight
  OMX_WhiteBalControlCloudy
  OMX_WhiteBalControlShade
  OMX_WhiteBalControlTungsten
  OMX_WhiteBalControlFluorescent
  OMX_WhiteBalControlIncandescent
  OMX_WhiteBalControlFlash
  OMX_WhiteBalControlHorizon

CAM_DRC
  OMX_DynRangeExpOff
  OMX_DynRangeExpLow
  OMX_DynRangeExpMedium
  OMX_DynRangeExpHigh

VIDEO_PROFILE
  OMX_VIDEO_AVCProfileHigh
  OMX_VIDEO_AVCProfileBaseline
  OMX_VIDEO_AVCProfileMain
*/

//Data of each component
typedef struct {
  //The handle is obtained with OMX_GetHandle() and is used on every function
  //that needs to manipulate a component. It is released with OMX_FreeHandle()
  OMX_HANDLETYPE handle;
  //Bitwise OR of flags. Used for blocking the current thread and waiting an
  //event. Used with vcos_event_flags_get() and vcos_event_flags_set()
  VCOS_EVENT_FLAGS_T flags;
  //The fullname of the component
  OMX_STRING name;
} component_t;

//Events used with vcos_event_flags_get() and vcos_event_flags_set()
typedef enum {
  EVENT_ERROR = 0x1,
  EVENT_PORT_ENABLE = 0x2,
  EVENT_PORT_DISABLE = 0x4,
  EVENT_STATE_SET = 0x8,
  EVENT_FLUSH = 0x10,
  EVENT_MARK_BUFFER = 0x20,
  EVENT_MARK = 0x40,
  EVENT_PORT_SETTINGS_CHANGED = 0x80,
  EVENT_PARAM_OR_CONFIG_CHANGED = 0x100,
  EVENT_BUFFER_FLAG = 0x200,
  EVENT_RESOURCES_ACQUIRED = 0x400,
  EVENT_DYNAMIC_RESOURCES_AVAILABLE = 0x800,
  EVENT_FILL_BUFFER_DONE = 0x1000,
  EVENT_EMPTY_BUFFER_DONE = 0x2000,
} component_event;

//Prototypes
OMX_ERRORTYPE event_handler (
    OMX_IN OMX_HANDLETYPE comp,
    OMX_IN OMX_PTR app_data,
    OMX_IN OMX_EVENTTYPE event,
    OMX_IN OMX_U32 data1,
    OMX_IN OMX_U32 data2,
    OMX_IN OMX_PTR event_data);
OMX_ERRORTYPE fill_buffer_done (
    OMX_IN OMX_HANDLETYPE comp,
    OMX_IN OMX_PTR app_data,
    OMX_IN OMX_BUFFERHEADERTYPE* buffer);
void wake (component_t* component, VCOS_UNSIGNED event);
void wait (
    component_t* component,
    VCOS_UNSIGNED events,
    VCOS_UNSIGNED* retrieved_events);
void init_component (component_t* component);
void deinit_component (component_t* component);
void load_camera_drivers (component_t* component);
void change_state (component_t* component, OMX_STATETYPE state);
void enable_port (component_t* component, OMX_U32 port);
void disable_port (component_t* component, OMX_U32 port);
void enable_encoder_output_port (
    component_t* encoder,
    OMX_BUFFERHEADERTYPE** encoder_output_buffer);
void disable_encoder_output_port (
    component_t* encoder,
    OMX_BUFFERHEADERTYPE* encoder_output_buffer);
void set_camera_settings (component_t* camera);
void set_h264_settings (component_t* encoder);

void omx_h264_init();
void omx_h264_deinit();
OMX_BUFFERHEADERTYPE* fill_frame_buffer();
