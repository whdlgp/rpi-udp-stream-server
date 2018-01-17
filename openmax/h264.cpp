#include "h264.h"

static OMX_ERRORTYPE error;
static OMX_BUFFERHEADERTYPE* encoder_output_buffer;
static component_t camera;
static component_t encoder;
static component_t null_sink;
static char camera_name[30];
static char encoder_name[30];
static char null_sink_name[30];

static OMX_PARAM_PORTDEFINITIONTYPE port_st;
static OMX_CONFIG_FRAMERATETYPE framerate_st;
static OMX_CONFIG_PORTBOOLEANTYPE capture_st;

//Function that is called when a component receives an event from a secondary
//thread
OMX_ERRORTYPE event_handler (
        OMX_IN OMX_HANDLETYPE comp,
        OMX_IN OMX_PTR app_data,
        OMX_IN OMX_EVENTTYPE event,
        OMX_IN OMX_U32 data1,
        OMX_IN OMX_U32 data2,
        OMX_IN OMX_PTR event_data){
    component_t* component = (component_t*)app_data;

    switch (event){
        case OMX_EventCmdComplete:
            switch (data1){
                case OMX_CommandStateSet:
                    DEBUG_MSG("event: %s, OMX_CommandStateSet, state: %s\n",
                            component->name, dump_OMX_STATETYPE ((OMX_STATETYPE)data2));
                    wake (component, EVENT_STATE_SET);
                    break;
                case OMX_CommandPortDisable:
                    DEBUG_MSG("event: %s, OMX_CommandPortDisable, port: %d\n",
                            component->name, data2);
                    wake (component, EVENT_PORT_DISABLE);
                    break;
                case OMX_CommandPortEnable:
                    DEBUG_MSG("event: %s, OMX_CommandPortEnable, port: %d\n",
                            component->name, data2);
                    wake (component, EVENT_PORT_ENABLE);
                    break;
                case OMX_CommandFlush:
                    DEBUG_MSG("event: %s, OMX_CommandFlush, port: %d\n",
                            component->name, data2);
                    wake (component, EVENT_FLUSH);
                    break;
                case OMX_CommandMarkBuffer:
                    DEBUG_MSG("event: %s, OMX_CommandMarkBuffer, port: %d\n",
                            component->name, data2);
                    wake (component, EVENT_MARK_BUFFER);
                    break;
            }
            break;
        case OMX_EventError:
            DEBUG_MSG("event: %s, %s\n", component->name, dump_OMX_ERRORTYPE ((OMX_ERRORTYPE)data1));
            wake (component, EVENT_ERROR);
            break;
        case OMX_EventMark:
            DEBUG_MSG("event: %s, OMX_EventMark\n", component->name);
            wake (component, EVENT_MARK);
            break;
        case OMX_EventPortSettingsChanged:
            DEBUG_MSG("event: %s, OMX_EventPortSettingsChanged, port: %d\n",
                    component->name, data1);
            wake (component, EVENT_PORT_SETTINGS_CHANGED);
            break;
        case OMX_EventParamOrConfigChanged:
            DEBUG_MSG("event: %s, OMX_EventParamOrConfigChanged, data1: %d, data2: "
                    "%X\n", component->name, data1, data2);
            wake (component, EVENT_PARAM_OR_CONFIG_CHANGED);
            break;
        case OMX_EventBufferFlag:
            DEBUG_MSG("event: %s, OMX_EventBufferFlag, port: %d\n",
                    component->name, data1);
            wake (component, EVENT_BUFFER_FLAG);
            break;
        case OMX_EventResourcesAcquired:
            DEBUG_MSG("event: %s, OMX_EventResourcesAcquired\n", component->name);
            wake (component, EVENT_RESOURCES_ACQUIRED);
            break;
        case OMX_EventDynamicResourcesAvailable:
            DEBUG_MSG("event: %s, OMX_EventDynamicResourcesAvailable\n",
                    component->name);
            wake (component, EVENT_DYNAMIC_RESOURCES_AVAILABLE);
            break;
        default:
            //This should never execute, just ignore
            DEBUG_MSG("event: unknown (%X)\n", event);
            break;
    }

    return OMX_ErrorNone;
}

//Function that is called when a component fills a buffer with data
OMX_ERRORTYPE fill_buffer_done (
        OMX_IN OMX_HANDLETYPE comp,
        OMX_IN OMX_PTR app_data,
        OMX_IN OMX_BUFFERHEADERTYPE* buffer){
    component_t* component = (component_t*)app_data;

    DEBUG_MSG("event: %s, fill_buffer_done\n", component->name);
    wake (component, EVENT_FILL_BUFFER_DONE);

    return OMX_ErrorNone;
}

void wake (component_t* component, VCOS_UNSIGNED event){
    vcos_event_flags_set (&component->flags, event, VCOS_OR);
}

void wait (
        component_t* component,
        VCOS_UNSIGNED events,
        VCOS_UNSIGNED* retrieved_events){
    VCOS_UNSIGNED set;
    if (vcos_event_flags_get (&component->flags, events | EVENT_ERROR,
                VCOS_OR_CONSUME, VCOS_SUSPEND, &set)){
        DEBUG_ERR("error: vcos_event_flags_get\n");
        exit (1);
    }
    if (set == EVENT_ERROR){
        exit (1);
    }
    if (retrieved_events){
        *retrieved_events = set;
    }
}

void init_component (component_t* component){
    DEBUG_MSG("initializing component %s\n", component->name);

    OMX_ERRORTYPE error;

    //Create the event flags
    if (vcos_event_flags_create (&component->flags, "component")){
        DEBUG_ERR("error: vcos_event_flags_create\n");
        exit (1);
    }

    //Each component has an event_handler and fill_buffer_done functions
    OMX_CALLBACKTYPE callbacks_st;
    callbacks_st.EventHandler = event_handler;
    callbacks_st.FillBufferDone = fill_buffer_done;

    //Get the handle
    if ((error = OMX_GetHandle (&component->handle, component->name, component,
                    &callbacks_st))){
        DEBUG_ERR("error: OMX_GetHandle: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Disable all the ports
    OMX_INDEXTYPE types[] = {
        OMX_IndexParamAudioInit,
        OMX_IndexParamVideoInit,
        OMX_IndexParamImageInit,
        OMX_IndexParamOtherInit
    };
    OMX_PORT_PARAM_TYPE ports_st;
    OMX_INIT_STRUCTURE (ports_st);

    int i;
    for (i=0; i<4; i++){
        if ((error = OMX_GetParameter (component->handle, types[i], &ports_st))){
            DEBUG_ERR("error: OMX_GetParameter: %s\n",
                    dump_OMX_ERRORTYPE (error));
            exit (1);
        }

        OMX_U32 port;
        for (port=ports_st.nStartPortNumber;
                port<ports_st.nStartPortNumber + ports_st.nPorts; port++){
            //Disable the port
            disable_port (component, port);
            //Wait to the event
            wait (component, EVENT_PORT_DISABLE, 0);
        }
    }
}

void deinit_component (component_t* component){
    DEBUG_MSG("deinitializing component %s\n", component->name);

    OMX_ERRORTYPE error;

    vcos_event_flags_delete (&component->flags);

    if ((error = OMX_FreeHandle (component->handle))){
        DEBUG_ERR("error: OMX_FreeHandle: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }
}

void load_camera_drivers (component_t* component){
    /*
       This is a specific behaviour of the Broadcom's Raspberry Pi OpenMAX IL
       implementation module because the OMX_SetConfig() and OMX_SetParameter() are
       blocking functions but the drivers are loaded asynchronously, that is, an
       event is fired to signal the completion. Basically, what you're saying is:

       "When the parameter with index OMX_IndexParamCameraDeviceNumber is set, load
       the camera drivers and emit an OMX_EventParamOrConfigChanged event"

       The red LED of the camera will be turned on after this call.
       */

    DEBUG_MSG("loading camera drivers\n");

    OMX_ERRORTYPE error;

    OMX_CONFIG_REQUESTCALLBACKTYPE cbs_st;
    OMX_INIT_STRUCTURE (cbs_st);
    cbs_st.nPortIndex = OMX_ALL;
    cbs_st.nIndex = OMX_IndexParamCameraDeviceNumber;
    cbs_st.bEnable = OMX_TRUE;
    if ((error = OMX_SetConfig (component->handle, OMX_IndexConfigRequestCallback,
                    &cbs_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    OMX_PARAM_U32TYPE dev_st;
    OMX_INIT_STRUCTURE (dev_st);
    dev_st.nPortIndex = OMX_ALL;
    //ID for the camera device
    dev_st.nU32 = 0;
    if ((error = OMX_SetParameter (component->handle,
                    OMX_IndexParamCameraDeviceNumber, &dev_st))){
        DEBUG_ERR("error: OMX_SetParameter: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    wait (component, EVENT_PARAM_OR_CONFIG_CHANGED, 0);
}

void change_state (component_t* component, OMX_STATETYPE state){
    DEBUG_MSG("changing %s state to %s\n", component->name,
            dump_OMX_STATETYPE (state));

    OMX_ERRORTYPE error;

    if ((error = OMX_SendCommand (component->handle, OMX_CommandStateSet, state,
                    0))){
        DEBUG_ERR("error: OMX_SendCommand: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }
}

void enable_port (component_t* component, OMX_U32 port){
    DEBUG_MSG("enabling port %d (%s)\n", port, component->name);

    OMX_ERRORTYPE error;

    if ((error = OMX_SendCommand (component->handle, OMX_CommandPortEnable,
                    port, 0))){
        DEBUG_ERR("error: OMX_SendCommand: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }
}

void disable_port (component_t* component, OMX_U32 port){
    DEBUG_MSG("disabling port %d (%s)\n", port, component->name);

    OMX_ERRORTYPE error;

    if ((error = OMX_SendCommand (component->handle, OMX_CommandPortDisable,
                    port, 0))){
        DEBUG_ERR("error: OMX_SendCommand: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }
}

void enable_encoder_output_port (
        component_t* encoder,
        OMX_BUFFERHEADERTYPE** encoder_output_buffer){
    //The port is not enabled until the buffer is allocated
    OMX_ERRORTYPE error;

    enable_port (encoder, 201);

    OMX_PARAM_PORTDEFINITIONTYPE port_st;
    OMX_INIT_STRUCTURE (port_st);
    port_st.nPortIndex = 201;
    if ((error = OMX_GetParameter (encoder->handle, OMX_IndexParamPortDefinition,
                    &port_st))){
        DEBUG_ERR("error: OMX_GetParameter: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }
    DEBUG_MSG("allocating %s output buffer\n", encoder->name);
    if ((error = OMX_AllocateBuffer (encoder->handle, encoder_output_buffer, 201,
                    0, port_st.nBufferSize))){
        DEBUG_ERR("error: OMX_AllocateBuffer: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    wait (encoder, EVENT_PORT_ENABLE, 0);
}

void disable_encoder_output_port (
        component_t* encoder,
        OMX_BUFFERHEADERTYPE* encoder_output_buffer){
    //The port is not disabled until the buffer is released
    OMX_ERRORTYPE error;

    disable_port (encoder, 201);

    //Free encoder output buffer
    DEBUG_MSG("releasing %s output buffer\n", encoder->name);
    if ((error = OMX_FreeBuffer (encoder->handle, 201, encoder_output_buffer))){
        DEBUG_ERR("error: OMX_FreeBuffer: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    wait (encoder, EVENT_PORT_DISABLE, 0);
}

void set_camera_settings (component_t* camera){
    DEBUG_MSG("configuring '%s' settings\n", camera->name);

    OMX_ERRORTYPE error;

    //Sharpness
    OMX_CONFIG_SHARPNESSTYPE sharpness_st;
    OMX_INIT_STRUCTURE (sharpness_st);
    sharpness_st.nPortIndex = OMX_ALL;
    sharpness_st.nSharpness = CAM_SHARPNESS;
    if ((error = OMX_SetConfig (camera->handle, OMX_IndexConfigCommonSharpness,
                    &sharpness_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Contrast
    OMX_CONFIG_CONTRASTTYPE contrast_st;
    OMX_INIT_STRUCTURE (contrast_st);
    contrast_st.nPortIndex = OMX_ALL;
    contrast_st.nContrast = CAM_CONTRAST;
    if ((error = OMX_SetConfig (camera->handle, OMX_IndexConfigCommonContrast,
                    &contrast_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Saturation
    OMX_CONFIG_SATURATIONTYPE saturation_st;
    OMX_INIT_STRUCTURE (saturation_st);
    saturation_st.nPortIndex = OMX_ALL;
    saturation_st.nSaturation = CAM_SATURATION;
    if ((error = OMX_SetConfig (camera->handle, OMX_IndexConfigCommonSaturation,
                    &saturation_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Brightness
    OMX_CONFIG_BRIGHTNESSTYPE brightness_st;
    OMX_INIT_STRUCTURE (brightness_st);
    brightness_st.nPortIndex = OMX_ALL;
    brightness_st.nBrightness = CAM_BRIGHTNESS;
    if ((error = OMX_SetConfig (camera->handle, OMX_IndexConfigCommonBrightness,
                    &brightness_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Exposure value
    OMX_CONFIG_EXPOSUREVALUETYPE exposure_value_st;
    OMX_INIT_STRUCTURE (exposure_value_st);
    exposure_value_st.nPortIndex = OMX_ALL;
    exposure_value_st.eMetering = CAM_METERING;
    exposure_value_st.xEVCompensation =
        (OMX_S32)((CAM_EXPOSURE_COMPENSATION<<16)/6.0);
    exposure_value_st.nShutterSpeedMsec = (OMX_U32)((CAM_SHUTTER_SPEED)*1e6);
    exposure_value_st.bAutoShutterSpeed = CAM_SHUTTER_SPEED_AUTO;
    exposure_value_st.nSensitivity = CAM_ISO;
    exposure_value_st.bAutoSensitivity = CAM_ISO_AUTO;
    if ((error = OMX_SetConfig (camera->handle,
                    OMX_IndexConfigCommonExposureValue, &exposure_value_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Exposure control
    OMX_CONFIG_EXPOSURECONTROLTYPE exposure_control_st;
    OMX_INIT_STRUCTURE (exposure_control_st);
    exposure_control_st.nPortIndex = OMX_ALL;
    exposure_control_st.eExposureControl = CAM_EXPOSURE;
    if ((error = OMX_SetConfig (camera->handle, OMX_IndexConfigCommonExposure,
                    &exposure_control_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Frame stabilisation
    OMX_CONFIG_FRAMESTABTYPE frame_stabilisation_st;
    OMX_INIT_STRUCTURE (frame_stabilisation_st);
    frame_stabilisation_st.nPortIndex = OMX_ALL;
    frame_stabilisation_st.bStab = CAM_FRAME_STABILIZATION;
    if ((error = OMX_SetConfig (camera->handle,
                    OMX_IndexConfigCommonFrameStabilisation, &frame_stabilisation_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //White balance
    OMX_CONFIG_WHITEBALCONTROLTYPE white_balance_st;
    OMX_INIT_STRUCTURE (white_balance_st);
    white_balance_st.nPortIndex = OMX_ALL;
    white_balance_st.eWhiteBalControl = CAM_WHITE_BALANCE;
    if ((error = OMX_SetConfig (camera->handle, OMX_IndexConfigCommonWhiteBalance,
                    &white_balance_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //White balance gains (if white balance is set to off)
    if (!CAM_WHITE_BALANCE){
        OMX_CONFIG_CUSTOMAWBGAINSTYPE white_balance_gains_st;
        OMX_INIT_STRUCTURE (white_balance_gains_st);
        white_balance_gains_st.xGainR = (CAM_WHITE_BALANCE_RED_GAIN << 16)/1000;
        white_balance_gains_st.xGainB = (CAM_WHITE_BALANCE_BLUE_GAIN << 16)/1000;
        if ((error = OMX_SetConfig (camera->handle, OMX_IndexConfigCustomAwbGains,
                        &white_balance_gains_st))){
            DEBUG_ERR("error: OMX_SetConfig: %s\n",
                    dump_OMX_ERRORTYPE (error));
            exit (1);
        }
    }

    //Image filter
    OMX_CONFIG_IMAGEFILTERTYPE image_filter_st;
    OMX_INIT_STRUCTURE (image_filter_st);
    image_filter_st.nPortIndex = OMX_ALL;
    image_filter_st.eImageFilter = CAM_IMAGE_FILTER;
    if ((error = OMX_SetConfig (camera->handle, OMX_IndexConfigCommonImageFilter,
                    &image_filter_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Mirror
    OMX_CONFIG_MIRRORTYPE mirror_st;
    OMX_INIT_STRUCTURE (mirror_st);
    mirror_st.nPortIndex = 71;
    mirror_st.eMirror = CAM_MIRROR;
    if ((error = OMX_SetConfig (camera->handle, OMX_IndexConfigCommonMirror,
                    &mirror_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Rotation
    OMX_CONFIG_ROTATIONTYPE rotation_st;
    OMX_INIT_STRUCTURE (rotation_st);
    rotation_st.nPortIndex = 71;
    rotation_st.nRotation = CAM_ROTATION;
    if ((error = OMX_SetConfig (camera->handle, OMX_IndexConfigCommonRotate,
                    &rotation_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Color enhancement
    OMX_CONFIG_COLORENHANCEMENTTYPE color_enhancement_st;
    OMX_INIT_STRUCTURE (color_enhancement_st);
    color_enhancement_st.nPortIndex = OMX_ALL;
    color_enhancement_st.bColorEnhancement = CAM_COLOR_ENABLE;
    color_enhancement_st.nCustomizedU = CAM_COLOR_U;
    color_enhancement_st.nCustomizedV = CAM_COLOR_V;
    if ((error = OMX_SetConfig (camera->handle,
                    OMX_IndexConfigCommonColorEnhancement, &color_enhancement_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Denoise
    OMX_CONFIG_BOOLEANTYPE denoise_st;
    OMX_INIT_STRUCTURE (denoise_st);
    denoise_st.bEnabled = CAM_NOISE_REDUCTION;
    if ((error = OMX_SetConfig (camera->handle,
                    OMX_IndexConfigStillColourDenoiseEnable, &denoise_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //ROI
    OMX_CONFIG_INPUTCROPTYPE roi_st;
    OMX_INIT_STRUCTURE (roi_st);
    roi_st.nPortIndex = OMX_ALL;
    roi_st.xLeft = (CAM_ROI_LEFT << 16)/100;
    roi_st.xTop = (CAM_ROI_TOP << 16)/100;
    roi_st.xWidth = (CAM_ROI_WIDTH << 16)/100;
    roi_st.xHeight = (CAM_ROI_HEIGHT << 16)/100;
    if ((error = OMX_SetConfig (camera->handle,
                    OMX_IndexConfigInputCropPercentages, &roi_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //DRC
    OMX_CONFIG_DYNAMICRANGEEXPANSIONTYPE drc_st;
    OMX_INIT_STRUCTURE (drc_st);
    drc_st.eMode = CAM_DRC;
    if ((error = OMX_SetConfig (camera->handle,
                    OMX_IndexConfigDynamicRangeExpansion, &drc_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }
}

void set_h264_settings (component_t* encoder){
    DEBUG_MSG("configuring '%s' settings\n", encoder->name);

    OMX_ERRORTYPE error;

    if (!VIDEO_QP){
        //Bitrate
        OMX_VIDEO_PARAM_BITRATETYPE bitrate_st;
        OMX_INIT_STRUCTURE (bitrate_st);
        bitrate_st.eControlRate = OMX_Video_ControlRateVariable;
        bitrate_st.nTargetBitrate = VIDEO_BITRATE;
        bitrate_st.nPortIndex = 201;
        if ((error = OMX_SetParameter (encoder->handle, OMX_IndexParamVideoBitrate,
                        &bitrate_st))){
            DEBUG_ERR("error: OMX_SetParameter: %s\n",
                    dump_OMX_ERRORTYPE (error));
            exit (1);
        }
    }else{
        //Quantization parameters
        OMX_VIDEO_PARAM_QUANTIZATIONTYPE quantization_st;
        OMX_INIT_STRUCTURE (quantization_st);
        quantization_st.nPortIndex = 201;
        //nQpB returns an error, it cannot be modified
        quantization_st.nQpI = VIDEO_QP_I;
        quantization_st.nQpP = VIDEO_QP_P;
        if ((error = OMX_SetParameter (encoder->handle,
                        OMX_IndexParamVideoQuantization, &quantization_st))){
            DEBUG_ERR("error: OMX_SetParameter: %s\n",
                    dump_OMX_ERRORTYPE (error));
            exit (1);
        }
    }

    //Codec
    OMX_VIDEO_PARAM_PORTFORMATTYPE format_st;
    OMX_INIT_STRUCTURE (format_st);
    format_st.nPortIndex = 201;
    //H.264/AVC
    format_st.eCompressionFormat = OMX_VIDEO_CodingAVC;
    if ((error = OMX_SetParameter (encoder->handle, OMX_IndexParamVideoPortFormat,
                    &format_st))){
        DEBUG_ERR("error: OMX_SetParameter: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //IDR period
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD idr_st;
    OMX_INIT_STRUCTURE (idr_st);
    idr_st.nPortIndex = 201;
    if ((error = OMX_GetConfig (encoder->handle,
                    OMX_IndexConfigVideoAVCIntraPeriod, &idr_st))){
        DEBUG_ERR("error: OMX_GetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }
    idr_st.nIDRPeriod = VIDEO_IDR_PERIOD;
    if ((error = OMX_SetConfig (encoder->handle,
                    OMX_IndexConfigVideoAVCIntraPeriod, &idr_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //SEI
    OMX_PARAM_BRCMVIDEOAVCSEIENABLETYPE sei_st;
    OMX_INIT_STRUCTURE (sei_st);
    sei_st.nPortIndex = 201;
    sei_st.bEnable = VIDEO_SEI;
    if ((error = OMX_SetParameter (encoder->handle,
                    OMX_IndexParamBrcmVideoAVCSEIEnable, &sei_st))){
        DEBUG_ERR("error: OMX_SetParameter: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //EEDE
    OMX_VIDEO_EEDE_ENABLE eede_st;
    OMX_INIT_STRUCTURE (eede_st);
    eede_st.nPortIndex = 201;
    eede_st.enable = VIDEO_EEDE;
    if ((error = OMX_SetParameter (encoder->handle, OMX_IndexParamBrcmEEDEEnable,
                    &eede_st))){
        DEBUG_ERR("error: OMX_SetParameter: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    OMX_VIDEO_EEDE_LOSSRATE eede_loss_rate_st;
    OMX_INIT_STRUCTURE (eede_loss_rate_st);
    eede_loss_rate_st.nPortIndex = 201;
    eede_loss_rate_st.loss_rate = VIDEO_EEDE_LOSS_RATE;
    if ((error = OMX_SetParameter (encoder->handle,
                    OMX_IndexParamBrcmEEDELossRate, &eede_loss_rate_st))){
        DEBUG_ERR("error: OMX_SetParameter: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //AVC Profile
    OMX_VIDEO_PARAM_AVCTYPE avc_st;
    OMX_INIT_STRUCTURE (avc_st);
    avc_st.nPortIndex = 201;
    if ((error = OMX_GetParameter (encoder->handle,
                    OMX_IndexParamVideoAvc, &avc_st))){
        DEBUG_ERR("error: OMX_GetParameter: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }
    avc_st.eProfile = VIDEO_PROFILE;
    if ((error = OMX_SetParameter (encoder->handle,
                    OMX_IndexParamVideoAvc, &avc_st))){
        DEBUG_ERR("error: OMX_SetParameter: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Inline SPS/PPS
    OMX_CONFIG_PORTBOOLEANTYPE headers_st;
    OMX_INIT_STRUCTURE (headers_st);
    headers_st.nPortIndex = 201;
    headers_st.bEnabled = VIDEO_INLINE_HEADERS;
    if ((error = OMX_SetParameter (encoder->handle,
                    OMX_IndexParamBrcmVideoAVCInlineHeaderEnable, &headers_st))){
        DEBUG_ERR("error: OMX_SetParameter: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Note: Motion vectors are not implemented in this program.
    //See for further details
    //https://github.com/gagle/raspberrypi-omxcam/blob/master/src/h264.c
    //https://github.com/gagle/raspberrypi-omxcam/blob/master/src/video.c
}

void omx_h264_init()
{
    strncpy(camera_name, "OMX.broadcom.camera", strlen("OMX.broadcom.camera"));
    strncpy(encoder_name, "OMX.broadcom.video_encode", strlen("OMX.broadcom.video_encode"));
    strncpy(null_sink_name, "OMX.broadcom.null_sink", strlen("OMX.broadcom.null_sink"));

    camera.name = &camera_name[0];
    encoder.name = &encoder_name[0];
    null_sink.name = &null_sink_name[0];
    
    //Initialize Broadcom's VideoCore APIs
    bcm_host_init ();

    //Initialize OpenMAX IL
    if ((error = OMX_Init ())){
        DEBUG_ERR("error: OMX_Init: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Initialize components
    init_component (&camera);
    init_component (&encoder);
    init_component (&null_sink);

    //Initialize camera drivers
    load_camera_drivers (&camera);

    //Configure camera port definition
    DEBUG_MSG("configuring %s port definition\n", camera.name);
    OMX_INIT_STRUCTURE (port_st);
    port_st.nPortIndex = 71;
    if ((error = OMX_GetParameter (camera.handle, OMX_IndexParamPortDefinition,
                    &port_st))){
        DEBUG_ERR("error: OMX_GetParameter: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    port_st.format.video.nFrameWidth = CAM_WIDTH;
    port_st.format.video.nFrameHeight = CAM_HEIGHT;
    port_st.format.video.nStride = CAM_WIDTH;
    port_st.format.video.xFramerate = VIDEO_FRAMERATE << 16;
    port_st.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    port_st.format.video.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
    if ((error = OMX_SetParameter (camera.handle, OMX_IndexParamPortDefinition,
                    &port_st))){
        DEBUG_ERR("error: OMX_SetParameter: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Preview port
    port_st.nPortIndex = 70;
    if ((error = OMX_SetParameter (camera.handle, OMX_IndexParamPortDefinition,
                    &port_st))){
        DEBUG_ERR("error: OMX_SetParameter: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    DEBUG_MSG("configuring %s framerate\n", camera.name); 
    OMX_INIT_STRUCTURE (framerate_st);
    framerate_st.nPortIndex = 71;
    framerate_st.xEncodeFramerate = port_st.format.video.xFramerate;
    if ((error = OMX_SetConfig (camera.handle, OMX_IndexConfigVideoFramerate,
                    &framerate_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Preview port
    framerate_st.nPortIndex = 70;
    if ((error = OMX_SetConfig (camera.handle, OMX_IndexConfigVideoFramerate,
                    &framerate_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Configure camera settings
    set_camera_settings (&camera);

    //Configure encoder port definition
    DEBUG_MSG("configuring %s port definition\n", encoder.name);
    OMX_INIT_STRUCTURE (port_st);
    port_st.nPortIndex = 201;
    if ((error = OMX_GetParameter (encoder.handle, OMX_IndexParamPortDefinition,
                    &port_st))){
        DEBUG_ERR("error: OMX_GetParameter: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }
    port_st.format.video.nFrameWidth = CAM_WIDTH;
    port_st.format.video.nFrameHeight = CAM_HEIGHT;
    port_st.format.video.nStride = CAM_WIDTH;
    port_st.format.video.xFramerate = VIDEO_FRAMERATE << 16;
    //Despite being configured later, these two fields need to be set
    port_st.format.video.nBitrate = VIDEO_QP ? 0 : VIDEO_BITRATE;
    port_st.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
    if ((error = OMX_SetParameter (encoder.handle, OMX_IndexParamPortDefinition,
                    &port_st))){
        DEBUG_ERR("error: OMX_SetParameter: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Configure H264
    set_h264_settings (&encoder);

    //Setup tunnels: camera (video) -> video_encode, camera (preview) -> null_sink
    DEBUG_MSG("configuring tunnels\n");
    if ((error = OMX_SetupTunnel (camera.handle, 71, encoder.handle, 200))){
        DEBUG_ERR("error: OMX_SetupTunnel: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }
    if ((error = OMX_SetupTunnel (camera.handle, 70, null_sink.handle, 240))){
        DEBUG_ERR("error: OMX_SetupTunnel: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Change state to IDLE
    change_state (&camera, OMX_StateIdle);
    wait (&camera, EVENT_STATE_SET, 0);
    change_state (&encoder, OMX_StateIdle);
    wait (&encoder, EVENT_STATE_SET, 0);
    change_state (&null_sink, OMX_StateIdle);
    wait (&null_sink, EVENT_STATE_SET, 0);

    //Enable the ports
    enable_port (&camera, 71);
    wait (&camera, EVENT_PORT_ENABLE, 0);
    enable_port (&camera, 70);
    wait (&camera, EVENT_PORT_ENABLE, 0);
    enable_port (&null_sink, 240);
    wait (&null_sink, EVENT_PORT_ENABLE, 0);
    enable_port (&encoder, 200);
    wait (&encoder, EVENT_PORT_ENABLE, 0);
    enable_encoder_output_port (&encoder, &encoder_output_buffer);

    //Change state to EXECUTING
    change_state (&camera, OMX_StateExecuting);
    wait (&camera, EVENT_STATE_SET, 0);
    change_state (&encoder, OMX_StateExecuting);
    wait (&encoder, EVENT_STATE_SET, 0);
    wait (&encoder, EVENT_PORT_SETTINGS_CHANGED, 0);
    change_state (&null_sink, OMX_StateExecuting);
    wait (&null_sink, EVENT_STATE_SET, 0);

    //Enable camera capture port. This basically says that the port 71 will be
    //used to get data from the camera. If you're capturing a still, the port 72
    //must be used
    DEBUG_MSG("enabling %s capture port\n", camera.name);
    OMX_INIT_STRUCTURE (capture_st);
    capture_st.nPortIndex = 71;
    capture_st.bEnabled = OMX_TRUE;
    if ((error = OMX_SetConfig (camera.handle, OMX_IndexConfigPortCapturing,
                    &capture_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }
}

void omx_h264_deinit()
{
    //Disable camera capture port
    DEBUG_MSG("disabling %s capture port\n", camera.name);
    capture_st.bEnabled = OMX_FALSE;
    if ((error = OMX_SetConfig (camera.handle, OMX_IndexConfigPortCapturing,
                    &capture_st))){
        DEBUG_ERR("error: OMX_SetConfig: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Change state to IDLE
    change_state (&camera, OMX_StateIdle);
    wait (&camera, EVENT_STATE_SET, 0);
    change_state (&encoder, OMX_StateIdle);
    wait (&encoder, EVENT_STATE_SET, 0);
    change_state (&null_sink, OMX_StateIdle);
    wait (&null_sink, EVENT_STATE_SET, 0);

    //Disable the tunnel ports
    disable_port (&camera, 71);
    wait (&camera, EVENT_PORT_DISABLE, 0);
    disable_port (&camera, 70);
    wait (&camera, EVENT_PORT_DISABLE, 0);
    disable_port (&null_sink, 240);
    wait (&null_sink, EVENT_PORT_DISABLE, 0);
    disable_port (&encoder, 200);
    wait (&encoder, EVENT_PORT_DISABLE, 0);
    disable_encoder_output_port (&encoder, encoder_output_buffer);

    //Change state to LOADED
    change_state (&camera, OMX_StateLoaded);
    wait (&camera, EVENT_STATE_SET, 0);
    change_state (&encoder, OMX_StateLoaded);
    wait (&encoder, EVENT_STATE_SET, 0);
    change_state (&null_sink, OMX_StateLoaded);
    wait (&null_sink, EVENT_STATE_SET, 0);

    //Deinitialize components
    deinit_component (&camera);
    deinit_component (&encoder);
    deinit_component (&null_sink);

    //Deinitialize OpenMAX IL
    if ((error = OMX_Deinit ())){
        DEBUG_ERR("error: OMX_Deinit: %s\n", dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Deinitialize Broadcom's VideoCore APIs
    bcm_host_deinit ();
}

OMX_BUFFERHEADERTYPE* fill_frame_buffer()
{
    //Get the buffer data
    if ((error = OMX_FillThisBuffer (encoder.handle, encoder_output_buffer))){
        DEBUG_ERR("error: OMX_FillThisBuffer: %s\n",
                dump_OMX_ERRORTYPE (error));
        exit (1);
    }

    //Wait until it's filled
    wait (&encoder, EVENT_FILL_BUFFER_DONE, 0);

    return encoder_output_buffer;
}
