#include <unistd.h>
#include <dlfcn.h>
#include<assert.h>

#include "MpiWrapper.h"
#include "utils/log.h"

#define mpp_err(s) CLog::Log(LOGERROR, s)

static const char* mpi_paths[] = {
    "/system/lib/libmpp.so"
};

MpiWrapper::MpiWrapper() :
    mpp_ctx(NULL),
    mpi(NULL), 
    mbi(NULL),
    init_ok(0)
{
    mpp_hdl.hdl         = NULL;
    mpp_hdl.mpp_create  = NULL;
    mpp_hdl.mpp_init    = NULL;
    mpp_hdl.mpp_destroy = NULL;
    mpp_hdl.mpp_check_support_format    = NULL;
    mpp_hdl.mpp_show_support_format     = NULL;

    mbi = (MppBufferGroupApi*)malloc(sizeof(MppBufferGroupApi));
    memset(mbi, 0, sizeof(MppBufferGroupApi));
    
    for (RK_U32 i = 0; i < (sizeof(mpi_paths) / sizeof((mpi_paths)[0])); i++) {
        mpp_hdl.hdl = dlopen(mpi_paths[i], RTLD_LAZY);
        if (mpp_hdl.hdl)
            break;
    }

    if (mpp_hdl.hdl) {
        mpp_hdl.mpp_create = (MPP_RET (*)(MppCtx *ctx, MppApi **mpi))dlsym(mpp_hdl.hdl, "mpp_create");
        mpp_hdl.mpp_init = (MPP_RET (*)(MppCtx ctx, MppCtxType type, MppCodingType coding))dlsym(mpp_hdl.hdl, "mpp_init");
        mpp_hdl.mpp_destroy = ( MPP_RET (*)(MppCtx ctx))dlsym(mpp_hdl.hdl, "mpp_destroy");
        mpp_hdl.mpp_check_support_format = (MPP_RET (*)(MppCtxType type, MppCodingType coding))dlsym(mpp_hdl.hdl, "mpp_check_support_format");
        mpp_hdl.mpp_show_support_format = (void (*)())dlsym(mpp_hdl.hdl, "mpp_show_support_format");
    }

    assert(mpp_hdl.mpp_create);
    assert(mpp_hdl.mpp_init);
    assert(mpp_hdl.mpp_destroy);

    mpp_hdl.mpp_create(&mpp_ctx, &mpi);

    assert(mpp_ctx);
    assert(mpi);

    assert(mbi);
    mbi->mpp_buffer_group_get =  (MPP_RET (*)(MppBufferGroup *group, MppBufferType type, MppBufferMode mode,
                                    const char *tag, const char *caller))dlsym(mpp_hdl.hdl, "mpp_buffer_group_get");
    mbi->mpp_buffer_import_with_tag = (MPP_RET (*)(MppBufferGroup group, MppBufferInfo *info, MppBuffer *buffer,
                                    const char *tag, const char *caller))dlsym(mpp_hdl.hdl, "mpp_buffer_import_with_tag");
    mbi->mpp_buffer_group_clear = (MPP_RET (*)(MppBufferGroup group))dlsym(mpp_hdl.hdl, "mpp_buffer_group_clear");
    mbi->mpp_buffer_get_with_tag =  (MPP_RET (*)(MppBufferGroup group, MppBuffer *buffer, size_t size,
                                    const char *tag, const char *caller))dlsym(mpp_hdl.hdl, "mpp_buffer_get_with_tag");
    mbi->mpp_buffer_put_with_caller = (MPP_RET (*)(MppBuffer buffer, const char *caller))dlsym(mpp_hdl.hdl, "mpp_buffer_put_with_caller");
    mbi->mpp_buffer_inc_ref_with_caller = (MPP_RET (*)(MppBuffer buffer, const char *caller))dlsym(mpp_hdl.hdl, "mpp_buffer_inc_ref_with_caller");

    assert(mbi->mpp_buffer_group_get);
    assert(mbi->mpp_buffer_import_with_tag);
    assert(mbi->mpp_buffer_group_clear);
    assert(mbi->mpp_buffer_get_with_tag);
    assert(mbi->mpp_buffer_put_with_caller);
    assert(mbi->mpp_buffer_inc_ref_with_caller);
    
}

MPP_RET MpiWrapper::init(MppCodingType coding, RK_U32 width, RK_U32 height)
{
    MPP_RET ret = MPP_OK;
    RK_U32 block_input = 0;
    RK_U32 block_output = 0;
    
    if (mpp_ctx == NULL || mpi == NULL) {
        mpp_err("found invalid context input.\n");
        return MPP_ERR_NULL_PTR;
    }

    
    /* setup input / output block mode 
           decoder: input=0, output=0
           encoder: input=0, output=1
      */
    ret = mpi->control(mpp_ctx, MPP_SET_INPUT_BLOCK, (MppParam)&block_input);
    if (MPP_OK != ret) {
        mpp_err("mpi->control MPP_SET_INPUT_BLOCK failed.\n");
    }
    
    ret = mpi->control(mpp_ctx, MPP_SET_OUTPUT_BLOCK, (MppParam)&block_output);
    if (MPP_OK != ret) {
        mpp_err("mpi->control MPP_SET_OUTPUT_BLOCK failed.\n");
    }

    ret = mpp_hdl.mpp_init(mpp_ctx, MPP_CTX_DEC, coding);
    if (ret) {
        mpp_err(" init error.\n");
        return ret;
    }

    init_ok = 1;
    return ret;
}

MPP_RET MpiWrapper::decode_put_packet(MppPacket pkt)
{
    MPP_RET ret = MPP_OK;

    if (!init_ok) {
        mpp_err("mpi invalid init.\n");
        return MPP_ERR_VPU_CODEC_INIT;
    }

    ret = mpi->decode_put_packet(mpp_ctx, pkt);
    if (ret != MPP_OK) {
        /* mpi max cache four packets */
        usleep(1000);
    }
    return ret;
}

MPP_RET MpiWrapper::decode_get_frame(MppFrame *pframe)
{
    MPP_RET ret = MPP_OK;
    
    if (!init_ok) {
        mpp_err("mpi invalid init.\n");
        return MPP_ERR_VPU_CODEC_INIT;
    }

    ret = mpi->decode_get_frame(mpp_ctx, pframe);
    return ret;
}

MPP_RET MpiWrapper::decode_reset()
{
    MPP_RET ret = MPP_OK;
    
    if (!init_ok) {
        mpp_err("mpi invalid init.\n");
        return MPP_ERR_VPU_CODEC_INIT;
    }

    ret = mpi->reset(mpp_ctx);
    return ret;
}

MPP_RET MpiWrapper::buffer_open_mem_pool(MppBufferGroup *group)
{
    MPP_RET ret = MPP_OK;

    if (!init_ok) {
        mpp_err("mpi invalid init.\n");
        return MPP_ERR_VPU_CODEC_INIT;
    }

    mbi->mpp_buffer_group_get(group, MPP_BUFFER_TYPE_ION, MPP_BUFFER_EXTERNAL, 
                            "MpiWrapper", __FUNCTION__);
    return ret;
}

MPP_RET MpiWrapper::buffer_reset_mem_pool(MppBufferGroup group)
{
    MPP_RET ret = MPP_OK;

    if (!init_ok) {
        mpp_err("mpi invalid init.\n");
        return MPP_ERR_VPU_CODEC_INIT;
    }

    assert(group);
    mbi->mpp_buffer_group_clear(group);
    return ret;
}

MPP_RET MpiWrapper::buffer_commit(MppBufferGroup group, MppBufferInfo *info)
{
    MPP_RET ret = MPP_OK;
    
    if (!init_ok) {
        mpp_err("mpi invalid init.\n");
        return MPP_ERR_VPU_CODEC_INIT;
    }

    assert(group);
    mbi->mpp_buffer_import_with_tag(group, info, NULL, "MpiWrapper", __FUNCTION__);
    return ret;
}

MPP_RET MpiWrapper::buffer_get_unused(MppBufferGroup group, MppBuffer *buffer, size_t size)
{
    MPP_RET ret = MPP_OK;
    
    if (!init_ok) {
        mpp_err("mpi invalid init.\n");
        return MPP_ERR_VPU_CODEC_INIT;
    }

    assert(group);
    assert(size);
    mbi->mpp_buffer_get_with_tag(group, buffer, size, "MpiWrapper", __FUNCTION__);
    return ret;
}

MPP_RET MpiWrapper::buffer_free(MppBuffer buffer)
{
    MPP_RET ret = MPP_OK;
    
    if (!init_ok) {
        mpp_err("mpi invalid init.\n");
        return MPP_ERR_VPU_CODEC_INIT;
    }

    assert(buffer);
    mbi->mpp_buffer_put_with_caller(buffer, "MpiWrapper");
    return ret;
}

MPP_RET MpiWrapper::buffer_inc_ref(MppBuffer buffer)
{
    MPP_RET ret = MPP_OK;

    if (!init_ok) {
        mpp_err("mpi invalid init.\n");
        return MPP_ERR_VPU_CODEC_INIT;
    }
    assert(buffer);
    mbi->mpp_buffer_inc_ref_with_caller(buffer, "MpiWrapper");
    return ret;
}


