
typedef unsigned char           RK_U8;
typedef unsigned short          RK_U16;
typedef unsigned int            RK_U32;
typedef unsigned long long int  RK_U64;


typedef signed char             RK_S8;
typedef signed short            RK_S16;
typedef signed int              RK_S32;
typedef signed long long int    RK_S64;

typedef signed int              RK_RET;

#define RK_OK                   0
#define RK_SUCCESS              0
#define RK_NOK                  -1

typedef enum {
    MPP_SUCCESS                 = RK_SUCCESS,
    MPP_OK                      = RK_OK,

    MPP_NOK                     = -1,
    MPP_ERR_UNKNOW              = -2,
    MPP_ERR_NULL_PTR            = -3,
    MPP_ERR_MALLOC              = -4,
    MPP_ERR_OPEN_FILE           = -5,
    MPP_ERR_VALUE               = -6,
    MPP_ERR_READ_BIT            = -7,

    MPP_ERR_BASE                = -1000,

    MPP_ERR_LIST_STREAM         = MPP_ERR_BASE - 1,
    MPP_ERR_INIT                = MPP_ERR_BASE - 2,
    MPP_ERR_VPU_CODEC_INIT      = MPP_ERR_BASE - 3,
    MPP_ERR_STREAM              = MPP_ERR_BASE - 4,
    MPP_ERR_FATAL_THREAD        = MPP_ERR_BASE - 5,
    MPP_ERR_NOMEM               = MPP_ERR_BASE - 6,
    MPP_ERR_PROTOL              = MPP_ERR_BASE - 7,
    MPP_FAIL_SPLIT_FRAME        = MPP_ERR_BASE - 8,
    MPP_ERR_VPUHW               = MPP_ERR_BASE - 9,
    MPP_EOS_STREAM_REACHED      = MPP_ERR_BASE - 11,

} MPP_RET;

#define CMD_MODULE_ID_MASK              (0x00F00000)
#define CMD_MODULE_OSAL                 (0x00100000)
#define CMD_MODULE_MPP                  (0x00200000)
#define CMD_MODULE_CODEC                (0x00300000)
#define CMD_MODULE_HAL                  (0x00400000)
#define CMD_CTX_ID_MASK                 (0x000F0000)
#define CMD_CTX_ID_DEC                  (0x00010000)
#define CMD_CTX_ID_ENC                  (0x00020000)
#define CMD_CTX_ID_ISP                  (0x00030000)
#define CMD_ID_MASK                     (0x0000FFFF)

typedef enum {
    MPP_OSAL_CMD_BASE                   = CMD_MODULE_OSAL,
    MPP_OSAL_CMD_END,

    MPP_CMD_BASE                        = CMD_MODULE_MPP,
    MPP_ENABLE_DEINTERLACE,
    MPP_SET_INPUT_BLOCK,
    MPP_SET_OUTPUT_BLOCK,
    MPP_CMD_END,

    MPP_CODEC_CMD_BASE                  = CMD_MODULE_CODEC,
    MPP_CODEC_GET_FRAME_INFO,
    MPP_CODEC_CMD_END,

    MPP_DEC_CMD_BASE                    = CMD_MODULE_CODEC | CMD_CTX_ID_DEC,
    MPP_DEC_SET_FRAME_INFO,             /* vpu api legacy control for buffer slot dimension init */
    MPP_DEC_SET_EXT_BUF_GROUP,          /* IMPORTANT: set external buffer group to mpp decoder */
    MPP_DEC_SET_INFO_CHANGE_READY,
    MPP_DEC_SET_INTERNAL_PTS_ENABLE,
    MPP_DEC_SET_PARSER_SPLIT_MODE,      /* Need to setup before init */
    MPP_DEC_SET_PARSER_FAST_MODE,       /* Need to setup before init */
    MPP_DEC_GET_STREAM_COUNT,
    MPP_DEC_GET_VPUMEM_USED_COUNT,
    MPP_DEC_SET_VC1_EXTRA_DATA,
    MPP_DEC_SET_OUTPUT_FORMAT,
    MPP_DEC_CMD_END,

    MPP_ENC_CMD_BASE                    = CMD_MODULE_CODEC | CMD_CTX_ID_ENC,
    MPP_ENC_SET_RC_CFG,
    MPP_ENC_GET_RC_CFG,
    MPP_ENC_SET_PREP_CFG,
    MPP_ENC_GET_PREP_CFG,
    MPP_ENC_SET_OSD_PLT_CFG,            /* set OSD palette, parameter should be pointer to MppEncOSDPlt */
    MPP_ENC_SET_OSD_DATA_CFG,           /* set OSD data with at most 8 regions, parameter should be pointer to MppEncOSDData */
    MPP_ENC_GET_OSD_CFG,
    MPP_ENC_SET_CFG,
    MPP_ENC_GET_CFG,
    MPP_ENC_SET_EXTRA_INFO,
    MPP_ENC_GET_EXTRA_INFO,
    MPP_ENC_SET_FORMAT,
    MPP_ENC_SET_IDR_FRAME,
    MPP_ENC_CMD_END,

    MPP_ISP_CMD_BASE                    = CMD_MODULE_CODEC | CMD_CTX_ID_ISP,
    MPP_ISP_CMD_END,

    MPP_HAL_CMD_BASE                    = CMD_MODULE_HAL,
    MPP_HAL_CMD_END,

    MPI_CMD_BUTT,
} MpiCmd;

typedef enum {
    MPP_PORT_INPUT,
    MPP_PORT_OUTPUT,
    MPP_PORT_BUTT,
} MppPortType;

typedef enum {
    MPP_CTX_DEC,
    MPP_CTX_ENC,
    MPP_CTX_ISP,
    MPP_CTX_BUTT,
} MppCtxType;

typedef enum {
    MPP_VIDEO_CodingUnused,             /**< Value when coding is N/A */
    MPP_VIDEO_CodingAutoDetect,         /**< Autodetection of coding type */
    MPP_VIDEO_CodingMPEG2,              /**< AKA: H.262 */
    MPP_VIDEO_CodingH263,               /**< H.263 */
    MPP_VIDEO_CodingMPEG4,              /**< MPEG-4 */
    MPP_VIDEO_CodingWMV,                /**< Windows Media Video (WMV1,WMV2,WMV3)*/
    MPP_VIDEO_CodingRV,                 /**< all versions of Real Video */
    MPP_VIDEO_CodingAVC,                /**< H.264/AVC */
    MPP_VIDEO_CodingMJPEG,              /**< Motion JPEG */
    MPP_VIDEO_CodingVP8,                /**< VP8 */
    MPP_VIDEO_CodingVP9,                /**< VP9 */
    MPP_VIDEO_CodingVC1 = 0x01000000,   /**< Windows Media Video (WMV1,WMV2,WMV3)*/
    MPP_VIDEO_CodingFLV1,               /**< Sorenson H.263 */
    MPP_VIDEO_CodingDIVX3,              /**< DIVX3 */
    MPP_VIDEO_CodingVP6,
    MPP_VIDEO_CodingHEVC,               /**< H.265/HEVC */
    MPP_VIDEO_CodingAVS,                /**< AVS+ */
    MPP_VIDEO_CodingKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    MPP_VIDEO_CodingVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    MPP_VIDEO_CodingMax = 0x7FFFFFFF
} MppCodingType;

typedef enum {
    MPP_BUFFER_TYPE_NORMAL,
    MPP_BUFFER_TYPE_ION,
    MPP_BUFFER_TYPE_V4L2,
    MPP_BUFFER_TYPE_DRM,
    MPP_BUFFER_TYPE_BUTT,
} MppBufferType;

typedef enum {
    MPP_BUFFER_INTERNAL,
    MPP_BUFFER_EXTERNAL,
    MPP_BUFFER_MODE_BUTT,
} MppBufferMode;

typedef void* MppCtx;
typedef void* MppPacket;
typedef void* MppFrame;
typedef void* MppParam;
typedef void* MppHdl;
typedef void* MppTask;
typedef void* MppBufferGroup;
typedef void* MppBuffer;

typedef struct MppApi_t {
    RK_U32  size;
    RK_U32  version;

    // simple data flow interface
    MPP_RET (*decode)(MppCtx ctx, MppPacket packet, MppFrame *frame);
    MPP_RET (*decode_put_packet)(MppCtx ctx, MppPacket packet);
    MPP_RET (*decode_get_frame)(MppCtx ctx, MppFrame *frame);

    MPP_RET (*encode)(MppCtx ctx, MppFrame frame, MppPacket *packet);
    MPP_RET (*encode_put_frame)(MppCtx ctx, MppFrame frame);
    MPP_RET (*encode_get_packet)(MppCtx ctx, MppPacket *packet);

    MPP_RET (*isp)(MppCtx ctx, MppFrame dst, MppFrame src);
    MPP_RET (*isp_put_frame)(MppCtx ctx, MppFrame frame);
    MPP_RET (*isp_get_frame)(MppCtx ctx, MppFrame *frame);

    // advance data flow interface
    MPP_RET (*dequeue)(MppCtx ctx, MppPortType type, MppTask *task);
    MPP_RET (*enqueue)(MppCtx ctx, MppPortType type, MppTask task);

    // control interface
    MPP_RET (*reset)(MppCtx ctx);
    MPP_RET (*control)(MppCtx ctx, MpiCmd cmd, MppParam param);

    RK_U32 reserv[16];
} MppApi;

typedef struct MppHandle_t {
    MppHdl hdl;
    MPP_RET (*mpp_create)(MppCtx *ctx, MppApi **mpi);
    MPP_RET (*mpp_init)(MppCtx ctx, MppCtxType type, MppCodingType coding);
    MPP_RET (*mpp_destroy)(MppCtx ctx);
    MPP_RET (*mpp_check_support_format)(MppCtxType type, MppCodingType coding);
    void    (*mpp_show_support_format)();
} MppHandle;

typedef struct MppBufferInfo_t {
    MppBufferType   type;
    size_t          size;
    void            *ptr;
    void            *hnd;
    int             fd;
    int             index;
} MppBufferInfo;

typedef struct MppBufferGroupApi_t {
    MPP_RET (*mpp_buffer_group_get)(MppBufferGroup *group, MppBufferType type, MppBufferMode mode,
                                    const char *tag, const char *caller);
    MPP_RET (*mpp_buffer_import_with_tag)(MppBufferGroup group, MppBufferInfo *info, MppBuffer *buffer,
                                    const char *tag, const char *caller);    
    MPP_RET (*mpp_buffer_group_clear)(MppBufferGroup group);
    MPP_RET (*mpp_buffer_get_with_tag)(MppBufferGroup group, MppBuffer *buffer, size_t size,
                                    const char *tag, const char *caller);    
    MPP_RET (*mpp_buffer_put_with_caller)(MppBuffer buffer, const char *caller);
    MPP_RET (*mpp_buffer_inc_ref_with_caller)(MppBuffer buffer, const char *caller);
} MppBufferGroupApi;

class MpiWrapper {
public:
    MpiWrapper();
    ~MpiWrapper();
    MPP_RET init(MppCodingType coding, RK_U32 width, RK_U32 height);
    MPP_RET decode_put_packet(MppPacket pkt);
    MPP_RET decode_get_frame(MppFrame *frame);
    MPP_RET decode_reset();
    MPP_RET buffer_open_mem_pool(MppBufferGroup *group);
    MPP_RET buffer_reset_mem_pool(MppBufferGroup group);
    MPP_RET buffer_commit(MppBufferGroup group, MppBufferInfo *info);
    MPP_RET buffer_get_unused(MppBufferGroup group, MppBuffer *buffer, size_t size);
    MPP_RET buffer_free(MppBuffer buffer);
	MPP_RET buffer_inc_ref(MppBuffer buffer);

private:
    MppHandle mpp_hdl;
    MppCtx mpp_ctx;
    MppApi *mpi;
    MppBufferGroupApi *mbi;
    RK_U32 init_ok;
};

