/*\
 *
 * PPA operation utils for LovyanGFX/M5GFX
 *
 * An incomplete experiment brought to you by tobozo, copyleft (c+) Aug. 2025
 *
\*/
#pragma once

#include "./gfx_utils.hpp" // M5Unified of LovyanGFX

#if __has_include(<M5GFX.hpp>) || __has_include(<M5Unified.hpp>)
  using m5gfx::Panel_DSI;
  #define GFX_BASE M5GFX
#elif __has_include(<LovyanGFX.hpp>)
  using lgfx::Panel_DSI;
  #define GFX_BASE LovyanGFX
#else
  #error "Please include M5GFX.hpp, M5Unified.hpp or LovyanGFX.hpp before including this file"
#endif

#if !defined SOC_MIPI_DSI_SUPPORTED || !defined CONFIG_IDF_TARGET_ESP32P4
  #error "PPA is only available with ESP32P4, and this implementation depends on MIPI/DSI support"
#endif

extern "C"
{
 #include "driver/ppa.h"
 #include "esp_heap_caps.h"
 #include "esp_cache.h"
 #include "esp_private/esp_cache_private.h"

 #define PPA_ALIGN_UP(x, align)  ((((x) + (align) - 1) / (align)) * (align))
 #define PPA_PTR_ALIGN_UP(p, align) ((void*)(((uintptr_t)(p) + (uintptr_t)((align) - 1)) & ~(uintptr_t)((align) - 1)))

 #define PPA_ALIGN_DOWN(x, align)  ((((x) - (align) - 1) / (align)) * (align))
 #define PPA_PTR_ALIGN_DOWN(p, align) ((void*)(((uintptr_t)(p) - (uintptr_t)((align) - 1)) & ~(uintptr_t)((align) - 1)))

 #define CSIZE CONFIG_CACHE_L1_CACHE_LINE_SIZE
 #define PPA_RESET_CACHE(d, s) esp_cache_msync((void *)PPA_PTR_ALIGN_DOWN(d, CSIZE), PPA_ALIGN_DOWN(s, CSIZE), ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);

}


namespace lgfx
{

  // debug tags for all ppa utils
  static constexpr const char* PPA_SPRITE_TAG = "PPA_Sprite";
  static constexpr const char* PPA_TAG        = "PPABase";
  static constexpr const char* SRM_TAG        = "PPA_SRM";
  static constexpr const char* BLEND_TAG      = "PPA_BLEND";
  static constexpr const char* FILL_TAG       = "PPA_FILL";

  class PPA_Sprite;
  class PPABase;
  class PPAFill;
  class PPABlend;
  class PPASrm;


  // debug helper to compensate for typeid() being disabled by compiler
  template <typename T> const char *TYPE_NAME()
  {
    #ifdef _MSC_VER
      return __FUNCSIG__;
    #else
      return __PRETTY_FUNCTION__;
    #endif
  }

  // memory helper for ppa operations
  static inline void* heap_alloc_ppa(size_t length, size_t*size=nullptr)
  {
    size_t cache_line_size;
    if(esp_cache_get_alignment(MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA, &cache_line_size)!=ESP_OK)
      return nullptr;
    assert(cache_line_size>=2);
    auto mask = cache_line_size-1;
    size_t aligned = (length + mask) & ~mask;
    auto ret = heap_caps_aligned_calloc(cache_line_size, 1, aligned, MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA);
    if( size ) {
      if( ret )
        *size = aligned;
      else
        *size = 0;
    }
    return ret;
  }

  // on_trans_done() callbacks
  bool lgfx_ppa_cb_sem_func(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data);
  bool lgfx_ppa_cb_bool_func(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data);

  // a little debug helper
  const char* ppa_operation_type_to_string( ppa_operation_t oper_type )
  {
    static constexpr const char* srm     = "srm";
    static constexpr const char* blend   = "blend";
    static constexpr const char* fill    = "fill";
    static constexpr const char* unknown = "unknown";
    switch(oper_type)
    {
      case PPA_OPERATION_BLEND: return blend; break;
      case PPA_OPERATION_SRM:   return srm;   break;
      case PPA_OPERATION_FILL:  return fill;  break;
      default:break;
    }
    return unknown;
  }


  static inline ppa_fill_color_mode_t ppa_fill_color_mode(uint8_t lgfx_color_bit_depth)
  {
    if( lgfx_color_bit_depth==16 )
      return PPA_FILL_COLOR_MODE_RGB565;
    if( lgfx_color_bit_depth==24 )
      return PPA_FILL_COLOR_MODE_RGB888;
    if( lgfx_color_bit_depth==32 )
      return PPA_FILL_COLOR_MODE_ARGB8888;
    return PPA_FILL_COLOR_MODE_RGB565;
  }


  static inline ppa_blend_color_mode_t ppa_blend_color_mode(uint8_t lgfx_color_bit_depth)
  {
    if( lgfx_color_bit_depth==16 )
      return PPA_BLEND_COLOR_MODE_RGB565;
    if( lgfx_color_bit_depth==24 )
      return PPA_BLEND_COLOR_MODE_RGB888;
    if( lgfx_color_bit_depth==32 )
      return PPA_BLEND_COLOR_MODE_ARGB8888;
    return PPA_BLEND_COLOR_MODE_RGB565;
  }


  static inline ppa_srm_color_mode_t ppa_srm_color_mode(uint8_t lgfx_color_bit_depth)
  {
    if( lgfx_color_bit_depth==16 )
      return PPA_SRM_COLOR_MODE_RGB565;
    if( lgfx_color_bit_depth==24 )
      return PPA_SRM_COLOR_MODE_RGB888;
    if( lgfx_color_bit_depth==32 )
      return PPA_SRM_COLOR_MODE_ARGB8888;
    return PPA_SRM_COLOR_MODE_RGB565;
  }

  // ---------------------------------------------------------------------------------------------



  // Same as LGFX_Sprite but with aligned memory for ppa operations, and restricted to 16 bits colors
  class PPA_Sprite : public LGFX_Sprite
  {
  public:

    PPA_Sprite(LovyanGFX* parent)
    : LGFX_Sprite(parent)
    {
      _panel = &_panel_sprite;
      setColorDepth(16);
      setPsram(true);
    }

    PPA_Sprite()
    : PPA_Sprite(nullptr)
    {}

    void* createSprite(int32_t w, int32_t h)
    {
      uint8_t bit_depth  = getColorDepth();
      uint8_t byte_depth = bit_depth/8;

      if(byte_depth<2) {
        ESP_LOGE(PPA_SPRITE_TAG, "only supports 16bits colors and more");
        return nullptr;
      }

      if(w<=0 || h<=0) {
        ESP_LOGE(PPA_SPRITE_TAG, "Invalid requested dimensions: [%d*%d]", w, h);
        return nullptr;
      }

      void* buf = lgfx::heap_alloc_ppa(w * h * byte_depth);

      if(!buf)
        return nullptr;

      setBuffer(buf, w, h, bit_depth);
      return _img;
    }

  };

  static SemaphoreHandle_t ppa_semaphore = nullptr;


  // ---------------------------------------------------------------------------------------------



  class PPABase
  {
  protected:
    ppa_client_config_t ppa_client_config;
    ppa_client_handle_t ppa_client_handle = nullptr;
    ppa_event_callbacks_t ppa_event_cb;

    volatile bool ppa_transfer_done = true;

    bool inited  = false;
    bool enabled = true;

    const bool async;
    const bool use_semaphore;

    const uint32_t output_w;
    const uint32_t output_h;

    uint8_t output_bytes_per_pixel;

    void* output_buffer = nullptr;

    GFX_BASE* outputGFX;

  public:


    ~PPABase()
    {
      if (async && use_semaphore && ppa_semaphore) {
        if( xSemaphoreTake(ppa_semaphore, ( TickType_t )10 ) != pdTRUE ) {
          ESP_LOGE(PPA_TAG, "Failed to take semaphore before deletion, incoming crash..?");
        }
      }

      if( ppa_client_handle )
        ppa_unregister_client(ppa_client_handle);
    }


    template <typename GFX>
    PPABase(GFX* out, ppa_operation_t oper_type, bool async = true, bool use_semaphore = false)
    : ppa_client_config({.oper_type=oper_type, .max_pending_trans_num=1, .data_burst_length=PPA_DATA_BURST_LENGTH_128}),
      async(async), use_semaphore(use_semaphore), output_w(out->width()), output_h(out->height()), outputGFX((GFX_BASE*)out)
    {
      if(!async && use_semaphore) {
        ESP_LOGW(PPA_TAG, "use_semaphore=true but async=false, async will be enabled anyway");
        async = true;
      }

      if( ppa_semaphore == NULL )
        ppa_semaphore = xSemaphoreCreateBinary();

      if(async && use_semaphore)
        ppa_event_cb.on_trans_done = lgfx_ppa_cb_sem_func;
      else
        ppa_event_cb.on_trans_done = lgfx_ppa_cb_bool_func;

      if( ESP_OK != ppa_register_client(&ppa_client_config, &ppa_client_handle) ) {
        ESP_LOGE(PPA_TAG, "Failed to ppa_register_client");
        enabled = false;
        return;
      }

      if( ESP_OK != ppa_client_register_event_callbacks(ppa_client_handle, &ppa_event_cb) ) {
        ESP_LOGE(PPA_TAG, "Failed to ppa_client_register_event_callbacks");
        enabled = false;
        return;
      }

      if( std::is_same<GFX, GFX_BASE>::value ) {
        output_buffer = ((Panel_DSI*)outputGFX->getPanel())->config_detail().buffer;
        output_bytes_per_pixel = 2; // panelDSI->getColorDepth() returns a weird value, so 16bits colors it is...
      } else if( std::is_same<GFX, LGFX_Sprite>::value || std::is_same<GFX, PPA_Sprite>::value  ) {
        if( outputGFX->getColorDepth() < 16 ) {
          ESP_LOGE(SRM_TAG, "Unsupported bit depth: %d", outputGFX->getColorDepth() );
          enabled = false;
          return;
        }
        output_bytes_per_pixel = out->getColorDepth()/8;
        output_buffer = ((LGFX_Sprite*)outputGFX)->getBuffer();
      } else {
        ESP_LOGE(SRM_TAG, "Unsupported GFX type: %s, accepted types are: LovyanGFX*, M5GFX*, LGFX_Sprite*, PPA_Sprite*", TYPE_NAME<GFX>() );
        enabled = false;
        return;
      }

      inited = true;
    }


    void setTransferDone(bool val)
    {
      ppa_transfer_done = val;
    }


    SemaphoreHandle_t getSemaphore()
    {
      return ppa_semaphore;
    }


    virtual bool available()
    {
      if(!enabled) {
        ESP_LOGE(PPA_TAG, "Call to available() when ppa_%s is disabled!", ppa_operation_type_to_string(ppa_client_config.oper_type) );
        return false;
      }
      return ppa_transfer_done;
    }


    template <typename T>
    bool exec(T *cfg)
    {
      if(!ready() || !cfg)
        return false;
      esp_err_t ret = ESP_FAIL;

      PPA_RESET_CACHE(output_buffer, output_w*output_h*output_bytes_per_pixel);

      switch(ppa_client_config.oper_type) {
        case PPA_OPERATION_SRM:   ret = ppa_do_scale_rotate_mirror(ppa_client_handle, (ppa_srm_oper_config_t*)cfg); break;
        case PPA_OPERATION_BLEND: ret = ppa_do_blend(ppa_client_handle, (ppa_blend_oper_config_t*)cfg); break;
        case PPA_OPERATION_FILL:  ret = ppa_do_fill(ppa_client_handle, (ppa_fill_oper_config_t*)cfg); break;
        default: ESP_LOGE(PPA_TAG, "Unimplemented PPA operation %d", ppa_client_config.oper_type);
      }

      if( ret!=ESP_OK ) { // callback failed, reset transfer
        setTransferDone(true);
        return false;
      }
      return true;
    }


  protected:

    bool ready()
    {
      if(!available()) {
        ESP_LOGV(PPA_TAG, "Skipping exec");
        return false;
      }

      if (async && use_semaphore) {
        if( xSemaphoreTake(ppa_semaphore, ( TickType_t )10 ) != pdTRUE ) {
          ESP_LOGE(PPA_TAG, "Failed to take semaphore");
          return false;
        }
      }

      setTransferDone(false);

      return true;
    }
  };



  // ---------------------------------------------------------------------------------------------



  // on_trans_done callback when using a semaphore
  bool IRAM_ATTR lgfx_ppa_cb_sem_func(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data)
  {
    if( user_data ) {
      auto ppa_emitter = (PPABase*)user_data;
      ppa_emitter->setTransferDone(true);
      auto sem = ppa_emitter->getSemaphore();
      if( sem ) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(sem, &xHigherPriorityTaskWoken);
        return (xHigherPriorityTaskWoken == pdTRUE);
      }
    }
    ESP_LOGE(PPA_TAG, "ppa callback triggered without semaphore");
    return false;
  }

  // on_trans_done callback when *not* using a semaphore
  bool lgfx_ppa_cb_bool_func(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data)
  {
    if( user_data ) {
      auto ppa_emitter = (PPABase*)user_data;
      ppa_emitter->setTransferDone(true);
    }
    return false;
  }



  // ---------------------------------------------------------------------------------------------



  class PPAFill : public PPABase
  {
  private:
     ppa_fill_oper_config_t oper_config =  ppa_fill_oper_config_t();

  public:

    template <typename GFX>
    PPAFill(GFX* out, bool async = true, bool use_semaphore = false)
    : PPABase(out, PPA_OPERATION_FILL, async, use_semaphore)
    { enabled = true; }

    template <typename T>
    bool fillRect( int32_t x, int32_t y, int32_t w, int32_t h, const T& color )
    {
      oper_config = {
        .out = {
          .buffer         = output_buffer,
          .buffer_size    = output_w*output_h*output_bytes_per_pixel,
          .pic_w          = output_w,
          .pic_h          = output_h,
          .block_offset_x = x,
          .block_offset_y = y,
          .fill_cm        = ppa_fill_color_mode( sizeof(T)*8 ), //  PPA_FILL_COLOR_MODE_RGB565,
          .yuv_range      = ppa_color_range_t(),
          .yuv_std        = ppa_color_conv_std_rgb_yuv_t()
        },
        .fill_block_w    = w,
        .fill_block_h    = h,
        .fill_argb_color = lgfx::color_convert<lgfx::argb8888_t , T>(color),
        .mode            = async ? PPA_TRANS_MODE_NON_BLOCKING : PPA_TRANS_MODE_BLOCKING,
        .user_data       = (void*)this,
      };

      return PPABase::exec(&oper_config);
    }

  };



  // ---------------------------------------------------------------------------------------------



  class PPABlend : public PPABase
  {
  private:
    ppa_blend_oper_config_t oper_config = ppa_blend_oper_config_t();

  public:

    template <typename GFX>
    PPABlend(GFX* out, bool async = true, bool use_semaphore = false)
    : PPABase(out, PPA_OPERATION_BLEND, async, use_semaphore)
    { enabled = true; }



    template <typename FG, typename BG, typename T>
    bool pushImageBlend(
      FG* fg, BG* bg,
      bool fg_ck_en=false, T fg_ck_col_low=0, T fg_ck_col_hi=0,
      bool bg_ck_en=false, T bg_ck_col_low=0, T bg_ck_col_hi=0,
      T fg_fix_col=0, T ck_default_col=0
    ) {
      assert(fg);
      assert(bg);

      if(!inited) {
        ESP_LOGE(BLEND_TAG, "Can't push after a failed init");
        return false;
      }

      if( output_bytes_per_pixel != 2 ) {
        ESP_LOGE(BLEND_TAG, "Unsupported output bit depth: %d", output_bytes_per_pixel*8 );
        return false;
      }

      color_pixel_rgb888_data_t bg_ck_rgb_low_thres  = (color_pixel_rgb888_data_t){};
      color_pixel_rgb888_data_t bg_ck_rgb_high_thres = (color_pixel_rgb888_data_t){};

      color_pixel_rgb888_data_t fg_ck_rgb_low_thres  = (color_pixel_rgb888_data_t){};
      color_pixel_rgb888_data_t fg_ck_rgb_high_thres = (color_pixel_rgb888_data_t){};

      color_pixel_rgb888_data_t fg_fix_rgb_val       = (color_pixel_rgb888_data_t){};
      color_pixel_rgb888_data_t ck_rgb_default_val   = (color_pixel_rgb888_data_t){};

      lgfx::rgb888_t srcColors[] = {
        bg_ck_col_low,        bg_ck_col_hi,          fg_ck_col_low,        fg_ck_col_hi,          fg_fix_col,      ck_default_col
      };
      color_pixel_rgb888_data_t* dstColors[] = {
        &bg_ck_rgb_low_thres, &bg_ck_rgb_high_thres, &fg_ck_rgb_low_thres, &fg_ck_rgb_high_thres, &fg_fix_rgb_val, &ck_rgb_default_val
      };

      size_t colorsize = sizeof(srcColors)/sizeof(lgfx::rgb888_t);
      for(int i=0;i<colorsize;i++) {
        dstColors[i]->r = srcColors[i].R8();
        dstColors[i]->g = srcColors[i].G8();
        dstColors[i]->b = srcColors[i].B8();
      }

      void *fg_buf;

      if( std::is_same<FG, GFX_BASE>::value ) {
        fg_buf = ((Panel_DSI*)outputGFX->getPanel())->config_detail().buffer; // assuming 16bpp
      } else if( std::is_same<FG, LGFX_Sprite>::value || std::is_same<FG, PPA_Sprite>::value  ) {
        fg_buf = ((LGFX_Sprite*)fg)->getBuffer();
        if( ((LGFX_Sprite*)fg)->getColorDepth() != 16 ) {
          ESP_LOGE(BLEND_TAG, "Unsupported fg input bit depth: %d", ((LGFX_Sprite*)fg)->getColorDepth() );
          return false;
        }
      } else {
        ESP_LOGE(BLEND_TAG, "Unsupported type: %s", TYPE_NAME<FG>() );
        return false;
      }

      void *bg_buf;

      if( std::is_same<BG, GFX_BASE>::value ) {
        bg_buf = ((Panel_DSI*)outputGFX->getPanel())->config_detail().buffer; // assuming 16bpp
      } else if( std::is_same<BG, LGFX_Sprite>::value || std::is_same<BG, PPA_Sprite>::value  ) {
        bg_buf = ((LGFX_Sprite*)bg)->getBuffer();
        if( ((LGFX_Sprite*)bg)->getColorDepth() != 16 ) {
          ESP_LOGE(BLEND_TAG, "Unsupported bg input bit depth: %d", ((LGFX_Sprite*)bg)->getColorDepth() );
          return false;
        }
      } else {
        ESP_LOGE(BLEND_TAG, "Unsupported type: %s", TYPE_NAME<BG>() );
        return false;
      }

      if(!fg_buf || !bg_buf || !output_buffer) {
        ESP_LOGE(BLEND_TAG, "At least one buffer is missing");
        return false;
      }

      // w/h for FG and BG should be identical and will be used as the w/h for the output block.
      if( fg->width() != output_w || fg->height() != output_h || bg->width() != output_w || bg->height() != output_h ) {
        ESP_LOGE(BLEND_TAG, "Dimensions mismatch");
        return false;
      }

      oper_config =
      {
        .in_bg =
        {
          .buffer         = bg_buf,
          .pic_w          = output_w,
          .pic_h          = output_h,
          .block_w        = output_w,
          .block_h        = output_h,
          .block_offset_x = 0,
          .block_offset_y = 0,
          .blend_cm       = ppa_blend_color_mode( bg->getColorDepth() ), // PPA_BLEND_COLOR_MODE_RGB565,
          .yuv_range      = ppa_color_range_t(),
          .yuv_std        = ppa_color_conv_std_rgb_yuv_t()
        },
        .in_fg =
        {
          .buffer         = fg_buf,
          .pic_w          = output_w,
          .pic_h          = output_h,
          .block_w        = output_w,
          .block_h        = output_h,
          .block_offset_x = 0,
          .block_offset_y = 0,
          .blend_cm       = ppa_blend_color_mode( fg->getColorDepth() ), // PPA_BLEND_COLOR_MODE_RGB565,
          .yuv_range      = ppa_color_range_t(),
          .yuv_std        = ppa_color_conv_std_rgb_yuv_t()
        },
        .out =
        {
          .buffer         = output_buffer,
          .buffer_size    = PPA_ALIGN_UP(output_w*output_h*output_bytes_per_pixel, CONFIG_CACHE_L1_CACHE_LINE_SIZE),
          .pic_w          = output_w,
          .pic_h          = output_h,
          .block_offset_x = 0,
          .block_offset_y = 0,
          .blend_cm       = ppa_blend_color_mode( output_bytes_per_pixel*8 ), // PPA_BLEND_COLOR_MODE_RGB565,
          .yuv_range      = ppa_color_range_t(),
          .yuv_std        = ppa_color_conv_std_rgb_yuv_t()
        },

        .bg_rgb_swap          = false,
        .bg_byte_swap         = fg->getSwapBytes(),
        .bg_alpha_update_mode = PPA_ALPHA_NO_CHANGE,
        .bg_alpha_fix_val     = 0x0,

        .fg_rgb_swap          = false,
        .fg_byte_swap         = bg->getSwapBytes(),
        .fg_alpha_update_mode = PPA_ALPHA_NO_CHANGE,
        .fg_alpha_fix_val     = 0xff,
        .fg_fix_rgb_val       = fg_fix_rgb_val,

        .bg_ck_en             = bg_ck_en,
        .bg_ck_rgb_low_thres  = bg_ck_rgb_low_thres,
        .bg_ck_rgb_high_thres = bg_ck_rgb_high_thres,

        .fg_ck_en             = fg_ck_en, // true
        .fg_ck_rgb_low_thres  = fg_ck_rgb_low_thres,
        .fg_ck_rgb_high_thres = fg_ck_rgb_high_thres,

        .ck_rgb_default_val   = ck_rgb_default_val,
        .ck_reverse_bg2fg     = false,

        .mode                 = async ? PPA_TRANS_MODE_NON_BLOCKING : PPA_TRANS_MODE_BLOCKING,
        .user_data            = (void*)this // attach semaphore
      };

      return PPABase::exec(&oper_config);
    }


    template <typename FG, typename BG, typename FGTransColor>
    bool pushImageBlend( FG* fg, BG* bg, FGTransColor fgtrans)
    {
      return pushImageBlend(fg, bg, true, fgtrans,fgtrans, false,0,0);
    }


    template <typename FG, typename BG, typename FGTransColor, typename BGTransColor>
    bool pushImageBlend( FG* fg, FGTransColor fgtrans, BG* bg, BGTransColor bgtrans)
    {
      return pushImageBlend(fg, bg, true, fgtrans,fgtrans, true, bgtrans,bgtrans);
    }


  };



  // ---------------------------------------------------------------------------------------------



  class PPASrm : public PPABase
  {

  private:
    ppa_srm_rotation_angle_t output_rotation = PPA_SRM_ROTATION_ANGLE_0;
    bool output_mirror_x = false;
    bool output_mirror_y = false;

    ppa_srm_rotation_angle_t getRotationFromAngle(float angle)
    {
      int rotation = fmod(angle, 360.0) / 90; // only 4 angles supported
      switch(rotation)
      {
        case 0: return PPA_SRM_ROTATION_ANGLE_0;
        case 1: return PPA_SRM_ROTATION_ANGLE_90;
        case 2: return PPA_SRM_ROTATION_ANGLE_180;
        case 3: return PPA_SRM_ROTATION_ANGLE_270;
      }
      return PPA_SRM_ROTATION_ANGLE_0;
    }

    ppa_srm_oper_config_t oper_config = ppa_srm_oper_config_t();

  public:

    ppa_srm_oper_config_t config() { return oper_config; }
    void config(ppa_srm_oper_config_t cfg) { oper_config=cfg; }


    template <typename GFX>
    PPASrm(GFX* out, bool async = true, bool use_semaphore = false)
    : PPABase(out, PPA_OPERATION_SRM, async, use_semaphore)
    {
      if(!inited)
        return;

      switch( outputGFX->getRotation()%8 )
      {
        case 0: output_rotation = PPA_SRM_ROTATION_ANGLE_0  ; output_mirror_x = false; output_mirror_y = false; break;
        case 1: output_rotation = PPA_SRM_ROTATION_ANGLE_270; output_mirror_x = false; output_mirror_y = false; break;
        case 2: output_rotation = PPA_SRM_ROTATION_ANGLE_180; output_mirror_x = false; output_mirror_y = false; break;
        case 3: output_rotation = PPA_SRM_ROTATION_ANGLE_90 ; output_mirror_x = false; output_mirror_y = false; break;
        case 4: output_rotation = PPA_SRM_ROTATION_ANGLE_0  ; output_mirror_x = false; output_mirror_y = true ; break;
        case 5: output_rotation = PPA_SRM_ROTATION_ANGLE_270; output_mirror_x = true ; output_mirror_y = false; break;
        case 6: output_rotation = PPA_SRM_ROTATION_ANGLE_180; output_mirror_x = true ; output_mirror_y = false; break;
        case 7: output_rotation = PPA_SRM_ROTATION_ANGLE_90 ; output_mirror_x = false; output_mirror_y = true ; break;
      }
      enabled = true;
    }

    // TODO: template <typename GFX> pushImageSRM(GFX* input, float dst_x, float dst_y, float scale_x, float scale_y) { }

    // NOTE: using the same signature as lgfx::pushImageRotateZoom()
    template <typename T>
    bool pushImageSRM(
      uint32_t dst_x, uint32_t dst_y,
      uint32_t src_x, uint32_t src_y,
      float angle,
      float scale_x, float scale_y,
      uint32_t src_w, uint32_t src_h,
      const T* input_buffer, bool swap_bytes=false
    ) {
      if(!inited)
        return false;

      if( sizeof(T) !=2 ) {
        ESP_LOGE(SRM_TAG, "Only 16bits colors supported");
        return false;
      }
      if(!input_buffer) {
        ESP_LOGE(SRM_TAG, "Buffer missing");
        enabled = false;
        return false;
      }

      auto rotation = getRotationFromAngle(angle);

      oper_config =
      {
        .in =
        {
          .buffer         = (const void*)input_buffer,
          .pic_w          = src_w,
          .pic_h          = src_h,
          .block_w        = src_w-src_x,
          .block_h        = src_h-src_y,
          .block_offset_x = src_x,
          .block_offset_y = src_y,
          .srm_cm         = ppa_srm_color_mode( sizeof(T)*8 ), // PPA_SRM_COLOR_MODE_RGB565,
          .yuv_range      = ppa_color_range_t(),
          .yuv_std        = ppa_color_conv_std_rgb_yuv_t()
        },
        .out =
        {
          .buffer         = output_buffer,
          .buffer_size    = output_w * output_h * output_bytes_per_pixel,
          // NOTE: axis for pic_* and block_offset_* will be inverted when output rotation is odd
          .pic_w          = output_w,
          .pic_h          = output_h,
          .block_offset_x = dst_x,
          .block_offset_y = dst_y,
          .srm_cm         = ppa_srm_color_mode( output_bytes_per_pixel*8 ), // PPA_SRM_COLOR_MODE_RGB565,
          .yuv_range      = ppa_color_range_t(),
          .yuv_std        = ppa_color_conv_std_rgb_yuv_t()
        },
        .rotation_angle    = angle==0 ? output_rotation : rotation,
        .scale_x           = scale_x,
        .scale_y           = scale_y,
        .mirror_x          = output_mirror_x,
        .mirror_y          = output_mirror_y,
        .rgb_swap          = 0,
        .byte_swap         = swap_bytes,
        .alpha_update_mode = PPA_ALPHA_NO_CHANGE,
        .alpha_fix_val     = 0xff,
        //.alpha_scale_ratio = 1.0f,
        .mode              = async ? PPA_TRANS_MODE_NON_BLOCKING : PPA_TRANS_MODE_BLOCKING,
        .user_data         = (void*)this
      };

      if( output_rotation%2==1 ) {
        std::swap( oper_config.out.pic_w, oper_config.out.pic_h );
        std::swap( oper_config.out.block_offset_x, oper_config.out.block_offset_y );
      }

      if(!async) {
        int32_t timeout = 10;
        while(!available() && timeout>0) {
          vTaskDelay(1);
          timeout--;
        }
      }

      enabled = true;

      return PPABase::exec(&oper_config);
    }

  };

}

using lgfx::PPA_Sprite;
using lgfx::PPASrm;
using lgfx::PPABlend;
using lgfx::PPAFill;
