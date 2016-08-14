#include "log_trivial.h"

std::shared_ptr<spdlog::logger> logger;

namespace spd = spdlog;

void init_logger()
{
    //spdlog::set_async_mode(4096);
    logger = spd::stderr_logger_mt("console", false);
    
}

void destroy_logger()
{
    spdlog::drop_all();
}
