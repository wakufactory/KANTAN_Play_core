#include <M5Unified.h>

#include "task_spi.hpp"
#include "gui.hpp"
#include "file_manage.hpp"

#include "system_registry.hpp"

namespace kanplay_ns {
//-------------------------------------------------------------------------

void task_spi_t::start(void)
{
  M5.Display.fillScreen(0);

  gui.init();
#if defined (M5UNIFIED_PC_BUILD)
  auto thread = SDL_CreateThread((SDL_ThreadFunction)task_func, "spi", this);
#else
  TaskHandle_t handle = nullptr;
  xTaskCreatePinnedToCore((TaskFunction_t)task_func, "spi", 4096, this, def::system::task_priority_spi, &handle, def::system::task_cpu_spi);
  system_registry.file_command.setNotifyTaskHandle(handle);
#endif
}

void task_spi_t::task_func(task_spi_t* me)
{
  registry_t::history_code_t history_code = 0;

  bool flg_notify = true;
  gui.startWrite();
  for (;;) {
    if (flg_notify) {
      gui.endWrite();
      // int proc_remain = 4;
      const registry_t::history_t* history;
      def::app::file_command_info_t file_command_info;
      while (nullptr != (history = system_registry.file_command.getHistory(history_code))) {
        file_command_info.raw = history->value;

M5_LOGV("file_command_info type:%d file:%d mem:%d ", file_command_info.dir_type, file_command_info.file_index, file_command_info.mem_index);

        switch (history->index) {
        default:
          break;

        case system_registry_t::reg_file_command_t::index_t::UPDATE_LIST:
          {
            file_manage.updateFileList(file_command_info.dir_type);
          }
          break;

        case system_registry_t::reg_file_command_t::index_t::FILE_LOAD:
          {
            auto mem = file_manage.loadFile(file_command_info.dir_type, file_command_info.file_index);
            if (mem != nullptr) {
              system_registry.operator_command.addQueue( { def::command::load_from_memory, mem->index } );
            } else {
              //TODO:読込に失敗した通知を何らかの形で行う
            }
            if (mem == nullptr || mem->dir_type != def::app::data_type_t::data_setting) {
              system_registry.popup_notify.setPopup(mem != nullptr, def::notify_type_t::NOTIFY_FILE_LOAD);
            }
          }
          break;

        case system_registry_t::reg_file_command_t::index_t::FILE_SAVE:
          {
            bool result = file_manage.saveFile(file_command_info.dir_type, file_command_info.mem_index);
            if (file_command_info.dir_type != def::app::data_type_t::data_setting) {
              system_registry.popup_notify.setPopup(result, def::notify_type_t::NOTIFY_FILE_SAVE);
            }
            if (result) {
              // 保存に成功していれば、未保存の編集の警告表示をクリア
              system_registry.runtime_info.setSongModified(false);
            }
          }
          break;
        }
      }
//       {
//         int file_index = system_registry.file_command.getFileIndex();
// // TODO:★★★file_commandにdir_typeやindexを設定する作業をすること★★★★
//         auto mem_index = file_manage.loadFile(def::app::data_type_t dir_type, file_index);

//         int result = file_manage.loadFromFileList(file_index);
//         if (result > 0) {
//           system_registry.operator_command.addQueue( { def::command::load_from_memory, 1 } );
//         }
//       }
      gui.startWrite();
    }
    int wait = !gui.update() ? 8 : 1;
#if defined (M5UNIFIED_PC_BUILD)
    M5.delay(wait);
    flg_notify = true;
#else
    system_registry.task_status.setSuspend(system_registry_t::reg_task_status_t::bitindex_t::TASK_SPI);
    flg_notify = ulTaskNotifyTake(pdTRUE, wait);
    system_registry.task_status.setWorking(system_registry_t::reg_task_status_t::bitindex_t::TASK_SPI);
#endif
  }
}

//-------------------------------------------------------------------------
}; // namespace kanplay_ns
