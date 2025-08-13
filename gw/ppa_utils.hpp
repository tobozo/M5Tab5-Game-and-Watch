#pragma once

#include "./gfx_utils.hpp"

extern "C"
{
 #include "driver/ppa.h"
 #include "esp_heap_caps.h"
 #include "esp_cache.h"
}

extern GWBox GWFBBox;

namespace m5
{

  static bool lgfx_ppa_srm_transfer_done = true;

  bool IRAM_ATTR lgfx_ppa_cb_sem_func(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data)
  {
    lgfx_ppa_srm_transfer_done = true;

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
    lgfx_ppa_srm_transfer_done = true;
    return false;
  }


  static SemaphoreHandle_t semaphore = NULL;


  class LGFX_PPA
  {

  private:
    ppa_client_config_t ppa_client_config;
    ppa_client_handle_t ppa_client_handle = nullptr;
    ppa_event_callbacks_t ppa_event_cb;
    bool async = false;
    bool use_semaphore = false;
    bool enabled = false;
  public:

    ppa_in_pic_blk_config_t in;
    ppa_out_pic_blk_config_t out;
    ppa_srm_oper_config_t oper_config;

    LGFX_PPA() { }

    void setup(GWBox input,  double scale_x, double scale_y, void *buffer, bool async = true, bool use_semaphore = false)
    {
      if(!buffer) {
        ESP_LOGE("LGFX_PPA", "Buffer missing");
        return;
      }

      if(!async && use_semaphore) {
        ESP_LOGW("LGFX_PPA", "use_semaphore=true but async=false, async value will be ignored");
        async = true;
      }

      this->use_semaphore = use_semaphore;
      this->async = async;

      Serial.printf("Setting up PPA-%s%s to box @[x=%d,y=%d] -> in[w=%d,h=%d] * [zoomx=%.2f,zoomy=%.2f] = out[w=%d,h=%d]\n",
        async?"async":"sync",
        use_semaphore?"+mux":"",
        (int)input.x, (int)input.y, (int)input.w, (int)input.h,
        scale_x, scale_y,
        (int)(input.w*scale_x), (int)(input.h*scale_y)
      );

      if( semaphore == NULL )
        semaphore = xSemaphoreCreateBinary();

      ppa_client_config.data_burst_length = PPA_DATA_BURST_LENGTH_128;
      ppa_client_config.oper_type = PPA_OPERATION_SRM;
      ppa_client_config.max_pending_trans_num = 1;

      if(async && use_semaphore)
        ppa_event_cb.on_trans_done = lgfx_ppa_cb_sem_func;
      else if(async)
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
          if( xSemaphoreTake(semaphore, ( TickType_t )10 ) != pdTRUE ) {
            Serial.println("Failed to take semaphore");
            return false;
          }
        }
        if( ESP_OK != ppa_unregister_client(ppa_client_handle) )
          return false;
      }

      if( async /*oper_config.mode == PPA_TRANS_MODE_NON_BLOCKING*/ ) {
        lgfx_ppa_srm_transfer_done = false;
      }

      if( ESP_OK != ppa_register_client(&ppa_client_config, &ppa_client_handle) )
        return false;
      if( ESP_OK != ppa_client_register_event_callbacks(ppa_client_handle, &ppa_event_cb) )
        return false;

      //esp_cache_msync((void*)in.buffer, in.block_w*in.block_h*sizeof(uint16_t), ESP_CACHE_MSYNC_FLAG_DIR_C2M);
      ppa_do_scale_rotate_mirror(ppa_client_handle, &oper_config);
      //esp_cache_msync(out.buffer, out.buffer_size, ESP_CACHE_MSYNC_FLAG_DIR_M2C);

      if(!async)
        lgfx_ppa_srm_transfer_done = true;
      return true;
    }

    bool available()
    {
      if(!enabled)
        return false;
      if( oper_config.mode == PPA_TRANS_MODE_NON_BLOCKING )
        return lgfx_ppa_srm_transfer_done;
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
      auto panel = (m5gfx::Panel_DSI*)M5.Display.getPanel();
      const uint32_t output_w = M5.Display.width();
      const uint32_t output_h = M5.Display.height();
      out.buffer = panel->config_detail().buffer;
      out.buffer_size = output_w * output_h * sizeof(uint16_t);
      // NOTE: invert width/height when rotation is odd
      out.pic_w = panel->height();
      out.pic_h = panel->width();
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
        .rotation_angle = tft.getRotation()==1 ? PPA_SRM_ROTATION_ANGLE_270 : PPA_SRM_ROTATION_ANGLE_90,
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
        gpu_cfg.user_data = (void *)semaphore; // attach semaphore
      } else {
        gpu_cfg.user_data = nullptr;
      }
      return gpu_cfg;
    }
  };

}


using m5::LGFX_PPA;
