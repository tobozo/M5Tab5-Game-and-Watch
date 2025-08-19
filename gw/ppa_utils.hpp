/*\
 *
 * M5Stack Tab5 Game&Watch Emulator
 *
 * A speculation brought to you by tobozo, copyleft (c+) 2025
 *
\*/
#pragma once

#include "./gfx_utils.hpp"

extern "C"
{
 #include "driver/ppa.h"
 #include "esp_heap_caps.h"
 #include "esp_cache.h"
 #include "esp_private/esp_cache_private.h"
}

extern GWBox GWFBBox;

namespace m5
{
  // NOTE: those are partial implementations specialized for the needs of this application


  const char* SRM_TAG = "LGFX_PPA_SRM";
  const char* BLEND_TAG = "LGFX_PPA_BLEND";

  static bool lgfx_ppa_transfer_done = true;

  bool IRAM_ATTR lgfx_ppa_cb_sem_func(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data)
  {
    lgfx_ppa_transfer_done = true;

    if( user_data ) {
      BaseType_t xHigherPriorityTaskWoken = pdFALSE;
      SemaphoreHandle_t sem = (SemaphoreHandle_t)user_data;
      xSemaphoreGiveFromISR(sem, &xHigherPriorityTaskWoken);
      return (xHigherPriorityTaskWoken == pdTRUE);
    }
    ESP_LOGE("LGFX_PPA", "ppa callback triggered without semaphore");
    return false;
  }

  bool lgfx_ppa_cb_bool_func(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data)
  {
    lgfx_ppa_transfer_done = true;
    return false;
  }

  static SemaphoreHandle_t ppa_semaphore = NULL;


  class LGFX_PPA_BLEND
  {
  private:
    ppa_client_config_t ppa_client_config;
    ppa_client_handle_t ppa_client_handle = nullptr;
    ppa_event_callbacks_t ppa_event_cb;
    bool async = false;
    bool use_semaphore = false;
    bool enabled = false;
    const uint32_t output_w;// = M5.Display.width();
    const uint32_t output_h;// = M5.Display.height();
    const uint8_t output_bytes_per_pixel;
    void* output_buffer;
  public:

    ppa_blend_oper_config_t oper_config;
    ppa_out_pic_blk_config_t output;
    ppa_in_pic_blk_config_t in_bg;
    ppa_in_pic_blk_config_t in_fg;

    LGFX_PPA_BLEND(size_t output_w, size_t output_h, uint8_t bytes_per_pixel, void*buffer)
    : output_w(output_w), output_h(output_h), output_bytes_per_pixel(bytes_per_pixel), output_buffer(buffer)
    {
    }

    void setup(GWBox fg, void *fg_buf, GWBox bg, void *bg_buf, bool async = true, bool use_semaphore = false)
    {
      if(!fg_buf || !bg_buf || !output_buffer) {
        ESP_LOGE(BLEND_TAG, "At least one buffer is missing");
        return;
      }

      // The blocks' width/height of FG and BG should be identical, and are the width/height values for the output block.
      if( fg.w != output_w || fg.h != output_h || bg.w != output_w || bg.h != output_h ) {
        ESP_LOGE(BLEND_TAG, "Dimensions mismatch");
        return;
      }

      if(!async && use_semaphore) {
        ESP_LOGW(BLEND_TAG, "use_semaphore=true but async=false, async value will be ignored");
        async = true;
      }

      this->use_semaphore = use_semaphore;
      this->async = async;

      size_t data_cache_line_size;

      ESP_ERROR_CHECK(esp_cache_get_alignment(MALLOC_CAP_SPIRAM, &data_cache_line_size));

      Serial.printf("Setting up PPA-Blend-%s%s (data_cache_line_size=%d)\n",
        async?"async":"sync",
        use_semaphore?"+mux":"",
        data_cache_line_size
      );

      if( ppa_semaphore == NULL )
        ppa_semaphore = xSemaphoreCreateBinary();

      ppa_client_config.data_burst_length = PPA_DATA_BURST_LENGTH_128;
      ppa_client_config.oper_type = PPA_OPERATION_BLEND;
      ppa_client_config.max_pending_trans_num = 1;

      if(async && use_semaphore)
        ppa_event_cb.on_trans_done = lgfx_ppa_cb_sem_func;
      else // if(async)
        ppa_event_cb.on_trans_done = lgfx_ppa_cb_bool_func;

      #pragma GCC diagnostic push
      #pragma GCC diagnostic ignored "-Wmissing-field-initializers"

      // If the color mode of the input picture is PPA_BLEND_COLOR_MODE_A4, then its block_w and block_offset_x fields must be even.
      in_bg = {
        .buffer         = bg_buf,
        .pic_w          = output_w,
        .pic_h          = output_h,
        .block_w        = output_w,
        .block_h        = output_h,
        .block_offset_x = 0,
        .block_offset_y = 0,
        .blend_cm       = PPA_BLEND_COLOR_MODE_RGB565
      };

      // If the color mode of the input picture is PPA_BLEND_COLOR_MODE_A4, then its block_w and block_offset_x fields must be even.
      in_fg = {
        .buffer         = fg_buf,
        .pic_w          = output_w,
        .pic_h          = output_h,
        .block_w        = output_w,
        .block_h        = output_h,
        .block_offset_x = 0,
        .block_offset_y = 0,
        .blend_cm       = PPA_BLEND_COLOR_MODE_RGB565,
      };

      output = {
        .buffer         = output_buffer,
        .buffer_size    = output_w*output_h*output_bytes_per_pixel,
        .pic_w          = output_w,
        .pic_h          = output_h,
        .block_offset_x = 0,
        .block_offset_y = 0,
        .blend_cm       = PPA_BLEND_COLOR_MODE_RGB565,
      };

      #pragma GCC diagnostic pop

      oper_config.in_bg = in_bg;
      oper_config.in_fg = in_fg;
      oper_config.out = output;

      oper_config.bg_alpha_update_mode = PPA_ALPHA_NO_CHANGE;
      //oper_config.bg_alpha_fix_val = 0x80;

      oper_config.bg_ck_rgb_low_thres = (color_pixel_rgb888_data_t) {};
      oper_config.bg_ck_rgb_high_thres = (color_pixel_rgb888_data_t) {};


      oper_config.fg_alpha_update_mode = PPA_ALPHA_NO_CHANGE;

      oper_config.fg_ck_rgb_low_thres = (color_pixel_rgb888_data_t) {
        .b = 0,
        .g = 0,
        .r = 0,
      };
      oper_config.fg_ck_rgb_high_thres = (color_pixel_rgb888_data_t) {
        .b = 0x1,
        .g = 0x1,
        .r = 0x1,
      };

      oper_config.fg_ck_en = true;
      oper_config.bg_ck_en = false;
      oper_config.fg_byte_swap = false;
      oper_config.bg_byte_swap = false;
      oper_config.ck_reverse_bg2fg = false;

      oper_config.mode = async ? PPA_TRANS_MODE_NON_BLOCKING : PPA_TRANS_MODE_BLOCKING;

      if (async && use_semaphore) {
        oper_config.user_data = (void *)ppa_semaphore; // attach semaphore
      } else {
        oper_config.user_data = nullptr;
      }

      enabled = true;
    }


    bool exec()
    {
      if(!available())
        return false;

      if (ppa_client_handle) { // skipped on first run
        if (async && use_semaphore /*oper_config.mode == PPA_TRANS_MODE_NON_BLOCKING*/) {
          if( xSemaphoreTake(ppa_semaphore, ( TickType_t )10 ) != pdTRUE ) {
            Serial.println("Failed to take semaphore");
            return false;
          }
        }
        if( ESP_OK != ppa_unregister_client(ppa_client_handle) )
          return false;
      }

      //if( async /*oper_config.mode == PPA_TRANS_MODE_NON_BLOCKING*/ ) {
        lgfx_ppa_transfer_done = false;
      //}

      if( ESP_OK != ppa_register_client(&ppa_client_config, &ppa_client_handle) )
        return false;
      if( ESP_OK != ppa_client_register_event_callbacks(ppa_client_handle, &ppa_event_cb) )
        return false;

      //esp_cache_msync((void*)in_bg.buffer, in_bg.block_w*in_bg.block_h*sizeof(uint16_t), ESP_CACHE_MSYNC_FLAG_DIR_C2M);
      //esp_cache_msync((void*)in_fg.buffer, in_fg.block_w*in_fg.block_h*sizeof(uint16_t), ESP_CACHE_MSYNC_FLAG_DIR_C2M);
      ppa_do_blend(ppa_client_handle, &oper_config);
      //esp_cache_msync((void*)output.buffer, output.block_w*output.block_h*sizeof(uint16_t), ESP_CACHE_MSYNC_FLAG_DIR_M2C);

      // if(!async)
      //   lgfx_ppa_transfer_done = true;
      return true;
    }

    bool available()
    {
      if(!enabled)
        return false;
      //if( oper_config.mode == PPA_TRANS_MODE_NON_BLOCKING )
        return lgfx_ppa_transfer_done;
      //return true; // mode=PPA_TRANS_MODE_BLOCKING, exec is always available
    }

  };


  // compensate for typeid() being disabled
  template <typename T> const char *TYPE_NAME()
  {
    #ifdef _MSC_VER
      return __FUNCSIG__;
    #else
      return __PRETTY_FUNCTION__;
    #endif
  }




  class LGFX_PPA_SRM
  {

  private:
    ppa_client_config_t ppa_client_config;
    ppa_client_handle_t ppa_client_handle = nullptr;
    ppa_event_callbacks_t ppa_event_cb;
    bool async = false;
    bool use_semaphore = false;
    bool enabled = false;
    bool inited = false;
    const uint32_t output_w;
    const uint32_t output_h;
    uint8_t output_bytes_per_pixel;
    void* output_buffer;
    ppa_srm_rotation_angle_t output_rotation = PPA_SRM_ROTATION_ANGLE_0;
    bool output_mirror_x = false;
    bool output_mirror_y = false;

  protected:
    M5GFX* outputGFX;

  public:

    ppa_in_pic_blk_config_t in;
    ppa_out_pic_blk_config_t out;
    ppa_srm_oper_config_t oper_config;


    ~LGFX_PPA_SRM() {
      if( ppa_client_handle )
        ppa_unregister_client(ppa_client_handle);
    }

    template <typename GFX>
    LGFX_PPA_SRM(GFX* out) : output_w(out->width()), output_h(out->height()), outputGFX((M5GFX*)out)
    {
      static const char* display_types[] = {
        "Unknown",
        "Panel DSI",
        "LGFX Sprite",
      };
      int display_id = 0;
      if( std::is_same<GFX, M5GFX>::value || std::is_same<GFX, LovyanGFX>::value  ) {
        output_buffer = ((m5gfx::Panel_DSI*)outputGFX->getPanel())->config_detail().buffer;
        output_bytes_per_pixel = 2; // panelDSI->getColorDepth() returns a weird value, so 16bits colors it is...
        display_id = 1;
      } else if( std::is_same<GFX, LGFX_Sprite>::value ) {
        if( outputGFX->getColorDepth() < 16 ) {
          ESP_LOGE(SRM_TAG, "Unsupported bit depth: %d", outputGFX->getColorDepth() );
          return;
        }
        output_bytes_per_pixel = out->getColorDepth()/8;
        output_buffer = ((LGFX_Sprite*)outputGFX)->getBuffer();
        display_id = 2;
      } else {
        ESP_LOGE(SRM_TAG, "Unsupported type: %s", TYPE_NAME<GFX>() );
        return;
      }
      switch( outputGFX->getRotation() )
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
      ESP_LOGI(SRM_TAG, "[%s] %s@%dBpp, rot=%d=>%d, mirrorx=%d, mirrory=%d",
        TYPE_NAME<GFX>(),
        display_types[display_id],
        output_bytes_per_pixel,
        outputGFX->getRotation(),
        output_rotation,
        output_mirror_x,
        output_mirror_y
      );
      inited = true;
    }


    LGFX_PPA_SRM(size_t output_w, size_t output_h, uint8_t bytes_per_pixel, void*buffer, ppa_srm_rotation_angle_t output_rotation)
    : output_w(output_w), output_h(output_h), output_bytes_per_pixel(bytes_per_pixel), output_buffer(buffer), output_rotation(output_rotation)
    {
    }

    void setup(GWBox input,  double scale_x, double scale_y, void *buffer, bool async = true, bool use_semaphore = false)
    {
      if(!inited) {
        ESP_LOGE(SRM_TAG, "Can't setup after a failed init");
        return;
      }

      if(!buffer) {
        ESP_LOGE(SRM_TAG, "Buffer missing");
        enabled = false;
        return;
      }

      if(scale_x<=0 || scale_y <=0) {
        ESP_LOGE(SRM_TAG, "Bad scale values");
        enabled = false;
        return;
      }

      if(!async && use_semaphore) {
        ESP_LOGW(SRM_TAG, "use_semaphore=true but async=false, async value will be ignored");
        async = true;
      }

      this->use_semaphore = use_semaphore;
      this->async = async;

      ESP_LOGD(SRM_TAG, "Setting up PPA-SRM-%s%s to box @[x=%d,y=%d] -> in[w=%d,h=%d] * [zoomx=%.2f,zoomy=%.2f] = out[w=%d,h=%d]",
        async?"async":"sync",
        use_semaphore?"+mux":"",
        (int)input.x, (int)input.y, (int)input.w, (int)input.h,
        scale_x, scale_y,
        (int)(input.w*scale_x), (int)(input.h*scale_y)
      );

      if( ppa_semaphore == NULL )
        ppa_semaphore = xSemaphoreCreateBinary();

      ppa_client_config.data_burst_length = PPA_DATA_BURST_LENGTH_128;
      ppa_client_config.oper_type = PPA_OPERATION_SRM;
      ppa_client_config.max_pending_trans_num = 1;

      if(async && use_semaphore)
        ppa_event_cb.on_trans_done = lgfx_ppa_cb_sem_func;
      else// if(async)
        ppa_event_cb.on_trans_done = lgfx_ppa_cb_bool_func;

      oper_config = getCfg(GWFBBox, buffer, input, scale_x, scale_y);

      enabled = true;
    }


    bool exec()
    {
      if(!available())
        return false;

      if (ppa_client_handle) { // skipped on first run
        if (async && use_semaphore /*oper_config.mode == PPA_TRANS_MODE_NON_BLOCKING*/) {
          if( xSemaphoreTake(ppa_semaphore, ( TickType_t )10 ) != pdTRUE ) {
            Serial.println("Failed to take semaphore");
            return false;
          }
        }
        if( ESP_OK != ppa_unregister_client(ppa_client_handle) )
          return false;
      }

      //if( async /*oper_config.mode == PPA_TRANS_MODE_NON_BLOCKING*/ ) {
        lgfx_ppa_transfer_done = false;
      //}

      if( ESP_OK != ppa_register_client(&ppa_client_config, &ppa_client_handle) )
        return false;
      if( ESP_OK != ppa_client_register_event_callbacks(ppa_client_handle, &ppa_event_cb) )
        return false;

      //esp_cache_msync((void*)in.buffer, in.block_w*in.block_h*sizeof(uint16_t), ESP_CACHE_MSYNC_FLAG_DIR_C2M);
      ppa_do_scale_rotate_mirror(ppa_client_handle, &oper_config);
      //esp_cache_msync(out.buffer, out.buffer_size, ESP_CACHE_MSYNC_FLAG_DIR_M2C);

      // if(!async)
      //   lgfx_ppa_transfer_done = true;
      return true;
    }

    bool available()
    {
      if(!enabled)
        return false;
      if( oper_config.mode == PPA_TRANS_MODE_NON_BLOCKING )
        return lgfx_ppa_transfer_done;
      return true; // mode=PPA_TRANS_MODE_BLOCKING, exec is always available
    }

  protected:

    ppa_in_pic_blk_config_t getInBlockCfg(GWBox input, void *buffer)
    {

      in.buffer  = buffer;
      in.pic_w   = input.w;
      in.pic_h   = input.h;
      in.block_w = input.w;
      in.block_h = input.h;
      in.block_offset_x = 0;
      in.block_offset_y = 0;
      in.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
      // gpu_cfg.in.blend_cm = PPA_BLEND_COLOR_MODE_RGB565;
      // gpu_cfg.in.fill_cm = PPA_FILL_COLOR_MODE_RG
      return in;
    }

    ppa_out_pic_blk_config_t getOutBlockCfg(GWBox block)
    {
      out.buffer = output_buffer;
      out.buffer_size = output_w * output_h * output_bytes_per_pixel;
      // NOTE: invert width/height when rotation is odd
      out.pic_w = output_h;
      out.pic_h = output_w;
      out.block_offset_x = block.y;
      out.block_offset_y = block.x;
      out.srm_cm = PPA_SRM_COLOR_MODE_RGB565;
      return out;
    }

    ppa_srm_oper_config_t getCfg(GWBox input, void *buffer, GWBox output, float scale_x, float scale_y)
    {
      #pragma GCC diagnostic push
      #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
      ppa_srm_oper_config_t gpu_cfg =
      {
        .in  = getInBlockCfg(input, buffer),
        .out = getOutBlockCfg(output),
        .rotation_angle = output_rotation, // tft.getRotation()==1 ? PPA_SRM_ROTATION_ANGLE_270 : PPA_SRM_ROTATION_ANGLE_90,
        .scale_x = scale_x,
        .scale_y = scale_y,
        .rgb_swap = 0,
        .byte_swap = true,
        // .mirror_x = 0,
        // .mirror_y = 0,
        // .alpha_update_mode = PPA_ALPHA_SCALE,
        // .alpha_scale_ratio = 0.5f;
        .mode = async ? PPA_TRANS_MODE_NON_BLOCKING : PPA_TRANS_MODE_BLOCKING,
      };
      #pragma GCC diagnostic pop
      if (async && use_semaphore) {
        gpu_cfg.user_data = (void *)ppa_semaphore; // attach semaphore
      } else {
        gpu_cfg.user_data = nullptr;
      }
      return gpu_cfg;
    }
  };

}


using m5::LGFX_PPA_SRM;
using m5::LGFX_PPA_BLEND;
